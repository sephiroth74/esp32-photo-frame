use anyhow::{anyhow, Context, Result};
use btleplug::api::{
    Central, Characteristic, Manager as _, Peripheral as _, PeripheralProperties, ScanFilter,
    WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use indicatif::{ProgressBar, ProgressStyle};
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
    rotation: u8,
    device_hint: Option<&str>,
) -> Result<()> {
    let bin_data = std::fs::read(bin_path)
        .with_context(|| format!("Failed to read binary image: {}", bin_path.display()))?;

    let image_size = bin_data.len();

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

        // Calculate expected image size based on device display type (mirror device dims)
        let expected_bw_size = device_info.width as usize * device_info.height as usize;
        let expected_6c_size = device_info.width as usize * device_info.height as usize;

        let is_valid_size = if device_info.display_type == 1 {
            // 6-color display - expected_6c_size
            image_size == expected_6c_size
                || image_size == (device_info.width as usize * device_info.height as usize * 3) / 8
        } else {
            // B/W display
            image_size == expected_bw_size
        };

        if !is_valid_size {
            eprintln!("\n⚠ Warning: Image size mismatch!");
            eprintln!("  Got {} bytes", image_size);
            if device_info.display_type == 1 {
                eprintln!(
                    "  Expected {} bytes (for 6-color {}x{})",
                    expected_6c_size, device_info.width, device_info.height
                );
            } else {
                eprintln!(
                    "  Expected {} bytes (for B/W {}x{})",
                    expected_bw_size, device_info.width, device_info.height
                );
            }
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

        println!("\n📷 Image Configuration:");
        println!("   Size: {} bytes", image_size);
        println!(
            "   Dimensions: {}x{}",
            device_info.width, device_info.height
        );
        println!(
            "   Display: {}",
            if device_info.display_type == 1 {
                "6-color"
            } else {
                "B/W"
            }
        );
        println!("   Rotation: {}", rotation);
        println!("   Chunk size: {} bytes", chunk_size);

        let chars: Vec<Characteristic> = peripheral.characteristics().into_iter().collect();
        let config_char = find_char(&chars, CONFIG_CHAR_UUID)?;
        let image_char = find_char(&chars, IMAGE_CHAR_UUID)?;

        println!("\n📤 Sending configuration...");
        let config_payload =
            build_config_payload(&bin_data, device_info.width, device_info.height, rotation);
        peripheral
            .write(&config_char, &config_payload, WriteType::WithResponse)
            .await
            .context("Failed to write BLE config")?;
        println!("✓ Configuration sent");

        sleep(Duration::from_millis(500)).await;

        println!("\n📤 Sending image data...");
        let num_chunks = (bin_data.len() + chunk_size - 1) / chunk_size;
        let pb = ProgressBar::new(bin_data.len() as u64);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("{spinner:.green} Uploading [{bar:30}] {bytes}/{total_bytes} ({eta})")
                .unwrap()
                .progress_chars("=> "),
        );

        for (i, chunk) in bin_data.chunks(chunk_size).enumerate() {
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
