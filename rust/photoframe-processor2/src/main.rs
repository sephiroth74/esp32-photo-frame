use clap::Parser;
use std::fs;
use std::process;

mod cli;
mod types;

/// Run PFR1 file validation and exit
fn run_validation(path: &std::path::Path) -> Result<(), Box<dyn std::error::Error>> {
    println!("═══════════════════════════════════════════════════════════════");
    println!("🔍 PFR1 FILE VALIDATION");
    println!("═══════════════════════════════════════════════════════════════");
    println!();
    println!("File: {}", path.display());
    println!();

    // Read file
    let data = match fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            println!("❌ ERROR: Failed to read file: {}", e);
            process::exit(1);
        }
    };

    println!("File size: {} bytes", data.len());
    println!();

    // Validate using photoframe-lib
    match photoframe_lib::validate_bin_file(&data) {
        Ok(validation) => {
            // Copy header fields to avoid unaligned reference issues (packed struct)
            let header = validation.header;
            let version = header.version;
            let header_len = header.header_len;
            let width = header.width;
            let height = header.height;
            let rotation = header.rotation;
            let color_mode = header.color_mode;
            let payload_len = header.payload_len;
            let header_crc32 = header.header_crc32;
            let payload_crc32 = validation.payload_crc32;

            println!("✅ VALIDATION SUCCESSFUL");
            println!();
            println!("HEADER INFORMATION:");
            println!("  Version:            {}", version);
            println!("  Header length:      {} bytes", header_len);
            println!(
                "  Magic:              0x{:08X} ('PFR1')",
                photoframe_lib::BIN_MAGIC
            );
            println!("  Width:              {} px", width);
            println!("  Height:             {} px", height);
            println!(
                "  Rotation:           {} ({}°)",
                rotation,
                rotation as u16 * 90
            );
            println!(
                "  Color mode:         {} ({})",
                color_mode,
                if color_mode == 0 {
                    "Black & White"
                } else {
                    "6-Color"
                }
            );
            println!("  Payload length:     {} bytes", payload_len);
            println!("  Actual payload:     {} bytes", validation.payload.len());
            println!(
                "  Expected size:      {} bytes ({}×{})",
                width as u32 * height as u32,
                width,
                height
            );
            println!("  Header CRC32:       0x{:08X}", header_crc32);
            println!("  Payload CRC32:      0x{:08X}", payload_crc32);
            println!();

            // Consistency checks
            let expected_payload = width as u32 * height as u32;
            if payload_len != expected_payload {
                println!("⚠️  WARNING: Payload length mismatch!");
                println!("   Header says:  {} bytes", payload_len);
                println!("   Expected:     {} bytes", expected_payload);
            }

            if validation.payload.len() != payload_len as usize {
                println!("⚠️  WARNING: Actual payload size doesn't match header!");
                println!("   Header says:  {} bytes", payload_len);
                println!("   Actual:       {} bytes", validation.payload.len());
            }

            if rotation > 3 {
                println!("⚠️  WARNING: Invalid rotation value: {}", rotation);
            }

            if color_mode > 1 {
                println!("⚠️  WARNING: Invalid color mode: {}", color_mode);
            }

            println!();

            Ok(())
        }
        Err(e) => {
            println!("❌ VALIDATION FAILED");
            println!();
            println!("Error: {}", e);
            println!();

            // Try to provide more details
            if data.len() < photoframe_lib::BIN_HEADER_SIZE {
                println!("File is too small to contain a valid PFR1 header.");
                println!("   Minimum size: {} bytes", photoframe_lib::BIN_HEADER_SIZE);
                println!("   Actual size:  {} bytes", data.len());
            } else if data.len() >= 4 {
                let magic = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
                if magic != photoframe_lib::BIN_MAGIC {
                    println!("Invalid magic number: 0x{:08X}", magic);
                    println!("   Expected: 0x{:08X} ('PFR1')", photoframe_lib::BIN_MAGIC);
                    println!("   This file is not a valid PFR1 file.");
                }
            }

            println!();
            println!("═══════════════════════════════════════════════════════════════");

            process::exit(1);
        }
    }
}

fn main() {
    let args = cli::Args::parse();

    // Handle validation mode early exit
    if let Some(ref validate_path) = args.validate {
        match run_validation(validate_path) {
            Ok(_) => process::exit(0),
            Err(e) => {
                eprintln!("Validation error: {}", e);
                process::exit(1);
            }
        }
    }

    let dither_strength: f32 = (args.dither_strength as f32) / 100f32;
    let saturation: f32 = (args.saturation as f32) / 100f32;

    println!("═══════════════════════════════════════════════════════════════");
    println!("Configuration Summary");
    println!("═══════════════════════════════════════════════════════════════");
    println!();

    // Input/Output
    println!("INPUT/OUTPUT:");
    println!("  Input paths:        {:?}", args.input);
    println!("  Output directory:   {}", args.output.display());
    println!("  File extensions:    {:?}", args.extensions.join(", "));
    println!("  Formats:            {:?}", args.output_formats);
    println!();

    // Display Configuration
    println!("DISPLAY CONFIGURATION:");
    println!("  Color type:         {:?}", args.processing_type);
    println!("  Target orientation: {:?}", args.target_orientation);
    println!();

    // Processing Options
    println!("PROCESSING OPTIONS:");
    if args.jobs > 0 {
        println!("  Parallel jobs:      {} (0 = auto)", args.jobs);
    }
    println!();

    // Image Adjustments
    println!("IMAGE ADJUSTMENTS:");
    if args.auto_optimize {
        println!("  Auto optimize:      {}", args.auto_optimize);
    } else {
        println!("  Auto color correct: {}", args.auto_color_correct);
        println!("  Brightness:         {:+} ", args.brightness);
        println!("  Contrast:           {:+} ", args.contrast);
        println!("  Saturation:         {} x", saturation);
    }
    println!();

    // Dithering
    println!("DITHERING:");
    println!("  Method:             {:?}", args.dithering_method);
    println!("  Strength:           {}", dither_strength);
    println!();

    // Portrait Pairing
    println!("PAIRING:");
    println!("  Divider width:      {} px", args.divider_width);
    println!("  Divider color:      #{}", args.divider_color);
    println!();

    // AI Features
    #[cfg(feature = "ai")]
    if args.detect_people {
        println!("AI FEATURES:");
        println!("  Confidence threshold: {:.2}", args.confidence_threshold);
        println!();
    }

    // Annotation
    if args.annotate {
        println!("ANNOTATION:");
        println!("  Font name:          {:?}", args.font);
        println!("  Font size:          {} px", args.font_size);
        println!();
    }

    // Configuration & Mode
    println!("CONFIGURATION & MODE:");
    println!("  Verbose:            {}", args.verbose);
    println!("  Debug mode:         {}", args.debug);
    println!("  JSON progress:      {}", args.json_progress);
    println!("  Generate report:    {}", args.report);
    println!("  Validate PFR1:      {:?}", args.validate);
    println!();

    println!("═══════════════════════════════════════════════════════════════");
    println!();

    // Validation and summary
    let mut validation_ok = true;

    if args.confidence_threshold < 0.0 || args.confidence_threshold > 1.0 {
        println!("  ❌ Confidence threshold must be 0.0-1.0");
        validation_ok = false;
    } else {
        println!("  ✓ Confidence threshold valid");
    }

    if args.input.is_empty() {
        println!("  ❌ At least one input path required");
        validation_ok = false;
    } else {
        println!("  ✓ Input paths specified");
    }

    if args.output.as_os_str().is_empty() {
        println!("  ❌ Output directory required");
        validation_ok = false;
    } else {
        println!("  ✓ Output directory specified");
    }

    println!();

    if validation_ok {
        println!("✨ All parameters valid! Ready for processing.");
    } else {
        println!("⚠️  Some parameters are invalid. Please review and try again.");
        std::process::exit(1);
    }

    println!();
}
