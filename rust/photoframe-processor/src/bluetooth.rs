use anyhow::{anyhow, Context, Result};
use btleplug::api::{
    Central, Characteristic, Manager as _, Peripheral as _, PeripheralProperties, ScanFilter,
    WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use crc32fast::Hasher as Crc32Hasher;
use indicatif::{ProgressBar, ProgressStyle};
use photoframe_lib::{parse_bin_file, BinHeader};
use std::path::Path;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio::runtime::Runtime;
use tokio::time::{sleep, timeout};
use uuid::Uuid;

#[allow(dead_code)]
const SERVICE_UUID: Uuid = Uuid::from_u128(0x0000180a_0000_1000_8000_00805f9b34fb);
const CONFIG_CHAR_UUID: Uuid = Uuid::from_u128(0x00002a29_0000_1000_8000_00805f9b34fb);
const IMAGE_CHAR_UUID: Uuid = Uuid::from_u128(0x00002a2a_0000_1000_8000_00805f9b34fb);
#[allow(dead_code)]
const STATUS_CHAR_UUID: Uuid = Uuid::from_u128(0x00002a2b_0000_1000_8000_00805f9b34fb);
const DEVICE_INFO_CHAR_UUID: Uuid = Uuid::from_u128(0x00002a2c_0000_1000_8000_00805f9b34fb);
const MANUFACTURER_ID: u16 = 0x1337; // Custom identifier for PhotoFrame devices
const MANUFACTURER_MAGIC: &[u8] = b"PFR1"; // Photo Frame Rev1
const DEVICE_PREFIX: &str = "ESP32-PhotoFrame";
const DEVICE_FALLBACK_PREFIX: &str = "PhotoFrame-";
const SCAN_SECONDS: u64 = 8;
const CHUNK_SIZE: usize = 512; // Conservative chunk size for reliability

#[repr(C, packed)]
struct BtImageConfig {
    magic: u16,
    version: u8,
    rotation: u8,
    width: u16,
    height: u16,
    timestamp: u32,
    image_size: u32,
    crc16: u16,
}

#[repr(C, packed)]
#[allow(dead_code)]
struct BtDeviceConfig {
    version: u8,
    display_type: u8,
    width: u16,
    height: u16,
    current_rotation: u8,
    mtu_size: u16,
}

pub struct DeviceInfo {
    pub version: u8,
    pub display_type: u8,
    pub width: u16,
    pub height: u16,
    pub current_rotation: u8,
    pub mtu_size: u16,
}

#[derive(Debug, Clone)]
#[allow(dead_code)]
struct LocalBinHeader(BinHeader);

pub fn scan_devices() -> Result<()> {
    let rt = Runtime::new()?;
    rt.block_on(async move {
        let manager = Manager::new().await?;
        let adapter = pick_adapter(&manager).await?;

        let pb = ProgressBar::new_spinner();
        pb.set_style(
            ProgressStyle::default_spinner()
                .template("{spinner:.blue} {msg}")
                .unwrap(),
        );
        pb.set_message("Scanning for PhotoFrame devices...");

        adapter
            .start_scan(ScanFilter::default())
            .await
            .context("Failed to start BLE scan")?;

        for i in 0..SCAN_SECONDS {
            pb.set_message(format!(
                "Scanning for PhotoFrame devices... ({}/{}s)",
                i + 1,
                SCAN_SECONDS
            ));
            sleep(Duration::from_secs(1)).await;
        }

        let peripherals = adapter.peripherals().await?;
        let mut matches = Vec::new();

        let pb = ProgressBar::new(peripherals.len() as u64);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("{spinner:.blue} Scanning devices [{bar:20}] {pos}/{len}")
                .unwrap()
                .progress_chars("=> "),
        );

        for peripheral in peripherals {
            if let Some(props) = peripheral.properties().await? {
                if is_photoframe(&props) {
                    matches.push((peripheral.clone(), props));
                }
            }
            pb.inc(1);
        }
        pb.finish_and_clear();

        adapter.stop_scan().await.ok();

        if matches.is_empty() {
            println!("\nNo PhotoFrame devices found.");
            println!("Make sure your PhotoFrame is powered on and in Bluetooth mode.");
            return Ok(());
        }

        println!("\nFound {} PhotoFrame device(s):\n", matches.len());
        for (idx, (p, props)) in matches.iter().enumerate() {
            let name = props
                .local_name
                .clone()
                .unwrap_or_else(|| "Unknown".to_string());

            let has_mfg = props
                .manufacturer_data
                .iter()
                .any(|(id, data)| *id == MANUFACTURER_ID && data.starts_with(MANUFACTURER_MAGIC));

            let marker = if has_mfg { "[MFG]" } else { "[NAME]" };

            let ident = prefer_identifier(p, props);
            let ident_label = if ident.contains(':') { "MAC" } else { "ID" };
            println!(
                "  [{}] {} {} ({}: {})",
                idx, marker, name, ident_label, ident
            );
        }

        println!("\nTo upload to a specific device, use:");
        println!("  --upload --device <NAME|MAC>");

        Ok(())
    })
}

/// Upload a binary image file to a PhotoFrame device with explicit dimensions
pub fn upload_image_with_dimensions(
    bin_path: &Path,
    _width: u16,
    _height: u16,
    _rotation: u8,
    device_hint: Option<&str>,
) -> Result<()> {
    let raw_data = std::fs::read(bin_path)
        .with_context(|| format!("Failed to read binary image: {}", bin_path.display()))?;

    let (bin_header, payload, payload_crc) =
        parse_bin_file(&raw_data).context("Failed to parse .bin header")?;
    validate_payload_crc(payload, payload_crc)?;
    // Own the payload for the async block
    let payload: Vec<u8> = payload.to_vec();

    let rt = Runtime::new()?;
    rt.block_on(async move {
        let manager = Manager::new().await?;
        let adapter = pick_adapter(&manager).await?;
        let peripheral = scan_and_select(&adapter, device_hint).await?;

        println!(
            "Connecting to {}...",
            describe_peripheral(&peripheral).await?
        );
        peripheral
            .connect()
            .await
            .context("Failed to connect to device")?;
        println!("✓ Connected");

        println!("Discovering BLE services...");
        peripheral
            .discover_services()
            .await
            .context("Failed to discover BLE services")?;
        println!("✓ Services discovered");

        // Read device configuration
        println!("Reading device configuration...");
        let device_info = read_device_info(&peripheral).await?;
        println!("✓ Device configuration received");

        // Validate payload size vs device display
        let expected_bw_size = (bin_header.width as usize) * (bin_header.height as usize);
        let expected_6c_size = expected_bw_size; // packed 6c should match pixel count for now

        let bw = bin_header.color_mode == 0;
        let w = bin_header.width as usize;
        let h = bin_header.height as usize;
        let is_valid_size = if !bw {
            payload.len() == expected_6c_size || payload.len() == (w * h * 3) / 8
        } else {
            payload.len() == expected_bw_size
        };

        if !is_valid_size {
            eprintln!("\n⚠ Warning: Image size mismatch (file vs header expectations)!");
            eprintln!("  Got {} bytes", payload.len());
            if bin_header.color_mode == 1 {
                eprintln!(
                    "  Expected {} bytes (for 6-color {}x{})",
                    expected_6c_size, w, h
                );
            } else {
                eprintln!(
                    "  Expected {} bytes (for B/W {}x{})",
                    expected_bw_size, w, h
                );
            }
        }

        // Warn if file color mode doesn't match device
        if (bin_header.color_mode == 1 && device_info.display_type == 0)
            || (bin_header.color_mode == 0 && device_info.display_type == 1)
        {
            eprintln!(
                "\n⚠ Warning: File color mode ({}) does not match device ({})",
                if bin_header.color_mode == 1 {
                    "6C"
                } else {
                    "BW"
                },
                if device_info.display_type == 1 {
                    "6C"
                } else {
                    "BW"
                }
            );
        }

        // Get effective chunk size from device
        let chunk_size = if device_info.mtu_size > 0 && device_info.mtu_size <= 4096 {
            device_info.mtu_size as usize
        } else {
            eprintln!(
                "⚠ Invalid MTU size {}, using default {}",
                device_info.mtu_size, CHUNK_SIZE
            );
            CHUNK_SIZE
        };

        println!("\n📷 Image Configuration (from file header):");
        println!("   Size: {} bytes", payload.len());
        println!("   Dimensions: {}x{}", w, h);
        println!(
            "   Display: {}",
            if bin_header.color_mode == 1 {
                "6-color"
            } else {
                "B/W"
            }
        );
        println!("   Rotation: {}", bin_header.rotation);
        println!("   Chunk size: {} bytes", chunk_size);

        let chars: Vec<Characteristic> = peripheral.characteristics().into_iter().collect();
        let config_char = find_char(&chars, CONFIG_CHAR_UUID)?;
        let image_char = find_char(&chars, IMAGE_CHAR_UUID)?;

        println!("\n📤 Sending configuration...");
        let config_payload = build_config_payload(
            &payload,
            bin_header.width,
            bin_header.height,
            bin_header.rotation,
        );
        peripheral
            .write(&config_char, &config_payload, WriteType::WithResponse)
            .await
            .context("Failed to write BLE config")?;
        println!("✓ Configuration sent");

        sleep(Duration::from_millis(500)).await;

        println!("\n📤 Sending image data...");
        let num_chunks = (payload.len() + chunk_size - 1) / chunk_size;
        let pb = ProgressBar::new(payload.len() as u64);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("{spinner:.green} Uploading [{bar:30}] {bytes}/{total_bytes} ({eta})")
                .unwrap()
                .progress_chars("=> "),
        );

        for (i, chunk) in payload.chunks(chunk_size).enumerate() {
            peripheral
                .write(&image_char, chunk, WriteType::WithResponse)
                .await
                .context("Failed to write image chunk")?;

            if i % 10 == 0 {
                sleep(Duration::from_millis(1)).await;
            }

            pb.inc(chunk.len() as u64);
            pb.set_message(format!("chunk {}/{}", i + 1, num_chunks));
        }
        pb.finish_with_message("Upload complete ✔");

        println!("All chunks sent. Waiting 2s before disconnect...");

        sleep(Duration::from_secs(2)).await;

        println!("Disconnecting...");
        match timeout(Duration::from_secs(5), peripheral.disconnect()).await {
            Ok(res) => {
                if let Err(e) = res {
                    eprintln!("⚠ Disconnect error: {e}");
                } else {
                    println!("Disconnected");
                }
            }
            Err(_) => {
                eprintln!("⚠ Disconnect timed out after 5s; proceeding");
            }
        }
        println!("\n✓ Transfer complete!");
        Ok(())
    })
}

async fn read_device_info(peripheral: &Peripheral) -> Result<DeviceInfo> {
    println!("\n📖 Reading device info...");
    let chars: Vec<Characteristic> = peripheral.characteristics().into_iter().collect();
    println!("   Found {} characteristics", chars.len());

    let device_info_char = find_char(&chars, DEVICE_INFO_CHAR_UUID)?;
    println!(
        "   Found device info characteristic: {}",
        DEVICE_INFO_CHAR_UUID
    );

    println!("   Requesting device config from device...");
    let device_data = peripheral
        .read(&device_info_char)
        .await
        .context("Failed to read device info")?;

    println!(
        "   Received {} bytes: {:02x?}",
        device_data.len(),
        device_data
    );

    if device_data.len() < 9 {
        return Err(anyhow!(
            "Device info too short: {} bytes (expected 9)",
            device_data.len()
        ));
    }

    // Parse BTDeviceConfig struct: BBHHBH = 9 bytes
    let version = device_data[0];
    let display_type = device_data[1];
    let width = u16::from_le_bytes([device_data[2], device_data[3]]);
    let height = u16::from_le_bytes([device_data[4], device_data[5]]);
    let current_rotation = device_data[6];
    let mtu_size = u16::from_le_bytes([device_data[7], device_data[8]]);

    let display_name = if display_type == 1 { "6-color" } else { "B/W" };

    println!("\n📊 Device Info:");
    println!("   Version: {}", version);
    println!("   Display: {} ({}x{})", display_name, width, height);
    println!("   Rotation: {}", current_rotation);
    println!("   MTU Size: {} bytes", mtu_size);

    Ok(DeviceInfo {
        version,
        display_type,
        width,
        height,
        current_rotation,
        mtu_size,
    })
}

async fn pick_adapter(manager: &Manager) -> Result<Adapter> {
    let adapters = manager.adapters().await?;
    adapters
        .into_iter()
        .next()
        .ok_or_else(|| anyhow!("No Bluetooth adapters found"))
}

async fn scan_and_select(adapter: &Adapter, device_hint: Option<&str>) -> Result<Peripheral> {
    adapter
        .start_scan(ScanFilter::default())
        .await
        .context("Failed to start BLE scan")?;
    sleep(Duration::from_secs(SCAN_SECONDS)).await;

    let peripherals = adapter.peripherals().await?;
    let mut matches = Vec::new();

    for peripheral in peripherals {
        if let Some(props) = peripheral.properties().await? {
            if is_photoframe(&props) {
                matches.push((peripheral.clone(), props));
            }
        }
    }

    adapter.stop_scan().await.ok();

    if matches.is_empty() {
        return Err(anyhow!("No PhotoFrame devices found over BLE"));
    }

    if let Some(hint) = device_hint {
        if let Some((p, _)) = matches
            .iter()
            .find(|(p, props)| matches_hint(p, props, hint))
            .cloned()
        {
            println!(
                "Selected device by hint '{}': {}",
                hint,
                describe_props(&p, None)?
            );
            return Ok(p);
        }
        println!(
            "Hint '{}' did not match; showing all detected devices",
            hint
        );
    }

    if matches.len() == 1 {
        let (p, _) = matches.remove(0);
        println!("Selected device: {}", describe_peripheral(&p).await?);
        return Ok(p);
    }

    println!("Found multiple PhotoFrame devices:");
    for (idx, (p, props)) in matches.iter().enumerate() {
        println!("  [{}] {}", idx, describe_props(p, Some(props))?);
    }
    print!("Select device [0-{}]: ", matches.len() - 1);
    use std::io::Write;
    std::io::stdout().flush().ok();
    let mut input = String::new();
    std::io::stdin().read_line(&mut input)?;
    let idx: usize = input
        .trim()
        .parse()
        .map_err(|_| anyhow!("Invalid selection"))?;
    if idx >= matches.len() {
        return Err(anyhow!("Selection out of range"));
    }

    Ok(matches.remove(idx).0)
}

fn matches_hint(peripheral: &Peripheral, props: &PeripheralProperties, hint: &str) -> bool {
    let hint_lower = hint.to_ascii_lowercase();
    let addr = props.address.to_string().to_ascii_lowercase();
    if addr == hint_lower {
        return true;
    }
    let id_str = peripheral.id().to_string().to_ascii_lowercase();
    if id_str.contains(&hint_lower) || id_str == hint_lower {
        return true;
    }
    if let Some(name) = &props.local_name {
        if name.to_ascii_lowercase().contains(&hint_lower) {
            return true;
        }
    }
    false
}

fn is_photoframe(props: &PeripheralProperties) -> bool {
    let from_manufacturer = props
        .manufacturer_data
        .iter()
        .any(|(id, data)| *id == MANUFACTURER_ID && data.starts_with(MANUFACTURER_MAGIC));

    let from_name = props
        .local_name
        .as_ref()
        .map(|n| n.starts_with(DEVICE_PREFIX) || n.starts_with(DEVICE_FALLBACK_PREFIX))
        .unwrap_or(false);

    from_manufacturer || from_name
}

async fn describe_peripheral(peripheral: &Peripheral) -> Result<String> {
    let props = peripheral
        .properties()
        .await?
        .ok_or_else(|| anyhow!("Missing peripheral properties"))?;
    describe_props(peripheral, Some(&props))
}

fn describe_props(peripheral: &Peripheral, props: Option<&PeripheralProperties>) -> Result<String> {
    let id = if let Some(p) = props {
        prefer_identifier(peripheral, p)
    } else {
        peripheral.id().to_string()
    };
    let name = props
        .and_then(|p| p.local_name.clone())
        .unwrap_or_else(|| "Unknown".to_string());
    let label = if id.contains(':') { "MAC" } else { "ID" };
    Ok(format!("{} ({}: {})", name, label, id))
}

fn find_char(chars: &[Characteristic], uuid: Uuid) -> Result<Characteristic> {
    chars
        .iter()
        .find(|c| c.uuid == uuid)
        .cloned()
        .ok_or_else(|| anyhow!(format!("Characteristic {} not found", uuid)))
}

fn prefer_identifier(peripheral: &Peripheral, props: &PeripheralProperties) -> String {
    let addr = props.address.to_string();
    let nonzero_hex = addr
        .chars()
        .filter(|c| c.is_ascii_hexdigit())
        .any(|c| c != '0');
    if nonzero_hex {
        addr
    } else {
        peripheral.id().to_string()
    }
}

fn build_config_payload(data: &[u8], width: u16, height: u16, rotation: u8) -> Vec<u8> {
    let mut cfg = BtImageConfig {
        magic: 0xBEEF,
        version: 0x01,
        rotation,
        width,  // Mirror device-reported width
        height, // Mirror device-reported height
        timestamp: SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_else(|_| Duration::from_secs(0))
            .as_secs() as u32,
        image_size: data.len() as u32,
        crc16: 0,
    };

    let mut raw = pack_config(&cfg, false);
    cfg.crc16 = crc16_modbus(&raw);
    raw = pack_config(&cfg, true);
    raw
}

fn pack_config(cfg: &BtImageConfig, include_crc: bool) -> Vec<u8> {
    let mut out = Vec::with_capacity(std::mem::size_of::<BtImageConfig>());
    out.extend_from_slice(&cfg.magic.to_le_bytes());
    out.push(cfg.version);
    out.push(cfg.rotation);
    out.extend_from_slice(&cfg.width.to_le_bytes());
    out.extend_from_slice(&cfg.height.to_le_bytes());
    out.extend_from_slice(&cfg.timestamp.to_le_bytes());
    out.extend_from_slice(&cfg.image_size.to_le_bytes());
    if include_crc {
        out.extend_from_slice(&cfg.crc16.to_le_bytes());
    }
    out
}

fn crc16_modbus(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for byte in data {
        crc ^= *byte as u16;
        for _ in 0..8 {
            if crc & 0x0001 != 0 {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    crc
}

// use shared parser from photoframe-lib

fn validate_payload_crc(payload: &[u8], expected_crc: u32) -> Result<()> {
    let mut hasher = Crc32Hasher::new();
    hasher.update(payload);
    let crc = hasher.finalize();
    if crc != expected_crc {
        return Err(anyhow!(
            "Payload CRC mismatch: expected 0x{:08x}, got 0x{:08x}",
            expected_crc,
            crc
        ));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crc16_modbus_known_vector() {
        // Standard test vector: "123456789" -> CRC16/MODBUS = 0x4B37
        let crc = crc16_modbus(b"123456789");
        assert_eq!(crc, 0x4B37);
    }

    #[test]
    fn build_config_payload_has_valid_crc_and_layout() {
        // Use deterministic payload and fields
        let data = vec![0xAAu8; 100];
        let width: u16 = 800;
        let height: u16 = 480;
        let rotation: u8 = 1;

        let raw = build_config_payload(&data, width, height, rotation);

        // Expected struct size (packed): 2+1+1+2+2+4+4+2 = 18
        assert_eq!(raw.len(), std::mem::size_of::<BtImageConfig>());

        // Verify magic (0xBEEF LE)
        assert_eq!(raw[0], 0xEF);
        assert_eq!(raw[1], 0xBE);

        // Version and rotation
        assert_eq!(raw[2], 0x01);
        assert_eq!(raw[3], rotation);

        // Width, Height
        assert_eq!(u16::from_le_bytes([raw[4], raw[5]]), width);
        assert_eq!(u16::from_le_bytes([raw[6], raw[7]]), height);

        // Image size
        assert_eq!(
            u32::from_le_bytes([raw[12], raw[13], raw[14], raw[15]]),
            data.len() as u32
        );

        // CRC should validate over all but the last 2 bytes
        let expected_crc = crc16_modbus(&raw[..raw.len() - 2]);
        let found_crc = u16::from_le_bytes([raw[16], raw[17]]);
        assert_eq!(found_crc, expected_crc);
    }

    #[test]
    fn parse_bin_file_valid() {
        // Build a minimal valid BIN with header + payload + payload CRC32
        let width: u16 = 800;
        let height: u16 = 480;
        let rotation: u8 = 0;
        let color_mode: u8 = 1; // 6-color
        let payload: Vec<u8> = (0..64u8).collect();

        let mut header = Vec::new();
        header.extend_from_slice(&photoframe_lib::BIN_MAGIC.to_le_bytes()); // magic (4)
        header.push(1); // version (1)
        header.extend_from_slice(&(photoframe_lib::BIN_HEADER_SIZE as u16).to_le_bytes()); // header_len (2)
        header.extend_from_slice(&width.to_le_bytes()); // width (2)
        header.extend_from_slice(&height.to_le_bytes()); // height (2)
        header.push(rotation); // rotation (1)
        header.push(color_mode); // color (1)
        header.extend_from_slice(&(payload.len() as u32).to_le_bytes()); // payload_len (4)

        // Compute header CRC32 over first BIN_HEADER_SIZE-4 bytes
        let mut hasher = Crc32Hasher::new();
        hasher.update(&header);
        let header_crc = hasher.finalize();
        header.extend_from_slice(&header_crc.to_le_bytes()); // header_crc32 (4)

        assert_eq!(header.len(), photoframe_lib::BIN_HEADER_SIZE);

        // Build full file: header + payload + payload CRC32
        let mut file = header;
        file.extend_from_slice(&payload);
        let mut ph = Crc32Hasher::new();
        ph.update(&payload);
        let payload_crc = ph.finalize();
        file.extend_from_slice(&payload_crc.to_le_bytes());

        let (parsed, parsed_payload, parsed_payload_crc) =
            parse_bin_file(&file).expect("parse_bin_file should accept valid file");

        let v = parsed.version;
        let hl = parsed.header_len;
        let pw = parsed.width;
        let ph = parsed.height;
        let pr = parsed.rotation;
        let pcm = parsed.color_mode;
        let ppl = parsed.payload_len;
        let phc = parsed.header_crc32;

        assert_eq!(v, 1);
        assert_eq!(hl, photoframe_lib::BIN_HEADER_SIZE as u16);
        assert_eq!(pw, width);
        assert_eq!(ph, height);
        assert_eq!(pr, rotation);
        assert_eq!(pcm, color_mode);
        assert_eq!(ppl, payload.len() as u32);
        assert_eq!(phc, header_crc);
        assert_eq!(parsed_payload, &payload[..]);
        assert_eq!(parsed_payload_crc, payload_crc);
    }

    #[test]
    fn parse_bin_file_rejects_bad_magic() {
        let width: u16 = 10;
        let height: u16 = 10;
        let payload: Vec<u8> = vec![0x55; 10];

        let mut header = Vec::new();
        header.extend_from_slice(&0xDEADBEEFu32.to_le_bytes()); // wrong magic
        header.push(1);
        header.extend_from_slice(&(photoframe_lib::BIN_HEADER_SIZE as u16).to_le_bytes());
        header.extend_from_slice(&width.to_le_bytes());
        header.extend_from_slice(&height.to_le_bytes());
        header.push(0);
        header.push(0);
        header.extend_from_slice(&(payload.len() as u32).to_le_bytes());
        let mut hasher = Crc32Hasher::new();
        hasher.update(&header);
        let header_crc = hasher.finalize();
        header.extend_from_slice(&header_crc.to_le_bytes());

        let mut file = header;
        file.extend_from_slice(&payload);
        let mut ph = Crc32Hasher::new();
        ph.update(&payload);
        let payload_crc = ph.finalize();
        file.extend_from_slice(&payload_crc.to_le_bytes());

        let err = parse_bin_file(&file).unwrap_err();
        assert!(err.to_string().contains("Invalid BIN magic"));
    }

    #[test]
    fn parse_bin_file_rejects_bad_header_crc() {
        let width: u16 = 10;
        let height: u16 = 10;
        let payload: Vec<u8> = vec![0x55; 10];

        let mut header = Vec::new();
        header.extend_from_slice(&photoframe_lib::BIN_MAGIC.to_le_bytes());
        header.push(1);
        header.extend_from_slice(&(photoframe_lib::BIN_HEADER_SIZE as u16).to_le_bytes());
        header.extend_from_slice(&width.to_le_bytes());
        header.extend_from_slice(&height.to_le_bytes());
        header.push(0);
        header.push(0);
        header.extend_from_slice(&(payload.len() as u32).to_le_bytes());
        // Intentionally wrong CRC
        header.extend_from_slice(&0x12345678u32.to_le_bytes());

        let mut file = header;
        file.extend_from_slice(&payload);
        let mut ph = Crc32Hasher::new();
        ph.update(&payload);
        let payload_crc = ph.finalize();
        file.extend_from_slice(&payload_crc.to_le_bytes());

        let err = parse_bin_file(&file).unwrap_err();
        assert!(err.to_string().contains("Header CRC mismatch"));
    }

    #[test]
    fn parse_bin_file_rejects_truncated_payload() {
        let width: u16 = 10;
        let height: u16 = 10;
        let payload_len: usize = 100;
        let payload: Vec<u8> = vec![0x77; 50]; // shorter than declared

        let mut header = Vec::new();
        header.extend_from_slice(&photoframe_lib::BIN_MAGIC.to_le_bytes());
        header.push(1);
        header.extend_from_slice(&(photoframe_lib::BIN_HEADER_SIZE as u16).to_le_bytes());
        header.extend_from_slice(&width.to_le_bytes());
        header.extend_from_slice(&height.to_le_bytes());
        header.push(0);
        header.push(0);
        header.extend_from_slice(&(payload_len as u32).to_le_bytes());
        let mut hasher = Crc32Hasher::new();
        hasher.update(&header);
        let header_crc = hasher.finalize();
        header.extend_from_slice(&header_crc.to_le_bytes());

        let mut file = header;
        file.extend_from_slice(&payload);
        // Even with CRC appended, total bytes will be less than declared total
        let mut ph = Crc32Hasher::new();
        ph.update(&payload);
        let payload_crc = ph.finalize();
        file.extend_from_slice(&payload_crc.to_le_bytes());

        let err = parse_bin_file(&file).unwrap_err();
        assert!(err.to_string().contains("BIN truncated: have"));
    }
}
