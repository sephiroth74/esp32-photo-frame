use crate::cli::Args;
use clap::Parser;
use std::fs;
use std::process;

mod cli;
mod discovery;
mod fs_utils;
mod image_inspector;
mod image_processor;
mod logging;
mod report;
mod types;

fn main() {
    let args = cli::Args::parse();
    let logger = logging::Logger::new(args.verbose && !args.json_progress);

    // Handle validation mode early exit
    if let Some(ref validate_path) = args.validate {
        match run_validation(validate_path, &logger) {
            Ok(_) => process::exit(0),
            Err(e) => {
                logger.error(&format!("Validation error: {}", e));
                process::exit(1);
            }
        }
    }

    print_configuration(&args, &logger);

    // Create output directories for all requested formats
    logger.info("Creating output directories...");

    if let Err(e) = fs_utils::create_format_directories(&args.output, &args.output_formats) {
        logger.error(&format!("Failed to create output directories: {}", e));
        process::exit(1);
    }

    logger.success("Output directories created successfully");
    logger.info("");
    logger.info("Discovering input files...");

    let discovery = discovery::Discovery::new(&args.extensions, &logger);
    let input_files = match discovery.discover(&args.input) {
        Ok(files) => files,
        Err(e) => {
            logger.error(&format!("Discovery failed: {}", e));
            process::exit(1);
        }
    };

    logger.success(&format!(
        "Discovery completed: {} file(s)",
        input_files.len()
    ));
    logger.info("");

    let mut report = report::Report::new(&args, input_files.clone());

    if !args.json_progress {
        logger.info("Validating image files...");
    }
    let inspector = image_inspector::ImageInspector::new(&logger);
    let inspection = inspector.inspect(&input_files, args.json_progress);

    report.set_image_results(inspection.valid.clone(), inspection.invalid.clone());

    if !args.json_progress {
        logger.success(&format!(
            "Image validation completed: {} valid, {} invalid",
            report.valid_images.len(),
            report.invalid_images.len()
        ));
        logger.info("");
    }

    // Create processing plan
    let processor = match image_processor::ImageProcessor::new(&args, &logger) {
        Ok(p) => p,
        Err(e) => {
            logger.error(&format!("Failed to initialize processor: {}", e));
            process::exit(1);
        }
    };
    let plan = match processor.plan(inspection.valid.clone(), &mut report) {
        Ok(plan) => plan,
        Err(e) => {
            logger.error(&format!("Failed to create processing plan: {}", e));
            process::exit(1);
        }
    };

    if plan.output_count() == 0 {
        logger.warning("No images to process after planning");
        process::exit(0);
    }

    logger.info("Starting image processing...");
    logger.info("");

    let processing = match processor.process(&plan, args.json_progress) {
        Ok(result) => result,
        Err(e) => {
            logger.error(&format!("Processing failed: {}", e));
            process::exit(1);
        }
    };

    if !args.json_progress {
        logger.success(&format!(
            "Processing completed: {} processed, {} failed",
            processing.processed.len(),
            processing.failed.len()
        ));
        logger.info("");
    }
}

/// Run PFR1 file validation and exit
fn run_validation(
    path: &std::path::Path,
    logger: &logging::Logger,
) -> Result<(), Box<dyn std::error::Error>> {
    logger.section("🔍 PFR1 FILE VALIDATION");
    logger.info(&format!("File: {}", path.display()));
    logger.info("");

    // Read file
    let data = match fs::read(path) {
        Ok(d) => d,
        Err(e) => {
            logger.error(&format!("Failed to read file: {}", e));
            process::exit(1);
        }
    };

    logger.info(&format!("File size: {} bytes", data.len()));
    logger.info("");

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

            logger.success("VALIDATION SUCCESSFUL");
            logger.info("");
            logger.config_section("HEADER INFORMATION");
            logger.config_item("Version", &version.to_string());
            logger.config_item("Header length", &format!("{} bytes", header_len));
            logger.config_item(
                "Magic",
                &format!("0x{:08X} ('PFR1')", photoframe_lib::BIN_MAGIC),
            );
            logger.config_item("Width", &format!("{} px", width));
            logger.config_item("Height", &format!("{} px", height));
            logger.config_item(
                "Rotation",
                &format!("{} ({}°)", rotation, rotation as u16 * 90),
            );
            logger.config_item(
                "Color mode",
                &format!(
                    "{} ({})",
                    color_mode,
                    if color_mode == 0 {
                        "Black & White"
                    } else {
                        "6-Color"
                    }
                ),
            );
            logger.config_item("Payload length", &format!("{} bytes", payload_len));
            logger.config_item(
                "Actual payload",
                &format!("{} bytes", validation.payload.len()),
            );
            logger.config_item(
                "Expected size",
                &format!(
                    "{} bytes ({}×{})",
                    width as u32 * height as u32,
                    width,
                    height
                ),
            );
            logger.config_item("Header CRC32", &format!("0x{:08X}", header_crc32));
            logger.config_item("Payload CRC32", &format!("0x{:08X}", payload_crc32));
            logger.info("");

            // Consistency checks
            let expected_payload = width as u32 * height as u32;
            if payload_len != expected_payload {
                logger.warning("Payload length mismatch!");
                logger.verbose(&format!("   Header says:  {} bytes", payload_len));
                logger.verbose(&format!("   Expected:     {} bytes", expected_payload));
            }

            if validation.payload.len() != payload_len as usize {
                logger.warning("Actual payload size doesn't match header!");
                logger.verbose(&format!("   Header says:  {} bytes", payload_len));
                logger.verbose(&format!(
                    "   Actual:       {} bytes",
                    validation.payload.len()
                ));
            }

            if rotation > 3 {
                logger.warning(&format!("Invalid rotation value: {}", rotation));
            }

            if color_mode > 1 {
                logger.warning(&format!("Invalid color mode: {}", color_mode));
            }

            logger.info("");

            Ok(())
        }
        Err(e) => {
            logger.error("VALIDATION FAILED");
            logger.info("");
            logger.info(&format!("Error: {}", e));
            logger.info("");

            // Try to provide more details
            if data.len() < photoframe_lib::BIN_HEADER_SIZE {
                logger.info("File is too small to contain a valid PFR1 header.");
                logger.info(&format!(
                    "   Minimum size: {} bytes",
                    photoframe_lib::BIN_HEADER_SIZE
                ));
                logger.info(&format!("   Actual size:  {} bytes", data.len()));
            } else if data.len() >= 4 {
                let magic = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
                if magic != photoframe_lib::BIN_MAGIC {
                    logger.info(&format!("Invalid magic number: 0x{:08X}", magic));
                    logger.info(&format!(
                        "   Expected: 0x{:08X} ('PFR1')",
                        photoframe_lib::BIN_MAGIC
                    ));
                    logger.info("   This file is not a valid PFR1 file.");
                }
            }

            logger.info("");
            logger.divider();

            process::exit(1);
        }
    }
}

fn print_configuration(args: &Args, logger: &logging::Logger) {
    let dither_strength: f32 = (args.dither_strength as f32) / 100f32;
    let saturation: f32 = (args.saturation as f32) / 100f32;

    logger.section("Configuration Summary");

    // Input/Output
    logger.config_section("INPUT/OUTPUT");
    logger.config_item("Input paths", &format!("{:?}", args.input));
    logger.config_item("Output directory", &args.output.display().to_string());
    logger.config_item("File extensions", &args.extensions);
    logger.config_item("Formats", &format!("{:?}", args.output_formats));
    logger.info("");

    // Display Configuration
    logger.config_section("DISPLAY CONFIGURATION");
    logger.config_item("Color type", &format!("{:?}", args.processing_type));
    logger.config_item(
        "Target orientation",
        &format!("{:?}", args.target_orientation),
    );
    logger.info("");

    // Processing Options
    if args.jobs > 0 {
        logger.config_section("PROCESSING OPTIONS");
        logger.config_item("Parallel jobs", &format!("{} (0 = auto)", args.jobs));
        logger.info("");
    }

    // Image Adjustments
    logger.config_section("IMAGE ADJUSTMENTS");
    if args.auto_optimize {
        logger.config_item("Auto optimize", &args.auto_optimize.to_string());
    } else {
        logger.config_item("Auto color correct", &args.auto_color.to_string());
        logger.config_item("Brightness", &format!("{:+} ", args.brightness));
        logger.config_item("Contrast", &format!("{:+} ", args.contrast));
        logger.config_item("Saturation", &format!("{} x", saturation));
    }
    logger.info("");

    // Dithering
    logger.config_section("DITHERING");
    logger.config_item("Method", &format!("{:?}", args.dithering_method));
    logger.config_item("Strength", &dither_strength.to_string());
    logger.info("");

    // Portrait Pairing
    logger.config_section("PAIRING");

    if args.no_pairing {
        logger.config_item("No-pairing", &args.no_pairing.to_string());
    } else {
        logger.config_item("Divider width", &format!("{} px", args.divider_width));
        logger.config_item("Divider color", &args.divider_color.to_string());
    }
    logger.info("");

    // AI Features
    #[cfg(feature = "ai")]
    if args.detect_people {
        logger.config_section("AI FEATURES");
        logger.config_item(
            "Confidence threshold",
            &format!("{:.2}", args.confidence_threshold),
        );
        logger.info("");
    }

    // Annotation
    if args.annotate {
        logger.config_section("ANNOTATION");
        logger.config_item("Font name", &format!("{:?}", args.font));
        logger.config_item("Font size", &format!("{} px", args.font_size));
        logger.config_item(
            "Background color",
            &format!(
                "#{:02X}{:02X}{:02X}{:02X}",
                args.annotation_background.0,
                args.annotation_background.1,
                args.annotation_background.2,
                args.annotation_background.3
            ),
        );
        logger.info("");
    }

    // Configuration & Mode
    logger.config_section("CONFIGURATION & MODE");
    logger.config_item("Debug mode", &args.debug.to_string());
    logger.config_item("Verbose", &args.verbose.to_string());
    logger.config_item("JSON progress", &args.json_progress.to_string());
    logger.config_item("Generate report", &args.report.to_string());
    logger.info("");

    logger.divider();
}
