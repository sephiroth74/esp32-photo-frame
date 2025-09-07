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
}

pub struct ProcessingEngine {
    config: ProcessingConfig,
}

impl ProcessingEngine {
    pub fn new(config: ProcessingConfig) -> Result<Self> {
        // Initialize thread pool with specified number of jobs
        rayon::ThreadPoolBuilder::new()
            .num_threads(config.parallel_jobs)
            .build_global()
            .context("Failed to initialize thread pool")?;

        Ok(Self { config })
    }

    /// Discover all image files in the input directories
    pub fn discover_images(&self, input_dirs: &[PathBuf]) -> Result<Vec<PathBuf>> {
        let mut image_files = Vec::new();

        for input_dir in input_dirs {
            verbose_println(self.config.verbose, &format!("Scanning directory: {}", input_dir.display()));

            let walker = WalkDir::new(input_dir)
                .follow_links(false)
                .max_depth(10); // Reasonable depth limit

            for entry in walker {
                let entry = entry.context("Failed to read directory entry")?;
                let path = entry.path();

                if path.is_file() && has_valid_extension(path, &self.config.extensions) {
                    image_files.push(path.to_path_buf());
                }
            }
        }

        // Sort for consistent processing order
        image_files.sort();
        
        verbose_println(self.config.verbose, &format!("Found {} image files", image_files.len()));
        Ok(image_files)
    }

    /// Process a batch of images with progress callback
    pub fn process_batch<F>(
        &self,
        image_files: &[PathBuf],
        output_dir: &Path,
        bin_dir: &Path,
        progress_callback: F,
    ) -> Result<Vec<Result<ProcessingResult>>>
    where
        F: Fn(usize) + Send + Sync,
    {
        use std::sync::atomic::{AtomicUsize, Ordering};
        let processed_count = AtomicUsize::new(0);

        let results: Vec<Result<ProcessingResult>> = image_files
            .par_iter()
            .enumerate()
            .map(|(index, image_path)| {
                let result = self.process_single_image(image_path, output_dir, bin_dir, index);
                
                let count = processed_count.fetch_add(1, Ordering::Relaxed) + 1;
                progress_callback(count);
                
                result
            })
            .collect();

        Ok(results)
    }

    /// Process a batch of images with multi-progress support
    pub fn process_batch_with_progress(
        &self,
        image_files: &[PathBuf],
        output_dir: &Path,
        bin_dir: &Path,
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
                    bin_dir, 
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

    /// Process a single image file
    fn process_single_image(
        &self,
        input_path: &Path,
        output_dir: &Path,
        bin_dir: &Path,
        index: usize,
    ) -> Result<ProcessingResult> {
        verbose_println(self.config.verbose, &format!("Processing: {}", input_path.display()));

        // Load and decode the image
        let img = image::open(input_path)
            .with_context(|| format!("Failed to open image: {}", input_path.display()))?;

        // Convert to RGB
        let rgb_img = img.to_rgb8();

        // Detect orientation and get metadata
        let orientation_info = orientation::detect_orientation(input_path, &rgb_img)?;
        
        verbose_println(self.config.verbose, &format!(
            "Image orientation: {:?}, is_portrait: {}",
            orientation_info.exif_orientation, orientation_info.is_portrait
        ));

        // Apply EXIF rotation if needed
        let rotated_img = orientation::apply_rotation(&rgb_img, orientation_info.exif_orientation)?;

        // Determine if this should be processed as portrait or landscape
        let is_portrait = orientation_info.is_portrait;
        let processing_result = if is_portrait {
            self.process_portrait_image(&rotated_img, input_path, output_dir, bin_dir, index)?
        } else {
            self.process_landscape_image(&rotated_img, input_path, output_dir, bin_dir, index)?
        };

        Ok(processing_result)
    }

    /// Process a single image file with detailed progress tracking
    fn process_single_image_with_progress(
        &self,
        input_path: &Path,
        output_dir: &Path,
        bin_dir: &Path,
        index: usize,
        progress_bar: &ProgressBar,
    ) -> Result<ProcessingResult> {
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

        // Stage 5: Process based on orientation (50-100%)
        progress_bar.set_position(50);
        let is_portrait = orientation_info.is_portrait;
        let processing_result = if is_portrait {
            progress_bar.set_message(format!("{} - Processing portrait", filename));
            self.process_portrait_image_with_progress(&rotated_img, input_path, output_dir, bin_dir, index, progress_bar)?
        } else {
            progress_bar.set_message(format!("{} - Processing landscape", filename));
            self.process_landscape_image_with_progress(&rotated_img, input_path, output_dir, bin_dir, index, progress_bar)?
        };

        progress_bar.set_position(100);
        progress_bar.set_message(format!("{} - Complete", filename));
        Ok(processing_result)
    }

    /// Process a landscape image (full size)
    fn process_landscape_image(
        &self,
        img: &RgbImage,
        input_path: &Path,
        output_dir: &Path,
        bin_dir: &Path,
        _index: usize,
    ) -> Result<ProcessingResult> {
        // Resize to target dimensions
        let resized_img = resize::smart_resize(
            img,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_process,
        )?;

        // Add text annotation
        let annotated_img = annotate::add_filename_annotation(
            &resized_img,
            input_path,
            &self.config.font_name,
            self.config.font_size,
            &self.config.annotation_background,
        )?;

        // Apply color conversion and dithering
        let processed_img = convert::process_image(&annotated_img, &self.config.processing_type)?;

        // Generate output filenames
        let output_filename = crate::utils::create_output_filename(input_path, None, "bmp");
        let bin_filename = crate::utils::create_output_filename(input_path, None, "bin");

        let bmp_path = output_dir.join(&output_filename);
        let bin_path = bin_dir.join(&bin_filename);

        // Save files based on output format configuration
        match &self.config.output_format {
            crate::cli::OutputType::Bmp | crate::cli::OutputType::Both => {
                processed_img.save(&bmp_path)
                    .with_context(|| format!("Failed to save BMP: {}", bmp_path.display()))?;
            }
            _ => {}
        }

        match &self.config.output_format {
            crate::cli::OutputType::Bin | crate::cli::OutputType::Both => {
                let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
                std::fs::write(&bin_path, binary_data)
                    .with_context(|| format!("Failed to save binary: {}", bin_path.display()))?;
            }
            _ => {}
        }

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path,
            bin_path,
            image_type: ImageType::Landscape,
            processing_time: std::time::Duration::from_millis(0), // TODO: measure actual time
        })
    }

    /// Process a landscape image (full size) with detailed progress tracking
    fn process_landscape_image_with_progress(
        &self,
        img: &RgbImage,
        input_path: &Path,
        output_dir: &Path,
        bin_dir: &Path,
        _index: usize,
        progress_bar: &ProgressBar,
    ) -> Result<ProcessingResult> {
        let filename = input_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        // Resize to target dimensions (60%)
        progress_bar.set_position(60);
        progress_bar.set_message(format!("{} - Resizing", filename));
        let resized_img = resize::smart_resize(
            img,
            self.config.target_width,
            self.config.target_height,
            self.config.auto_process,
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

        // Generate output filenames
        let output_filename = crate::utils::create_output_filename(input_path, None, "bmp");
        let bin_filename = crate::utils::create_output_filename(input_path, None, "bin");

        let bmp_path = output_dir.join(&output_filename);
        let bin_path = bin_dir.join(&bin_filename);

        // Save files based on output format configuration
        match &self.config.output_format {
            crate::cli::OutputType::Bmp | crate::cli::OutputType::Both => {
                progress_bar.set_position(90);
                progress_bar.set_message(format!("{} - Saving BMP", filename));
                processed_img.save(&bmp_path)
                    .with_context(|| format!("Failed to save BMP: {}", bmp_path.display()))?;
            }
            _ => {}
        }

        match &self.config.output_format {
            crate::cli::OutputType::Bin | crate::cli::OutputType::Both => {
                progress_bar.set_position(95);
                progress_bar.set_message(format!("{} - Generating binary", filename));
                let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
                std::fs::write(&bin_path, binary_data)
                    .with_context(|| format!("Failed to save binary: {}", bin_path.display()))?;
            }
            _ => {}
        }

        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path,
            bin_path,
            image_type: ImageType::Landscape,
            processing_time: std::time::Duration::from_millis(0), // TODO: measure actual time
        })
    }

    /// Process a portrait image (half width for later combination)
    fn process_portrait_image(
        &self,
        img: &RgbImage,
        input_path: &Path,
        _output_dir: &Path,
        _bin_dir: &Path,
        _index: usize,
    ) -> Result<ProcessingResult> {
        // For now, process portraits as individual images
        // TODO: Implement portrait pairing and combination logic
        let half_width = self.config.target_width / 2;
        
        let _resized_img = resize::smart_resize(
            img,
            half_width,
            self.config.target_height,
            self.config.auto_process,
        )?;

        // TODO: Store portrait for later combination
        // For now, return a placeholder result
        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path: PathBuf::new(),
            bin_path: PathBuf::new(),
            image_type: ImageType::Portrait,
            processing_time: std::time::Duration::from_millis(0),
        })
    }

    /// Process a portrait image (half width for later combination) with detailed progress tracking
    fn process_portrait_image_with_progress(
        &self,
        img: &RgbImage,
        input_path: &Path,
        _output_dir: &Path,
        _bin_dir: &Path,
        _index: usize,
        progress_bar: &ProgressBar,
    ) -> Result<ProcessingResult> {
        let filename = input_path.file_name()
            .and_then(|f| f.to_str())
            .unwrap_or("unknown");

        // For now, process portraits as individual images
        // TODO: Implement portrait pairing and combination logic
        progress_bar.set_position(60);
        progress_bar.set_message(format!("{} - Portrait resizing", filename));
        let half_width = self.config.target_width / 2;
        
        let _resized_img = resize::smart_resize(
            img,
            half_width,
            self.config.target_height,
            self.config.auto_process,
        )?;

        progress_bar.set_position(90);
        progress_bar.set_message(format!("{} - Portrait queued for pairing", filename));
        // TODO: Store portrait for later combination
        // For now, return a placeholder result
        Ok(ProcessingResult {
            input_path: input_path.to_path_buf(),
            bmp_path: PathBuf::new(),
            bin_path: PathBuf::new(),
            image_type: ImageType::Portrait,
            processing_time: std::time::Duration::from_millis(0),
        })
    }
}

#[derive(Debug)]
pub struct ProcessingResult {
    pub input_path: PathBuf,
    pub bmp_path: PathBuf,
    pub bin_path: PathBuf,
    pub image_type: ImageType,
    pub processing_time: std::time::Duration,
}

#[derive(Debug)]
pub enum ImageType {
    Landscape,
    Portrait,
    CombinedPortrait,
}