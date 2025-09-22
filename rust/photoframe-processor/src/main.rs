use anyhow::{Context, Result};
use clap::Parser;
use console::style;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use std::time::Instant;

mod cli;
mod image_processing;
mod utils;

use cli::{Args, ColorType};
use image_processing::{
    color_correction, ImageType, ProcessingConfig, ProcessingEngine, ProcessingResult,
    ProcessingType, SkipReason,
};
use utils::{
    create_progress_bar, format_duration, validate_inputs, verbose_println,
};

impl From<ColorType> for ProcessingType {
    fn from(color_type: ColorType) -> Self {
        match color_type {
            ColorType::BlackWhite => ProcessingType::BlackWhite,
            ColorType::SixColor => ProcessingType::SixColor,
            ColorType::SevenColor => ProcessingType::SevenColor,
        }
    }
}

/// Handle find hash functionality - find original filename from hash
fn handle_find_hash(hash: &str, _args: &Args) -> Result<()> {
    use std::collections::HashMap;

    println!(
        "{}",
        style(format!(
            "Finding original filename for hash: {}",
            hash
        ))
        .bold()
        .cyan()
    );
    println!();

    // Validate hash format (should be 8 hex characters)
    if hash.len() != 8 {
        return Err(anyhow::anyhow!(
            "Invalid hash length '{}'. Expected 8 characters, got {}",
            hash, hash.len()
        ));
    }

    if !hash.chars().all(|c| c.is_ascii_hexdigit()) {
        return Err(anyhow::anyhow!(
            "Invalid hash format '{}'. Hash must contain only hexadecimal characters (0-9, a-f)",
            hash
        ));
    }

    println!("{}",
        style("Searching for matching filenames...")
            .dim()
    );
    println!();

    // This is a demonstration - in a real implementation, you would:
    // 1. Search through common directories for image files
    // 2. Generate hashes for each filename found
    // 3. Compare with the target hash
    // 4. Return matching original filename(s)

    // For now, show the concept with some example mappings
    let mut example_mappings = HashMap::new();
    example_mappings.insert("a1b2c3d4", "vacation_2023.jpg");
    example_mappings.insert("e5f6g7h8", "birthday_party.png");
    example_mappings.insert("12345678", "sunset_beach.heic");
    example_mappings.insert("abcdef01", "family_photo.jpg");

    if let Some(original_filename) = example_mappings.get(hash) {
        println!("{}",
            style("✓ Found matching original filename!")
                .green()
                .bold()
        );
        println!();
        println!("  {}: {}",
            style("Hash").bold().cyan(),
            style(hash).bold().yellow()
        );
        println!("  {}: {}",
            style("Original").bold().cyan(),
            style(original_filename).bold().green()
        );
        println!();
        println!("{}",
            style("Note: This is a demonstration with sample data.")
                .dim()
        );
        println!("{}",
            style("In a full implementation, this would search your actual image directories.")
                .dim()
        );
    } else {
        println!("{}",
            style("No matching filename found for this hash.")
                .yellow()
                .bold()
        );
        println!();
        println!("{}",
            style("The hash lookup feature would:")
                .dim()
        );
        println!("{}",
            style("1. Scan specified directories for image files")
                .dim()
        );
        println!("{}",
            style("2. Generate 8-character hashes for each filename")
                .dim()
        );
        println!("{}",
            style("3. Return the original filename if hash matches")
                .dim()
        );
        println!();
        println!("{}",
            style("Usage: photoframe-processor --find-hash <8-char-hex>")
                .dim()
        );
    }

    Ok(())
}

/// Handle find original functionality - decode combined filenames
fn handle_find_original(combined_filename: &str, _args: &Args) -> Result<()> {
    use base64::{engine::general_purpose, Engine as _};

    println!(
        "{}",
        style(format!(
            "Decoding combined filename: {}",
            combined_filename
        ))
        .bold()
        .cyan()
    );
    println!();

    // Parse the combined filename format: combined_PREFIX_BASE64_BASE64.bin
    if !combined_filename.starts_with("combined_") {
        return Err(anyhow::anyhow!(
            "Invalid combined filename '{}'. Expected format: 'combined_PREFIX_BASE64_BASE64.bin'",
            combined_filename
        ));
    }

    // Remove the "combined_" prefix and ".bin" suffix
    let without_prefix = &combined_filename[9..]; // Skip "combined_"
    let without_suffix = if without_prefix.ends_with(".bin") {
        &without_prefix[..without_prefix.len() - 4] // Remove ".bin"
    } else if without_prefix.ends_with(".bmp") {
        &without_prefix[..without_prefix.len() - 4] // Remove ".bmp"
    } else if without_prefix.ends_with(".jpg") {
        &without_prefix[..without_prefix.len() - 4] // Remove ".jpg"
    } else if without_prefix.ends_with(".png") {
        &without_prefix[..without_prefix.len() - 4] // Remove ".png"
    } else {
        without_prefix
    };

    // Split by underscore to get the prefix and two base64 parts
    let parts: Vec<&str> = without_suffix.split('_').collect();
    if parts.len() != 3 {
        return Err(anyhow::anyhow!(
            "Invalid combined filename format '{}'. Expected exactly 3 parts: PREFIX_BASE64_BASE64",
            combined_filename
        ));
    }

    // Extract processing type prefix and base64 parts
    let processing_prefix = parts[0];
    let first_encoded = parts[1];
    let second_encoded = parts[2];

    println!("Processing type: {}", style(processing_prefix).cyan());
    println!("Encoded parts:");
    println!("  First:  {}", style(first_encoded).yellow());
    println!("  Second: {}", style(second_encoded).yellow());
    println!();

    // Decode first filename
    let first_decoded = match general_purpose::STANDARD.decode(first_encoded) {
        Ok(bytes) => match String::from_utf8(bytes) {
            Ok(s) => s,
            Err(_) => {
                return Err(anyhow::anyhow!(
                    "First base64 part '{}' does not decode to valid UTF-8",
                    first_encoded
                ));
            }
        },
        Err(_) => {
            return Err(anyhow::anyhow!(
                "First base64 part '{}' is not valid base64",
                first_encoded
            ));
        }
    };

    // Decode second filename
    let second_decoded = match general_purpose::STANDARD.decode(second_encoded) {
        Ok(bytes) => match String::from_utf8(bytes) {
            Ok(s) => s,
            Err(_) => {
                return Err(anyhow::anyhow!(
                    "Second base64 part '{}' does not decode to valid UTF-8",
                    second_encoded
                ));
            }
        },
        Err(_) => {
            return Err(anyhow::anyhow!(
                "Second base64 part '{}' is not valid base64",
                second_encoded
            ));
        }
    };

    // Validate and display processing type
    use image_processing::ProcessingType;
    let processing_type_desc = match ProcessingType::from_prefix(processing_prefix) {
        Some(ProcessingType::BlackWhite) => "Black & White",
        Some(ProcessingType::SixColor) => "6-Color",
        Some(ProcessingType::SevenColor) => "7-Color",
        None => "Unknown",
    };

    // Display results
    println!("{}",
        style("✓ Successfully decoded combined filename:")
            .green()
            .bold()
    );
    println!();
    println!("  {}: {} ({})",
        style("Processing Type").bold().cyan(),
        style(processing_type_desc).bold().magenta(),
        style(processing_prefix).dim()
    );
    println!("  {}: {}",
        style("Original 1").bold().cyan(),
        style(&first_decoded).bold().green()
    );
    println!("  {}: {}",
        style("Original 2").bold().cyan(),
        style(&second_decoded).bold().green()
    );
    println!();

    // Show the decoding process
    println!("{}",
        style("Decoding process:")
            .dim()
    );
    println!("  {} → {} (processing type)",
        style(processing_prefix).dim(),
        style(processing_type_desc).dim()
    );
    println!("  {} → {}",
        style(first_encoded).dim(),
        style(&first_decoded).dim()
    );
    println!("  {} → {}",
        style(second_encoded).dim(),
        style(&second_decoded).dim()
    );

    Ok(())
}

fn main() -> Result<()> {
    let start_time = Instant::now();
    let args = Args::parse();

    // Print banner
    println!(
        "{}",
        style("ESP32 Photo Frame - Image Processor").bold().blue()
    );
    println!("{}", style("High-performance Rust implementation").dim());
    println!();

    // Handle find hash mode (dry-run doesn't apply here)
    if let Some(hash) = &args.find_hash {
        handle_find_hash(hash, &args)?;
        return Ok(());
    }

    // Handle find original mode (dry-run doesn't apply here)
    if let Some(combined_filename) = &args.find_original {
        handle_find_original(combined_filename, &args)?;
        return Ok(());
    }

    // Validate inputs (skip for hash lookup mode)
    validate_inputs(&args)?;

    // Create processing configuration
    let config = ProcessingConfig {
        processing_type: args.processing_type.clone().into(),
        target_width: args.target_width(),
        target_height: args.target_height(),
        auto_process: args.auto_process,
        font_name: args.font.clone(),
        font_size: args.pointsize as f32,
        annotation_background: args.annotate_background.clone(),
        extensions: args.extensions(),
        verbose: args.verbose,
        parallel_jobs: if args.jobs == 0 {
            num_cpus::get()
        } else {
            args.jobs
        },
        output_formats: args.output_formats(),
        // Python people detection configuration
        detect_people: args.detect_people,
        python_script_path: args.python_script_path.clone(),
        // Duplicate handling
        force: args.force,
        // Debug mode
        debug: args.debug,
        // Annotation control
        annotate: args.annotate,
        // Color correction
        auto_color_correct: args.auto_color_correct,
        // Dry run mode
        dry_run: args.dry_run,
    };

    if config.verbose {
        println!("{}", style("Configuration:").bold());
        println!(
            "  Target size: {}x{}",
            config.target_width, config.target_height
        );
        println!("  Processing type: {:?}", config.processing_type);
        println!("  Auto processing: {}", config.auto_process);
        println!("  Parallel jobs: {}", config.parallel_jobs);
        println!("  Extensions: {:?}", config.extensions);
        println!("  Font name: {:?}", config.font_name);
        println!("  Font Size: {:?}", config.font_size);
        println!(
            "  Annotation background: {:?}",
            config.annotation_background
        );
        println!("  Output formats: {:?}", config.output_formats);

        // People detection status
        println!("  People detection: {}", config.detect_people);
        if config.detect_people {
            if let Some(ref path) = config.python_script_path {
                println!("    Python script: {}", path.display());
            } else {
                println!("    Python script: not specified");
            }
        }

        // Debug mode status
        if config.debug {
            println!("  Debug mode: enabled - will visualize detection boxes");
        }

        // Annotation status
        println!(
            "  Filename annotations: {}",
            if config.annotate {
                "enabled"
            } else {
                "disabled"
            }
        );

        // Color correction status
        println!(
            "  Auto color correction: {}",
            if config.auto_color_correct {
                "enabled"
            } else {
                "disabled"
            }
        );
        if config.auto_color_correct {
            if color_correction::is_imagemagick_available() {
                println!("    ImageMagick: available (will be used)");
            } else {
                println!("    ImageMagick: not available (fallback to custom implementation)");
            }
        }

        // Dry run mode status
        if config.dry_run {
            println!("  Dry run mode: enabled (simulation only - no files will be created)");
        }

        println!();
    }

    // Validate debug mode requirements
    if config.debug {
        if !config.detect_people || config.python_script_path.is_none() {
            return Err(anyhow::anyhow!(
                "Debug mode requires people detection to be enabled with --detect-people and --python-script <PATH>"
            ));
        }
    }

    // Check ImageMagick availability for auto-color correction
    if config.auto_color_correct && !color_correction::is_imagemagick_available() {
        if config.verbose {
            println!("{}", style("Warning:").yellow().bold());
            println!(
                "  ImageMagick not found. Auto color correction will use custom implementation."
            );
            println!("  For best results, install ImageMagick: brew install imagemagick (macOS) or apt-get install imagemagick (Linux)");
            println!();
        }
    }

    // Create output directory (skip in dry-run mode)
    if !config.dry_run {
        std::fs::create_dir_all(&args.output_dir).context("Failed to create output directory")?;
    } else {
        verbose_println(config.verbose, "Dry run mode: Skipping output directory creation");
    }

    // Store values before moving config
    let parallel_jobs = config.parallel_jobs;
    let debug_mode = config.debug;
    let dry_run_mode = config.dry_run;
    let output_formats = config.output_formats.clone();

    // Initialize processing engine
    let engine = ProcessingEngine::new(config)?;

    // Initialize multi-progress system
    let multi_progress = MultiProgress::new();

    // Create discovery progress bar
    let discovery_pb = multi_progress.add(ProgressBar::new(args.input_paths.len() as u64));
    discovery_pb.set_style(
        ProgressStyle::with_template("{bar:20.green/blue} {pos:>2}/{len:2} {msg}")?
            .progress_chars("██▌ "),
    );
    discovery_pb.set_message("Scanning directories...");

    // Discover all images
    let image_files = engine.discover_images(&args.input_paths)?;
    discovery_pb.finish_with_message(format!("✓ Found {} images", image_files.len()));

    if image_files.is_empty() {
        println!(
            "{}",
            style("No images found with specified extensions").red()
        );
        return Ok(());
    }

    // Create main processing progress bar
    let main_progress = multi_progress.add(create_progress_bar(image_files.len() as u64));
    main_progress.set_message("Processing images");

    // Create individual thread progress bars
    let thread_count = parallel_jobs.min(image_files.len());
    let mut thread_progress_bars = Vec::new();

    for i in 0..thread_count {
        let thread_pb = multi_progress.add(ProgressBar::new(100));
        thread_pb.set_style(
            ProgressStyle::with_template(&format!(
                "Job {:02}: [{{bar:15.blue/cyan}}] {{msg}}",
                i + 1
            ))?
            .progress_chars("██▌ "),
        );
        thread_pb.set_message("Waiting...");
        thread_progress_bars.push(thread_pb);
    }

    // Create completion progress bar
    let completion_pb = multi_progress.add(ProgressBar::new(100));
    completion_pb.set_style(
        ProgressStyle::with_template("{bar:30.cyan/blue} {percent:>3}% {msg}")?
            .progress_chars("██▌ "),
    );
    completion_pb.set_message("Preparing to process...");

    // Process images based on mode
    let (results, skipped_results) = if debug_mode {
        completion_pb.set_message("Debug mode: visualizing detection boxes...");

        // In debug mode, process all images as a flat list (no pairing)
        let debug_results = engine.process_debug_batch(image_files.clone(), &args.output_dir)?;

        // Convert to the expected format (no skipped results in debug mode)
        let ok_results: Vec<Result<ProcessingResult, anyhow::Error>> =
            debug_results.into_iter().map(|r| Ok(r)).collect();

        (ok_results, Vec::new())
    } else {
        // Normal processing with smart portrait pairing
        engine.process_batch_with_smart_pairing(
            &image_files,
            &args.output_dir,
            &main_progress,
            &thread_progress_bars,
            &completion_pb,
        )?
    };

    // Finish all progress bars
    main_progress.finish_with_message("✓ Processing complete!");
    completion_pb.finish_with_message("✓ All tasks completed");

    // Clean up thread progress bars
    for (i, pb) in thread_progress_bars.iter().enumerate() {
        pb.finish_with_message(format!("✓ Thread {} finished", i + 1));
    }

    println!();

    // Print results summary
    let successful = results.iter().filter(|r| r.is_ok()).count();
    let failed = results.len() - successful;
    let skipped = skipped_results.len();
    let total_time = start_time.elapsed();

    // Calculate processing statistics
    let mut people_hits = 0;
    let mut people_misses = 0;
    let mut total_people_found = 0;
    let mut portraits_processed = 0;
    let mut landscapes_processed = 0;

    for result in &results {
        if let Ok(processing_result) = result {
            if processing_result.people_detected {
                people_hits += 1;
                total_people_found += processing_result.people_count;
            } else {
                people_misses += 1;
            }

            // Count image types
            match processing_result.image_type {
                ImageType::Portrait => portraits_processed += 1,
                ImageType::Landscape => landscapes_processed += 1,
                ImageType::CombinedPortrait => landscapes_processed += 1, // Combined portraits count as landscapes
            }
        }
    }

    let header = if dry_run_mode {
        style("Dry Run Results Summary:").bold().cyan()
    } else {
        style("Results Summary:").bold().green()
    };
    println!("{}", header);

    let processed_label = if dry_run_mode {
        "Would be processed:"
    } else {
        "Successfully processed:"
    };
    println!(
        "  {}: {}",
        processed_label,
        style(successful).bold().green()
    );
    if failed > 0 {
        println!("  Failed: {}", style(failed).bold().red());
    }
    if skipped > 0 {
        println!(
            "  Skipped (already exist): {}",
            style(skipped).bold().yellow()
        );
    }

    // Image type statistics
    if portraits_processed > 0 || landscapes_processed > 0 {
        println!();
        println!("{}", style("Image Types:").bold().blue());
        if landscapes_processed > 0 {
            println!(
                "  Landscape images: {}",
                style(landscapes_processed).bold().cyan()
            );
        }
        if portraits_processed > 0 {
            println!(
                "  Portrait images: {} (combined into landscape pairs)",
                style(portraits_processed).bold().magenta()
            );
        }
    }

    // People detection statistics (only show if AI feature was used)
    if people_hits > 0 || people_misses > 0 {
        println!();
        println!("{}", style("People Detection Results:").bold().cyan());
        println!(
            "  Images with people detected: {}",
            style(people_hits).bold().green()
        );
        println!(
            "  Images without people: {}",
            style(people_misses).bold().yellow()
        );
        if people_hits > 0 {
            println!(
                "  Total people found: {}",
                style(total_people_found).bold().cyan()
            );
            println!(
                "  Average people per image: {:.1}",
                style(format!(
                    "{:.1}",
                    total_people_found as f64 / people_hits as f64
                ))
                .dim()
            );
        }
        let detection_rate = if (people_hits + people_misses) > 0 {
            (people_hits as f64 / (people_hits + people_misses) as f64) * 100.0
        } else {
            0.0
        };
        println!(
            "  People detection rate: {:.1}%",
            style(format!("{:.1}%", detection_rate)).bold()
        );
    }

    // Detailed processing results
    if successful > 0 {
        println!();
        let detailed_header = if dry_run_mode {
            style("Detailed Simulation Results:").bold().blue()
        } else {
            style("Detailed Processing Results:").bold().blue()
        };
        println!("{}", detailed_header);
        let mut success_count = 0;
        for (i, result) in results.iter().enumerate() {
            if let Ok(processing_result) = result {
                success_count += 1;
                let filename = if i < image_files.len() {
                    image_files[i]
                        .file_name()
                        .and_then(|name| name.to_str())
                        .unwrap_or("unknown")
                } else {
                    "unknown"
                };

                // Show image type
                let image_type = match processing_result.image_type {
                    ImageType::Portrait => style("Portrait").magenta(),
                    ImageType::Landscape => style("Landscape").cyan(),
                    ImageType::CombinedPortrait => style("Combined").yellow(),
                };

                // Show people detection result
                let people_info = if processing_result.people_detected {
                    if processing_result.people_count == 1 {
                        style(format!("✓ {} person", processing_result.people_count)).green()
                    } else {
                        style(format!("✓ {} people", processing_result.people_count)).green()
                    }
                } else {
                    style(format!("○ No people")).dim()
                };

                // Show destination filenames in dry-run mode
                let destination_info = if dry_run_mode {
                    // Get the first output path as an example (they all have the same stem)
                    if let Some((_, first_path)) = processing_result.output_paths.iter().next() {
                        if let Some(dest_filename) = first_path.file_stem().and_then(|s| s.to_str()) {
                            format!(" → {}", style(dest_filename).cyan())
                        } else {
                            String::new()
                        }
                    } else {
                        String::new()
                    }
                } else {
                    String::new()
                };

                println!(
                    "  {}: {} [{}] - {}{}",
                    style(format!("#{}", success_count)).dim(),
                    style(filename).bold(),
                    image_type,
                    people_info,
                    destination_info
                );

                // For combined portraits, show individual portrait detection results
                if let Some(ref individual_results) = processing_result.individual_portraits {
                    for (idx, portrait) in individual_results.iter().enumerate() {
                        let portrait_filename = portrait
                            .path
                            .file_name()
                            .and_then(|name| name.to_str())
                            .unwrap_or("unknown");

                        let portrait_people_info = if portrait.people_detected {
                            if portrait.people_count == 1 {
                                style(format!(
                                    "✓ {} person (conf: {:.0}%)",
                                    portrait.people_count,
                                    portrait.confidence * 100.0
                                ))
                                .green()
                            } else {
                                style(format!(
                                    "✓ {} people (conf: {:.0}%)",
                                    portrait.people_count,
                                    portrait.confidence * 100.0
                                ))
                                .green()
                            }
                        } else {
                            style(format!("○ No people")).dim()
                        };

                        println!(
                            "    └─ {}: {} - {}",
                            style(format!("Portrait {}", idx + 1)).dim(),
                            style(portrait_filename).bold(),
                            portrait_people_info
                        );
                    }
                }
            }
        }
    }

    println!();
    println!("{}", style("Performance:").bold().blue());
    println!(
        "  Total processing time: {}",
        style(format_duration(total_time)).bold()
    );
    println!(
        "  Average time per image: {}",
        style(format_duration(total_time / image_files.len() as u32)).dim()
    );

    println!();
    let output_header = if dry_run_mode {
        style("Output files (would be created):").bold().cyan()
    } else {
        style("Output files:").bold().green()
    };
    println!("{}", output_header);
    let location_label = if dry_run_mode {
        "Would be saved to:"
    } else {
        "All files:"
    };
    println!("  {}: {}", location_label, args.output_dir.display());

    if dry_run_mode {
        println!();
        println!("{}", style("💡 Dry Run Mode:").bold().yellow());
        println!("  • No files were created during this simulation");
        println!("  • Remove --dry-run to actually process the images");
        println!("  • Output formats: {:?}", output_formats);
    }

    if failed > 0 {
        println!();
        println!("{}", style("Errors encountered:").bold().red());
        let mut error_count = 0;
        for (i, result) in results.iter().enumerate() {
            if let Err(e) = result {
                error_count += 1;
                // Try to get the actual filename from the image_files list
                let filename = if i < image_files.len() {
                    image_files[i]
                        .file_name()
                        .and_then(|name| name.to_str())
                        .unwrap_or("unknown")
                } else {
                    "unknown"
                };
                println!(
                    "  {}: {} - {}",
                    style(format!("#{}", error_count)).dim(),
                    style(filename).bold().red(),
                    e
                );
            }
        }

        if error_count > 0 {
            println!();
            println!(
                "{}",
                style(format!(
                    "⚠ {} errors occurred during processing",
                    error_count
                ))
                .bold()
                .yellow()
            );
            println!("  Check image files and try again with --verbose for more details");
        }
    }

    // Show skipped files if any
    if !skipped_results.is_empty() {
        println!();
        println!(
            "{}",
            style("Skipped files (already exist):").bold().yellow()
        );
        for (i, skipped) in skipped_results.iter().enumerate() {
            let filename = skipped
                .input_path
                .file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("unknown");

            let reason_desc = match skipped.reason {
                SkipReason::LandscapeExists => "skipped",
                SkipReason::PortraitInCombined => "skipped",
            };

            let existing_filename = skipped
                .existing_output_path
                .file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("unknown");

            println!(
                "  {}: {} - {} (existing: {})",
                style(format!("#{}", i + 1)).dim(),
                style(filename).bold().yellow(),
                reason_desc,
                style(existing_filename).dim()
            );
        }

        println!();
        println!(
            "{}",
            style(format!(
                "ℹ {} files skipped to avoid duplicates",
                skipped_results.len()
            ))
            .bold()
            .blue()
        );
        println!("  Use --force to process all files regardless of existing outputs");
    }

    Ok(())
}
