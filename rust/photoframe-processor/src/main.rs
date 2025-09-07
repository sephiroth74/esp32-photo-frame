use anyhow::{Context, Result};
use clap::Parser;
use console::style;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use std::time::Instant;

mod cli;
mod image_processing;
mod utils;

use cli::{Args, ColorType};
use image_processing::{ProcessingConfig, ProcessingEngine, ProcessingType};
use utils::{create_progress_bar, format_duration, validate_inputs};

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
    let args = Args::parse();

    // Print banner
    println!("{}", style("ESP32 Photo Frame - Image Processor").bold().blue());
    println!("{}", style("High-performance Rust implementation").dim());
    println!();

    // Validate inputs
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
        // YOLO people detection configuration
        #[cfg(feature = "ai")]
        detect_people: args.detect_people,
        #[cfg(feature = "ai")]
        yolo_assets_dir: args.yolo_assets_dir.clone(),
        #[cfg(feature = "ai")]
        yolo_confidence: args.yolo_confidence,
        #[cfg(feature = "ai")]
        yolo_nms_threshold: args.yolo_nms_threshold,
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
        
        // AI feature status
        #[cfg(feature = "ai")]
        {
            println!("  AI features: {}", style("ENABLED").green().bold());
            println!("  People detection: {}", config.detect_people);
            if config.detect_people {
                println!("    YOLO assets dir: {:?}", config.yolo_assets_dir);
                println!("    YOLO confidence: {:.2}", config.yolo_confidence);
                println!("    YOLO NMS threshold: {:.2}", config.yolo_nms_threshold);
            }
        }
        
        #[cfg(not(feature = "ai"))]
        {
            println!("  AI features: {}", style("DISABLED").red());
            println!("    Note: Compile with --features ai to enable YOLO people detection");
        }
        
        println!();
    }

    // Create output directories
    std::fs::create_dir_all(&args.output_dir)
        .context("Failed to create output directory")?;
    
    let bin_dir = args.output_dir.join("bin");
    std::fs::create_dir_all(&bin_dir)
        .context("Failed to create binary output directory")?;

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

    // Process all images with multi-progress support
    let results = engine.process_batch_with_progress(
        &image_files, 
        &args.output_dir, 
        &bin_dir, 
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
    let total_time = start_time.elapsed();

    println!("{}", style("Results Summary:").bold().green());
    println!("  Successfully processed: {}", style(successful).bold().green());
    if failed > 0 {
        println!("  Failed: {}", style(failed).bold().red());
    }
    println!("  Total processing time: {}", style(format_duration(total_time)).bold());
    println!("  Average time per image: {}", 
        style(format_duration(total_time / image_files.len() as u32)).dim());
    
    println!();
    println!("{}", style("Output files:").bold());
    println!("  BMP files: {}", args.output_dir.display());
    println!("  Binary files: {}", bin_dir.display());

    if failed > 0 {
        println!();
        println!("{}", style("Errors encountered:").bold().red());
        for (i, result) in results.iter().enumerate() {
            if let Err(e) = result {
                println!("  {}: {}", style(format!("Image {}", i + 1)).dim(), e);
            }
        }
    }

    Ok(())
}