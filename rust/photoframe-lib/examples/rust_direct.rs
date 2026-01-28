use photoframe_lib::convert_image_from_bytes;
use photoframe_lib::types::DisplayType;
use std::{fs, path::Path};

fn main() {
    let path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "tests/data/default_portrait.jpg".to_string());

    if !Path::new(&path).exists() {
        eprintln!("Input image not found: {}", path);
        std::process::exit(1);
    }

    let bytes = fs::read(&path).expect("failed to read image");

    match convert_image_from_bytes(&bytes, DisplayType::SixColors) {
        Some((payload, w, h)) => {
            fs::create_dir_all("examples/output").ok();
            // For SixColors the convert returns the demo wrapper (4-byte header + payload)
            let expected_raw_len = (w as usize) * (h as usize);
            let data_to_write: &[u8] = if payload.len() == 4 + expected_raw_len {
                &payload[4..]
            } else if payload.len() == expected_raw_len {
                &payload
            } else {
                // fallback: try to strip header if present
                if payload.len() > 4 {
                    &payload[payload.len() - expected_raw_len..]
                } else {
                    &payload
                }
            };

            let out_path = format!("examples/output/demo_6c_{}x{}.pfr1", w, h);
            fs::write(&out_path, data_to_write).expect("failed to write output");
            println!("Wrote {} bytes to {}", data_to_write.len(), out_path);
        }
        None => {
            eprintln!("Failed to decode/convert image");
            std::process::exit(2);
        }
    }

    if let Some((payload6, w, h)) = convert_image_from_bytes(&bytes, DisplayType::SixColors) {
        let out_path = format!("examples/output/demo_6c_{}x{}.pfr1", w, h);
        fs::write(&out_path, &payload6).expect("failed to write output");
        println!("Wrote {} bytes to {}", payload6.len(), out_path);
    } else {
        eprintln!("Failed to decode/convert image to 6 colors");
    }

    if let Some((payload_bw, w, h)) = convert_image_from_bytes(&bytes, DisplayType::BlackAndWhite) {
        let out_path = format!("examples/output/demo_bw_{}x{}.pfr1", w, h);
        fs::write(&out_path, &payload_bw).expect("failed to write output");
        println!("Wrote {} bytes to {}", payload_bw.len(), out_path);
    } else {
        eprintln!("Failed to decode/convert image to black and white");
    }
}
