use photoframe_lib::{BIN_HEADER_SIZE, build_bin_file};
use std::fs;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Create a simple test PFR1 file
    let width = 16u16;
    let height = 10u16;
    let rotation = 1u8; // 90°
    let color_mode = 1u8; // 6-color
    let version = 1u8;

    // Create dummy payload (width × height bytes)
    let payload_size = (width as usize) * (height as usize);
    let payload: Vec<u8> = (0..payload_size).map(|i| (i % 256) as u8).collect();

    // Build PFR1 file
    let pfr1_data = build_bin_file(&payload, width, height, rotation, color_mode, version);

    // Write to file
    let output_path = "/tmp/test.pfr1";
    fs::write(output_path, &pfr1_data)?;

    println!("✅ Created test PFR1 file: {}", output_path);
    println!("   Size: {} bytes", pfr1_data.len());
    println!("   Dimensions: {}×{}", width, height);
    println!("   Rotation: {} ({}°)", rotation, rotation as u16 * 90);
    println!("   Color mode: {} (6-color)", color_mode);
    println!();
    println!("Expected structure:");
    println!("   Header:  {} bytes", BIN_HEADER_SIZE);
    println!("   Payload: {} bytes", payload_size);
    println!("   CRC32:   4 bytes");
    println!("   Total:   {} bytes", BIN_HEADER_SIZE + payload_size + 4);

    Ok(())
}
