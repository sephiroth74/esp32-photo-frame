pub mod annotate;
pub mod auto_optimizer;
pub mod batch;
pub mod binary;
pub mod color_correction;
pub mod combine;
pub mod convert;
pub mod convert_improved;
pub mod dithering;
pub mod optimization_report;
pub mod orientation;
pub mod resize;

// Use ONNX detector when AI feature is enabled
#[cfg(feature = "ai")]
pub mod onnx_detection;
#[cfg(feature = "ai")]
pub use onnx_detection as subject_detection;

#[cfg(not(feature = "ai"))]
pub mod subject_detection_stub;
#[cfg(not(feature = "ai"))]
pub use subject_detection_stub as subject_detection;

use anyhow::{Context, Result};
use image::{Rgb, RgbImage};
use imageproc::drawing::draw_hollow_rect_mut;
use imageproc::rect::Rect;
use indicatif::ProgressBar;
use rayon::prelude::*;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use walkdir::WalkDir;

use crate::cli::DitherMethod;
use crate::utils::{create_readable_combined_filename, has_valid_extension, verbose_println};
use combine::combine_processed_portraits;
use orientation::get_effective_target_dimensions;

#[derive(Debug, Clone, strum_macros::EnumIter)]
pub enum ProcessingType {
    BlackWhite,
    SixColor,
}

impl ProcessingType {
    /// Get the filename prefix for this processing type
    pub fn get_prefix(&self) -> &'static str {
        match self {
            ProcessingType::BlackWhite => "bw",
            ProcessingType::SixColor => "6c",
        }
    }

    /// Parse prefix back to ProcessingType
    #[allow(dead_code)]
    pub fn from_prefix(prefix: &str) -> Option<Self> {
        match prefix {
            "bw" => Some(ProcessingType::BlackWhite),
            "6c" => Some(ProcessingType::SixColor),
            _ => None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ProcessingConfig {
    pub processing_type: ProcessingType,
    pub target_width: u32,
    pub target_height: u32,
    pub auto_orientation: bool,
    pub font_name: String,
    pub font_size: f32,
    pub annotation_background: String,
    pub extensions: Vec<String>,
    pub verbose: bool,
    pub parallel_jobs: usize,
    pub output_formats: Vec<crate::cli::OutputType>,
    // Python people detection configuration
    pub detect_people: bool,
    // Debug mode
    pub debug: bool,
    // Annotation control
    pub annotate: bool,
    // Color correction
    pub auto_color_correct: bool,
    // Dry run mode
    pub dry_run: bool,
    // Confidence threshold for people detection
    pub confidence_threshold: f32,
    // Portrait combination divider settings
    pub divider_width: u32,
    pub divider_color: Rgb<u8>,
    // Dithering method
    pub dithering_method: DitherMethod,
    // Dithering strength (0.0-2.0, multiplier for error diffusion)
    pub dither_strength: f32,
    // Contrast adjustment (-100 to 100, applied before dithering)
    pub contrast: i32,
    // Brightness adjustment (-100 to 100, applied before dithering)
    pub brightness: i32,
    // Saturation boost multiplier (0.5-2.0, 1.0 = no change)
    pub saturation_boost: f32,
    // Auto-optimization
    pub auto_optimize: bool,
    pub optimization_report: bool,
    // Target display orientation
    pub target_orientation: crate::cli::TargetOrientation,
    // Pre-rotation flag (true for 6c portrait mode only)
    pub needs_pre_rotation: bool,
    // JSON progress output (suppresses all other output)
    pub json_progress: bool,
}

pub struct ProcessingEngine {
    config: ProcessingConfig,
    subject_detector: Option<subject_detection::SubjectDetector>,
    optimization_report: Arc<Mutex<optimization_report::OptimizationReport>>,
}

impl ProcessingEngine {
    /// Create output subdirectories for each format and return format-specific paths
    fn create_format_directories(&self, base_output_dir: &Path) -> Result<()> {
        if self.config.dry_run {
            verbose_println(
                self.config.verbose,
                "Dry run mode: Skipping directory creation",
            );
            return Ok(());
        }

        for format in &self.config.output_formats {
            let format_dir = match format {
                crate::cli::OutputType::Bmp => base_output_dir.join("bmp"),
                crate::cli::OutputType::Bin => base_output_dir.join("bin"),
                crate::cli::OutputType::Jpg => base_output_dir.join("jpg"),
                crate::cli::OutputType::Png => base_output_dir.join("png"),
            };

            std::fs::create_dir_all(&format_dir).with_context(|| {
                format!(
                    "Failed to create format directory: {}",
                    format_dir.display()
                )
            })?;

            verbose_println(
                self.config.verbose,
                &format!("Created output directory: {}", format_dir.display()),
            );
        }
        Ok(())
    }

    /// Get the output path for a specific format
    fn get_format_output_path(
        &self,
        base_output_dir: &Path,
        input_path: &Path,
        format: &crate::cli::OutputType,
        suffix: Option<&str>,
    ) -> PathBuf {
        let format_dir = match format {
            crate::cli::OutputType::Bmp => base_output_dir.join("bmp"),
            crate::cli::OutputType::Bin => base_output_dir.join("bin"),
            crate::cli::OutputType::Jpg => base_output_dir.join("jpg"),
            crate::cli::OutputType::Png => base_output_dir.join("png"),
        };

        let extension = match format {
            crate::cli::OutputType::Bmp => "bmp",
            crate::cli::OutputType::Bin => "bin",
            crate::cli::OutputType::Jpg => "jpg",
            crate::cli::OutputType::Png => "png",
        };

        // Use readable filename generation
        let filename = match crate::utils::create_readable_filename(
            input_path,
            self.config.processing_type.get_prefix(),
            extension,
        ) {
            Ok(name) => {
                // If suffix is provided, insert it before the extension
                if let Some(suffix) = suffix {
                    if let Some(dot_pos) = name.rfind('.') {
                        let mut result = name[..dot_pos].to_string();
                        result.push_str("__");
                        result.push_str(suffix);
                        result.push_str(&name[dot_pos..]);
                        result
                    } else {
                        name
                    }
                } else {
                    name
                }
            }
            Err(e) => {
                // If we can't generate a readable filename, use a fallback
                let fallback_name = format!(
                    "{}_unknown_{:08x}.{}",
                    self.config.processing_type.get_prefix(),
                    rand::random::<u32>(),
                    extension
                );
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Failed to generate readable filename: {}, using fallback: {}",
                        e, fallback_name
                    ),
                );
                fallback_name
            }
        };
        format_dir.join(filename)
    }

    /// Get the combined portrait output path for a specific format
    fn get_combined_format_output_path(
        &self,
        base_output_dir: &Path,
        left_path: &Path,
        right_path: &Path,
        format: &crate::cli::OutputType,
    ) -> PathBuf {
        let format_dir = match format {
            crate::cli::OutputType::Bmp => base_output_dir.join("bmp"),
            crate::cli::OutputType::Bin => base_output_dir.join("bin"),
            crate::cli::OutputType::Jpg => base_output_dir.join("jpg"),
            crate::cli::OutputType::Png => base_output_dir.join("png"),
        };

        let extension = match format {
            crate::cli::OutputType::Bmp => "bmp",
            crate::cli::OutputType::Bin => "bin",
            crate::cli::OutputType::Jpg => "jpg",
            crate::cli::OutputType::Png => "png",
        };

        // Use readable filename generation for combined portraits
        let filename = match create_readable_combined_filename(
            left_path,
            right_path,
            self.config.processing_type.get_prefix(),
            extension,
        ) {
            Ok(name) => name,
            Err(e) => {
                // If we can't generate a readable filename, use a fallback
                let fallback_name = format!(
                    "combined_{}_{:08x}.{}",
                    self.config.processing_type.get_prefix(),
                    rand::random::<u32>(),
                    extension
                );
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Failed to generate readable combined filename: {}, using fallback: {}",
                        e, fallback_name
                    ),
                );
                fallback_name
            }
        };
        format_dir.join(filename)
    }

    pub fn new(config: ProcessingConfig) -> Result<Self> {
        // Initialize thread pool with specified number of jobs
        // Note: build_global() can only be called once per process
        // If it fails, the global pool is already initialized, which is fine
        let _ = rayon::ThreadPoolBuilder::new()
            .num_threads(config.parallel_jobs)
            .build_global();

        let subject_detector = if config.detect_people {
            #[cfg(feature = "ai")]
            {
                // Use native ONNX detector when AI feature is enabled
                verbose_println(
                    config.verbose,
                    "Initializing native ONNX people detection...",
                );
                verbose_println(
                    config.verbose,
                    "Using embedded YOLO11 model (no external files required)",
                );

                match subject_detection::create_default_detector(config.verbose) {
                    Ok(detector) => {
                        verbose_println(
                            config.verbose,
                            "✓ ONNX people detection initialized successfully",
                        );
                        Some(detector)
                    }
                    Err(e) => {
                        verbose_println(
                            config.verbose,
                            &format!("⚠ Failed to initialize ONNX detection: {}", e),
                        );
                        verbose_println(config.verbose, "  Continuing without people detection");
                        None
                    }
                }
            }

            #[cfg(not(feature = "ai"))]
            {
                // When AI feature is not enabled, people detection is not available
                verbose_println(
                    config.verbose,
                    "⚠ People detection requested but not available",
                );
                verbose_println(
                    config.verbose,
                    "  Rebuild with --features ai to enable native ONNX people detection",
                );
                None
            }
        } else {
            None
        };

        Ok(Self {
            config,
            subject_detector,
            optimization_report: Arc::new(Mutex::new(
                optimization_report::OptimizationReport::new(),
            )),
        })
    }

    /// Get the optimization report (for printing at the end)
    pub fn get_optimization_report(&self) -> Arc<Mutex<optimization_report::OptimizationReport>> {
        Arc::clone(&self.optimization_report)
    }

    /// Discover images in the specified paths
    pub fn discover_images(&self, input_paths: &[PathBuf]) -> Result<Vec<PathBuf>> {
        let mut image_files = Vec::new();

        for input_path in input_paths {
            if input_path.is_file() {
                // Handle single file
                if has_valid_extension(input_path, &self.config.extensions) {
                    verbose_println(
                        self.config.verbose,
                        &format!("Adding file: {}", input_path.display()),
                    );
                    image_files.push(input_path.clone());
                } else {
                    println!(
                        "Warning: File {} has unsupported extension",
                        input_path.display()
                    );
                }
            } else if input_path.is_dir() {
                // Handle directory - recursively scan all subdirectories
                verbose_println(
                    self.config.verbose,
                    &format!("Scanning directory recursively: {}", input_path.display()),
                );

                // No depth limit - traverse all subdirectories
                let walker = WalkDir::new(input_path)
                    .follow_links(false)
                    .sort_by_file_name(); // Sort entries for consistent order

                let mut dir_count = 0;
                let mut current_dir = input_path.clone();

                for entry in walker {
                    let entry = entry.context("Failed to read directory entry")?;
                    let path = entry.path();

                    // Track directory changes for verbose output
                    if let Some(parent) = path.parent() {
                        if parent != current_dir {
                            current_dir = parent.to_path_buf();
                            dir_count += 1;
                            verbose_println(
                                self.config.verbose,
                                &format!("  Scanning subdirectory: {}", parent.display()),
                            );
                        }
                    }

                    if path.is_file() && has_valid_extension(path, &self.config.extensions) {
                        verbose_println(
                            self.config.verbose,
                            &format!(
                                "    Found image: {}",
                                path.file_name().unwrap_or_default().to_string_lossy()
                            ),
                        );
                        image_files.push(path.to_path_buf());
                    }
                }

                if dir_count > 0 {
                    verbose_println(
                        self.config.verbose,
                        &format!("  Scanned {} subdirectories", dir_count),
                    );
                }
            } else {
                return Err(anyhow::anyhow!(
                    "Input path does not exist: {}",
                    input_path.display()
                ));
            }
        }

        // Sort for consistent processing order
        image_files.sort();

        verbose_println(
            self.config.verbose,
            &format!("Found {} image files", image_files.len()),
        );
        Ok(image_files)
    }

    /// Process a batch of images with smart portrait pairing and multi-progress support
    pub fn process_batch_with_smart_pairing(
        &self,
        image_files: &[PathBuf],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        completion_progress.set_message("Creating output directories...");

        // Create subdirectories for each output format
        self.create_format_directories(output_dir)?;

        // Process all images without duplicate checking
        let images_to_process = image_files.to_vec();

        completion_progress.set_message("Analyzing image orientations...");

        // Separate portrait and landscape images
        let mut portrait_files = Vec::new();
        let mut landscape_files = Vec::new();

        for (_i, path) in images_to_process.iter().enumerate() {
            // Analysis phase - don't update main progress during analysis as it's tracking actual image processing

            // Use fast orientation detection (no full image loading)
            match orientation::detect_orientation_fast(path) {
                Ok(orientation_info) => {
                    if orientation_info.is_portrait {
                        portrait_files.push(path.clone());
                    } else {
                        landscape_files.push(path.clone());
                    }
                }
                Err(e) => {
                    verbose_println(
                        self.config.verbose,
                        &format!(
                            "⚠ Failed to detect orientation for {}: {}",
                            path.display(),
                            e
                        ),
                    );
                    // Default to landscape processing for problematic files
                    landscape_files.push(path.clone());
                }
            }
        }

        verbose_println(
            self.config.verbose,
            &format!(
                "Separated {} portrait images and {} landscape images",
                portrait_files.len(),
                landscape_files.len()
            ),
        );

        // Calculate actual number of output images after pairing
        let actual_output_count = match self.config.target_orientation {
            crate::cli::TargetOrientation::Landscape => {
                // Landscape target: landscapes processed individually + portraits paired (2→1)
                landscape_files.len() + (portrait_files.len() / 2)
            }
            crate::cli::TargetOrientation::Portrait => {
                // Portrait target: portraits processed individually + landscapes paired (2→1)
                portrait_files.len() + (landscape_files.len() / 2)
            }
        };

        // Calculate images that will be skipped (unpaired)
        let skipped_count = match self.config.target_orientation {
            crate::cli::TargetOrientation::Landscape => portrait_files.len() % 2,
            crate::cli::TargetOrientation::Portrait => landscape_files.len() % 2,
        };

        // Progress bar should track total images being processed, not output count
        // This includes all images, even those that will be combined
        let total_images_to_process = images_to_process.len();
        main_progress.set_length(total_images_to_process as u64);

        verbose_println(
            self.config.verbose,
            &format!(
                "Will produce {} output images ({} input images after pairing)",
                actual_output_count,
                images_to_process.len()
            ),
        );

        // Warn about skipped images
        if skipped_count > 0 {
            let skipped_type = match self.config.target_orientation {
                crate::cli::TargetOrientation::Landscape => "portrait",
                crate::cli::TargetOrientation::Portrait => "landscape",
            };
            println!(
                "⚠️  {} {} image{} will be skipped (unpaired)",
                skipped_count,
                skipped_type,
                if skipped_count > 1 { "s" } else { "" }
            );
        }

        let mut all_results = Vec::new();

        // Pairing behavior depends on target orientation
        match self.config.target_orientation {
            crate::cli::TargetOrientation::Landscape => {
                // LANDSCAPE TARGET: Process landscapes individually, pair portraits side-by-side
                verbose_println(
                    self.config.verbose,
                    "Target orientation: Landscape (pairing portraits side-by-side)",
                );

                // Process landscape images individually (each increments main progress by 1)
                if !landscape_files.is_empty() {
                    completion_progress.set_message("Processing landscape images...");
                    let landscape_results = self.process_batch_with_progress(
                        &landscape_files,
                        output_dir,
                        main_progress,
                        thread_progress_bars,
                        completion_progress,
                    )?;
                    all_results.extend(landscape_results);
                }

                // Process portrait images in pairs (side-by-side)
                if !portrait_files.is_empty() {
                    completion_progress.set_message("Processing portrait pairs (side-by-side)...");
                    let portrait_results = self.process_portrait_pairs_with_progress(
                        &portrait_files,
                        output_dir,
                        main_progress,
                        thread_progress_bars,
                        completion_progress,
                    )?;
                    all_results.extend(portrait_results);
                }
            }
            crate::cli::TargetOrientation::Portrait => {
                // PORTRAIT TARGET: Process portraits individually, pair landscapes top-bottom
                verbose_println(
                    self.config.verbose,
                    "Target orientation: Portrait (pairing landscapes top-bottom)",
                );

                // Process portrait images individually (each increments main progress by 1)
                if !portrait_files.is_empty() {
                    completion_progress.set_message("Processing portrait images...");
                    let portrait_results = self.process_batch_with_progress(
                        &portrait_files,
                        output_dir,
                        main_progress,
                        thread_progress_bars,
                        completion_progress,
                    )?;
                    all_results.extend(portrait_results);
                }

                // Process landscape images in pairs (top-bottom)
                if !landscape_files.is_empty() {
                    completion_progress.set_message("Processing landscape pairs (top-bottom)...");
                    let landscape_results = self.process_landscape_pairs_with_progress(
                        &landscape_files,
                        output_dir,
                        main_progress,
                        thread_progress_bars,
                        completion_progress,
                    )?;
                    all_results.extend(landscape_results);
                }
            }
        }

        // Main progress should already be at 100% from incremental updates
        completion_progress.set_message("Batch processing complete");

        Ok(all_results)
    }

    /// Process a batch of images with multi-progress support (original method for landscapes)
    pub fn process_batch_with_progress(
        &self,
        image_files: &[PathBuf],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        use std::collections::HashMap;
        use std::sync::atomic::{AtomicUsize, Ordering};

        let processed_count = AtomicUsize::new(0);
        let thread_assignment = Arc::new(Mutex::new(HashMap::new()));
        let next_thread_id = AtomicUsize::new(0);

        completion_progress.set_message("Processing images in parallel...");

        let results: Vec<Result<ProcessingResult>> = image_files
            .par_iter()
            .enumerate()
            .map(|(index, image_path)| {
                // Get or assign thread progress bar
                let current_thread_id = rayon::current_thread_index().unwrap_or(0);
                let pb_index = {
                    let mut assignment = thread_assignment.lock().unwrap();
                    if let Some(&pb_index) = assignment.get(&current_thread_id) {
                        pb_index
                    } else {
                        let pb_index = next_thread_id.fetch_add(1, Ordering::Relaxed)
                            % thread_progress_bars.len();
                        assignment.insert(current_thread_id, pb_index);
                        pb_index
                    }
                };

                let thread_pb = &thread_progress_bars[pb_index];

                // Update thread progress
                if let Some(filename) = image_path.file_name().and_then(|f| f.to_str()) {
                    thread_pb.set_message(format!("Processing {}", filename));
                }

                // Process the image
                let result = self
                    .process_single_image_with_progress(image_path, output_dir, index, thread_pb);

                // Update main progress
                let count = processed_count.fetch_add(1, Ordering::Relaxed) + 1;
                main_progress.inc(1);
                main_progress.set_message(format!("Completed: {}/{}", count, image_files.len()));

                // Update completion progress
                let progress_percent = (count as f64 / image_files.len() as f64) * 100.0;
                completion_progress.set_position(progress_percent as u64);
                completion_progress.set_message(format!(
                    "Processing: {}/{}",
                    count,
                    image_files.len()
                ));

                // Mark thread as idle
                thread_pb.set_message("Idle");

                result
            })
            .collect();

        completion_progress.set_message("Finalizing results...");
        Ok(results)
    }

    /// Process a single image file with detailed progress tracking
    fn process_single_image_with_progress(
        &self,
        input_path: &Path,
        output_dir: &Path,
        index: usize,
        progress_bar: &ProgressBar,
    ) -> Result<ProcessingResult> {
        let start_time = std::time::Instant::now();
        let filename = input_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        verbose_println(
            self.config.verbose,
            &format!("Processing: {}", input_path.display()),
        );

        // Stage 1: Load image (10-15%)
        progress_bar.set_position(10);
        progress_bar.set_message(format!("{} - Loading", filename));
        std::thread::yield_now();
        progress_bar.set_position(12);
        let img = image::open(input_path)
            .with_context(|| format!("Failed to open image: {}", input_path.display()))?;
        progress_bar.set_position(15);

        // Stage 2: Convert to RGB (15-20%)
        progress_bar.set_position(15);
        progress_bar.set_message(format!("{} - Converting to RGB", filename));
        std::thread::yield_now();
        progress_bar.set_position(17);
        let rgb_img = img.to_rgb8();
        progress_bar.set_position(20);

        // Stage 3: Detect orientation (20-30%)
        progress_bar.set_position(20);
        progress_bar.set_message(format!("{} - Detecting orientation", filename));
        std::thread::yield_now();
        progress_bar.set_position(25);
        let orientation_info = orientation::detect_orientation(input_path, &rgb_img)?;
        progress_bar.set_position(30);

        verbose_println(
            self.config.verbose,
            &format!(
                "Image orientation: {:?}, is_portrait: {}",
                orientation_info.exif_orientation, orientation_info.is_portrait
            ),
        );

        // Stage 4: Apply rotation (30-40%)
        progress_bar.set_position(30);
        progress_bar.set_message(format!("{} - Applying rotation", filename));
        std::thread::yield_now();
        progress_bar.set_position(35);
        let rotated_img = orientation::apply_rotation(&rgb_img, orientation_info.exif_orientation)?;
        progress_bar.set_position(40);

        // Stage 5: People detection (40-50%)
        progress_bar.set_position(40);
        progress_bar.set_message(format!("{} - Detecting people", filename));
        std::thread::yield_now();
        progress_bar.set_position(45);
        let people_detection = self.detect_people_with_logging(&rotated_img, input_path);
        progress_bar.set_position(50);

        // Stage 6: Process based on orientation (50-100%)
        progress_bar.set_position(50);
        let is_portrait = orientation_info.is_portrait;
        let processing_result = if is_portrait {
            progress_bar.set_message(format!("{} - Processing portrait", filename));
            self.process_portrait_image_with_progress(
                &rotated_img,
                input_path,
                output_dir,
                index,
                progress_bar,
                people_detection,
                start_time,
                orientation_info.exif_orientation,
            )?
        } else {
            progress_bar.set_message(format!("{} - Processing landscape", filename));
            self.process_landscape_image_with_progress(
                &rotated_img,
                input_path,
                output_dir,
                index,
                progress_bar,
                people_detection,
                start_time,
                orientation_info.exif_orientation,
            )?
        };

        progress_bar.set_position(100);
        progress_bar.set_message(format!("{} - Complete", filename));
        Ok(processing_result)
    }

    /// Process a landscape image (full size) with detailed progress tracking
    fn process_landscape_image_with_progress(
        &self,
        img: &RgbImage,
        input_path: &Path,
        output_dir: &Path,
        _index: usize,
        progress_bar: &ProgressBar,
        people_detection: Option<subject_detection::SubjectDetectionResult>,
        start_time: std::time::Instant,
        orientation: orientation::ExifOrientation,
    ) -> Result<ProcessingResult> {
        let filename = input_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        // Resize to target dimensions (60-70%)
        progress_bar.set_position(60);
        if people_detection.is_some() && people_detection.as_ref().unwrap().person_count > 0 {
            progress_bar.set_message(format!("{} - Smart resizing (people-aware)", filename));
            verbose_println(
                self.config.verbose,
                "🎯 Applying people-aware smart cropping",
            );
        } else {
            progress_bar.set_message(format!("{} - Resizing", filename));
        }

        std::thread::yield_now();
        progress_bar.set_position(62); // Show progress during resize
        let resized_img = resize::smart_resize_with_people_detection(
            img,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_orientation,
            people_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(70); // Resize complete

        // Add text annotation (70-75%) - conditional based on config
        progress_bar.set_position(70);
        let annotated_img = if self.config.annotate {
            progress_bar.set_message(format!("{} - Adding annotation", filename));
            std::thread::yield_now();
            progress_bar.set_position(72);
            let result = annotate::add_filename_annotation(
                &resized_img,
                input_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?;
            progress_bar.set_position(75);
            result
        } else {
            progress_bar.set_message(format!("{} - Skipping annotation", filename));
            progress_bar.set_position(75);
            resized_img
        };

        // Auto-optimization: analyze image and determine optimal parameters (75%)
        let (
            optimized_dither_method,
            optimized_dither_strength,
            optimized_contrast,
            optimized_brightness,
            optimized_auto_color,
            image_analysis,
        ) = if self.config.auto_optimize {
            progress_bar.set_message(format!("{} - Analyzing image", filename));
            let optimization_result =
                auto_optimizer::analyze_and_optimize(&annotated_img, people_detection.as_ref())?;
            let analysis = auto_optimizer::analyze_image_characteristics(
                &annotated_img,
                people_detection.as_ref(),
            )?;

            (
                optimization_result.dither_method,
                optimization_result.dither_strength,
                (optimization_result.contrast_adjustment * 100.0) as i32, // Convert from -1.0..1.0 to -100..100
                self.config.brightness, // Use config brightness (not auto-optimized yet)
                optimization_result.auto_color_correct,
                Some(analysis),
            )
        } else {
            // Use config defaults when auto-optimization is disabled
            (
                self.config.dithering_method.clone(),
                self.config.dither_strength,
                self.config.contrast,
                self.config.brightness,
                self.config.auto_color_correct,
                None,
            )
        };

        // Apply automatic color correction if enabled (75-76%)
        progress_bar.set_position(75);
        let color_corrected_img = if optimized_auto_color {
            progress_bar.set_message(format!("{} - Auto color correction", filename));
            std::thread::yield_now();
            let corrected = color_correction::apply_auto_color_correction_with_saturation(
                &annotated_img,
                self.config.saturation_boost,
            )?;
            progress_bar.set_position(76);
            corrected
        } else {
            progress_bar.set_position(76);
            annotated_img
        };

        // Apply brightness and contrast adjustments (76-78%) - combined for efficiency
        progress_bar.set_position(76);
        let brightness_contrast_adjusted_img =
            if optimized_brightness != 0 || optimized_contrast != 0 {
                eprintln!(
                    "[Pipeline] Applying brightness={} contrast={}",
                    optimized_brightness, optimized_contrast
                );

                if optimized_brightness != 0 && optimized_contrast != 0 {
                    progress_bar
                        .set_message(format!("{} - Adjusting brightness and contrast", filename));
                } else if optimized_brightness != 0 {
                    progress_bar.set_message(format!("{} - Adjusting brightness", filename));
                } else {
                    progress_bar.set_message(format!("{} - Adjusting contrast", filename));
                }
                std::thread::yield_now();

                // Apply both together using ImageMagick when available
                let adjusted = if color_correction::is_imagemagick_available() {
                    eprintln!("[Pipeline] Using ImageMagick for brightness+contrast");
                    color_correction::apply_imagemagick_brightness_contrast_public(
                        &color_corrected_img,
                        optimized_brightness,
                        optimized_contrast,
                    )?
                } else {
                    // Fall back to separate operations
                    eprintln!(
                        "[Pipeline] ImageMagick not available, using fallback Rust implementation"
                    );
                    let temp = color_correction::apply_brightness_adjustment(
                        &color_corrected_img,
                        optimized_brightness,
                    )?;
                    color_correction::apply_contrast_adjustment(&temp, optimized_contrast)?
                };

                progress_bar.set_position(78);
                adjusted
            } else {
                eprintln!("[Pipeline] Skipping brightness and contrast (both are 0)");
                progress_bar.set_position(78);
                color_corrected_img
            };

        // Apply color conversion and dithering (78-85%) - use optimized parameters
        progress_bar.set_message(format!("{} - Color processing", filename));
        std::thread::yield_now();
        progress_bar.set_position(80); // Show progress during color processing
        let processed_img = convert::process_image(
            &brightness_contrast_adjusted_img,
            &self.config.processing_type,
            &optimized_dither_method,
            optimized_dither_strength,
        )?;
        progress_bar.set_position(85); // Color processing complete

        // Apply 90° CW rotation if needed (85-88%)
        // Pre-rotation is only needed for 6c portrait mode (writeDemoBitmap doesn't support rotation)
        // B/W displays use GxEPD2 setRotation() so no pre-rotation needed
        let final_img = if self.config.needs_pre_rotation {
            progress_bar.set_message(format!("{} - Pre-rotating for portrait display", filename));
            progress_bar.set_position(86);
            verbose_println(
                self.config.verbose,
                "🔄 Pre-rotating 90° CW for 6c portrait display (writeDemoBitmap requires pre-rotated data)",
            );
            let rotated = image::imageops::rotate90(&processed_img);
            progress_bar.set_position(88);
            rotated
        } else {
            progress_bar.set_position(88);
            processed_img
        };

        // Save files to multiple formats (90-95%)
        progress_bar.set_position(90);

        let mut saved_paths = std::collections::HashMap::new();
        let progress_per_format = 5.0 / self.config.output_formats.len() as f64;
        let mut current_progress = 90.0;

        for (_, format) in self.config.output_formats.iter().enumerate() {
            let format_name = match format {
                crate::cli::OutputType::Bmp => "BMP",
                crate::cli::OutputType::Bin => "Binary",
                crate::cli::OutputType::Jpg => "JPG",
                crate::cli::OutputType::Png => "PNG",
            };

            progress_bar.set_message(format!("{} - Saving {}", filename, format_name));

            let output_path = self.get_format_output_path(output_dir, input_path, format, None);

            if self.config.dry_run {
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Dry run: Would save {} '{}' to {}",
                        format_name,
                        filename,
                        output_path.display()
                    ),
                );
            } else {
                match format {
                    crate::cli::OutputType::Bmp
                    | crate::cli::OutputType::Jpg
                    | crate::cli::OutputType::Png => {
                        final_img.save(&output_path).with_context(|| {
                            format!("Failed to save {}: {}", format_name, output_path.display())
                        })?;
                    }
                    crate::cli::OutputType::Bin => {
                        // Use appropriate binary format based on display type
                        let binary_data = match self.config.processing_type {
                            ProcessingType::BlackWhite => {
                                // B/W displays use the original ESP32 compressed format (RRRGGGBB)
                                binary::convert_to_esp32_binary(&final_img)?
                            }
                            ProcessingType::SixColor => {
                                // 6c displays use demo bitmap mode 1 format for drawDemoBitmap()
                                binary::convert_to_demo_bitmap_mode1(&final_img)?
                            }
                        };
                        std::fs::write(&output_path, binary_data).with_context(|| {
                            format!("Failed to save binary: {}", output_path.display())
                        })?;
                    }
                }
            }

            saved_paths.insert(format.clone(), output_path);

            current_progress += progress_per_format;
            progress_bar.set_position(current_progress as u64);
        }

        progress_bar.set_position(95);

        // Collect processing report data if enabled
        if self.config.optimization_report {
            let was_rotated = !matches!(
                orientation,
                orientation::ExifOrientation::TopLeft | orientation::ExifOrientation::Undefined
            );
            let color_skipped = !optimized_auto_color && self.config.auto_color_correct;

            let entry = optimization_report::OptimizationEntry {
                input_filename: optimization_report::extract_filename(input_path),
                output_filename: saved_paths
                    .get(&self.config.output_formats[0])
                    .and_then(|p| p.file_name())
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown")
                    .to_string(),
                dither_method: optimized_dither_method,
                dither_strength: optimized_dither_strength,
                contrast: optimized_contrast as f32,
                auto_color: optimized_auto_color,
                people_detected: people_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
                is_pastel: image_analysis.as_ref().map_or(false, |a| a.is_pastel_toned),
                was_rotated,
                color_skipped,
            };

            self.optimization_report
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .add_landscape(entry);
        }

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            output_paths: saved_paths,
            image_type: ImageType::Landscape,
            processing_time: start_time.elapsed(),
            people_detected: people_detection
                .as_ref()
                .map_or(false, |d| d.person_count > 0),
            people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
            individual_portraits: None,
        })
    }

    /// Process a portrait image (individual portrait as half-width landscape)
    fn process_portrait_image_with_progress(
        &self,
        img: &RgbImage,
        input_path: &Path,
        output_dir: &Path,
        _index: usize,
        progress_bar: &ProgressBar,
        people_detection: Option<subject_detection::SubjectDetectionResult>,
        start_time: std::time::Instant,
        orientation: orientation::ExifOrientation,
    ) -> Result<ProcessingResult> {
        let filename = input_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        // Process portrait as individual half-width image for now
        // TODO: Implement proper portrait pairing and combination logic
        progress_bar.set_position(60);
        if people_detection.is_some() && people_detection.as_ref().unwrap().person_count > 0 {
            progress_bar.set_message(format!("{} - Portrait resizing (people-aware)", filename));
            verbose_println(
                self.config.verbose,
                "🎯 Applying people-aware portrait cropping",
            );
        } else {
            progress_bar.set_message(format!("{} - Portrait resizing", filename));
        }

        // Portrait resize dimensions depend on target orientation:
        // - Landscape target: half-width (will be paired side-by-side)
        // - Portrait target: full-width (individual display)
        let portrait_width = match self.config.target_orientation {
            crate::cli::TargetOrientation::Landscape => self.config.target_width / 2,
            crate::cli::TargetOrientation::Portrait => self.config.target_width,
        };
        std::thread::yield_now();

        progress_bar.set_position(62); // Show progress during resize
        let resized_img = resize::smart_resize_with_people_detection(
            img,
            portrait_width,
            self.config.target_height,
            self.config.auto_orientation,
            people_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(70); // Resize complete

        // Add text annotation (70%) - conditional based on config
        progress_bar.set_position(70);
        let annotated_img = if self.config.annotate {
            progress_bar.set_message(format!("{} - Adding annotation", filename));
            annotate::add_filename_annotation(
                &resized_img,
                input_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?
        } else {
            progress_bar.set_message(format!("{} - Skipping annotation", filename));
            resized_img
        };

        // Auto-optimization: analyze image and determine optimal parameters (portrait)
        let (
            optimized_dither_method,
            optimized_dither_strength,
            optimized_contrast,
            optimized_brightness,
            optimized_auto_color,
            image_analysis,
        ) = if self.config.auto_optimize {
            progress_bar.set_message(format!("{} - Analyzing portrait", filename));
            let optimization_result =
                auto_optimizer::analyze_and_optimize(&annotated_img, people_detection.as_ref())?;
            let analysis = auto_optimizer::analyze_image_characteristics(
                &annotated_img,
                people_detection.as_ref(),
            )?;

            (
                optimization_result.dither_method,
                optimization_result.dither_strength,
                (optimization_result.contrast_adjustment * 100.0) as i32, // Convert from -1.0..1.0 to -100..100
                self.config.brightness, // Use config brightness (not auto-optimized yet)
                optimization_result.auto_color_correct,
                Some(analysis),
            )
        } else {
            // Use config defaults when auto-optimization is disabled
            (
                self.config.dithering_method.clone(),
                self.config.dither_strength,
                self.config.contrast,
                self.config.brightness,
                self.config.auto_color_correct,
                None,
            )
        };

        // Apply automatic color correction if enabled (80-81%)
        progress_bar.set_position(80);
        let color_corrected_img = if optimized_auto_color {
            progress_bar.set_message(format!("{} - Auto color correction", filename));
            std::thread::yield_now();
            let corrected = color_correction::apply_auto_color_correction_with_saturation(
                &annotated_img,
                self.config.saturation_boost,
            )?;
            progress_bar.set_position(81);
            corrected
        } else {
            progress_bar.set_position(81);
            annotated_img
        };

        // Apply brightness and contrast adjustments (81-82%) - combined for efficiency
        progress_bar.set_position(81);
        let brightness_contrast_adjusted_img =
            if optimized_brightness != 0 || optimized_contrast != 0 {
                eprintln!(
                    "[Pipeline] Applying brightness={} contrast={} to portrait",
                    optimized_brightness, optimized_contrast
                );

                if optimized_brightness != 0 && optimized_contrast != 0 {
                    progress_bar
                        .set_message(format!("{} - Adjusting brightness and contrast", filename));
                } else if optimized_brightness != 0 {
                    progress_bar.set_message(format!("{} - Adjusting brightness", filename));
                } else {
                    progress_bar.set_message(format!("{} - Adjusting contrast", filename));
                }
                std::thread::yield_now();

                // Apply both together using ImageMagick when available
                let adjusted = if color_correction::is_imagemagick_available() {
                    eprintln!("[Pipeline] Using ImageMagick for brightness+contrast");
                    color_correction::apply_imagemagick_brightness_contrast_public(
                        &color_corrected_img,
                        optimized_brightness,
                        optimized_contrast,
                    )?
                } else {
                    // Fall back to separate operations
                    eprintln!(
                        "[Pipeline] ImageMagick not available, using fallback Rust implementation"
                    );
                    let temp = color_correction::apply_brightness_adjustment(
                        &color_corrected_img,
                        optimized_brightness,
                    )?;
                    color_correction::apply_contrast_adjustment(&temp, optimized_contrast)?
                };

                progress_bar.set_position(82);
                adjusted
            } else {
                eprintln!("[Pipeline] Skipping brightness and contrast (both are 0)");
                progress_bar.set_position(82);
                color_corrected_img
            };

        // Apply color conversion and dithering (82-85%) - use optimized parameters
        progress_bar.set_message(format!("{} - Color processing", filename));
        std::thread::yield_now();
        let processed_img = convert::process_image(
            &brightness_contrast_adjusted_img,
            &self.config.processing_type,
            &optimized_dither_method,
            optimized_dither_strength,
        )?;
        progress_bar.set_position(85);

        // Apply 90° CW rotation if needed (85-88%)
        // Pre-rotation is only needed for 6c portrait mode
        let final_img = if self.config.needs_pre_rotation {
            progress_bar.set_message(format!("{} - Pre-rotating for portrait display", filename));
            progress_bar.set_position(86);
            verbose_println(
                self.config.verbose,
                "🔄 Pre-rotating 90° CW for 6c portrait display (writeDemoBitmap requires pre-rotated data)",
            );
            let rotated = image::imageops::rotate90(&processed_img);
            progress_bar.set_position(88);
            rotated
        } else {
            progress_bar.set_position(88);
            processed_img
        };

        // Save portrait files to multiple formats (90-95%)
        progress_bar.set_position(90);

        let mut saved_paths = std::collections::HashMap::new();
        let progress_per_format = 5.0 / self.config.output_formats.len() as f64;
        let mut current_progress = 90.0;

        for format in &self.config.output_formats {
            let format_name = match format {
                crate::cli::OutputType::Bmp => "portrait BMP",
                crate::cli::OutputType::Bin => "portrait Binary",
                crate::cli::OutputType::Jpg => "portrait JPG",
                crate::cli::OutputType::Png => "portrait PNG",
            };

            progress_bar.set_message(format!("{} - Saving {}", filename, format_name));

            let output_path =
                self.get_format_output_path(output_dir, input_path, format, Some("portrait"));

            if self.config.dry_run {
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Dry run: Would save {} '{}' to {}",
                        format_name,
                        filename,
                        output_path.display()
                    ),
                );
            } else {
                match format {
                    crate::cli::OutputType::Bmp
                    | crate::cli::OutputType::Jpg
                    | crate::cli::OutputType::Png => {
                        final_img.save(&output_path).with_context(|| {
                            format!("Failed to save {}: {}", format_name, output_path.display())
                        })?;
                    }
                    crate::cli::OutputType::Bin => {
                        // Use appropriate binary format based on display type
                        let binary_data = match self.config.processing_type {
                            ProcessingType::BlackWhite => {
                                // B/W displays use the original ESP32 compressed format (RRRGGGBB)
                                binary::convert_to_esp32_binary(&final_img)?
                            }
                            ProcessingType::SixColor => {
                                // 6c displays use demo bitmap mode 1 format for drawDemoBitmap()
                                binary::convert_to_demo_bitmap_mode1(&final_img)?
                            }
                        };
                        std::fs::write(&output_path, binary_data).with_context(|| {
                            format!("Failed to save portrait binary: {}", output_path.display())
                        })?;
                    }
                }
            }

            saved_paths.insert(format.clone(), output_path);

            current_progress += progress_per_format;
            progress_bar.set_position(current_progress as u64);
        }

        progress_bar.set_position(95);

        // Collect processing report data if enabled
        if self.config.optimization_report {
            let was_rotated = !matches!(
                orientation,
                orientation::ExifOrientation::TopLeft | orientation::ExifOrientation::Undefined
            );
            let color_skipped = !optimized_auto_color && self.config.auto_color_correct;

            let entry = optimization_report::OptimizationEntry {
                input_filename: optimization_report::extract_filename(input_path),
                output_filename: saved_paths
                    .get(&self.config.output_formats[0])
                    .and_then(|p| p.file_name())
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown")
                    .to_string(),
                dither_method: optimized_dither_method,
                dither_strength: optimized_dither_strength,
                contrast: optimized_contrast as f32,
                auto_color: optimized_auto_color,
                people_detected: people_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
                is_pastel: image_analysis.as_ref().map_or(false, |a| a.is_pastel_toned),
                was_rotated,
                color_skipped,
            };

            self.optimization_report
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .add_portrait(entry);
        }

        verbose_println(self.config.verbose, &"ℹ️ Portrait processed as individual half-width image. Future enhancement will combine portraits in pairs.".to_string());

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            output_paths: saved_paths,
            image_type: ImageType::Portrait,
            processing_time: start_time.elapsed(),
            people_detected: people_detection
                .as_ref()
                .map_or(false, |d| d.person_count > 0),
            people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
            individual_portraits: None,
        })
    }

    /// Detect people in an image and return detection result with verbose logging
    fn detect_people_with_logging(
        &self,
        img: &RgbImage,
        input_path: &Path,
    ) -> Option<subject_detection::SubjectDetectionResult> {
        if let Some(ref detector) = self.subject_detector {
            verbose_println(
                self.config.verbose,
                &format!(
                    "🔍 Running people detection on: {}",
                    input_path.file_name().unwrap_or_default().to_string_lossy()
                ),
            );

            match detector.detect_people(img, self.config.confidence_threshold) {
                Ok(result) => {
                    verbose_println(self.config.verbose, "  ✅ Python detection completed");

                    if self.config.verbose {
                        println!("📊 People Detection Results:");
                        println!("   • People found: {}", result.person_count);

                        if result.person_count > 0 {
                            println!("   • Highest confidence: {:.2}%", result.confidence * 100.0);
                            println!(
                                "   • Detection center: ({}, {})",
                                result.center.0, result.center.1
                            );
                            println!(
                                "   • Offset from image center: ({:+}, {:+})",
                                result.offset_from_center.0, result.offset_from_center.1
                            );

                            // Calculate offset percentage for better understanding
                            let (img_width, img_height) = img.dimensions();
                            let offset_x_pct =
                                (result.offset_from_center.0 as f32 / img_width as f32) * 100.0;
                            let offset_y_pct =
                                (result.offset_from_center.1 as f32 / img_height as f32) * 100.0;
                            println!(
                                "   • Offset percentage: ({:+.1}%, {:+.1}%)",
                                offset_x_pct, offset_y_pct
                            );

                            // Provide cropping advice
                            if offset_x_pct.abs() > 10.0 || offset_y_pct.abs() > 10.0 {
                                println!("   • 🎯 Smart cropping recommended: significant people offset detected");
                            } else {
                                println!("   • ✓ People well-centered, standard cropping suitable");
                            }
                        } else {
                            println!("   • ℹ No people detected, using standard image processing");
                        }
                    }
                    Some(result)
                }
                Err(e) => {
                    verbose_println(
                        self.config.verbose,
                        &format!("❌ People detection failed: {}", e),
                    );
                    None
                }
            }
        } else {
            None
        }
    }

    /// Process portrait images by pairing them and combining into landscape images
    pub fn process_portrait_pairs_with_progress(
        &self,
        portrait_files: &[PathBuf],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        if portrait_files.is_empty() {
            return Ok(vec![]);
        }

        verbose_println(
            self.config.verbose,
            &format!("Processing {} portrait images", portrait_files.len()),
        );

        // Randomize portrait order for more interesting combinations
        let mut portrait_files = portrait_files.to_vec();
        use rand::seq::SliceRandom;
        let mut rng = rand::rng();
        portrait_files.shuffle(&mut rng);

        // Check if there's an unpaired portrait (odd number)
        let unpaired_portrait = if portrait_files.len() % 2 == 1 {
            Some(portrait_files.pop().unwrap())
        } else {
            None
        };

        if unpaired_portrait.is_some() {
            verbose_println(
                self.config.verbose,
                &format!(
                    "ℹ Discarding leftover portrait: {}",
                    unpaired_portrait
                        .unwrap()
                        .file_name()
                        .unwrap_or_default()
                        .to_string_lossy()
                ),
            );
        }

        // Create pairs from randomized portraits
        let portrait_pairs: Vec<(PathBuf, PathBuf)> = portrait_files
            .chunks_exact(2)
            .map(|chunk| (chunk[0].clone(), chunk[1].clone()))
            .collect();

        verbose_println(
            self.config.verbose,
            &format!("Created {} portrait pairs", portrait_pairs.len()),
        );

        completion_progress.set_message("Processing portrait pairs in parallel...");

        // Process each pair individually using the thread pool (just like landscapes)
        let results = self.process_portrait_pairs_parallel(
            &portrait_pairs,
            output_dir,
            main_progress,
            thread_progress_bars,
            completion_progress,
        )?;

        completion_progress.set_message("Portrait processing complete");
        Ok(results)
    }

    /// Process portrait pairs in parallel using the thread pool (similar to landscape processing)
    fn process_portrait_pairs_parallel(
        &self,
        portrait_pairs: &[(PathBuf, PathBuf)],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        use std::collections::HashMap;
        use std::sync::atomic::{AtomicUsize, Ordering};

        let processed_count = AtomicUsize::new(0);
        let thread_assignment = Arc::new(Mutex::new(HashMap::new()));
        let next_thread_id = AtomicUsize::new(0);

        completion_progress.set_message("Processing portrait pairs in parallel...");

        let results: Vec<Result<ProcessingResult>> = portrait_pairs
            .par_iter()
            .enumerate()
            .map(|(index, (left_path, right_path))| {
                // Get or assign thread progress bar
                let current_thread_id = rayon::current_thread_index().unwrap_or(0);
                let pb_index = {
                    let mut assignment = thread_assignment.lock().unwrap();
                    if let Some(&pb_index) = assignment.get(&current_thread_id) {
                        pb_index
                    } else {
                        let pb_index = next_thread_id.fetch_add(1, Ordering::Relaxed)
                            % thread_progress_bars.len();
                        assignment.insert(current_thread_id, pb_index);
                        pb_index
                    }
                };

                let thread_pb = &thread_progress_bars[pb_index];

                // Update thread progress
                let left_filename = left_path
                    .file_name()
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown");
                let right_filename = right_path
                    .file_name()
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown");
                thread_pb.set_message(format!("Processing {} + {}", left_filename, right_filename));

                // Process the portrait pair
                let result = self.process_single_portrait_pair_with_progress(
                    left_path, right_path, output_dir, index, thread_pb,
                );

                // Update main progress - increment by 2 since we're processing 2 images
                let pairs_processed = processed_count.fetch_add(1, Ordering::Relaxed) + 1;
                main_progress.inc(2); // Processing 2 images even though they create 1 output
                main_progress.set_message(format!("Completed: {} portrait pairs", pairs_processed));

                // Update completion progress
                let progress_percent =
                    (pairs_processed as f64 / portrait_pairs.len() as f64) * 100.0;
                completion_progress.set_position(progress_percent as u64);
                completion_progress.set_message(format!(
                    "Processing portrait pairs: {}/{}",
                    pairs_processed,
                    portrait_pairs.len()
                ));

                // Mark thread as idle
                thread_pb.set_message("Idle");

                result
            })
            .collect();

        completion_progress.set_message("Finalizing portrait results...");
        Ok(results)
    }

    /// Process a single portrait pair from raw image files (complete pipeline)
    fn process_single_portrait_pair_with_progress(
        &self,
        left_path: &Path,
        right_path: &Path,
        output_dir: &Path,
        _index: usize,
        progress_bar: &ProgressBar,
    ) -> Result<ProcessingResult> {
        let start_time = std::time::Instant::now();
        let left_filename = left_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");
        let right_filename = right_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        verbose_println(
            self.config.verbose,
            &format!(
                "Processing portrait pair: {} + {}",
                left_filename, right_filename
            ),
        );

        // Stage 1: Load both images (10-15%)
        progress_bar.set_position(10);
        progress_bar.set_message(format!("Loading {} + {}", left_filename, right_filename));
        std::thread::yield_now();

        progress_bar.set_position(12);
        let left_img = image::open(left_path)
            .with_context(|| format!("Failed to open left image: {}", left_path.display()))?
            .to_rgb8();
        progress_bar.set_position(13);
        let right_img = image::open(right_path)
            .with_context(|| format!("Failed to open right image: {}", right_path.display()))?
            .to_rgb8();
        progress_bar.set_position(15);

        // Stage 2: Apply EXIF rotation (15-25%)
        progress_bar.set_position(15);
        progress_bar.set_message(format!(
            "Applying rotation to {} + {}",
            left_filename, right_filename
        ));
        std::thread::yield_now();

        progress_bar.set_position(18);
        let (left_rotated, left_orientation) =
            match orientation::detect_orientation(left_path, &left_img) {
                Ok(orientation_info) => {
                    let exif_orient = orientation_info.exif_orientation;
                    (
                        orientation::apply_rotation(&left_img, exif_orient)
                            .unwrap_or_else(|_| left_img.clone()),
                        exif_orient,
                    )
                }
                Err(_) => (left_img.clone(), orientation::ExifOrientation::Undefined),
            };

        progress_bar.set_position(22);
        let (right_rotated, right_orientation) =
            match orientation::detect_orientation(right_path, &right_img) {
                Ok(orientation_info) => {
                    let exif_orient = orientation_info.exif_orientation;
                    (
                        orientation::apply_rotation(&right_img, exif_orient)
                            .unwrap_or_else(|_| right_img.clone()),
                        exif_orient,
                    )
                }
                Err(_) => (right_img.clone(), orientation::ExifOrientation::Undefined),
            };
        progress_bar.set_position(25);

        // Stage 3: People detection (25-35%)
        progress_bar.set_position(25);
        progress_bar.set_message(format!(
            "Detecting people in {} + {}",
            left_filename, right_filename
        ));
        std::thread::yield_now();

        progress_bar.set_position(28);
        let left_detection = if self.subject_detector.is_some() {
            self.detect_people_with_logging(&left_rotated, left_path)
        } else {
            None
        };

        progress_bar.set_position(32);
        let right_detection = if self.subject_detector.is_some() {
            self.detect_people_with_logging(&right_rotated, right_path)
        } else {
            None
        };
        progress_bar.set_position(35);

        // Stage 4: Resize to half-width (35-45%)
        progress_bar.set_position(35);
        progress_bar.set_message(format!("Resizing {} + {}", left_filename, right_filename));
        std::thread::yield_now();

        let half_width = self.config.target_width / 2;

        progress_bar.set_position(38); // Show progress during left resize
        let left_resized = resize::smart_resize_with_people_detection(
            &left_rotated,
            half_width,
            self.config.target_height,
            self.config.auto_orientation,
            left_detection.as_ref(),
            self.config.verbose,
        )?;

        progress_bar.set_position(42); // Show progress during right resize
        let right_resized = resize::smart_resize_with_people_detection(
            &right_rotated,
            half_width,
            self.config.target_height,
            self.config.auto_orientation,
            right_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(45); // Both resizes complete

        // Auto-optimization: analyze both images and determine optimal parameters (portrait pair)
        let (
            left_optimized_dither,
            left_optimized_strength,
            left_optimized_contrast,
            left_optimized_brightness,
            left_optimized_auto_color,
            left_analysis,
        ) = if self.config.auto_optimize {
            progress_bar.set_message(format!("Analyzing {}", left_filename));
            let optimization_result =
                auto_optimizer::analyze_and_optimize(&left_resized, left_detection.as_ref())?;
            let analysis = auto_optimizer::analyze_image_characteristics(
                &left_resized,
                left_detection.as_ref(),
            )?;

            (
                optimization_result.dither_method,
                optimization_result.dither_strength,
                (optimization_result.contrast_adjustment * 100.0) as i32, // Convert from -1.0..1.0 to -100..100
                self.config.brightness, // Use config brightness (not auto-optimized yet)
                optimization_result.auto_color_correct,
                Some(analysis),
            )
        } else {
            (
                self.config.dithering_method.clone(),
                self.config.dither_strength,
                self.config.contrast,
                self.config.brightness,
                self.config.auto_color_correct,
                None,
            )
        };

        let (
            right_optimized_dither,
            right_optimized_strength,
            right_optimized_contrast,
            right_optimized_brightness,
            right_optimized_auto_color,
            right_analysis,
        ) = if self.config.auto_optimize {
            progress_bar.set_message(format!("Analyzing {}", right_filename));
            let optimization_result =
                auto_optimizer::analyze_and_optimize(&right_resized, right_detection.as_ref())?;
            let analysis = auto_optimizer::analyze_image_characteristics(
                &right_resized,
                right_detection.as_ref(),
            )?;

            (
                optimization_result.dither_method,
                optimization_result.dither_strength,
                (optimization_result.contrast_adjustment * 100.0) as i32, // Convert from -1.0..1.0 to -100..100
                self.config.brightness, // Use config brightness (not auto-optimized yet)
                optimization_result.auto_color_correct,
                Some(analysis),
            )
        } else {
            (
                self.config.dithering_method.clone(),
                self.config.dither_strength,
                self.config.contrast,
                self.config.brightness,
                self.config.auto_color_correct,
                None,
            )
        };

        // Stage 5: Apply color correction if enabled (45-48%) - per-image optimization
        progress_bar.set_position(45);
        progress_bar.set_position(46); // Processing left image correction
        let left_color_corrected = if left_optimized_auto_color {
            color_correction::apply_auto_color_correction_with_saturation(
                &left_resized,
                self.config.saturation_boost,
            )?
        } else {
            left_resized
        };
        progress_bar.set_position(47); // Processing right image correction
        let right_color_corrected = if right_optimized_auto_color {
            color_correction::apply_auto_color_correction_with_saturation(
                &right_resized,
                self.config.saturation_boost,
            )?
        } else {
            right_resized
        };
        progress_bar.set_position(48);

        // Stage 5b: Apply brightness adjustment if enabled (48-49%) - per-image optimization
        progress_bar.set_position(48);
        let left_brightness_adjusted = if left_optimized_brightness != 0 {
            color_correction::apply_brightness_adjustment(
                &left_color_corrected,
                left_optimized_brightness,
            )?
        } else {
            left_color_corrected
        };
        let right_brightness_adjusted = if right_optimized_brightness != 0 {
            color_correction::apply_brightness_adjustment(
                &right_color_corrected,
                right_optimized_brightness,
            )?
        } else {
            right_color_corrected
        };
        progress_bar.set_position(49);

        // Stage 5c: Apply contrast adjustment if enabled (49-50%) - per-image optimization
        progress_bar.set_position(49);
        let left_contrast_adjusted = if left_optimized_contrast != 0 {
            color_correction::apply_contrast_adjustment(
                &left_brightness_adjusted,
                left_optimized_contrast,
            )?
        } else {
            left_brightness_adjusted
        };
        let right_contrast_adjusted = if right_optimized_contrast != 0 {
            color_correction::apply_contrast_adjustment(
                &right_brightness_adjusted,
                right_optimized_contrast,
            )?
        } else {
            right_brightness_adjusted
        };
        progress_bar.set_position(50);

        // Stage 6: Apply image processing (50-55%) - per-image optimized parameters
        progress_bar.set_message(format!(
            "Processing colors for {} + {}",
            left_filename, right_filename
        ));
        std::thread::yield_now();

        progress_bar.set_position(52); // Processing left image colors
        let left_processed = convert::process_image(
            &left_contrast_adjusted,
            &self.config.processing_type,
            &left_optimized_dither,
            left_optimized_strength,
        )?;
        progress_bar.set_position(54); // Processing right image colors
        let right_processed = convert::process_image(
            &right_contrast_adjusted,
            &self.config.processing_type,
            &right_optimized_dither,
            right_optimized_strength,
        )?;
        progress_bar.set_position(55); // Color processing complete

        // Stage 6: Apply annotations (55-65%) - conditional based on config
        progress_bar.set_position(55);
        let (left_annotated, right_annotated) = if self.config.annotate {
            progress_bar.set_message(format!(
                "Adding annotations to {} + {}",
                left_filename, right_filename
            ));
            std::thread::yield_now();

            progress_bar.set_position(58); // Annotating left image
            let left_annotated = annotate::add_filename_annotation(
                &left_processed,
                left_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?;

            progress_bar.set_position(62); // Annotating right image
            let right_annotated = annotate::add_filename_annotation(
                &right_processed,
                right_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?;
            progress_bar.set_position(65); // Annotations complete

            (left_annotated, right_annotated)
        } else {
            progress_bar.set_message(format!(
                "Skipping annotations for {} + {}",
                left_filename, right_filename
            ));
            progress_bar.set_position(65);
            (left_processed, right_processed)
        };

        // Stage 7: Combine images (65-75%)
        progress_bar.set_position(65);
        progress_bar.set_message(format!("Combining {} + {}", left_filename, right_filename));
        std::thread::yield_now();

        progress_bar.set_position(68); // Show progress during combining
        let combined_img = combine_processed_portraits(
            &left_annotated,
            &right_annotated,
            self.config.target_width,
            self.config.target_height,
            self.config.divider_width,
            self.config.divider_color,
        )?;
        progress_bar.set_position(72); // Combining complete

        // Apply 90° CW rotation if needed (72-75%)
        // Pre-rotation is only needed for 6c portrait mode
        let final_combined_img = if self.config.needs_pre_rotation {
            progress_bar.set_message(format!(
                "Pre-rotating combined for portrait: {} + {}",
                left_filename, right_filename
            ));
            verbose_println(
                self.config.verbose,
                "🔄 Pre-rotating combined 90° CW for 6c portrait display",
            );
            image::imageops::rotate90(&combined_img)
        } else {
            combined_img
        };
        progress_bar.set_position(75); // Rotation complete (or skipped)

        // Stage 8: Save final result (75-95%)
        progress_bar.set_position(75);
        progress_bar.set_message("Saving combined image".to_string());
        std::thread::yield_now();

        // Save combined files to multiple formats (75-95%)
        progress_bar.set_position(75);

        let mut saved_paths = std::collections::HashMap::new();
        let progress_per_format = 20.0 / self.config.output_formats.len() as f64;
        let mut current_progress = 75.0;

        for format in &self.config.output_formats {
            let format_name = match format {
                crate::cli::OutputType::Bmp => "combined BMP",
                crate::cli::OutputType::Bin => "combined Binary",
                crate::cli::OutputType::Jpg => "combined JPG",
                crate::cli::OutputType::Png => "combined PNG",
            };

            progress_bar.set_message(format!(
                "Saving {}: {} + {}",
                format_name, left_filename, right_filename
            ));

            let output_path =
                self.get_combined_format_output_path(output_dir, left_path, right_path, format);

            if self.config.dry_run {
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Dry run: Would save {} '{}' + '{}' to {}",
                        format_name,
                        left_filename,
                        right_filename,
                        output_path.display()
                    ),
                );
            } else {
                match format {
                    crate::cli::OutputType::Bmp
                    | crate::cli::OutputType::Jpg
                    | crate::cli::OutputType::Png => {
                        final_combined_img.save(&output_path).with_context(|| {
                            format!("Failed to save {}: {}", format_name, output_path.display())
                        })?;
                    }
                    crate::cli::OutputType::Bin => {
                        // Use appropriate binary format based on display type
                        let binary_data = match self.config.processing_type {
                            ProcessingType::BlackWhite => {
                                // B/W displays use the original ESP32 compressed format (RRRGGGBB)
                                binary::convert_to_esp32_binary(&final_combined_img)?
                            }
                            ProcessingType::SixColor => {
                                // 6c displays use demo bitmap mode 1 format for drawDemoBitmap()
                                binary::convert_to_demo_bitmap_mode1(&final_combined_img)?
                            }
                        };
                        std::fs::write(&output_path, &binary_data).with_context(|| {
                            format!("Failed to save combined binary: {}", output_path.display())
                        })?;
                    }
                }
            }

            saved_paths.insert(format.clone(), output_path);

            current_progress += progress_per_format;
            progress_bar.set_position(current_progress as u64);
        }

        progress_bar.set_position(95);

        progress_bar.set_position(100);
        progress_bar.set_message(format!("Completed {} + {}", left_filename, right_filename));

        verbose_println(
            self.config.verbose,
            &format!(
                "✅ Portrait pair completed in {:.2}s",
                start_time.elapsed().as_secs_f32()
            ),
        );

        // Create individual portrait results for the summary
        let individual_portraits = vec![
            IndividualPortraitResult {
                path: left_path.to_path_buf(),
                people_detected: left_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: left_detection.as_ref().map_or(0, |d| d.person_count),
                confidence: left_detection.as_ref().map_or(0.0, |d| d.confidence),
            },
            IndividualPortraitResult {
                path: right_path.to_path_buf(),
                people_detected: right_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: right_detection.as_ref().map_or(0, |d| d.person_count),
                confidence: right_detection.as_ref().map_or(0.0, |d| d.confidence),
            },
        ];

        // Calculate combined statistics
        let combined_people_detected = individual_portraits.iter().any(|p| p.people_detected);
        let combined_people_count = individual_portraits.iter().map(|p| p.people_count).sum();

        // Collect processing report data if enabled
        if self.config.optimization_report {
            let left_was_rotated = !matches!(
                left_orientation,
                orientation::ExifOrientation::TopLeft | orientation::ExifOrientation::Undefined
            );
            let left_color_skipped = !left_optimized_auto_color && self.config.auto_color_correct;
            let right_was_rotated = !matches!(
                right_orientation,
                orientation::ExifOrientation::TopLeft | orientation::ExifOrientation::Undefined
            );
            let right_color_skipped = !right_optimized_auto_color && self.config.auto_color_correct;

            let left_entry = optimization_report::OptimizationEntry {
                input_filename: optimization_report::extract_filename(left_path),
                output_filename: format!(
                    "{} (left)",
                    optimization_report::extract_filename(left_path)
                ),
                dither_method: left_optimized_dither,
                dither_strength: left_optimized_strength,
                contrast: left_optimized_contrast as f32,
                auto_color: left_optimized_auto_color,
                people_detected: left_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: left_detection.as_ref().map_or(0, |d| d.person_count),
                is_pastel: left_analysis.as_ref().map_or(false, |a| a.is_pastel_toned),
                was_rotated: left_was_rotated,
                color_skipped: left_color_skipped,
            };

            let right_entry = optimization_report::OptimizationEntry {
                input_filename: optimization_report::extract_filename(right_path),
                output_filename: format!(
                    "{} (right)",
                    optimization_report::extract_filename(right_path)
                ),
                dither_method: right_optimized_dither,
                dither_strength: right_optimized_strength,
                contrast: right_optimized_contrast as f32,
                auto_color: right_optimized_auto_color,
                people_detected: right_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: right_detection.as_ref().map_or(0, |d| d.person_count),
                is_pastel: right_analysis.as_ref().map_or(false, |a| a.is_pastel_toned),
                was_rotated: right_was_rotated,
                color_skipped: right_color_skipped,
            };

            let combined_output = saved_paths
                .get(&self.config.output_formats[0])
                .and_then(|p| p.file_name())
                .and_then(|f| f.to_str())
                .unwrap_or("unknown")
                .to_string();

            let pair_entry = optimization_report::PortraitPairEntry {
                left: left_entry,
                right: right_entry,
                combined_output,
            };

            self.optimization_report
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .add_portrait_pair(pair_entry);
        }

        Ok(ProcessingResult {
            input_path: left_path.to_path_buf(), // Use first image as representative
            output_paths: saved_paths,
            image_type: ImageType::CombinedPortrait,
            processing_time: start_time.elapsed(),
            people_detected: combined_people_detected,
            people_count: combined_people_count,
            individual_portraits: Some(individual_portraits),
        })
    }

    /// Process landscape pairs with progress tracking (for portrait target displays)
    pub fn process_landscape_pairs_with_progress(
        &self,
        landscape_files: &[PathBuf],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        if landscape_files.is_empty() {
            return Ok(vec![]);
        }

        verbose_println(
            self.config.verbose,
            &format!("Processing {} landscape images", landscape_files.len()),
        );

        // Randomize landscape order for more interesting combinations
        let mut landscape_files = landscape_files.to_vec();
        use rand::seq::SliceRandom;
        let mut rng = rand::rng();
        landscape_files.shuffle(&mut rng);

        // Check if there's an unpaired landscape (odd number)
        let unpaired_landscape = if landscape_files.len() % 2 == 1 {
            Some(landscape_files.pop().unwrap())
        } else {
            None
        };

        if unpaired_landscape.is_some() {
            verbose_println(
                self.config.verbose,
                &format!(
                    "ℹ Discarding leftover landscape: {}",
                    unpaired_landscape
                        .unwrap()
                        .file_name()
                        .unwrap_or_default()
                        .to_string_lossy()
                ),
            );
        }

        // Create pairs from randomized landscapes
        let landscape_pairs: Vec<(PathBuf, PathBuf)> = landscape_files
            .chunks_exact(2)
            .map(|chunk| (chunk[0].clone(), chunk[1].clone()))
            .collect();

        verbose_println(
            self.config.verbose,
            &format!("Created {} landscape pairs", landscape_pairs.len()),
        );

        completion_progress.set_message("Processing landscape pairs in parallel...");

        // Process each pair individually using the thread pool
        let results = self.process_landscape_pairs_parallel(
            &landscape_pairs,
            output_dir,
            main_progress,
            thread_progress_bars,
            completion_progress,
        )?;

        completion_progress.set_message("Landscape processing complete");
        Ok(results)
    }

    /// Process landscape pairs in parallel using the thread pool
    fn process_landscape_pairs_parallel(
        &self,
        landscape_pairs: &[(PathBuf, PathBuf)],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        use std::collections::HashMap;
        use std::sync::atomic::{AtomicUsize, Ordering};

        let processed_count = AtomicUsize::new(0);
        let thread_assignment = Arc::new(Mutex::new(HashMap::new()));
        let next_thread_id = AtomicUsize::new(0);

        completion_progress.set_message("Processing landscape pairs in parallel...");

        let results: Vec<Result<ProcessingResult>> = landscape_pairs
            .par_iter()
            .enumerate()
            .map(|(index, (top_path, bottom_path))| {
                // Get or assign thread progress bar
                let current_thread_id = rayon::current_thread_index().unwrap_or(0);
                let pb_index = {
                    let mut assignment = thread_assignment.lock().unwrap();
                    if let Some(&pb_index) = assignment.get(&current_thread_id) {
                        pb_index
                    } else {
                        let pb_index = next_thread_id.fetch_add(1, Ordering::Relaxed)
                            % thread_progress_bars.len();
                        assignment.insert(current_thread_id, pb_index);
                        pb_index
                    }
                };

                let thread_pb = &thread_progress_bars[pb_index];

                // Update thread progress
                let top_filename = top_path
                    .file_name()
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown");
                let bottom_filename = bottom_path
                    .file_name()
                    .and_then(|f| f.to_str())
                    .unwrap_or("unknown");
                thread_pb.set_message(format!("Processing {} + {}", top_filename, bottom_filename));

                // Process the landscape pair
                let result = self.process_single_landscape_pair_with_progress(
                    top_path,
                    bottom_path,
                    output_dir,
                    index,
                    thread_pb,
                );

                // Update main progress - increment by 2 since we're processing 2 images
                let pairs_processed = processed_count.fetch_add(1, Ordering::Relaxed) + 1;
                main_progress.inc(2); // Processing 2 images even though they create 1 output
                main_progress
                    .set_message(format!("Completed: {} landscape pairs", pairs_processed));

                // Update completion progress
                let progress_percent =
                    (pairs_processed as f64 / landscape_pairs.len() as f64) * 100.0;
                completion_progress.set_position(progress_percent as u64);
                completion_progress.set_message(format!(
                    "Processing landscape pairs: {}/{}",
                    pairs_processed,
                    landscape_pairs.len()
                ));

                // Mark thread as idle
                thread_pb.set_message("Idle");

                result
            })
            .collect();

        completion_progress.set_message("Finalizing landscape results...");
        Ok(results)
    }

    /// Process a single landscape pair from raw image files (complete pipeline for portrait target)
    fn process_single_landscape_pair_with_progress(
        &self,
        top_path: &Path,
        bottom_path: &Path,
        output_dir: &Path,
        _index: usize,
        progress_bar: &ProgressBar,
    ) -> Result<ProcessingResult> {
        let start_time = std::time::Instant::now();
        let top_filename = top_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");
        let bottom_filename = bottom_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        verbose_println(
            self.config.verbose,
            &format!(
                "Processing landscape pair: {} + {}",
                top_filename, bottom_filename
            ),
        );

        // Stage 1: Load both images (10-15%)
        progress_bar.set_position(10);
        progress_bar.set_message(format!("Loading {} + {}", top_filename, bottom_filename));
        std::thread::yield_now();

        progress_bar.set_position(12);
        let top_img = image::open(top_path)
            .with_context(|| format!("Failed to open top image: {}", top_path.display()))?
            .to_rgb8();
        progress_bar.set_position(13);
        let bottom_img = image::open(bottom_path)
            .with_context(|| format!("Failed to open bottom image: {}", bottom_path.display()))?
            .to_rgb8();
        progress_bar.set_position(15);

        // Stage 2: Apply EXIF rotation (15-25%)
        progress_bar.set_position(15);
        progress_bar.set_message(format!(
            "Applying rotation to {} + {}",
            top_filename, bottom_filename
        ));
        std::thread::yield_now();

        progress_bar.set_position(18);
        let (top_rotated, top_orientation) =
            match orientation::detect_orientation(top_path, &top_img) {
                Ok(orientation_info) => {
                    let exif_orient = orientation_info.exif_orientation;
                    (
                        orientation::apply_rotation(&top_img, exif_orient)
                            .unwrap_or_else(|_| top_img.clone()),
                        exif_orient,
                    )
                }
                Err(_) => (top_img.clone(), orientation::ExifOrientation::Undefined),
            };

        progress_bar.set_position(22);
        let (bottom_rotated, bottom_orientation) =
            match orientation::detect_orientation(bottom_path, &bottom_img) {
                Ok(orientation_info) => {
                    let exif_orient = orientation_info.exif_orientation;
                    (
                        orientation::apply_rotation(&bottom_img, exif_orient)
                            .unwrap_or_else(|_| bottom_img.clone()),
                        exif_orient,
                    )
                }
                Err(_) => (bottom_img.clone(), orientation::ExifOrientation::Undefined),
            };
        progress_bar.set_position(25);

        // Stage 3: People detection (25-35%)
        progress_bar.set_position(25);
        progress_bar.set_message(format!(
            "Detecting people in {} + {}",
            top_filename, bottom_filename
        ));
        std::thread::yield_now();

        progress_bar.set_position(28);
        let top_detection = if self.subject_detector.is_some() {
            self.detect_people_with_logging(&top_rotated, top_path)
        } else {
            None
        };

        progress_bar.set_position(32);
        let bottom_detection = if self.subject_detector.is_some() {
            self.detect_people_with_logging(&bottom_rotated, bottom_path)
        } else {
            None
        };
        progress_bar.set_position(35);

        // Stage 4: Resize landscapes for combination (35-45%)
        progress_bar.set_position(35);
        progress_bar.set_message(format!("Resizing {} + {}", top_filename, bottom_filename));
        std::thread::yield_now();

        // For pre-rotation mode (6c portrait), we need to:
        // 1. Resize to half-height × target_width (e.g., 240×800 becomes 400×480 after rotation)
        // 2. Rotate 90° CW
        // 3. Combine side-by-side (like portraits)
        //
        // For normal mode:
        // 1. Resize to target_width × half-height (e.g., 800×240)
        // 2. Combine top-bottom
        let (resize_width, resize_height) = if self.config.needs_pre_rotation {
            // Portrait mode: resize to half-width × full-height (will be rotated to full-height × half-width)
            // 480×400 → rotate90 → 400×480, then combine side-by-side → 800×480
            (self.config.target_height, self.config.target_width / 2) // e.g., 480×400
        } else {
            // Landscape mode: standard full-width × half-height, combine top-bottom
            (self.config.target_width, self.config.target_height / 2) // e.g., 800×240
        };

        progress_bar.set_position(38);
        let top_resized = resize::smart_resize_with_people_detection(
            &top_rotated,
            resize_width,
            resize_height,
            self.config.auto_orientation,
            top_detection.as_ref(),
            self.config.verbose,
        )?;

        progress_bar.set_position(42);
        let bottom_resized = resize::smart_resize_with_people_detection(
            &bottom_rotated,
            resize_width,
            resize_height,
            self.config.auto_orientation,
            bottom_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(45);

        // Stage 5: Auto-optimization or use global settings
        progress_bar.set_position(45);
        let (
            top_optimized_dither,
            top_optimized_strength,
            top_optimized_contrast,
            top_optimized_brightness,
            top_optimized_auto_color,
            top_analysis,
        ) = if self.config.auto_optimize {
            progress_bar.set_message(format!("Analyzing {}", top_filename));
            let optimization_result =
                auto_optimizer::analyze_and_optimize(&top_resized, top_detection.as_ref())?;
            let analysis = auto_optimizer::analyze_image_characteristics(
                &top_resized,
                top_detection.as_ref(),
            )?;

            (
                optimization_result.dither_method,
                optimization_result.dither_strength,
                (optimization_result.contrast_adjustment * 100.0) as i32, // Convert from -1.0..1.0 to -100..100
                self.config.brightness, // Use config brightness (not auto-optimized yet)
                optimization_result.auto_color_correct,
                Some(analysis),
            )
        } else {
            (
                self.config.dithering_method.clone(),
                self.config.dither_strength,
                self.config.contrast,
                self.config.brightness,
                self.config.auto_color_correct,
                None,
            )
        };

        let (
            bottom_optimized_dither,
            bottom_optimized_strength,
            bottom_optimized_contrast,
            bottom_optimized_brightness,
            bottom_optimized_auto_color,
            bottom_analysis,
        ) = if self.config.auto_optimize {
            progress_bar.set_message(format!("Analyzing {}", bottom_filename));
            let optimization_result =
                auto_optimizer::analyze_and_optimize(&bottom_resized, bottom_detection.as_ref())?;
            let analysis = auto_optimizer::analyze_image_characteristics(
                &bottom_resized,
                bottom_detection.as_ref(),
            )?;

            (
                optimization_result.dither_method,
                optimization_result.dither_strength,
                (optimization_result.contrast_adjustment * 100.0) as i32, // Convert from -1.0..1.0 to -100..100
                self.config.brightness, // Use config brightness (not auto-optimized yet)
                optimization_result.auto_color_correct,
                Some(analysis),
            )
        } else {
            (
                self.config.dithering_method.clone(),
                self.config.dither_strength,
                self.config.contrast,
                self.config.brightness,
                self.config.auto_color_correct,
                None,
            )
        };

        // Stage 6: Apply color correction if enabled (45-48%)
        progress_bar.set_position(45);
        progress_bar.set_position(46);
        let top_color_corrected = if top_optimized_auto_color {
            color_correction::apply_auto_color_correction_with_saturation(
                &top_resized,
                self.config.saturation_boost,
            )?
        } else {
            top_resized
        };
        progress_bar.set_position(47);
        let bottom_color_corrected = if bottom_optimized_auto_color {
            color_correction::apply_auto_color_correction_with_saturation(
                &bottom_resized,
                self.config.saturation_boost,
            )?
        } else {
            bottom_resized
        };
        progress_bar.set_position(48);

        // Stage 7: Apply brightness adjustment (48-49%)
        progress_bar.set_position(48);
        let top_brightness_adjusted = if top_optimized_brightness != 0 {
            color_correction::apply_brightness_adjustment(
                &top_color_corrected,
                top_optimized_brightness,
            )?
        } else {
            top_color_corrected
        };
        let bottom_brightness_adjusted = if bottom_optimized_brightness != 0 {
            color_correction::apply_brightness_adjustment(
                &bottom_color_corrected,
                bottom_optimized_brightness,
            )?
        } else {
            bottom_color_corrected
        };
        progress_bar.set_position(49);

        // Stage 8: Apply contrast adjustment (49-50%)
        progress_bar.set_position(49);
        let top_contrast_adjusted = if top_optimized_contrast != 0 {
            color_correction::apply_contrast_adjustment(
                &top_brightness_adjusted,
                top_optimized_contrast,
            )?
        } else {
            top_brightness_adjusted
        };
        let bottom_contrast_adjusted = if bottom_optimized_contrast != 0 {
            color_correction::apply_contrast_adjustment(
                &bottom_brightness_adjusted,
                bottom_optimized_contrast,
            )?
        } else {
            bottom_brightness_adjusted
        };
        progress_bar.set_position(50);

        // Stage 8: Apply image processing (50-55%)
        progress_bar.set_message(format!(
            "Processing colors for {} + {}",
            top_filename, bottom_filename
        ));
        std::thread::yield_now();

        progress_bar.set_position(52);
        let top_processed = convert::process_image(
            &top_contrast_adjusted,
            &self.config.processing_type,
            &top_optimized_dither,
            top_optimized_strength,
        )?;
        progress_bar.set_position(54);
        let bottom_processed = convert::process_image(
            &bottom_contrast_adjusted,
            &self.config.processing_type,
            &bottom_optimized_dither,
            bottom_optimized_strength,
        )?;
        progress_bar.set_position(55);

        // Stage 9: Apply annotations (55-65%)
        progress_bar.set_position(55);
        let (top_annotated, bottom_annotated) = if self.config.annotate {
            progress_bar.set_message(format!(
                "Adding annotations to {} + {}",
                top_filename, bottom_filename
            ));
            std::thread::yield_now();

            progress_bar.set_position(58);
            let top_annotated = annotate::add_filename_annotation(
                &top_processed,
                top_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?;
            progress_bar.set_position(61);
            let bottom_annotated = annotate::add_filename_annotation(
                &bottom_processed,
                bottom_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?;
            progress_bar.set_position(65);
            (top_annotated, bottom_annotated)
        } else {
            progress_bar.set_message(format!(
                "Skipping annotations for {} + {}",
                top_filename, bottom_filename
            ));
            progress_bar.set_position(65);
            (top_processed, bottom_processed)
        };

        // Stage 10: Rotate (if needed) and combine images (65-75%)
        progress_bar.set_position(65);

        let combined = if self.config.needs_pre_rotation {
            // Pre-rotation mode (6c portrait): rotate then combine side-by-side
            progress_bar.set_message(format!(
                "Rotating {} + {} for portrait",
                top_filename, bottom_filename
            ));

            // Rotate both images 90° CW (240×800 → 800×240 becomes 240×800 → 800×240)
            // Wait, dimensions after rotation: 240×800 → rotate90 → 800×240
            progress_bar.set_position(67);
            let top_rotated = image::imageops::rotate90(&top_annotated);
            let bottom_rotated = image::imageops::rotate90(&bottom_annotated);
            progress_bar.set_position(70);

            // Now combine side-by-side like portraits
            progress_bar.set_message(format!(
                "Combining {} + {} (side-by-side rotated)",
                top_filename, bottom_filename
            ));
            combine_processed_portraits(
                &top_rotated,
                &bottom_rotated,
                self.config.target_width,
                self.config.target_height,
                self.config.divider_width,
                self.config.divider_color,
            )?
        } else {
            // Normal mode: combine top-bottom
            progress_bar.set_message(format!(
                "Combining {} + {} (top-bottom)",
                top_filename, bottom_filename
            ));
            progress_bar.set_position(70);
            combine::combine_processed_landscapes(
                &top_annotated,
                &bottom_annotated,
                self.config.target_width,
                self.config.target_height,
                self.config.divider_width,
                self.config.divider_color,
            )?
        };
        progress_bar.set_position(75);

        // Save combined files to multiple formats (75-95%)
        progress_bar.set_position(75);

        let mut saved_paths = std::collections::HashMap::new();
        let progress_per_format = 20.0 / self.config.output_formats.len() as f64;
        let mut current_progress = 75.0;

        for format in &self.config.output_formats {
            let format_name = match format {
                crate::cli::OutputType::Bmp => "combined BMP",
                crate::cli::OutputType::Bin => "combined Binary",
                crate::cli::OutputType::Jpg => "combined JPG",
                crate::cli::OutputType::Png => "combined PNG",
            };

            progress_bar.set_message(format!(
                "Saving {}: {} + {}",
                format_name, top_filename, bottom_filename
            ));

            let output_path =
                self.get_combined_format_output_path(output_dir, top_path, bottom_path, format);

            if self.config.dry_run {
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Dry run: Would save {} '{}' + '{}' to {}",
                        format_name,
                        top_filename,
                        bottom_filename,
                        output_path.display()
                    ),
                );
            } else {
                match format {
                    crate::cli::OutputType::Bmp
                    | crate::cli::OutputType::Jpg
                    | crate::cli::OutputType::Png => {
                        combined.save(&output_path).with_context(|| {
                            format!("Failed to save {}: {}", format_name, output_path.display())
                        })?;
                    }
                    crate::cli::OutputType::Bin => {
                        // Use appropriate binary format based on display type
                        let binary_data = match self.config.processing_type {
                            ProcessingType::BlackWhite => {
                                // B/W displays use the original ESP32 compressed format (RRRGGGBB)
                                binary::convert_to_esp32_binary(&combined)?
                            }
                            ProcessingType::SixColor => {
                                // 6c displays use demo bitmap mode 1 format for drawDemoBitmap()
                                binary::convert_to_demo_bitmap_mode1(&combined)?
                            }
                        };
                        std::fs::write(&output_path, &binary_data).with_context(|| {
                            format!("Failed to save combined binary: {}", output_path.display())
                        })?;
                    }
                }

                saved_paths.insert(format.clone(), output_path);
            }

            current_progress += progress_per_format;
            progress_bar.set_position(current_progress as u64);
        }
        progress_bar.set_position(95);

        // Stage 12: Add to optimization report if enabled (95-100%)
        progress_bar.set_position(95);
        if self.config.optimization_report {
            let top_was_rotated = top_orientation != orientation::ExifOrientation::Undefined
                && top_orientation != orientation::ExifOrientation::TopLeft;
            let bottom_was_rotated = bottom_orientation != orientation::ExifOrientation::Undefined
                && bottom_orientation != orientation::ExifOrientation::TopLeft;

            let top_color_skipped = top_analysis
                .as_ref()
                .map_or(false, |a| a.is_pastel_toned && !top_optimized_auto_color);
            let bottom_color_skipped = bottom_analysis
                .as_ref()
                .map_or(false, |a| a.is_pastel_toned && !bottom_optimized_auto_color);

            let top_entry = optimization_report::OptimizationEntry {
                input_filename: optimization_report::extract_filename(top_path),
                output_filename: format!(
                    "{} (top)",
                    optimization_report::extract_filename(top_path)
                ),
                dither_method: top_optimized_dither,
                dither_strength: top_optimized_strength,
                contrast: top_optimized_contrast as f32,
                auto_color: top_optimized_auto_color,
                people_detected: top_detection.as_ref().map_or(false, |d| d.person_count > 0),
                people_count: top_detection.as_ref().map_or(0, |d| d.person_count),
                is_pastel: top_analysis.as_ref().map_or(false, |a| a.is_pastel_toned),
                was_rotated: top_was_rotated,
                color_skipped: top_color_skipped,
            };

            let bottom_entry = optimization_report::OptimizationEntry {
                input_filename: optimization_report::extract_filename(bottom_path),
                output_filename: format!(
                    "{} (bottom)",
                    optimization_report::extract_filename(bottom_path)
                ),
                dither_method: bottom_optimized_dither,
                dither_strength: bottom_optimized_strength,
                contrast: bottom_optimized_contrast as f32,
                auto_color: bottom_optimized_auto_color,
                people_detected: bottom_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: bottom_detection.as_ref().map_or(0, |d| d.person_count),
                is_pastel: bottom_analysis
                    .as_ref()
                    .map_or(false, |a| a.is_pastel_toned),
                was_rotated: bottom_was_rotated,
                color_skipped: bottom_color_skipped,
            };

            let combined_output = saved_paths
                .get(&self.config.output_formats[0])
                .and_then(|p| p.file_name())
                .and_then(|f| f.to_str())
                .unwrap_or("unknown")
                .to_string();

            let pair_entry = optimization_report::LandscapePairEntry {
                top: top_entry,
                bottom: bottom_entry,
                combined_output,
            };

            self.optimization_report
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .add_landscape_pair(pair_entry);
        }
        progress_bar.set_position(100);

        let individual_landscapes = vec![
            IndividualPortraitResult {
                path: top_path.to_path_buf(),
                people_detected: top_detection.as_ref().map_or(false, |d| d.person_count > 0),
                people_count: top_detection.as_ref().map_or(0, |d| d.person_count),
                confidence: top_detection.as_ref().map_or(0.0, |d| d.confidence),
            },
            IndividualPortraitResult {
                path: bottom_path.to_path_buf(),
                people_detected: bottom_detection
                    .as_ref()
                    .map_or(false, |d| d.person_count > 0),
                people_count: bottom_detection.as_ref().map_or(0, |d| d.person_count),
                confidence: bottom_detection.as_ref().map_or(0.0, |d| d.confidence),
            },
        ];

        let combined_people_detected = individual_landscapes.iter().any(|p| p.people_detected);
        let combined_people_count = individual_landscapes.iter().map(|p| p.people_count).sum();

        Ok(ProcessingResult {
            input_path: top_path.to_path_buf(), // Use first image as representative
            output_paths: saved_paths,
            image_type: ImageType::CombinedLandscape,
            processing_time: start_time.elapsed(),
            people_detected: combined_people_detected,
            people_count: combined_people_count,
            individual_portraits: Some(individual_landscapes),
        })
    }

    /// Process images in debug mode - visualize detection boxes and crop area
    pub fn process_debug_batch(
        &self,
        input_files: Vec<PathBuf>,
        output_dir: &Path,
    ) -> Result<Vec<ProcessingResult>> {
        if !self.config.debug {
            return Err(anyhow::anyhow!(
                "Debug processing called but debug mode not enabled"
            ));
        }

        let subject_detector = self
            .subject_detector
            .as_ref()
            .ok_or_else(|| anyhow::anyhow!("Debug mode requires people detection"))?;

        let mut results = Vec::new();

        for input_path in input_files {
            let result = self.process_debug_single(&input_path, output_dir, subject_detector)?;
            results.push(result);
        }

        Ok(results)
    }

    /// Process a single image in debug mode
    fn process_debug_single(
        &self,
        input_path: &Path,
        output_dir: &Path,
        subject_detector: &subject_detection::SubjectDetector,
    ) -> Result<ProcessingResult> {
        let filename = input_path
            .file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        verbose_println(
            self.config.verbose,
            &format!("🔍 Debug processing: {}", filename),
        );

        // Load original image
        let rgb_img = image::open(input_path)
            .with_context(|| format!("Failed to load image: {}", input_path.display()))?
            .to_rgb8();

        // Apply orientation correction (same as main processing pipeline)
        // This ensures debug images are saved with correct orientation and
        // detection boxes align properly with the displayed image
        let orientation_info = orientation::detect_orientation(input_path, &rgb_img)?;
        let img = orientation::apply_rotation(&rgb_img, orientation_info.exif_orientation)?;

        verbose_println(
            self.config.verbose,
            &format!(
                "📱 Debug orientation: {:?} ({})",
                orientation_info.exif_orientation,
                if orientation_info.exif_orientation == orientation::ExifOrientation::TopLeft {
                    "no rotation needed"
                } else {
                    "rotation applied for correct display"
                }
            ),
        );

        verbose_println(
            self.config.verbose,
            &format!(
                "   • Original dimensions: {}x{}",
                rgb_img.width(),
                rgb_img.height()
            ),
        );

        // Run people detection on correctly oriented image
        let detection_result =
            subject_detector.detect_people(&img, self.config.confidence_threshold)?;

        // For debug mode, we also need the raw JSON data from find_subject.py to draw all boxes
        let raw_detection_data = self.get_raw_detection_data(&img, subject_detector)?;

        // Create annotated image with detection boxes
        let mut annotated_img = img.clone();
        self.draw_debug_annotations(&mut annotated_img, &detection_result, &raw_detection_data)?;

        // Save debug images to multiple formats
        let mut saved_paths = std::collections::HashMap::new();

        for format in &self.config.output_formats {
            // Debug mode always saves as image format (no binary)
            let actual_format = match format {
                crate::cli::OutputType::Bin => &crate::cli::OutputType::Png, // Convert bin to PNG for debug
                _ => format,
            };

            let output_path =
                self.get_format_output_path(output_dir, input_path, actual_format, Some("debug"));

            if self.config.dry_run {
                verbose_println(
                    self.config.verbose,
                    &format!(
                        "Dry run: Would save debug image '{}' to {}",
                        filename,
                        output_path.display()
                    ),
                );
            } else {
                // Ensure the debug output directory exists
                if let Some(parent_dir) = output_path.parent() {
                    std::fs::create_dir_all(parent_dir).with_context(|| {
                        format!(
                            "Failed to create debug output directory: {}",
                            parent_dir.display()
                        )
                    })?;
                }

                // Save annotated image (no processing, just the annotations)
                annotated_img.save(&output_path).with_context(|| {
                    format!("Failed to save debug image: {}", output_path.display())
                })?;

                verbose_println(
                    self.config.verbose,
                    &format!(
                        "✅ Debug image saved: {}",
                        output_path
                            .file_name()
                            .unwrap_or_default()
                            .to_string_lossy()
                    ),
                );
            }

            saved_paths.insert(format.clone(), output_path.clone());
        }

        // Determine correct image type based on oriented dimensions
        let image_type = if img.height() > img.width() {
            ImageType::Portrait
        } else {
            ImageType::Landscape
        };

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            output_paths: saved_paths,
            image_type,
            processing_time: std::time::Duration::from_millis(0),
            people_detected: detection_result.person_count > 0,
            people_count: detection_result.person_count,
            individual_portraits: None,
        })
    }

    /// Get raw detection data from ONNX detector for debug visualization
    fn get_raw_detection_data(
        &self,
        img: &RgbImage,
        subject_detector: &subject_detection::SubjectDetector,
    ) -> Result<subject_detection::python_yolo_integration::FindSubjectResult> {
        // Use ONNX detection to get results
        let detection_result =
            subject_detector.detect_people(img, self.config.confidence_threshold)?;

        // Convert ONNX detection result to FindSubjectResult for compatibility with debug rendering
        let image_dimensions = img.dimensions();

        // Convert individual detections to Detection format
        let detections: Vec<subject_detection::python_yolo_integration::Detection> =
            detection_result
                .individual_detections
                .iter()
                .map(|(x_min, y_min, x_max, y_max, confidence)| {
                    subject_detection::python_yolo_integration::Detection {
                        bounding_box: [*x_min as i32, *y_min as i32, *x_max as i32, *y_max as i32],
                        confidence: *confidence,
                        class: "person".to_string(),
                        class_id: 0,
                    }
                })
                .collect();

        // Create combined bounding box
        let combined_box = if let Some((x_min, y_min, x_max, y_max)) = detection_result.bounding_box
        {
            [x_min as i32, y_min as i32, x_max as i32, y_max as i32]
        } else {
            [0, 0, image_dimensions.0 as i32, image_dimensions.1 as i32]
        };

        Ok(
            subject_detection::python_yolo_integration::FindSubjectResult {
                image: "onnx_detection".to_string(),
                imagesize: subject_detection::python_yolo_integration::ImageSize {
                    width: image_dimensions.0,
                    height: image_dimensions.1,
                },
                bounding_box: combined_box,
                center: [detection_result.center.0, detection_result.center.1],
                offset: [
                    detection_result.offset_from_center.0,
                    detection_result.offset_from_center.1,
                ],
                detections,
                error: None,
            },
        )
    }

    /// Draw debug annotations on the image
    fn draw_debug_annotations(
        &self,
        img: &mut RgbImage,
        detection_result: &subject_detection::SubjectDetectionResult,
        raw_data: &subject_detection::python_yolo_integration::FindSubjectResult,
    ) -> Result<()> {
        let (img_width, img_height) = img.dimensions();

        // Colors for different box types
        let green = Rgb([0u8, 255u8, 0u8]); // Individual detections (green boxes)
        let blue = Rgb([0u8, 0u8, 255u8]); // Global bounding box (blue box)
        let red = Rgb([255u8, 0u8, 0u8]); // Crop area (red box)

        if detection_result.person_count > 0 {
            // 1. Draw individual detection boxes (GREEN)
            for (i, detection) in raw_data.detections.iter().enumerate() {
                let bbox = &detection.bounding_box;
                // Individual detections use [x, y, width, height] format
                let x1 = bbox[0].max(0) as u32;
                let y1 = bbox[1].max(0) as u32;
                let x2 = bbox[2].max(0).min(img_width as i32) as u32;
                let y2 = bbox[3].max(0).min(img_height as i32) as u32;

                let width = x2 - x1;
                let height = y2 - y1;

                // Clamp to image boundaries
                let x1 = x1.min(img_width.saturating_sub(1));
                let y1 = y1.min(img_height.saturating_sub(1));

                if width > 0 && height > 0 {
                    let rect = Rect::at(x1 as i32, y1 as i32).of_size(width, height);
                    draw_hollow_rect_mut(img, rect, green);

                    // Draw label with class and confidence
                    let label = format!("{} {:.0}%", detection.class, detection.confidence * 100.0);
                    self.draw_label(img, x1, y1, &label, green)?;

                    verbose_println(
                        self.config.verbose,
                        &format!(
                            "  🟢 Detection {}: {} at ({}, {}, {}, {}) conf:{:.2}",
                            i + 1,
                            detection.class,
                            x1,
                            y1,
                            x2,
                            y2,
                            detection.confidence
                        ),
                    );
                }
            }

            // 2. Draw global bounding box (BLUE)
            let global_bbox = &raw_data.bounding_box;
            let gx_min = global_bbox[0].max(0) as u32;
            let gy_min = global_bbox[1].max(0) as u32;
            let gx_max = global_bbox[2].max(0).min(img_width as i32) as u32;
            let gy_max = global_bbox[3].max(0).min(img_height as i32) as u32;

            if gx_max > gx_min && gy_max > gy_min {
                let gwidth = gx_max - gx_min;
                let gheight = gy_max - gy_min;

                // Draw thick blue box (3 pixel thick stroke)
                for offset in 1..4 {
                    let thick_rect = Rect::at((gx_min as i32) - offset, (gy_min as i32) - offset)
                        .of_size(gwidth + (offset * 2) as u32, gheight + (offset * 2) as u32);
                    draw_hollow_rect_mut(img, thick_rect, blue);
                }

                verbose_println(
                    self.config.verbose,
                    &format!(
                        "  🔵 Global box: ({}, {}, {}, {})",
                        gx_min, gy_min, gx_max, gy_max
                    ),
                );
            }

            // 3. Draw center point as a cross
            let center_x = detection_result.center.0;
            let center_y = detection_result.center.1;

            // Draw a larger, more visible cross (20x20)
            for i in 0..20 {
                // Horizontal line
                if center_x >= 10 && center_x + 10 < img_width && center_y < img_height {
                    img.put_pixel(center_x - 10 + i, center_y, green);
                }
                // Vertical line
                if center_y >= 10 && center_y + 10 < img_height && center_x < img_width {
                    img.put_pixel(center_x, center_y - 10 + i, green);
                }
            }

            verbose_println(
                self.config.verbose,
                &format!("  ✚ Subject center: ({}, {})", center_x, center_y),
            );
        }

        // 4. Draw crop area (RED) - EXACT same logic as actual processing
        let (crop_x, crop_y, crop_width, crop_height) =
            self.calculate_actual_crop_area(img_width, img_height, detection_result)?;

        //let crop_rect = Rect::at(crop_x as i32, crop_y as i32).of_size(crop_width, crop_height);

        for offset in 0..6 {
            let _offset = offset - 3; // Center the thickness
            let thick_crop_rect = Rect::at((crop_x as i32) - _offset, (crop_y as i32) - _offset)
                .of_size(
                    (crop_width as i32 + (_offset * 2)) as u32,
                    (crop_height as i32 + (_offset * 2)) as u32,
                );
            draw_hollow_rect_mut(img, thick_crop_rect, red);
        }

        verbose_println(
            self.config.verbose,
            &format!(
                "  🔴 Crop area: ({}, {}, {}, {})",
                crop_x,
                crop_y,
                crop_x + crop_width,
                crop_y + crop_height
            ),
        );

        Ok(())
    }

    /// Calculate the exact crop area that would be used in actual processing
    /// This replicates the logic from smart_resize_with_people_detection
    fn calculate_actual_crop_area(
        &self,
        src_width: u32,
        src_height: u32,
        detection_result: &subject_detection::SubjectDetectionResult,
    ) -> Result<(u32, u32, u32, u32)> {
        // Determine if image is portrait for auto-process consideration
        let image_is_portrait = src_height > src_width;

        // Get effective target dimensions (maybe swapped in auto mode)
        let (eff_width, eff_height) = get_effective_target_dimensions(
            image_is_portrait,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_orientation,
        );

        // Calculate crop dimensions to maintain aspect ratio
        let target_aspect = eff_width as f64 / eff_height as f64;
        let source_aspect = src_width as f64 / src_height as f64;

        let (crop_width, crop_height) = if source_aspect > target_aspect {
            // Source is wider - crop width
            let new_width = (src_height as f64 * target_aspect) as u32;
            (new_width.min(src_width), src_height)
        } else {
            // Source is taller - crop height
            let new_height = (src_width as f64 / target_aspect) as u32;
            (src_width, new_height.min(src_height))
        };

        // Calculate crop offset - use people detection if available
        let (crop_x, crop_y) = if detection_result.person_count > 0 {
            // Use people-aware cropping
            resize::calculate_people_aware_crop_offset(
                src_width,
                src_height,
                crop_width,
                crop_height,
                detection_result,
            )
        } else {
            // No people detected, use standard center cropping
            resize::standard_crop_offset(src_width, src_height, crop_width, crop_height)
        };

        Ok((crop_x, crop_y, crop_width, crop_height))
    }

    /// Draw a text label with background for visibility
    fn draw_label(
        &self,
        img: &mut RgbImage,
        x: u32,
        y: u32,
        text: &str,
        color: Rgb<u8>,
    ) -> Result<()> {
        // Use simple pixel-based text representation
        self.draw_simple_text(img, x, y, text, color);
        Ok(())
    }

    /// Simple pixel-based text drawing with actual readable characters
    fn draw_simple_text(&self, img: &mut RgbImage, x: u32, y: u32, text: &str, color: Rgb<u8>) {
        let char_width = 6u32;
        let char_height = 8u32;
        let text_width = (text.len() * char_width as usize) as u32;

        let (img_width, img_height) = img.dimensions();

        // Draw black background
        for dy in 0..char_height {
            for dx in 0..text_width {
                let px = x + dx;
                let py = y.saturating_sub(char_height) + dy;
                if px < img_width && py < img_height {
                    img.put_pixel(px, py, Rgb([0u8, 0u8, 0u8])); // Black background
                }
            }
        }

        // Draw each character
        for (i, ch) in text.chars().enumerate() {
            let char_x = x + (i * char_width as usize) as u32;
            let char_y = y.saturating_sub(char_height);
            self.draw_char(img, char_x, char_y, ch, color);
        }
    }

    /// Draw a single character using a simple 5x7 pixel font
    fn draw_char(&self, img: &mut RgbImage, x: u32, y: u32, ch: char, color: Rgb<u8>) {
        let (img_width, img_height) = img.dimensions();

        // Simple 5x7 bitmap font patterns (each u32 represents a row, bits 0-4 are pixels)
        let pattern = match ch {
            'p' | 'P' => [
                0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000,
            ], // P
            'e' | 'E' => [
                0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111,
            ], // E
            'r' | 'R' => [
                0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001,
            ], // R
            's' | 'S' => [
                0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11111,
            ], // S
            'o' | 'O' => [
                0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110,
            ], // O
            'n' | 'N' => [
                0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001,
            ], // N
            '0' => [
                0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110,
            ], // 0
            '1' => [
                0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110,
            ], // 1
            '2' => [
                0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111,
            ], // 2
            '3' => [
                0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110,
            ], // 3
            '4' => [
                0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010,
            ], // 4
            '5' => [
                0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110,
            ], // 5
            '6' => [
                0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110,
            ], // 6
            '7' => [
                0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000,
            ], // 7
            '8' => [
                0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110,
            ], // 8
            '9' => [
                0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110,
            ], // 9
            '%' => [
                0b11001, 0b11010, 0b00100, 0b01000, 0b10000, 0b01011, 0b10011,
            ], // %
            ' ' => [
                0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000,
            ], // space
            _ => [
                0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110,
            ], // default (O)
        };

        // Draw the character pixel by pixel
        for (row, &pattern_row) in pattern.iter().enumerate() {
            for col in 0..5 {
                if pattern_row & (1 << (4 - col)) != 0 {
                    let px = x + col;
                    let py = y + row as u32;
                    if px < img_width && py < img_height {
                        img.put_pixel(px, py, color);
                    }
                }
            }
        }
    }
}

#[derive(Debug)]
pub struct ProcessingResult {
    #[allow(dead_code)]
    pub input_path: PathBuf,
    #[allow(dead_code)]
    pub output_paths: std::collections::HashMap<crate::cli::OutputType, PathBuf>,
    #[allow(dead_code)]
    pub image_type: ImageType,
    #[allow(dead_code)]
    pub processing_time: std::time::Duration,
    #[allow(dead_code)]
    pub people_detected: bool,
    #[allow(dead_code)]
    pub people_count: usize,
    // For combined portraits, store individual results
    #[allow(dead_code)]
    pub individual_portraits: Option<Vec<IndividualPortraitResult>>,
}

#[derive(Debug)]
pub struct IndividualPortraitResult {
    #[allow(dead_code)]
    pub path: PathBuf,
    #[allow(dead_code)]
    pub people_detected: bool,
    #[allow(dead_code)]
    pub people_count: usize,
    #[allow(dead_code)]
    pub confidence: f32,
}

#[derive(Debug)]
pub enum ImageType {
    Landscape,
    Portrait,
    #[allow(dead_code)]
    CombinedPortrait,
    #[allow(dead_code)]
    CombinedLandscape,
}
