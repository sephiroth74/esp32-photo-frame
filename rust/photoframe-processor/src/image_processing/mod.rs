pub mod binary;
pub mod combine;
pub mod convert;
pub mod orientation;
pub mod resize;
pub mod annotate;
pub mod batch;
pub mod subject_detection;

use anyhow::{Context, Result};
use image::{Rgb, RgbImage};
use imageproc::drawing::draw_hollow_rect_mut;
use imageproc::rect::Rect;
use indicatif::ProgressBar;
use rayon::prelude::*;
use std::collections::HashSet;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use walkdir::WalkDir;

use crate::utils::{create_combined_portrait_filename, generate_filename_hash, has_valid_extension, verbose_println};
use combine::combine_processed_portraits;
use orientation::get_effective_target_dimensions;

#[derive(Debug, Clone)]
pub enum ProcessingType {
    BlackWhite,
    SixColor,
    SevenColor,
}

#[derive(Debug, Clone)]
pub struct ProcessingConfig {
    pub processing_type: ProcessingType,
    pub target_width: u32,
    pub target_height: u32,
    pub auto_process: bool,
    pub font_name: String,
    pub font_size: f32,
    pub annotation_background: String,
    pub extensions: Vec<String>,
    pub verbose: bool,
    pub parallel_jobs: usize,
    pub output_format: crate::cli::OutputType,
    // Python people detection configuration
    pub detect_people: bool,
    pub python_script_path: Option<PathBuf>,
    // Duplicate handling
    pub force: bool,
    // Debug mode
    pub debug: bool,
    // Annotation control
    pub annotate: bool,
}

pub struct ProcessingEngine {
    config: ProcessingConfig,
    subject_detector: Option<subject_detection::SubjectDetector>,
}

impl ProcessingEngine {
    pub fn new(config: ProcessingConfig) -> Result<Self> {
        // Initialize thread pool with specified number of jobs
        rayon::ThreadPoolBuilder::new()
            .num_threads(config.parallel_jobs)
            .build_global()
            .context("Failed to initialize thread pool")?;

        let subject_detector = if config.detect_people {
            if let Some(ref script_path) = config.python_script_path {
                verbose_println(config.verbose, "Initializing Python people detection...");
                verbose_println(config.verbose, &format!("Using script: {}", script_path.display()));
                match subject_detection::create_default_detector(script_path) {
                    Ok(detector) => {
                        verbose_println(config.verbose, "✓ Python people detection initialized successfully");
                        Some(detector)
                    }
                    Err(e) => {
                        verbose_println(config.verbose, &format!("⚠ Failed to initialize Python detection: {}", e));
                        verbose_println(config.verbose, "  Continuing without people detection");
                        None
                    }
                }
            } else {
                verbose_println(config.verbose, "⚠ People detection requested but no Python script path provided");
                verbose_println(config.verbose, "  Use --python-script to specify the path to find_subject.py");
                None
            }
        } else {
            None
        };

        Ok(Self { 
            config,
            subject_detector,
        })
    }

    /// Scan destination folder for existing files and create skip mapping
    fn scan_existing_outputs(&self, output_dir: &Path) -> Result<(HashSet<String>, HashSet<String>)> {
        let mut existing_landscape_hashes = HashSet::new();
        let mut existing_portrait_hashes = HashSet::new();
        
        if !output_dir.exists() {
            return Ok((existing_landscape_hashes, existing_portrait_hashes));
        }
        
        for entry in std::fs::read_dir(output_dir)? {
            let entry = entry?;
            let path = entry.path();
            
            if let Some(filename) = path.file_name().and_then(|f| f.to_str()) {
                // Check for BMP or BIN files
                if filename.ends_with(".bmp") || filename.ends_with(".bin") {
                    let stem = filename.rsplit_once('.').map(|(s, _)| s).unwrap_or(filename);
                    
                    if stem.starts_with("combined_") {
                        // Parse combined portrait filenames: "combined_{hash1}_{hash2}"
                        if let Some(hashes_part) = stem.strip_prefix("combined_") {
                            // Split by underscore - with hashes, this is much simpler since hashes don't contain underscores
                            if let Some(underscore_pos) = hashes_part.find('_') {
                                let (hash1, hash2) = hashes_part.split_at(underscore_pos);
                                let hash2 = &hash2[1..]; // Remove the leading underscore
                                
                                // Check that these look like valid hashes (8 hex characters)
                                if hash1.len() == 8 && hash2.len() == 8 && 
                                   hash1.chars().all(|c| c.is_ascii_hexdigit()) &&
                                   hash2.chars().all(|c| c.is_ascii_hexdigit()) {
                                    existing_portrait_hashes.insert(hash1.to_string());
                                    existing_portrait_hashes.insert(hash2.to_string());
                                    
                                    verbose_println(self.config.verbose, &format!(
                                        "Found existing combined portrait: {} (contains hashes {} and {})", 
                                        filename, hash1, hash2
                                    ));
                                }
                            }
                        }
                    } else {
                        // Regular landscape image - check if it looks like a hash
                        if stem.len() == 8 && stem.chars().all(|c| c.is_ascii_hexdigit()) {
                            existing_landscape_hashes.insert(stem.to_string());
                            verbose_println(self.config.verbose, &format!(
                                "Found existing landscape: {} (hash: {})", filename, stem
                            ));
                        } else {
                            verbose_println(self.config.verbose, &format!(
                                "Skipping file with non-hash filename: {}", filename
                            ));
                        }
                    }
                }
            }
        }
        
        Ok((existing_landscape_hashes, existing_portrait_hashes))
    }

    /// Discover all image files in the input paths (directories or individual files)
    pub fn discover_images(&self, input_paths: &[PathBuf]) -> Result<Vec<PathBuf>> {
        let mut image_files = Vec::new();

        for input_path in input_paths {
            if input_path.is_file() {
                // Handle single file
                if has_valid_extension(input_path, &self.config.extensions) {
                    verbose_println(self.config.verbose, &format!("Adding file: {}", input_path.display()));
                    image_files.push(input_path.clone());
                } else {
                    println!("Warning: File {} has unsupported extension", input_path.display());
                }
            } else if input_path.is_dir() {
                // Handle directory
                verbose_println(self.config.verbose, &format!("Scanning directory: {}", input_path.display()));

                let walker = WalkDir::new(input_path)
                    .follow_links(false)
                    .max_depth(10); // Reasonable depth limit

                for entry in walker {
                    let entry = entry.context("Failed to read directory entry")?;
                    let path = entry.path();

                    if path.is_file() && has_valid_extension(path, &self.config.extensions) {
                        image_files.push(path.to_path_buf());
                    }
                }
            } else {
                return Err(anyhow::anyhow!("Input path does not exist: {}", input_path.display()));
            }
        }

        // Sort for consistent processing order
        image_files.sort();
        
        verbose_println(self.config.verbose, &format!("Found {} image files", image_files.len()));
        Ok(image_files)
    }

    /// Filter out images that already exist in the output directory (unless --force is used)
    /// Returns (images_to_process, skipped_results)
    pub fn filter_existing_images(&self, image_files: &[PathBuf], output_dir: &Path) -> Result<(Vec<PathBuf>, Vec<SkippedResult>)> {
        if self.config.force {
            verbose_println(self.config.verbose, "Force mode enabled - processing all images regardless of existing files");
            return Ok((image_files.to_vec(), vec![]));
        }

        let (existing_landscape_hashes, existing_portrait_hashes) = self.scan_existing_outputs(output_dir)?;
        let mut images_to_process = Vec::new();
        let mut skipped_results = Vec::new();

        for image_path in image_files {
            let stem = image_path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("unknown");
            
            let filename_hash = generate_filename_hash(stem);

            // Check if this image should be skipped
            let should_skip = if existing_landscape_hashes.contains(&filename_hash) {
                // Landscape image already exists
                let existing_bmp = output_dir.join(format!("{}.bmp", filename_hash));
                let existing_bin = output_dir.join(format!("{}.bin", filename_hash));
                let existing_path = if existing_bmp.exists() { existing_bmp } else { existing_bin };
                
                skipped_results.push(SkippedResult {
                    input_path: image_path.clone(),
                    reason: SkipReason::LandscapeExists,
                    existing_output_path: existing_path,
                });
                true
            } else if existing_portrait_hashes.contains(&filename_hash) {
                // Portrait image is already part of a combined image
                // Find the combined file that contains this portrait hash
                let mut combined_path = None;
                for entry in std::fs::read_dir(output_dir)? {
                    let entry = entry?;
                    let filename = entry.file_name();
                    let filename_str = filename.to_string_lossy();
                    
                    if (filename_str.ends_with(".bmp") || filename_str.ends_with(".bin")) && 
                       filename_str.starts_with("combined_") && 
                       filename_str.contains(&filename_hash) {
                        combined_path = Some(entry.path());
                        break;
                    }
                }
                
                if let Some(path) = combined_path {
                    skipped_results.push(SkippedResult {
                        input_path: image_path.clone(),
                        reason: SkipReason::PortraitInCombined,
                        existing_output_path: path,
                    });
                    true
                } else {
                    false // Shouldn't happen, but be safe
                }
            } else {
                false
            };

            if !should_skip {
                images_to_process.push(image_path.clone());
            }
        }

        let skipped_count = skipped_results.len();
        let processing_count = images_to_process.len();
        
        if skipped_count > 0 {
            verbose_println(self.config.verbose, &format!(
                "Duplicate check: {} images to process, {} skipped (already exist)", 
                processing_count, skipped_count
            ));
        } else {
            verbose_println(self.config.verbose, &format!(
                "Duplicate check: {} images to process, no duplicates found", 
                processing_count
            ));
        }

        Ok((images_to_process, skipped_results))
    }


    /// Process a batch of images with smart portrait pairing and multi-progress support
    /// Returns (processing_outcomes, skipped_results)
    pub fn process_batch_with_smart_pairing(
        &self,
        image_files: &[PathBuf],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<(Vec<Result<ProcessingResult>>, Vec<SkippedResult>)> {
        completion_progress.set_message("Checking for duplicate files...");
        
        // Filter out images that already exist (unless --force is used)
        let (images_to_process, skipped_results) = self.filter_existing_images(image_files, output_dir)?;
        
        if images_to_process.is_empty() {
            completion_progress.set_message("No images to process");
            return Ok((vec![], skipped_results));
        }
        
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
                    verbose_println(self.config.verbose, &format!("⚠ Failed to detect orientation for {}: {}", path.display(), e));
                    // Default to landscape processing for problematic files
                    landscape_files.push(path.clone());
                }
            }
        }
        
        verbose_println(self.config.verbose, &format!(
            "Separated {} portrait images and {} landscape images", 
            portrait_files.len(), landscape_files.len()
        ));
        
        let mut all_results = Vec::new();
        
        // Process landscape images (each increments main progress by 1)
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
        
        // Process portrait images in pairs
        if !portrait_files.is_empty() {
            completion_progress.set_message("Processing portrait pairs...");
            // Don't reset main_progress position - let it continue from where landscapes left off
            
            let portrait_results = self.process_portrait_pairs_with_progress(
                &portrait_files,
                output_dir,
                main_progress,
                thread_progress_bars,
                completion_progress,
            )?;
            all_results.extend(portrait_results);
        }
        
        // Main progress should already be at 100% from incremental updates
        completion_progress.set_message("Batch processing complete");
        
        Ok((all_results, skipped_results))
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
        use std::sync::atomic::{AtomicUsize, Ordering};
        use std::collections::HashMap;
        
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
                        let pb_index = next_thread_id.fetch_add(1, Ordering::Relaxed) % thread_progress_bars.len();
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
                let result = self.process_single_image_with_progress(
                    image_path, 
                    output_dir, 
                    index,
                    thread_pb
                );
                
                // Update main progress
                let count = processed_count.fetch_add(1, Ordering::Relaxed) + 1;
                main_progress.inc(1);
                main_progress.set_message(format!("Completed: {}/{}", count, image_files.len()));
                
                // Update completion progress
                let progress_percent = (count as f64 / image_files.len() as f64) * 100.0;
                completion_progress.set_position(progress_percent as u64);
                completion_progress.set_message(format!("Processing: {}/{}", count, image_files.len()));
                
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
        let filename = input_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");
        
        verbose_println(self.config.verbose, &format!("Processing: {}", input_path.display()));

        // Stage 1: Load image (10%)
        progress_bar.set_position(10);
        progress_bar.set_message(format!("{} - Loading", filename));
        let img = image::open(input_path)
            .with_context(|| format!("Failed to open image: {}", input_path.display()))?;

        // Stage 2: Convert to RGB (20%)
        progress_bar.set_position(20);
        progress_bar.set_message(format!("{} - Converting to RGB", filename));
        let rgb_img = img.to_rgb8();

        // Stage 3: Detect orientation (30%)
        progress_bar.set_position(30);
        progress_bar.set_message(format!("{} - Detecting orientation", filename));
        let orientation_info = orientation::detect_orientation(input_path, &rgb_img)?;
        
        verbose_println(self.config.verbose, &format!(
            "Image orientation: {:?}, is_portrait: {}",
            orientation_info.exif_orientation, orientation_info.is_portrait
        ));

        // Stage 4: Apply rotation (40%)
        progress_bar.set_position(40);
        progress_bar.set_message(format!("{} - Applying rotation", filename));
        let rotated_img = orientation::apply_rotation(&rgb_img, orientation_info.exif_orientation)?;

        // Stage 5: People detection (45%)
        progress_bar.set_position(45);
        progress_bar.set_message(format!("{} - Detecting people", filename));
        let people_detection = self.detect_people_with_logging(&rotated_img, input_path);

        // Stage 6: Process based on orientation (50-100%)
        progress_bar.set_position(50);
        let is_portrait = orientation_info.is_portrait;
        let processing_result = if is_portrait {
            progress_bar.set_message(format!("{} - Processing portrait", filename));
            self.process_portrait_image_with_progress(&rotated_img, input_path, output_dir, index, progress_bar, people_detection, start_time)?
        } else {
            progress_bar.set_message(format!("{} - Processing landscape", filename));
            self.process_landscape_image_with_progress(&rotated_img, input_path, output_dir, index, progress_bar, people_detection, start_time)?
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
    ) -> Result<ProcessingResult> {
        let filename = input_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        // Resize to target dimensions (60-65%)
        progress_bar.set_position(60);
        if people_detection.is_some() && people_detection.as_ref().unwrap().person_count > 0 {
            progress_bar.set_message(format!("{} - Smart resizing (people-aware)", filename));
            verbose_println(self.config.verbose, "🎯 Applying people-aware smart cropping");
        } else {
            progress_bar.set_message(format!("{} - Resizing", filename));
        }
        
        progress_bar.set_position(62); // Show progress during resize
        let resized_img = resize::smart_resize_with_people_detection(
            img,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_process,
            people_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(65); // Resize complete

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

        // Apply color conversion and dithering (80-85%)
        progress_bar.set_position(80);
        progress_bar.set_message(format!("{} - Color processing", filename));
        progress_bar.set_position(82); // Show progress during color processing
        let processed_img = convert::process_image(&annotated_img, &self.config.processing_type)?;
        progress_bar.set_position(85); // Color processing complete

        // Generate output filenames in the same directory
        let output_filename = match &self.config.output_format {
            crate::cli::OutputType::Bmp => crate::utils::create_output_filename(input_path, None, "bmp"),
            crate::cli::OutputType::Bin => crate::utils::create_output_filename(input_path, None, "bin"),
            crate::cli::OutputType::Jpg => crate::utils::create_output_filename(input_path, None, "jpg"),
            crate::cli::OutputType::Png => crate::utils::create_output_filename(input_path, None, "png"),
        };
        let bin_filename = crate::utils::create_output_filename(input_path, None, "bin");

        let output_path = output_dir.join(&output_filename);
        let bin_path = output_dir.join(&bin_filename);

        // Save files based on output format configuration (90-95%)
        match &self.config.output_format {
            crate::cli::OutputType::Bmp => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving BMP", filename));
                processed_img.save(&output_path)
                    .with_context(|| format!("Failed to save BMP: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Bin => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Generating binary", filename));
                progress_bar.set_position(92);
                let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
                progress_bar.set_position(94);
                std::fs::write(&bin_path, binary_data)
                    .with_context(|| format!("Failed to save binary: {}", bin_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Jpg => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving JPG", filename));
                processed_img.save(&output_path)
                    .with_context(|| format!("Failed to save JPG: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Png => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving PNG", filename));
                processed_img.save(&output_path)
                    .with_context(|| format!("Failed to save PNG: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
        }

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path: output_path.clone(),
            bin_path,
            image_type: ImageType::Landscape,
            processing_time: start_time.elapsed(),
            people_detected: people_detection.as_ref().map_or(false, |d| d.person_count > 0),
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
    ) -> Result<ProcessingResult> {
        let filename = input_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        // Process portrait as individual half-width image for now
        // TODO: Implement proper portrait pairing and combination logic
        progress_bar.set_position(60);
        if people_detection.is_some() && people_detection.as_ref().unwrap().person_count > 0 {
            progress_bar.set_message(format!("{} - Portrait resizing (people-aware)", filename));
            verbose_println(self.config.verbose, "🎯 Applying people-aware portrait cropping");
        } else {
            progress_bar.set_message(format!("{} - Portrait resizing", filename));
        }
        
        // For now, process as half-width landscape to fit the display
        let half_width = self.config.target_width / 2;
        
        progress_bar.set_position(62); // Show progress during resize
        let resized_img = resize::smart_resize_with_people_detection(
            img,
            half_width,
            self.config.target_height,
            self.config.auto_process,
            people_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(65); // Resize complete

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

        // Apply color conversion and dithering (80%)
        progress_bar.set_position(80);
        progress_bar.set_message(format!("{} - Color processing", filename));
        let processed_img = convert::process_image(&annotated_img, &self.config.processing_type)?;

        // Generate output filenames with portrait suffix
        let output_filename = match &self.config.output_format {
            crate::cli::OutputType::Bmp => crate::utils::create_output_filename(input_path, Some("portrait"), "bmp"),
            crate::cli::OutputType::Bin => crate::utils::create_output_filename(input_path, Some("portrait"), "bin"),
            crate::cli::OutputType::Jpg => crate::utils::create_output_filename(input_path, Some("portrait"), "jpg"),
            crate::cli::OutputType::Png => crate::utils::create_output_filename(input_path, Some("portrait"), "png"),
        };
        let bin_filename = crate::utils::create_output_filename(input_path, Some("portrait"), "bin");

        let output_path = output_dir.join(&output_filename);
        let bin_path = output_dir.join(&bin_filename);

        // Save files based on output format configuration (90-95%)
        match &self.config.output_format {
            crate::cli::OutputType::Bmp => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving portrait BMP", filename));
                processed_img.save(&output_path)
                    .with_context(|| format!("Failed to save portrait BMP: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Bin => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Generating portrait binary", filename));
                progress_bar.set_position(92);
                let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
                progress_bar.set_position(94);
                progress_bar.set_message(format!("{} - Saving portrait binary", filename));
                std::fs::write(&bin_path, binary_data)
                    .with_context(|| format!("Failed to save portrait binary: {}", bin_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Jpg => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving portrait JPG", filename));
                processed_img.save(&output_path)
                    .with_context(|| format!("Failed to save portrait JPG: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Png => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving portrait PNG", filename));
                processed_img.save(&output_path)
                    .with_context(|| format!("Failed to save portrait PNG: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
        }

        verbose_println(self.config.verbose, 
            &"ℹ️ Portrait processed as individual half-width image. Future enhancement will combine portraits in pairs.".to_string());

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path: output_path.clone(),
            bin_path,
            image_type: ImageType::Portrait,
            processing_time: start_time.elapsed(),
            people_detected: people_detection.as_ref().map_or(false, |d| d.person_count > 0),
            people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
            individual_portraits: None,
        })
    }

    /// Detect people in an image and return detection result with verbose logging
    fn detect_people_with_logging(&self, img: &RgbImage, input_path: &Path) -> Option<subject_detection::SubjectDetectionResult> {
        if let Some(ref detector) = self.subject_detector {
            verbose_println(self.config.verbose, &format!("🔍 Running people detection on: {}", 
                input_path.file_name().unwrap_or_default().to_string_lossy()));
            
            match detector.detect_people(img) {
                Ok(result) => {
                    verbose_println(self.config.verbose, "  ✅ Python detection completed");
                    
                    if self.config.verbose {
                        println!("📊 People Detection Results:");
                        println!("   • People found: {}", result.person_count);
                        
                        if result.person_count > 0 {
                            println!("   • Highest confidence: {:.2}%", result.confidence * 100.0);
                            println!("   • Detection center: ({}, {})", result.center.0, result.center.1);
                            println!("   • Offset from image center: ({:+}, {:+})", 
                                result.offset_from_center.0, result.offset_from_center.1);
                            
                            // Calculate offset percentage for better understanding
                            let (img_width, img_height) = img.dimensions();
                            let offset_x_pct = (result.offset_from_center.0 as f32 / img_width as f32) * 100.0;
                            let offset_y_pct = (result.offset_from_center.1 as f32 / img_height as f32) * 100.0;
                            println!("   • Offset percentage: ({:+.1}%, {:+.1}%)", offset_x_pct, offset_y_pct);
                            
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
                    verbose_println(self.config.verbose, &format!("❌ People detection failed: {}", e));
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

        verbose_println(self.config.verbose, &format!("Processing {} portrait images", portrait_files.len()));

        // Randomize portrait order for more interesting combinations
        let mut portrait_files = portrait_files.to_vec();
        use rand::seq::SliceRandom;
        let mut rng = rand::rng();
        portrait_files.shuffle(&mut rng);
        
        // If odd number of portraits, discard the last one
        if portrait_files.len() % 2 == 1 {
            let discarded = portrait_files.pop().unwrap();
            verbose_println(self.config.verbose, &format!("ℹ Discarding leftover portrait: {}", 
                discarded.file_name().unwrap_or_default().to_string_lossy()));
        }

        // Create pairs from randomized portraits
        let portrait_pairs: Vec<(PathBuf, PathBuf)> = portrait_files
            .chunks_exact(2)
            .map(|chunk| (chunk[0].clone(), chunk[1].clone()))
            .collect();
            
        verbose_println(self.config.verbose, &format!("Created {} portrait pairs", portrait_pairs.len()));

        completion_progress.set_message("Processing portrait pairs in parallel...");
        
        // Process each pair individually using the thread pool (just like landscapes)
        let results = self.process_portrait_pairs_parallel(
            &portrait_pairs,
            output_dir,
            main_progress,
            thread_progress_bars,
            completion_progress,
        )?;

        completion_progress.set_message("Portrait pairing complete");
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
        use std::sync::atomic::{AtomicUsize, Ordering};
        use std::collections::HashMap;
        
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
                        let pb_index = next_thread_id.fetch_add(1, Ordering::Relaxed) % thread_progress_bars.len();
                        assignment.insert(current_thread_id, pb_index);
                        pb_index
                    }
                };
                
                let thread_pb = &thread_progress_bars[pb_index];
                
                // Update thread progress
                let left_filename = left_path.file_name().and_then(|f| f.to_str()).unwrap_or("unknown");
                let right_filename = right_path.file_name().and_then(|f| f.to_str()).unwrap_or("unknown");
                thread_pb.set_message(format!("Processing {} + {}", left_filename, right_filename));
                
                // Process the portrait pair
                let result = self.process_single_portrait_pair_with_progress(
                    left_path, 
                    right_path,
                    output_dir, 
                    index,
                    thread_pb
                );
                
                // Update main progress - increment by 2 since we processed 2 images
                let pairs_processed = processed_count.fetch_add(1, Ordering::Relaxed) + 1;
                main_progress.inc(2); // Each pair processes 2 images
                let images_processed = pairs_processed * 2;
                let total_portrait_images = portrait_pairs.len() * 2;
                main_progress.set_message(format!("Completed: {}/{}", images_processed, total_portrait_images));
                
                // Update completion progress
                let progress_percent = (pairs_processed as f64 / portrait_pairs.len() as f64) * 100.0;
                completion_progress.set_position(progress_percent as u64);
                completion_progress.set_message(format!("Processing pairs: {}/{} ({} images)", 
                    pairs_processed, portrait_pairs.len(), images_processed));
                
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
        let left_filename = left_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");
        let right_filename = right_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");
        
        verbose_println(self.config.verbose, &format!("Processing portrait pair: {} + {}", left_filename, right_filename));

        // Stage 1: Load both images (10%)
        progress_bar.set_position(10);
        progress_bar.set_message(format!("Loading {} + {}", left_filename, right_filename));
        
        let left_img = image::open(left_path)
            .with_context(|| format!("Failed to open left image: {}", left_path.display()))?
            .to_rgb8();
        let right_img = image::open(right_path)
            .with_context(|| format!("Failed to open right image: {}", right_path.display()))?
            .to_rgb8();

        // Stage 2: Apply EXIF rotation (20%)
        progress_bar.set_position(20);
        progress_bar.set_message(format!("Applying rotation to {} + {}", left_filename, right_filename));
        
        let left_rotated = match orientation::detect_orientation(left_path, &left_img) {
            Ok(orientation_info) => {
                orientation::apply_rotation(&left_img, orientation_info.exif_orientation)
                    .unwrap_or_else(|_| left_img.clone())
            }
            Err(_) => left_img.clone(),
        };
        
        let right_rotated = match orientation::detect_orientation(right_path, &right_img) {
            Ok(orientation_info) => {
                orientation::apply_rotation(&right_img, orientation_info.exif_orientation)
                    .unwrap_or_else(|_| right_img.clone())
            }
            Err(_) => right_img.clone(),
        };

        // Stage 3: People detection (30%)
        progress_bar.set_position(30);
        progress_bar.set_message(format!("Detecting people in {} + {}", left_filename, right_filename));
        
        let left_detection = if self.subject_detector.is_some() {
            self.detect_people_with_logging(&left_rotated, left_path)
        } else {
            None
        };
        
        let right_detection = if self.subject_detector.is_some() {
            self.detect_people_with_logging(&right_rotated, right_path)
        } else {
            None
        };

        // Stage 4: Resize to half-width (40-50%)
        progress_bar.set_position(40);
        progress_bar.set_message(format!("Resizing {} + {}", left_filename, right_filename));
        
        let half_width = self.config.target_width / 2;
        
        progress_bar.set_position(42); // Show progress during left resize
        let left_resized = resize::smart_resize_with_people_detection(
            &left_rotated,
            half_width,
            self.config.target_height,
            self.config.auto_process,
            left_detection.as_ref(),
            self.config.verbose,
        )?;
        
        progress_bar.set_position(46); // Show progress during right resize
        let right_resized = resize::smart_resize_with_people_detection(
            &right_rotated,
            half_width,
            self.config.target_height,
            self.config.auto_process,
            right_detection.as_ref(),
            self.config.verbose,
        )?;
        progress_bar.set_position(50); // Both resizes complete

        // Stage 5: Apply image processing (50-55%)
        progress_bar.set_position(50);
        progress_bar.set_message(format!("Processing colors for {} + {}", left_filename, right_filename));
        
        progress_bar.set_position(52); // Processing left image colors
        let left_processed = convert::process_image(&left_resized, &self.config.processing_type)?;
        progress_bar.set_position(54); // Processing right image colors  
        let right_processed = convert::process_image(&right_resized, &self.config.processing_type)?;
        progress_bar.set_position(55); // Color processing complete

        // Stage 6: Apply annotations (60-65%) - conditional based on config
        progress_bar.set_position(60);
        let (left_annotated, right_annotated) = if self.config.annotate {
            progress_bar.set_message(format!("Adding annotations to {} + {}", left_filename, right_filename));
            
            progress_bar.set_position(62); // Annotating left image
            let left_annotated = annotate::add_filename_annotation(
                &left_processed,
                left_path,
                &self.config.font_name,
                self.config.font_size,
                &self.config.annotation_background,
            )?;
            
            progress_bar.set_position(64); // Annotating right image
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
            progress_bar.set_message(format!("Skipping annotations for {} + {}", left_filename, right_filename));
            progress_bar.set_position(65);
            (left_processed, right_processed)
        };

        // Stage 7: Combine images (70-75%)
        progress_bar.set_position(70);
        progress_bar.set_message(format!("Combining {} + {}", left_filename, right_filename));
        
        progress_bar.set_position(72); // Show progress during combining
        let combined_img = combine_processed_portraits(&left_annotated, &right_annotated, self.config.target_width, self.config.target_height)?;
        progress_bar.set_position(75); // Combining complete

        // Stage 8: Save final result (80-90%)
        progress_bar.set_position(80);
        progress_bar.set_message(format!("Saving combined image"));
        
        // Generate combined filename
        let output_filename = match &self.config.output_format {
            crate::cli::OutputType::Bmp => create_combined_portrait_filename(left_path, right_path, "bmp"),
            crate::cli::OutputType::Bin => create_combined_portrait_filename(left_path, right_path, "bin"),
            crate::cli::OutputType::Jpg => create_combined_portrait_filename(left_path, right_path, "jpg"),
            crate::cli::OutputType::Png => create_combined_portrait_filename(left_path, right_path, "png"),
        };
        let bin_filename = create_combined_portrait_filename(left_path, right_path, "bin");

        let output_path = output_dir.join(&output_filename);
        let bin_path = output_dir.join(&bin_filename);

        // Save based on output format
        match self.config.output_format {
            crate::cli::OutputType::Bmp => {
                progress_bar.set_position(85);
                progress_bar.set_message(format!("Saving BMP: {} + {}", left_filename, right_filename));
                combined_img.save(&output_path)
                    .with_context(|| format!("Failed to save combined BMP: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Bin => {
                progress_bar.set_position(85);
                progress_bar.set_message(format!("Converting to binary: {} + {}", left_filename, right_filename));
                let binary_data = binary::convert_to_esp32_binary(&combined_img)?;
                progress_bar.set_position(90);
                progress_bar.set_message(format!("Saving binary: {} + {}", left_filename, right_filename));
                std::fs::write(&bin_path, &binary_data)
                    .with_context(|| format!("Failed to save combined binary: {}", bin_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Jpg => {
                progress_bar.set_position(85);
                progress_bar.set_message(format!("Saving JPG: {} + {}", left_filename, right_filename));
                combined_img.save(&output_path)
                    .with_context(|| format!("Failed to save combined JPG: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
            crate::cli::OutputType::Png => {
                progress_bar.set_position(85);
                progress_bar.set_message(format!("Saving PNG: {} + {}", left_filename, right_filename));
                combined_img.save(&output_path)
                    .with_context(|| format!("Failed to save combined PNG: {}", output_path.display()))?;
                progress_bar.set_position(95);
            }
        }

        progress_bar.set_position(100);
        progress_bar.set_message(format!("Completed {} + {}", left_filename, right_filename));
        
        verbose_println(self.config.verbose, 
            &format!("✅ Portrait pair completed in {:.2}s", start_time.elapsed().as_secs_f32()));

        // Create individual portrait results for the summary
        let individual_portraits = vec![
            IndividualPortraitResult {
                path: left_path.to_path_buf(),
                people_detected: left_detection.as_ref().map_or(false, |d| d.person_count > 0),
                people_count: left_detection.as_ref().map_or(0, |d| d.person_count),
                confidence: left_detection.as_ref().map_or(0.0, |d| d.confidence),
            },
            IndividualPortraitResult {
                path: right_path.to_path_buf(),
                people_detected: right_detection.as_ref().map_or(false, |d| d.person_count > 0),
                people_count: right_detection.as_ref().map_or(0, |d| d.person_count),
                confidence: right_detection.as_ref().map_or(0.0, |d| d.confidence),
            },
        ];

        // Calculate combined statistics
        let combined_people_detected = individual_portraits.iter().any(|p| p.people_detected);
        let combined_people_count = individual_portraits.iter().map(|p| p.people_count).sum();

        Ok(ProcessingResult {
            input_path: left_path.to_path_buf(), // Use first image as representative
            bmp_path: output_path.clone(),
            bin_path,
            image_type: ImageType::CombinedPortrait,
            processing_time: start_time.elapsed(),
            people_detected: combined_people_detected,
            people_count: combined_people_count,
            individual_portraits: Some(individual_portraits),
        })
    }

    /// Process images in debug mode - visualize detection boxes and crop area
    pub fn process_debug_batch(
        &self,
        input_files: Vec<PathBuf>,
        output_dir: &Path,
    ) -> Result<Vec<ProcessingResult>> {
        if !self.config.debug {
            return Err(anyhow::anyhow!("Debug processing called but debug mode not enabled"));
        }

        let subject_detector = self.subject_detector.as_ref()
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
        let filename = input_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        verbose_println(self.config.verbose, &format!("🔍 Debug processing: {}", filename));

        // Load original image
        let img = image::open(input_path)
            .with_context(|| format!("Failed to load image: {}", input_path.display()))?
            .to_rgb8();

        // Run people detection
        let detection_result = subject_detector.detect_people(&img)?;

        // For debug mode, we also need the raw JSON data from find_subject.py to draw all boxes
        let raw_detection_data = self.get_raw_detection_data(&img, subject_detector)?;

        // Create annotated image with detection boxes
        let mut annotated_img = img.clone();
        self.draw_debug_annotations(&mut annotated_img, &detection_result, &raw_detection_data)?;

        // Generate output filename
        let output_filename = match &self.config.output_format {
            crate::cli::OutputType::Bmp => crate::utils::create_output_filename(input_path, Some("debug"), "bmp"),
            crate::cli::OutputType::Bin => crate::utils::create_output_filename(input_path, Some("debug"), "bin"), // Will save as image anyway
            crate::cli::OutputType::Jpg => crate::utils::create_output_filename(input_path, Some("debug"), "jpg"),
            crate::cli::OutputType::Png => crate::utils::create_output_filename(input_path, Some("debug"), "png"),
        };

        let output_path = output_dir.join(&output_filename);

        // Save annotated image (no processing, just the annotations)
        annotated_img.save(&output_path)
            .with_context(|| format!("Failed to save debug image: {}", output_path.display()))?;

        verbose_println(self.config.verbose, &format!("✅ Debug image saved: {}", output_filename));

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path: output_path,
            bin_path: output_dir.join("dummy.bin"), // Not used in debug mode
            image_type: ImageType::Landscape, // Debug doesn't distinguish
            processing_time: std::time::Duration::from_millis(0),
            people_detected: detection_result.person_count > 0,
            people_count: detection_result.person_count,
            individual_portraits: None,
        })
    }

    /// Get raw detection data from find_subject.py for debug visualization
    fn get_raw_detection_data(
        &self,
        img: &RgbImage,
        _subject_detector: &subject_detection::SubjectDetector,
    ) -> Result<subject_detection::python_yolo_integration::FindSubjectResult> {
        // Save image to temporary file for Python script
        let temp_dir = std::env::temp_dir();
        let temp_filename = format!("debug_input_{}.jpg", std::process::id());
        let temp_path = temp_dir.join(temp_filename);
        
        // Save image as JPEG
        img.save(&temp_path)?;
        
        // Run Python script directly to get raw JSON
        let output = std::process::Command::new("python3")
            .arg(self.config.python_script_path.as_ref().unwrap())
            .arg("--image")
            .arg(&temp_path)
            .arg("--output-format")
            .arg("json")
            .output()
            .map_err(|e| anyhow::anyhow!("Failed to execute find_subject.py: {}", e))?;
        
        // Clean up temporary file
        let _ = std::fs::remove_file(&temp_path);
        
        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(anyhow::anyhow!("find_subject.py failed: {}", stderr));
        }
        
        // Parse JSON output
        let stdout = String::from_utf8_lossy(&output.stdout);
        let result: subject_detection::python_yolo_integration::FindSubjectResult = serde_json::from_str(&stdout)
            .map_err(|e| anyhow::anyhow!("Failed to parse JSON output: {} | Raw: {}", e, stdout))?;
        
        Ok(result)
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
        let blue = Rgb([0u8, 0u8, 255u8]);  // Global bounding box (blue box)
        let red = Rgb([255u8, 0u8, 0u8]);   // Crop area (red box)

        if detection_result.person_count > 0 {
            // 1. Draw individual detection boxes (GREEN)
            for (i, detection) in raw_data.detections.iter().enumerate() {
                let bbox = &detection.bounding_box;
                // Individual detections use [x, y, width, height] format
                let x = bbox[0].max(0) as u32;
                let y = bbox[1].max(0) as u32; 
                let width = bbox[2].max(0) as u32;
                let height = bbox[3].max(0) as u32;
                
                // Clamp to image boundaries
                let x = x.min(img_width.saturating_sub(1));
                let y = y.min(img_height.saturating_sub(1));
                let width = width.min(img_width.saturating_sub(x));
                let height = height.min(img_height.saturating_sub(y));
                
                if width > 0 && height > 0 {
                    let rect = Rect::at(x as i32, y as i32).of_size(width, height);
                    draw_hollow_rect_mut(img, rect, green);
                    
                    // Draw label with class and confidence
                    let label = format!("{} {:.0}%", detection.class, detection.confidence * 100.0);
                    self.draw_label(img, x, y, &label, green)?;
                    
                    verbose_println(self.config.verbose, &format!("  🟢 Detection {}: {} at ({}, {}) {}x{} conf:{:.2}", 
                        i+1, detection.class, x, y, width, height, detection.confidence));
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
                for offset in 0..3 {
                    let thick_rect = Rect::at((gx_min as i32) - offset, (gy_min as i32) - offset)
                        .of_size(gwidth + (offset * 2) as u32, gheight + (offset * 2) as u32);
                    draw_hollow_rect_mut(img, thick_rect, blue);
                }
                
                verbose_println(self.config.verbose, &format!("  🔵 Global box: ({}, {}) {}x{}", gx_min, gy_min, gwidth, gheight));
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

            verbose_println(self.config.verbose, &format!("  ✚ Subject center: ({}, {})", center_x, center_y));
        }

        // 4. Draw crop area (RED) - EXACT same logic as actual processing
        let (crop_x, crop_y, crop_width, crop_height) = self.calculate_actual_crop_area(img_width, img_height, detection_result)?;

        let crop_rect = Rect::at(crop_x as i32, crop_y as i32)
            .of_size(crop_width, crop_height);
        draw_hollow_rect_mut(img, crop_rect, red);

        verbose_println(self.config.verbose, &format!("  🔴 Crop area: ({}, {}) {}x{}", crop_x, crop_y, crop_width, crop_height));

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
        
        // Get effective target dimensions (may be swapped in auto mode)
        let (eff_width, eff_height) = get_effective_target_dimensions(
            image_is_portrait,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_process,
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
                src_width, src_height,
                crop_width, crop_height,
                detection_result
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
            'p' | 'P' => [0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000],  // P
            'e' | 'E' => [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111],  // E
            'r' | 'R' => [0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001],  // R
            's' | 'S' => [0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11111],  // S
            'o' | 'O' => [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],  // O
            'n' | 'N' => [0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001],  // N
            '0' => [0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110],        // 0
            '1' => [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],        // 1
            '2' => [0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111],        // 2
            '3' => [0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110],        // 3
            '4' => [0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010],        // 4
            '5' => [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],        // 5
            '6' => [0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],        // 6
            '7' => [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000],        // 7
            '8' => [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],        // 8
            '9' => [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110],        // 9
            '%' => [0b11001, 0b11010, 0b00100, 0b01000, 0b10000, 0b01011, 0b10011],        // %
            ' ' => [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000],        // space
            _ => [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],          // default (O)
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
#[allow(dead_code)]
pub enum ProcessingOutcome {
    Success(ProcessingResult),
    Skipped(SkippedResult),
}

#[derive(Debug)]
pub struct ProcessingResult {
    #[allow(dead_code)]
    pub input_path: PathBuf,
    #[allow(dead_code)]
    pub bmp_path: PathBuf,
    #[allow(dead_code)]
    pub bin_path: PathBuf,
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
pub struct SkippedResult {
    #[allow(dead_code)]
    pub input_path: PathBuf,
    #[allow(dead_code)]
    pub reason: SkipReason,
    #[allow(dead_code)]
    pub existing_output_path: PathBuf,
}

#[derive(Debug)]
pub enum SkipReason {
    LandscapeExists,
    PortraitInCombined,
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
}