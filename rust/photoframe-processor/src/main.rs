use anyhow::{Context, Result};
use clap::Parser;
use console::style;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use serde_json;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Instant;

mod cli;
mod config_file;
mod image_processing;
mod json_output;
mod utils;

use cli::{Args, ColorType, DitherMethod};
use image_processing::{
    color_correction, ImageType, ProcessingConfig, ProcessingEngine, ProcessingResult,
    ProcessingType,
};
use json_output::JsonMessage;
use utils::{create_progress_bar, format_duration, validate_inputs, verbose_println};

impl From<ColorType> for ProcessingType {
    fn from(color_type: ColorType) -> Self {
        match color_type {
            ColorType::BlackWhite => ProcessingType::BlackWhite,
            ColorType::SixColor => ProcessingType::SixColor,
        }
    }
}

fn main() -> Result<()> {
    let start_time = Instant::now();
    let mut args = Args::parse();

    // Load config file if specified
    args.load_and_merge_config()?;

    // Print banner
    println!(
        "{}",
        style("ESP32 Photo Frame - Image Processor").bold().blue()
    );
    println!("{}", style("High-performance Rust implementation").dim());
    println!();

    // Validate inputs
    validate_inputs(&args)?;

    // Parse divider color
    let divider_color_rgba = image_processing::annotate::parse_hex_color(&args.divider_color)
        .with_context(|| format!("Invalid divider color: {}", args.divider_color))?;
    let divider_color = image::Rgb([
        divider_color_rgba.0,
        divider_color_rgba.1,
        divider_color_rgba.2,
    ]);

    // Create processing configuration
    let (target_width, target_height) = args.get_dimensions();

    let config = ProcessingConfig {
        processing_type: args.processing_type.clone().into(),
        target_width,
        target_height,
        auto_orientation: true, // Always enabled for smart orientation detection
        font_name: args.font.clone(),
        font_size: args.pointsize as f32,
        annotation_background: args.annotate_background.clone(),
        extensions: args.parse_extensions(),
        verbose: args.verbose,
        parallel_jobs: if args.jobs == 0 {
            num_cpus::get()
        } else {
            args.jobs
        },
        output_formats: args
            .parse_output_formats()
            .map_err(|e| anyhow::anyhow!(e))?,
        // AI people detection configuration
        detect_people: args.detect_people(),
        // Debug mode
        debug: args.debug,
        // Annotation control
        annotate: args.annotate,
        // Color correction
        auto_color_correct: args.auto_color_correct,
        // Dry run mode
        dry_run: args.dry_run,
        // Confidence threshold for people detection
        confidence_threshold: args.confidence_threshold(),
        // Portrait combination divider settings
        divider_width: args.divider_width,
        divider_color,
        // Dithering method
        dithering_method: args.dithering_method.clone(),
        // Dithering strength
        dither_strength: args.dither_strength,
        // Contrast adjustment
        contrast: args.contrast,
        // Brightness adjustment
        brightness: args.brightness,
        // Saturation boost
        saturation_boost: args.saturation_boost,
        // Auto-optimization
        auto_optimize: args.auto_optimize,
        // Always collect report data in JSON mode, otherwise use --report flag
        optimization_report: args.json_progress || args.report,
        // Target display orientation
        target_orientation: args.target_orientation.clone(),
        // Pre-rotation (only for 6c portrait mode)
        needs_pre_rotation: args.needs_pre_rotation(),
        // JSON progress output
        json_progress: args.json_progress,
    };

    // Suppress all output when JSON progress is enabled (except JSON)
    if config.verbose && !config.json_progress {
        println!("{}", style("Configuration:").bold());
        println!(
            "  Target size: {}x{}",
            config.target_width, config.target_height
        );
        println!("  Processing type: {:?}", config.processing_type);
        println!("  Auto processing: {}", config.auto_orientation);
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
            println!("    Using embedded YOLO11 model");
            println!(
                "    Confidence threshold: {:.2}",
                config.confidence_threshold
            );
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

        // Portrait combination divider settings
        println!("  Portrait divider:");
        println!("    Width: {} pixels", config.divider_width);
        println!(
            "    Color: RGB({}, {}, {})",
            config.divider_color[0], config.divider_color[1], config.divider_color[2]
        );

        // Dithering method
        println!(
            "  Dithering method: {}",
            match config.dithering_method {
                DitherMethod::FloydSteinberg => "Floyd-Steinberg (enhanced)",
                DitherMethod::Atkinson => "Atkinson (brightness preserving)",
                DitherMethod::Stucki => "Stucki (pattern reducing)",
                DitherMethod::JarvisJudiceNinke => "Jarvis-Judice-Ninke (photo optimized)",
                DitherMethod::Ordered => "Ordered/Bayer (text optimized)",
            }
        );
        println!("    Strength: {:.1}x", config.dither_strength);

        // Contrast adjustment
        println!(
            "  Contrast adjustment: {}{}",
            if config.contrast >= 0 { "+" } else { "" },
            config.contrast
        );

        // Brightness adjustment
        println!(
            "  Brightness adjustment: {}{}",
            if config.brightness >= 0 { "+" } else { "" },
            config.brightness
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
        if !config.detect_people {
            return Err(anyhow::anyhow!(
                "Debug mode requires people detection to be enabled with --detect-people"
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
        verbose_println(
            config.verbose,
            "Dry run mode: Skipping output directory creation",
        );
    }

    // Store values before moving config
    let parallel_jobs = config.parallel_jobs;
    let debug_mode = config.debug;
    let dry_run_mode = config.dry_run;
    let json_progress = config.json_progress;
    let output_formats = config.output_formats.clone();

    // Initialize processing engine
    let engine = ProcessingEngine::new(config)?;

    // Initialize multi-progress system (disabled in debug or JSON mode)
    let multi_progress = MultiProgress::new();

    // Create discovery progress bar (hidden in debug/JSON mode)
    let discovery_pb = if debug_mode || json_progress {
        ProgressBar::hidden()
    } else {
        let pb = multi_progress.add(ProgressBar::new(args.input_paths.len() as u64));
        pb.set_style(
            ProgressStyle::with_template("{bar:20.green/blue} {pos:>2}/{len:2} {msg}")?
                .progress_chars("██▌ "),
        );
        pb.set_message("Scanning directories...");
        pb
    };

    // Discover all images
    let image_files = engine.discover_images(&args.input_paths)?;

    if json_progress {
        // JSON mode: emit initial progress
        JsonMessage::progress(0, image_files.len(), "Discovered images");
    } else if debug_mode {
        verbose_println(true, &format!("Found {} image files", image_files.len()));
    } else {
        discovery_pb.finish_with_message(format!("✓ Found {} images", image_files.len()));
    }

    if image_files.is_empty() {
        if !json_progress {
            println!(
                "{}",
                style("No images found with specified extensions").red()
            );
        }
        return Ok(());
    }

    // Create main processing progress bar (only if not in debug mode)
    let main_progress = if json_progress {
        // JSON mode: Create a progress bar that emits JSON on updates
        let pb = ProgressBar::hidden();
        pb.set_length(image_files.len() as u64);
        pb.set_draw_target(indicatif::ProgressDrawTarget::hidden());

        // Wrap with callback to emit JSON on progress
        // Note: We'll emit JSON manually based on results count
        pb
    } else if debug_mode {
        ProgressBar::hidden()
    } else {
        let pb = multi_progress.add(create_progress_bar(image_files.len() as u64));
        pb.set_message("Processing images");
        pb
    };

    // Create individual thread progress bars (only if not in debug mode)
    let thread_count = parallel_jobs.min(image_files.len());
    let mut thread_progress_bars = Vec::new();

    if debug_mode || json_progress {
        // In debug mode, create hidden progress bars
        for _ in 0..thread_count {
            thread_progress_bars.push(ProgressBar::hidden());
        }
    } else {
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
    }

    // Create completion progress bar (only if not in debug mode)
    let completion_pb = if debug_mode || json_progress {
        ProgressBar::hidden()
    } else {
        let pb = multi_progress.add(ProgressBar::new(100));
        pb.set_style(
            ProgressStyle::with_template("{bar:30.cyan/blue} {percent:>3}% {msg}")?
                .progress_chars("██▌ "),
        );
        pb.set_message("Preparing to process...");
        pb
    };

    // Spawn JSON progress monitoring thread if in JSON mode
    let processing_complete = Arc::new(AtomicBool::new(false));
    let json_monitor_handle = if json_progress {
        let main_pb_clone = main_progress.clone();
        let complete_flag = Arc::clone(&processing_complete);

        Some(std::thread::spawn(move || {
            let mut last_position = 0;

            loop {
                let current_position = main_pb_clone.position() as usize;
                let total = main_pb_clone.length().unwrap_or(0) as usize;

                if current_position != last_position {
                    JsonMessage::progress(
                        current_position,
                        total,
                        format!("Processing {}/{}", current_position, total),
                    );
                    last_position = current_position;
                }

                // Exit when processing is complete
                if complete_flag.load(Ordering::Relaxed) {
                    // Emit final progress with updated total
                    let final_position = main_pb_clone.position() as usize;
                    let final_total = main_pb_clone.length().unwrap_or(0) as usize;
                    if final_position != last_position {
                        JsonMessage::progress(
                            final_position,
                            final_total,
                            format!("Processing {}/{}", final_position, final_total),
                        );
                    }
                    break;
                }

                std::thread::sleep(std::time::Duration::from_millis(40)); // 25 FPS
            }
        }))
    } else {
        None
    };

    // Process images based on mode
    let results = if debug_mode {
        verbose_println(true, "Debug mode: visualizing detection boxes...");

        // In debug mode, process all images as a flat list (no pairing)
        let debug_results = engine.process_debug_batch(image_files.clone(), &args.output_dir)?;

        // Convert to the expected format
        let ok_results: Vec<Result<ProcessingResult, anyhow::Error>> =
            debug_results.into_iter().map(|r| Ok(r)).collect();

        ok_results
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

    // Signal processing completion and wait for JSON monitor thread to finish
    processing_complete.store(true, Ordering::Relaxed);
    if let Some(handle) = json_monitor_handle {
        let _ = handle.join();
    }

    // Finish all progress bars (only if not in debug mode)
    if json_progress {
        // JSON mode: Don't emit any more progress messages here
        // The complete message will be sent later with full report
    } else if debug_mode {
        verbose_println(true, "✓ Processing complete!");
    } else {
        main_progress.finish_with_message("✓ Processing complete!");
        completion_pb.finish_with_message("✓ All tasks completed");

        // Clean up thread progress bars
        for (i, pb) in thread_progress_bars.iter().enumerate() {
            pb.finish_with_message(format!("✓ Thread {} finished", i + 1));
        }
    }

    if !json_progress {
        println!();
    }

    // Print results summary
    let successful = results.iter().filter(|r| r.is_ok()).count();
    let failed = results.len() - successful;
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

            // Count image types - count the ORIGINAL images that were processed
            match processing_result.image_type {
                ImageType::Portrait => portraits_processed += 1,
                ImageType::Landscape => landscapes_processed += 1,
                ImageType::CombinedPortrait => {
                    // Two portrait images were combined side-by-side
                    portraits_processed += 2;
                }
                ImageType::CombinedLandscape => {
                    // Two landscape images were combined top-bottom
                    landscapes_processed += 2;
                }
            }
        }
    }

    // Calculate skipped images (discovered but not processed - e.g., unpaired images)
    let total_images_processed = portraits_processed + landscapes_processed;
    let skipped = if image_files.len() > total_images_processed {
        image_files.len() - total_images_processed
    } else {
        0
    };

    // Handle JSON complete message first if in JSON mode
    if json_progress {
        // JSON mode: emit final summary with report data
        let report = engine.get_optimization_report();
        let report_guard = report.lock().unwrap();

        // Always generate summary
        let mut summary_json = report_guard.get_summary();

        // Add skipped count to the summary
        if let Some(obj) = summary_json.as_object_mut() {
            obj.insert(
                "skipped_images".to_string(),
                serde_json::Value::Number(serde_json::Number::from(skipped)),
            );
        }

        // In JSON mode, ALWAYS include the full report (regardless of --report flag)
        // The GUI can choose whether to show it or not
        let report_json =
            Some(serde_json::to_value(&*report_guard).unwrap_or(serde_json::Value::Null));

        JsonMessage::complete(
            image_files.len(),
            successful,
            failed,
            total_time.as_secs_f64(),
            report_json,
            summary_json,
        );
    }
    // Skip all console output in JSON mode (only JSON events are emitted)
    else {
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
        // Show total images processed, not output count
        println!(
            "  {}: {} images",
            processed_label,
            style(total_images_processed).bold().green()
        );
        // Show output count if different from processed count
        if successful != total_images_processed && successful > 0 {
            println!(
                "  Output files generated: {}",
                style(successful).bold().cyan()
            );
        }
        if failed > 0 {
            println!("  Failed: {}", style(failed).bold().red());
        }
        if skipped > 0 {
            println!("  Skipped (unpaired): {}", style(skipped).bold().yellow());
        }

        // Image type statistics
        if portraits_processed > 0 || landscapes_processed > 0 {
            println!();
            println!("{}", style("Image Types:").bold().blue());

            // Show portrait images
            if portraits_processed > 0 {
                let portrait_msg =
                    if args.target_orientation == crate::cli::TargetOrientation::Landscape {
                        format!(
                            "  Portrait images: {} (combined side-by-side into landscape pairs)",
                            style(portraits_processed).bold().magenta()
                        )
                    } else {
                        format!(
                            "  Portrait images: {} (processed individually)",
                            style(portraits_processed).bold().magenta()
                        )
                    };
                println!("{}", portrait_msg);
            }

            // Show landscape images
            if landscapes_processed > 0 {
                let landscape_msg =
                    if args.target_orientation == crate::cli::TargetOrientation::Portrait {
                        format!(
                            "  Landscape images: {} (combined top-bottom into portrait pairs)",
                            style(landscapes_processed).bold().cyan()
                        )
                    } else {
                        format!(
                            "  Landscape images: {} (processed individually)",
                            style(landscapes_processed).bold().cyan()
                        )
                    };
                println!("{}", landscape_msg);
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
                "  People detection rate: {}",
                style(format!("{:.1}%", detection_rate)).bold()
            );
        }

        // Print processing report table if enabled (replaces detailed results)
        if args.report {
            println!();
            let report = engine.get_optimization_report();
            let report_guard = report.lock().unwrap();
            report_guard.print_formatted(&args.report_format);
        } else if successful > 0 {
            // Show simple detailed results only if report is not enabled
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
                        ImageType::CombinedPortrait => style("Combined H").yellow(),
                        ImageType::CombinedLandscape => style("Combined V").yellow(),
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
                        if let Some((_, first_path)) = processing_result.output_paths.iter().next()
                        {
                            if let Some(dest_filename) =
                                first_path.file_stem().and_then(|s| s.to_str())
                            {
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

        // Print performance stats only in normal mode (not JSON)
        if !json_progress {
            // Normal mode: print performance stats
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
        }

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
    } // End of !json_progress block

    Ok(())
}
