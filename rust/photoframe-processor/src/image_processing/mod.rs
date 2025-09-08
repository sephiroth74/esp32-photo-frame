pub mod binary;
pub mod combine;
pub mod convert;
pub mod orientation;
pub mod resize;
pub mod annotate;
pub mod batch;
pub mod subject_detection;

use anyhow::{Context, Result};
use image::RgbImage;
use indicatif::ProgressBar;
use rayon::prelude::*;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use walkdir::WalkDir;

use crate::utils::{has_valid_extension, verbose_println};
use combine::combine_processed_portraits;

#[derive(Debug, Clone)]
pub enum ProcessingType {
    BlackWhite,
    SixColor,
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


    /// Process a batch of images with smart portrait pairing and multi-progress support
    pub fn process_batch_with_smart_pairing(
        &self,
        image_files: &[PathBuf],
        output_dir: &Path,
        main_progress: &ProgressBar,
        thread_progress_bars: &[ProgressBar],
        completion_progress: &ProgressBar,
    ) -> Result<Vec<Result<ProcessingResult>>> {
        completion_progress.set_message("Analyzing image orientations...");
        
        // Separate portrait and landscape images
        let mut portrait_files = Vec::new();
        let mut landscape_files = Vec::new();
        
        for (i, path) in image_files.iter().enumerate() {
            let progress = (i as f64 / image_files.len() as f64) * 10.0; // First 10% for analysis
            main_progress.set_position(progress as u64);
            
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
        
        // Process landscape images normally (10-60% progress)
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
        
        // Process portrait images in pairs (60-100% progress)  
        if !portrait_files.is_empty() {
            completion_progress.set_message("Processing portrait pairs...");
            main_progress.set_position(60);
            
            let portrait_results = self.process_portrait_pairs_with_progress(
                &portrait_files,
                output_dir,
                main_progress,
                thread_progress_bars,
                completion_progress,
            )?;
            all_results.extend(portrait_results);
        }
        
        main_progress.set_position(100);
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

        // Resize to target dimensions (60%)
        progress_bar.set_position(60);
        if people_detection.is_some() && people_detection.as_ref().unwrap().person_count > 0 {
            progress_bar.set_message(format!("{} - Smart resizing (people-aware)", filename));
            verbose_println(self.config.verbose, "🎯 Applying people-aware smart cropping");
        } else {
            progress_bar.set_message(format!("{} - Resizing", filename));
        }
        
        let resized_img = resize::smart_resize_with_people_detection(
            img,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_process,
            people_detection.as_ref(),
        )?;

        // Add text annotation (70%)
        progress_bar.set_position(70);
        progress_bar.set_message(format!("{} - Adding annotation", filename));
        let annotated_img = annotate::add_filename_annotation(
            &resized_img,
            input_path,
            &self.config.font_name,
            self.config.font_size,
            &self.config.annotation_background,
        )?;

        // Apply color conversion and dithering (80%)
        progress_bar.set_position(80);
        progress_bar.set_message(format!("{} - Color processing", filename));
        let processed_img = convert::process_image(&annotated_img, &self.config.processing_type)?;

        // Generate output filenames in the same directory
        let output_filename = crate::utils::create_output_filename(input_path, None, "bmp");
        let bin_filename = crate::utils::create_output_filename(input_path, None, "bin");

        let bmp_path = output_dir.join(&output_filename);
        let bin_path = output_dir.join(&bin_filename);

        // Save files based on output format configuration
        match &self.config.output_format {
            crate::cli::OutputType::Bmp => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving BMP", filename));
                processed_img.save(&bmp_path)
                    .with_context(|| format!("Failed to save BMP: {}", bmp_path.display()))?;
            }
            crate::cli::OutputType::Bin => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Generating binary", filename));
                let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
                std::fs::write(&bin_path, binary_data)
                    .with_context(|| format!("Failed to save binary: {}", bin_path.display()))?;
            }
        }

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path,
            bin_path,
            image_type: ImageType::Landscape,
            processing_time: start_time.elapsed(),
            people_detected: people_detection.as_ref().map_or(false, |d| d.person_count > 0),
            people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
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
        
        let resized_img = resize::smart_resize_with_people_detection(
            img,
            half_width,
            self.config.target_height,
            self.config.auto_process,
            people_detection.as_ref(),
        )?;

        // Add text annotation (70%)
        progress_bar.set_position(70);
        progress_bar.set_message(format!("{} - Adding annotation", filename));
        let annotated_img = annotate::add_filename_annotation(
            &resized_img,
            input_path,
            &self.config.font_name,
            self.config.font_size,
            &self.config.annotation_background,
        )?;

        // Apply color conversion and dithering (80%)
        progress_bar.set_position(80);
        progress_bar.set_message(format!("{} - Color processing", filename));
        let processed_img = convert::process_image(&annotated_img, &self.config.processing_type)?;

        // Generate output filenames with portrait suffix
        let output_filename = crate::utils::create_output_filename(input_path, Some("portrait"), "bmp");
        let bin_filename = crate::utils::create_output_filename(input_path, Some("portrait"), "bin");

        let bmp_path = output_dir.join(&output_filename);
        let bin_path = output_dir.join(&bin_filename);

        // Save files based on output format configuration
        match &self.config.output_format {
            crate::cli::OutputType::Bmp => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving portrait BMP", filename));
                processed_img.save(&bmp_path)
                    .with_context(|| format!("Failed to save portrait BMP: {}", bmp_path.display()))?;
            }
            crate::cli::OutputType::Bin => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Generating portrait binary", filename));
                let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
                std::fs::write(&bin_path, binary_data)
                    .with_context(|| format!("Failed to save portrait binary: {}", bin_path.display()))?;
            }
        }

        verbose_println(self.config.verbose, 
            &format!("ℹ️ Portrait processed as individual half-width image. Future enhancement will combine portraits in pairs."));

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path,
            bin_path,
            image_type: ImageType::Portrait,
            processing_time: start_time.elapsed(),
            people_detected: people_detection.as_ref().map_or(false, |d| d.person_count > 0),
            people_count: people_detection.as_ref().map_or(0, |d| d.person_count),
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
                            
                            if let Some((x, y, w, h)) = result.bounding_box {
                                println!("   • Combined bounding box: {}×{} at ({}, {})", w, h, x, y);
                            }
                            
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

        // Load all portrait images and apply EXIF rotations
        completion_progress.set_message("Loading and rotating portrait images...");
        let mut portrait_data = Vec::new();
        
        for (i, path) in portrait_files.iter().enumerate() {
            let progress = (i as f64 / portrait_files.len() as f64) * 30.0; // First 30% for loading
            main_progress.set_position(progress as u64);
            
            match image::open(path) {
                Ok(img) => {
                    let rgb_img = img.to_rgb8();
                    
                    // 1. Apply EXIF rotation to get final orientation
                    let rotated_img = match orientation::detect_orientation(path, &rgb_img) {
                        Ok(orientation_info) => {
                            match orientation::apply_rotation(&rgb_img, orientation_info.exif_orientation) {
                                Ok(rotated) => rotated,
                                Err(e) => {
                                    verbose_println(self.config.verbose, &format!("⚠ Failed to apply rotation to {}: {}", path.display(), e));
                                    rgb_img
                                }
                            }
                        }
                        Err(e) => {
                            verbose_println(self.config.verbose, &format!("⚠ Failed to detect orientation for {}: {}", path.display(), e));
                            rgb_img
                        }
                    };
                    
                    // 2. Detect people if enabled (for smart cropping)
                    let people_detection = if self.subject_detector.is_some() {
                        self.detect_people_with_logging(&rotated_img, path)
                    } else {
                        None
                    };
                    
                    // 3. Smart crop and resize to half-landscape size (e.g., 400x480)
                    let half_width = self.config.target_width / 2;
                    let resized_img = resize::smart_resize_with_people_detection(
                        &rotated_img,
                        half_width,
                        self.config.target_height,
                        self.config.auto_process,
                        people_detection.as_ref(),
                    )?;
                    
                    // 4. Apply image processing (B&W conversion + Floyd-Steinberg dithering)
                    let processed_img = convert::process_image(&resized_img, &self.config.processing_type)?;
                    
                    // 5. Apply annotation if configured
                    let final_img = if !self.config.font_name.is_empty() {
                        annotate::add_filename_annotation(
                            &processed_img,
                            path,
                            &self.config.font_name,
                            self.config.font_size, // Normal font size since image is already at final size
                            &self.config.annotation_background,
                        )?
                    } else {
                        processed_img
                    };
                    
                    portrait_data.push((final_img, path.clone()));
                }
                Err(e) => {
                    verbose_println(self.config.verbose, &format!("⚠ Failed to load {}: {}", path.display(), e));
                }
            }
        }

        // Randomize portrait order for more interesting combinations
        completion_progress.set_message("Randomizing portrait pairs...");
        use rand::seq::SliceRandom;
        let mut rng = rand::thread_rng();
        portrait_data.shuffle(&mut rng);
        
        // If odd number of portraits, discard the last one
        if portrait_data.len() % 2 == 1 {
            let discarded = portrait_data.pop().unwrap();
            verbose_println(self.config.verbose, &format!("ℹ Discarding leftover portrait: {}", 
                discarded.1.file_name().unwrap_or_default().to_string_lossy()));
        }

        // Create pairs from randomized images
        let image_pairs: Vec<(RgbImage, RgbImage)> = portrait_data
            .chunks_exact(2)
            .map(|chunk| (chunk[0].0.clone(), chunk[1].0.clone()))
            .collect();
            
        let path_pairs: Vec<(PathBuf, PathBuf)> = portrait_data
            .chunks_exact(2)
            .map(|chunk| (chunk[0].1.clone(), chunk[1].1.clone()))
            .collect();

        verbose_println(self.config.verbose, &format!("Created {} portrait pairs", image_pairs.len()));

        // Process each pair in parallel
        completion_progress.set_message("Processing portrait pairs...");
        use std::sync::atomic::{AtomicUsize, Ordering};
        use std::collections::HashMap;
        
        let processed_count = AtomicUsize::new(0);
        let thread_assignment = Arc::new(Mutex::new(HashMap::new()));
        let next_thread_id = AtomicUsize::new(0);

        let results: Vec<Result<ProcessingResult>> = image_pairs
            .par_iter()
            .zip(path_pairs.par_iter())
            .enumerate()
            .map(|(index, ((left_img, right_img), (left_path, right_path)))| {
                // Get or assign thread progress bar
                let current_thread_id = rayon::current_thread_index().unwrap_or(0);
                let pb_index = {
                    let mut assignment = thread_assignment.lock().unwrap();
                    if let Some(&pb_index) = assignment.get(&current_thread_id) {
                        pb_index
                    } else {
                        let pb_index = next_thread_id.fetch_add(1, Ordering::SeqCst) % thread_progress_bars.len();
                        assignment.insert(current_thread_id, pb_index);
                        pb_index
                    }
                };
                
                let thread_pb = &thread_progress_bars[pb_index];
                
                let result = self.process_portrait_pair_with_progress(
                    left_img, right_img,
                    left_path, right_path,
                    output_dir,
                    index,
                    thread_pb,
                );

                // Update progress
                let completed = processed_count.fetch_add(1, Ordering::SeqCst) + 1;
                let progress = 50.0 + (completed as f64 / image_pairs.len() as f64) * 50.0; // Second 50% for processing
                main_progress.set_position(progress as u64);
                
                thread_pb.set_message("Idle");
                
                result
            })
            .collect();

        completion_progress.set_message("Portrait pairing complete");
        Ok(results)
    }

    /// Process a single portrait pair by combining them into a landscape image
    fn process_portrait_pair_with_progress(
        &self,
        left_img: &RgbImage,
        right_img: &RgbImage,
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
        
        verbose_println(self.config.verbose, &format!("Combining portraits: {} + {}", left_filename, right_filename));

        progress_bar.set_position(20);
        progress_bar.set_message(format!("Combining {} + {}", left_filename, right_filename));

        // Images are already fully processed (rotated, cropped, resized, processed, annotated)
        // Just combine them side by side using simple placement
        let combined_img = combine_processed_portraits(left_img, right_img, self.config.target_width, self.config.target_height)?;

        progress_bar.set_position(60);

        progress_bar.set_position(70);

        // Generate combined filename: "combined_{filename1}_{filename2}"
        let left_stem = left_path.file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("unknown");
        let right_stem = right_path.file_stem()
            .and_then(|s| s.to_str()) 
            .unwrap_or("unknown");
        
        let combined_filename = format!("combined_{}_{}", left_stem, right_stem);
        let output_filename = format!("{}.bmp", combined_filename);
        let bin_filename = format!("{}.bin", combined_filename);

        let bmp_path = output_dir.join(&output_filename);
        let bin_path = output_dir.join(&bin_filename);

        // Images are already processed, just save the combined result
        match self.config.output_format {
            crate::cli::OutputType::Bmp => {
                progress_bar.set_message(format!("Saving combined BMP"));
                combined_img.save(&bmp_path)
                    .with_context(|| format!("Failed to save combined BMP: {}", bmp_path.display()))?;
            }
            crate::cli::OutputType::Bin => {
                progress_bar.set_message(format!("Generating combined binary"));
                let binary_data = binary::convert_to_esp32_binary(&combined_img)?;
                std::fs::write(&bin_path, &binary_data)
                    .with_context(|| format!("Failed to save combined binary: {}", bin_path.display()))?;
            }
        }

        progress_bar.set_position(100);
        
        verbose_println(self.config.verbose, 
            &format!("✅ Combined portrait pair completed in {:.2}s", start_time.elapsed().as_secs_f32()));

        Ok(ProcessingResult {
            input_path: left_path.to_path_buf(), // Use first image as representative
            bmp_path,
            bin_path,
            image_type: ImageType::Portrait,
            processing_time: start_time.elapsed(),
            people_detected: false, // Portrait combination doesn't use people detection currently
            people_count: 0,
        })
    }

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
}

#[derive(Debug)]
pub enum ImageType {
    Landscape,
    Portrait,
    #[allow(dead_code)]
    CombinedPortrait,
}