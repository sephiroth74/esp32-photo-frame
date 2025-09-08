use anyhow::{Context, Result};
use clap::Parser;
use console::style;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use std::time::Instant;

mod cli;
mod image_processing;
mod utils;

use cli::{Args, ColorType};
use image_processing::{ImageType, ProcessingConfig, ProcessingEngine, ProcessingType, SkipReason};
use utils::{create_progress_bar, format_duration, validate_inputs, find_original_filename_for_hash};

impl From<ColorType> for ProcessingType {
    fn from(color_type: ColorType) -> Self {
        match color_type {
            ColorType::BlackWhite => ProcessingType::BlackWhite,
            ColorType::SixColor => ProcessingType::SixColor,
        }
    }
}

/// Handle hash lookup functionality
fn handle_hash_lookup(target_hash: &str, args: &Args) -> Result<()> {
    // Validate hash format (8 hex characters)
    if target_hash.len() != 8 || !target_hash.chars().all(|c| c.is_ascii_hexdigit()) {
        return Err(anyhow::anyhow!(
            "Invalid hash format '{}'. Expected 8 hex characters (e.g., '7af9ecca')",
            target_hash
        ));
    }

    println!("{}", style(format!("Searching for original filename with hash: {}", target_hash)).bold().cyan());
    println!();

    // Use current directory if no search directories specified
    let search_dirs: Vec<std::path::PathBuf> = if args.input_paths.is_empty() {
        vec![std::env::current_dir()?]
    } else {
        args.input_paths.clone()
    };

    // Convert to Path references
    let search_paths: Vec<&std::path::Path> = search_dirs.iter().map(|p| p.as_path()).collect();

    // Use default extensions if not specified
    let extensions = args.extensions();
    
    println!("Search directories:");
    for path in &search_paths {
        println!("  {}", path.display());
    }
    println!("Extensions: {:?}", extensions);
    println!();

    // Find matching filenames
    let matches = find_original_filename_for_hash(&search_paths, target_hash, &extensions)?;

    if matches.is_empty() {
        println!("{}", style("No matching files found").yellow().bold());
        println!();
        println!("Tips:");
        println!("  • Make sure you're searching in the correct directories");
        println!("  • Check that the hash is correct (8 hex characters)");  
        println!("  • Verify the file extensions match: {:?}", extensions);
    } else {
        println!("{}", style(format!("Found {} matching file(s):", matches.len())).green().bold());
        println!();
        
        for (i, file_path) in matches.iter().enumerate() {
            let path = std::path::Path::new(file_path);
            let filename = path.file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("unknown");
            let stem = path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("unknown");
            
            println!("  {}: {}", 
                style(format!("#{}", i + 1)).dim(),
                style(filename).bold().green()
            );
            println!("      Path: {}", style(file_path).dim());
            println!("      Stem: {} → Hash: {}", 
                style(stem).cyan(), 
                style(target_hash).yellow()
            );
            println!();
        }
        
        if matches.len() == 1 {
            println!("{}", style("✓ Exact match found!").green().bold());
        } else {
            println!("{}", style("⚠ Multiple matches found - this is unusual but possible").yellow().bold());
        }
    }

    Ok(())
}

fn main() -> Result<()> {
    let start_time = Instant::now();
    let args = Args::parse();

    // Print banner
    println!("{}", style("ESP32 Photo Frame - Image Processor").bold().blue());
    println!("{}", style("High-performance Rust implementation").dim());
    println!();

    // Handle hash lookup mode
    if let Some(target_hash) = &args.find_hash {
        handle_hash_lookup(target_hash, &args)?;
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
        parallel_jobs: if args.jobs == 0 { num_cpus::get() } else { args.jobs },
        output_format: args.output_format.clone(),
        // Python people detection configuration
        detect_people: args.detect_people,
        python_script_path: args.python_script_path.clone(),
        // Duplicate handling
        force: args.force,
    };

    if config.verbose {
        println!("{}", style("Configuration:").bold());
        println!("  Target size: {}x{}", config.target_width, config.target_height);
        println!("  Processing type: {:?}", config.processing_type);
        println!("  Auto processing: {}", config.auto_process);
        println!("  Parallel jobs: {}", config.parallel_jobs);
        println!("  Extensions: {:?}", config.extensions);
        println!("  Font name: {:?}", config.font_name);
        println!("  Font Size: {:?}", config.font_size);
        println!("  Annotation background: {:?}", config.annotation_background);
        println!("  Output format: {:?}", config.output_format);
        
        // People detection status
        println!("  People detection: {}", config.detect_people);
        if config.detect_people {
            if let Some(ref path) = config.python_script_path {
                println!("    Python script: {}", path.display());
            } else {
                println!("    Python script: not specified");
            }
        }
        
        println!();
    }

    // Create output directory
    std::fs::create_dir_all(&args.output_dir)
        .context("Failed to create output directory")?;

    // Store values before moving config
    let parallel_jobs = config.parallel_jobs;
    
    // Initialize processing engine
    let engine = ProcessingEngine::new(config)?;

    // Initialize multi-progress system
    let multi_progress = MultiProgress::new();
    
    // Create discovery progress bar
    let discovery_pb = multi_progress.add(ProgressBar::new(args.input_paths.len() as u64));
    discovery_pb.set_style(
        ProgressStyle::with_template("{bar:20.green/blue} {pos:>2}/{len:2} {msg}")?
            .progress_chars("██▌ ")
    );
    discovery_pb.set_message("Scanning directories...");

    // Discover all images
    let image_files = engine.discover_images(&args.input_paths)?;
    discovery_pb.finish_with_message(format!("✓ Found {} images", image_files.len()));
    
    if image_files.is_empty() {
        println!("{}", style("No images found with specified extensions").red());
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
            ProgressStyle::with_template(&format!("Thread {}: [{{bar:15.blue/cyan}}] {{msg}}", i + 1))?
                .progress_chars("██▌ ")
        );
        thread_pb.set_message("Waiting...");
        thread_progress_bars.push(thread_pb);
    }

    // Create completion progress bar
    let completion_pb = multi_progress.add(ProgressBar::new(100));
    completion_pb.set_style(
        ProgressStyle::with_template("{bar:30.cyan/blue} {percent:>3}% {msg}")?
            .progress_chars("██▌ ")
    );
    completion_pb.set_message("Preparing to process...");

    // Process all images with smart portrait pairing
    let (results, skipped_results) = engine.process_batch_with_smart_pairing(
        &image_files, 
        &args.output_dir, 
        &main_progress,
        &thread_progress_bars,
        &completion_pb
    )?;

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

    println!("{}", style("Results Summary:").bold().green());
    println!("  Successfully processed: {}", style(successful).bold().green());
    if failed > 0 {
        println!("  Failed: {}", style(failed).bold().red());
    }
    if skipped > 0 {
        println!("  Skipped (already exist): {}", style(skipped).bold().yellow());
    }
    
    // Image type statistics
    if portraits_processed > 0 || landscapes_processed > 0 {
        println!();
        println!("{}", style("Image Types:").bold().blue());
        if landscapes_processed > 0 {
            println!("  Landscape images: {}", style(landscapes_processed).bold().cyan());
        }
        if portraits_processed > 0 {
            println!("  Portrait images: {} (combined into landscape pairs)", style(portraits_processed).bold().magenta());
        }
    }
    
    // People detection statistics (only show if AI feature was used)
    if people_hits > 0 || people_misses > 0 {
        println!();
        println!("{}", style("People Detection Results:").bold().cyan());
        println!("  Images with people detected: {}", style(people_hits).bold().green());
        println!("  Images without people: {}", style(people_misses).bold().yellow());
        if people_hits > 0 {
            println!("  Total people found: {}", style(total_people_found).bold().cyan());
            println!("  Average people per image: {:.1}", 
                style(format!("{:.1}", total_people_found as f64 / people_hits as f64)).dim());
        }
        let detection_rate = if (people_hits + people_misses) > 0 {
            (people_hits as f64 / (people_hits + people_misses) as f64) * 100.0
        } else {
            0.0
        };
        println!("  People detection rate: {:.1}%", 
            style(format!("{:.1}%", detection_rate)).bold());
    }
    
    // Detailed processing results
    if successful > 0 {
        println!();
        println!("{}", style("Detailed Processing Results:").bold().blue());
        let mut success_count = 0;
        for (i, result) in results.iter().enumerate() {
            if let Ok(processing_result) = result {
                success_count += 1;
                let filename = if i < image_files.len() {
                    image_files[i].file_name()
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
                
                println!("  {}: {} [{}] - {}", 
                    style(format!("#{}", success_count)).dim(),
                    style(filename).bold(),
                    image_type,
                    people_info
                );
                
                // For combined portraits, show individual portrait detection results
                if let Some(ref individual_results) = processing_result.individual_portraits {
                    for (idx, portrait) in individual_results.iter().enumerate() {
                        let portrait_filename = portrait.path.file_name()
                            .and_then(|name| name.to_str())
                            .unwrap_or("unknown");
                        
                        let portrait_people_info = if portrait.people_detected {
                            if portrait.people_count == 1 {
                                style(format!("✓ {} person (conf: {:.0}%)", portrait.people_count, portrait.confidence * 100.0)).green()
                            } else {
                                style(format!("✓ {} people (conf: {:.0}%)", portrait.people_count, portrait.confidence * 100.0)).green()
                            }
                        } else {
                            style(format!("○ No people")).dim()
                        };
                        
                        println!("    └─ {}: {} - {}", 
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
    println!("  Total processing time: {}", style(format_duration(total_time)).bold());
    println!("  Average time per image: {}", 
        style(format_duration(total_time / image_files.len() as u32)).dim());
    
    println!();
    println!("{}", style("Output files:").bold());
    println!("  All files: {}", args.output_dir.display());

    if failed > 0 {
        println!();
        println!("{}", style("Errors encountered:").bold().red());
        let mut error_count = 0;
        for (i, result) in results.iter().enumerate() {
            if let Err(e) = result {
                error_count += 1;
                // Try to get the actual filename from the image_files list
                let filename = if i < image_files.len() {
                    image_files[i].file_name()
                        .and_then(|name| name.to_str())
                        .unwrap_or("unknown")
                } else {
                    "unknown"
                };
                println!("  {}: {} - {}", 
                    style(format!("#{}", error_count)).dim(), 
                    style(filename).bold().red(),
                    e
                );
            }
        }
        
        if error_count > 0 {
            println!();
            println!("{}", style(format!("⚠ {} errors occurred during processing", error_count)).bold().yellow());
            println!("  Check image files and try again with --verbose for more details");
        }
    }

    // Show skipped files if any
    if !skipped_results.is_empty() {
        println!();
        println!("{}", style("Skipped files (already exist):").bold().yellow());
        for (i, skipped) in skipped_results.iter().enumerate() {
            let filename = skipped.input_path.file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("unknown");
            
            let reason_desc = match skipped.reason {
                SkipReason::LandscapeExists => "skipped",
                SkipReason::PortraitInCombined => "skipped",
            };
            
            let existing_filename = skipped.existing_output_path.file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("unknown");
            
            println!("  {}: {} - {} (existing: {})", 
                style(format!("#{}", i + 1)).dim(), 
                style(filename).bold().yellow(),
                reason_desc,
                style(existing_filename).dim()
            );
        }
        
        println!();
        println!("{}", style(format!("ℹ {} files skipped to avoid duplicates", skipped_results.len())).bold().blue());
        println!("  Use --force to process all files regardless of existing outputs");
    }

    Ok(())
}