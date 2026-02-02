mod annotate;
mod color_correction;
mod combine;
mod debug_viz;
mod face_detection;
mod imagemagick;
mod output;
mod pairing;
mod resize;
mod smart_crop;
mod types;

use annotate::add_date_annotation;
use color_correction::apply_color_correction;
use combine::combine_paired_images;
use debug_viz::draw_detection_boxes;
use output::save_outputs;
use smart_crop::{crop_image, resize_image};
pub use types::{ProcessingPlan, SingleImage};

use crate::cli::Args;
use crate::image_processor::face_detection::Face;
use crate::json_output::JsonMessage;
use crate::logging::Logger;
use crate::report::{ImageInfo, ImageOrientation, Report};
use crate::types::{ColorType, HexColor, Orientation, Size};
use anyhow::{Context, Result};
use image::RgbImage;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use photoframe_lib::{DitheringMethod, apply_dithering};
use rayon::ThreadPoolBuilder;
use rayon::prelude::*;
use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::{Duration, Instant};
use tempfile::Builder;

/// Get the project-relative temp directory
/// TODO: Change back to std::env::temp_dir() once development is complete
fn get_temp_dir() -> Result<PathBuf> {
    let temp_dir = Path::new("./temp");
    if !temp_dir.exists() {
        fs::create_dir_all(temp_dir).context("Failed to create temp directory")?;
    }
    Ok(temp_dir.to_path_buf())
}

/// Clean the temp directory at the start of processing
/// Removes all files except .gitignore and README.md
#[allow(dead_code)]
fn clean_temp_dir() -> Result<()> {
    let temp_dir = get_temp_dir()?;

    if let Ok(entries) = fs::read_dir(&temp_dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_file() {
                if let Some(name) = path.file_name().and_then(|n| n.to_str()) {
                    // Keep .gitignore and README.md
                    if name != ".gitignore" && name != "README.md" {
                        let _ = fs::remove_file(&path);
                    }
                }
            }
        }
    }

    Ok(())
}

pub struct ImageProcessor<'a> {
    args: &'a Args,
    logger: &'a Logger,
}

impl<'a> ImageProcessor<'a> {
    pub fn new(args: &'a Args, logger: &'a Logger) -> Result<Self> {
        Ok(Self { args, logger })
    }

    /// Analyze the validated images and create a processing plan
    /// Updates the report with unpaired images if any
    pub fn plan(&self, images: Vec<ImageInfo>, report: &mut Report) -> io::Result<ProcessingPlan> {
        self.logger.section("Image Processing Plan");

        if images.is_empty() {
            self.logger.warning("No valid images to process");
            return Ok(ProcessingPlan::new());
        }

        let plan = if self.args.no_pairing {
            self.plan_no_pairing(images)?
        } else {
            self.plan_with_pairing(images, report)?
        };

        // Print summary
        self.print_plan_summary(&plan);

        Ok(plan)
    }

    /// Create a plan where all images are processed individually
    fn plan_no_pairing(&self, images: Vec<ImageInfo>) -> io::Result<ProcessingPlan> {
        self.logger
            .info("Mode: No pairing - all images will be processed individually");

        let mut plan = ProcessingPlan::new();

        for image_info in images {
            plan.single_images.push(SingleImage::new(image_info));
        }

        Ok(plan)
    }

    /// Create a plan where images are paired based on target orientation
    /// - If target is landscape: portrait images are paired, landscape processed individually
    /// - If target is portrait: landscape images are paired, portrait processed individually
    fn plan_with_pairing(
        &self,
        images: Vec<ImageInfo>,
        report: &mut Report,
    ) -> io::Result<ProcessingPlan> {
        // Determine which images to pair based on target orientation
        let target_is_landscape = matches!(
            self.args.target_orientation,
            Orientation::Landscape | Orientation::LandscapeReverse
        );

        let (single_orientation, pair_orientation, message) = if target_is_landscape {
            (
                ImageOrientation::Landscape,
                ImageOrientation::Portrait,
                "Mode: Smart pairing - portrait images will be paired for landscape output",
            )
        } else {
            (
                ImageOrientation::Portrait,
                ImageOrientation::Landscape,
                "Mode: Smart pairing - landscape images will be paired for portrait output",
            )
        };

        if self.logger.is_verbose() {
            self.logger.info(message);
        }

        let mut plan = ProcessingPlan::new();
        let mut single_images = Vec::new();
        let mut pair_images = Vec::new();

        // Separate images by orientation (already classified during inspection)
        self.logger.verbose("Organizing images by orientation...");
        for image_info in images {
            match image_info.orientation {
                o if o == single_orientation => single_images.push(image_info),
                o if o == pair_orientation => pair_images.push(image_info),
                _ => unreachable!(),
            }
        }

        self.logger.verbose(&format!(
            "Found {} images for single processing and {} images for pairing",
            single_images.len(),
            pair_images.len()
        ));

        // Add all single-orientation images as individual images
        for image_info in single_images {
            plan.single_images.push(SingleImage::new(image_info));
        }

        // Pair the pair-orientation images
        let (paired, unpaired) = pairing::pair_portraits(pair_images);
        plan.paired_images = paired;

        // Handle unpaired image
        if let Some(unpaired_image) = unpaired {
            let orientation_name = if pair_orientation == ImageOrientation::Portrait {
                "portrait"
            } else {
                "landscape"
            };
            self.logger.warning(&format!(
                "Image '{}' will be skipped (odd number of {} images)",
                unpaired_image.path.display(),
                orientation_name
            ));
            report.unpaired_images.push(unpaired_image);
        }

        Ok(plan)
    }

    fn print_plan_summary(&self, plan: &ProcessingPlan) {
        self.logger.divider();
        self.logger.config_section("Processing Summary");
        self.logger
            .config_item("Single images", &plan.single_images.len().to_string());
        self.logger
            .config_item("Paired images", &plan.paired_images.len().to_string());

        if !plan.unpaired_images.is_empty() {
            self.logger.config_item(
                "Unpaired (skipped)",
                &plan.unpaired_images.len().to_string(),
            );
        }

        self.logger
            .config_item("Total input images", &plan.input_count().to_string());
        self.logger
            .config_item("Total output images", &plan.output_count().to_string());
        self.logger.divider();
    }

    /// Process all images in the plan using multithreading and temporary files
    pub fn process(&self, plan: &ProcessingPlan, json_progress: bool) -> Result<ProcessingResult> {
        // Note: Skipping temp directory cleanup to allow inspection of intermediate files during development
        // Uncomment below to enable cleanup if needed
        // clean_temp_dir().context("Failed to clean temp directory")?;

        let jobs = self.build_jobs(plan);
        if jobs.is_empty() {
            self.logger.warning("No images to process");
            return Ok(ProcessingResult {
                processed: Vec::new(),
                failed: Vec::new(),
                processed_details: Vec::new(),
                paired_details: Vec::new(),
            });
        }

        let total_jobs = jobs.len();

        let thread_count = if self.args.jobs == 0 {
            num_cpus::get()
        } else {
            self.args.jobs
        };

        if !json_progress {
            self.logger.info(&format!(
                "Processing with {} thread(s)",
                thread_count.max(1)
            ));
        }

        let pool = ThreadPoolBuilder::new()
            .num_threads(thread_count.max(1))
            .build()
            .context("Failed to create thread pool")?;

        let multi = MultiProgress::new();
        let global_bar = if json_progress || self.args.verbose {
            None
        } else {
            let pb = multi.add(ProgressBar::new(total_jobs as u64));
            pb.set_style(
                ProgressStyle::with_template("Global [{bar:40.cyan/blue}] {pos}/{len} {eta}")
                    .unwrap()
                    .progress_chars("=>-"),
            );
            pb.set_message("Processing images");
            Some(pb)
        };

        let thread_bars: Option<Vec<ProgressBar>> = if json_progress || self.args.verbose {
            None
        } else {
            let bars = (0..thread_count.max(1))
                .map(|idx| {
                    let pb = multi.add(ProgressBar::new(6));
                    pb.set_style(
                        ProgressStyle::with_template(
                            "{spinner:.red} {msg} [{bar:20.cyan/blue}] {pos}/{len}",
                        )
                        .unwrap()
                        .progress_chars("=>-"),
                    );
                    pb.set_message(format!("Job {:2}: {:25}", idx + 1, "idle"));
                    pb.enable_steady_tick(Duration::from_millis(120));
                    pb
                })
                .collect();
            Some(bars)
        };

        let logger = self.logger;
        let json_counter = AtomicUsize::new(0);

        let results: Vec<(Result<ProcessedImage>, PathBuf)> = pool.install(|| {
            jobs.into_par_iter()
                .enumerate()
                .map(|(_idx, job)| {
                    let mut thread_idx = 0usize;
                    let pb = thread_bars.as_ref().map(|bars| {
                        thread_idx = rayon::current_thread_index().unwrap_or(0) % bars.len();
                        bars[thread_idx].clone()
                    });

                    if let Some(pb) = &pb {
                        let filename = job
                            .image
                            .path
                            .file_name()
                            .map(|v| v.to_string_lossy())
                            .unwrap_or_else(|| job.image.path.to_string_lossy());
                        let slice = &filename.as_ref()[..25.min(filename.len())];
                        pb.set_message(format!("[{:2}] {:25}", thread_idx + 1, slice));
                        pb.set_position(0);
                    }

                    let result = process_job(&job, pb.as_ref(), logger);

                    if json_progress {
                        let current = json_counter.fetch_add(1, Ordering::Relaxed) + 1;
                        let message = format!("Processing {}", job.image.path.display());
                        JsonMessage::progress(current, total_jobs, message);

                        if let Err(err) = &result {
                            JsonMessage::file_failed(&job.image.path, err.to_string());
                        }
                    }

                    if let Some(pb) = &pb {
                        match &result {
                            Ok(_) => {
                                pb.set_message(format!("[{:2}] {:25}", thread_idx + 1, "done"))
                            }
                            Err(_) => {
                                pb.set_message(format!("[{:2}] {:25}", thread_idx + 1, "failed"))
                            }
                        }
                        pb.set_position(0);
                    }

                    if let Some(global) = &global_bar {
                        global.inc(1);
                    }

                    (result, job.image.path.clone())
                })
                .collect()
        });

        if let Some(global) = &global_bar {
            global.finish_and_clear();
        }

        let mut processed = Vec::new();
        let mut failed = Vec::new();

        for (result, path) in results {
            match result {
                Ok(item) => processed.push(item),
                Err(_) => failed.push(path),
            }
        }

        // Combine paired images if any
        let combined = combine_paired_images(
            &processed,
            &multi,
            json_progress,
            self.args.verbose,
            self.args.jobs,
            self.args.target_orientation,
            self.args.divider_width,
            self.args.divider_color,
            self.logger,
        )?;

        // Save outputs in requested formats
        let output_paths = save_outputs(
            &combined,
            &self.args.output,
            &self.args.output_formats,
            self.args.processing_type,
            self.args.target_orientation,
            &multi,
            json_progress,
            self.args.jobs,
            self.logger,
        )?;

        // Build report details for single and paired images
        let processed_details = combined
            .iter()
            .enumerate()
            .filter(|(_, img)| !img.paired)
            .map(|(idx, img)| {
                let output_path = output_paths
                    .get(idx)
                    .and_then(|p| p.clone())
                    .unwrap_or_else(|| img.source.clone());
                crate::report::ProcessingDetail::new(
                    img.source.clone(),
                    output_path,
                    img.width,
                    img.height,
                    img.people_count.unwrap_or(0),
                    img.processing_time_ms,
                    img.detection_time_ms,
                )
            })
            .collect();

        let mut pair_output_paths: BTreeMap<usize, Option<PathBuf>> = BTreeMap::new();
        for (idx, img) in combined
            .iter()
            .enumerate()
            .filter(|(_, img)| img.paired && img.pair_index.is_none())
        {
            if let Some(pair_id) = img.pair_id {
                let output_path = output_paths.get(idx).and_then(|p| p.clone());
                pair_output_paths.insert(pair_id, output_path);
            }
        }

        let mut paired_details = Vec::new();
        let mut paired_groups: BTreeMap<usize, Vec<&ProcessedImage>> = BTreeMap::new();

        for img in processed.iter().filter(|img| img.paired) {
            if let Some(pair_id) = img.pair_id {
                paired_groups.entry(pair_id).or_default().push(img);
            } else {
                paired_details.push(crate::report::PairedImageDetail::new(
                    None,
                    img.source.clone(),
                    img.width,
                    img.height,
                    img.people_count.unwrap_or(0),
                    img.processing_time_ms,
                    img.detection_time_ms,
                ));
            }
        }

        for (pair_id, mut imgs) in paired_groups {
            imgs.sort_by_key(|img| img.pair_index.unwrap_or(0));
            let last_index = imgs.len().saturating_sub(1);
            let pair_output_path = pair_output_paths.get(&pair_id).and_then(|p| p.clone());

            for (idx, img) in imgs.iter().enumerate() {
                let output_path = if idx == last_index {
                    pair_output_path.clone()
                } else {
                    None
                };

                paired_details.push(crate::report::PairedImageDetail::new(
                    output_path,
                    img.source.clone(),
                    img.width,
                    img.height,
                    img.people_count.unwrap_or(0),
                    img.processing_time_ms,
                    img.detection_time_ms,
                ));
            }
        }

        Ok(ProcessingResult {
            processed: combined,
            failed,
            processed_details,
            paired_details,
        })
    }

    fn build_jobs(&self, plan: &ProcessingPlan) -> Vec<ProcessingJob> {
        let base_size: Size = self
            .args
            .processing_type
            .into_size(self.args.target_orientation);
        let paired_size = paired_target_size(base_size, self.args.target_orientation);

        let mut jobs = Vec::new();

        for single in &plan.single_images {
            jobs.push(ProcessingJob {
                image: single.info.clone(),
                target_size: base_size,
                paired: false,
                pair_id: None,
                pair_index: None,
                processing_type: self.args.processing_type,
                auto_color_correct: self.args.auto_color,
                brightness: self.args.brightness,
                contrast: self.args.contrast,
                saturation: self.args.saturation,
                dithering_method: self.args.dithering_method,
                dither_strength: self.args.dither_strength as f32 / 100.0,
                detect_people: self.args.detect_people,
                confidence_threshold: self.args.confidence_threshold,
                debug: self.args.debug,
                annotate: self.args.annotate,
                font_name: self.args.font.clone(),
                font_size: self.args.font_size,
                annotation_background: self.args.annotation_background,
            });
        }

        for (pair_id, pair) in plan.paired_images.iter().enumerate() {
            jobs.push(ProcessingJob {
                image: pair.first.clone(),
                target_size: paired_size,
                paired: true,
                pair_id: Some(pair_id),
                pair_index: Some(0),
                processing_type: self.args.processing_type,
                auto_color_correct: self.args.auto_color,
                brightness: self.args.brightness,
                contrast: self.args.contrast,
                saturation: self.args.saturation,
                dithering_method: self.args.dithering_method,
                dither_strength: self.args.dither_strength as f32 / 100.0,
                detect_people: self.args.detect_people,
                confidence_threshold: self.args.confidence_threshold,
                debug: self.args.debug,
                annotate: self.args.annotate,
                font_name: self.args.font.clone(),
                font_size: self.args.font_size,
                annotation_background: self.args.annotation_background,
            });
            jobs.push(ProcessingJob {
                image: pair.second.clone(),
                target_size: paired_size,
                paired: true,
                pair_id: Some(pair_id),
                pair_index: Some(1),
                processing_type: self.args.processing_type,
                auto_color_correct: self.args.auto_color,
                brightness: self.args.brightness,
                contrast: self.args.contrast,
                saturation: self.args.saturation,
                dithering_method: self.args.dithering_method,
                dither_strength: self.args.dither_strength as f32 / 100.0,
                detect_people: self.args.detect_people,
                confidence_threshold: self.args.confidence_threshold,
                debug: self.args.debug,
                annotate: self.args.annotate,
                font_name: self.args.font.clone(),
                font_size: self.args.font_size,
                annotation_background: self.args.annotation_background,
            });
        }

        jobs
    }
}

#[derive(Debug, Clone)]
struct ProcessingJob {
    image: ImageInfo,
    target_size: Size,
    paired: bool,
    pair_id: Option<usize>,    // ID to identify which pair this belongs to
    pair_index: Option<usize>, // 0 for first image, 1 for second image in pair
    processing_type: ColorType,
    auto_color_correct: bool,
    brightness: i32,
    contrast: i32,
    saturation: u32,
    dithering_method: DitheringMethod,
    dither_strength: f32,
    detect_people: bool,
    confidence_threshold: f32,
    debug: bool,
    annotate: bool,
    font_name: String,
    font_size: u32,
    annotation_background: HexColor,
}

#[derive(Debug)]
pub struct ProcessingResult {
    pub processed: Vec<ProcessedImage>,
    pub failed: Vec<PathBuf>,
    pub processed_details: Vec<crate::report::ProcessingDetail>,
    pub paired_details: Vec<crate::report::PairedImageDetail>,
}

#[derive(Debug, Clone)]
#[allow(dead_code)]
pub struct ProcessedImage {
    pub source: PathBuf,
    pub temp_path: PathBuf,
    pub target_size: Size,
    pub paired: bool,
    pub pair_id: Option<usize>,
    pub pair_index: Option<usize>,
    pub paired_source: Option<PathBuf>, // Source of paired image (for combined filenames)
    pub orientation: ImageOrientation,
    pub people_count: Option<usize>,
    pub width: u32,
    pub height: u32,
    pub processing_time_ms: u128,
    pub detection_time_ms: u128,
}

fn paired_target_size(base: Size, target_orientation: Orientation) -> Size {
    let is_landscape = matches!(
        target_orientation,
        Orientation::Landscape | Orientation::LandscapeReverse
    );

    if is_landscape {
        Size {
            width: base.width / 2,
            height: base.height,
        }
    } else {
        Size {
            width: base.width,
            height: base.height / 2,
        }
    }
}

fn process_job(
    job: &ProcessingJob,
    progress: Option<&ProgressBar>,
    logger: &Logger,
) -> Result<ProcessedImage> {
    let processing_start = Instant::now();
    let filename = job
        .image
        .path
        .file_name()
        .map(|v| v.to_string_lossy())
        .unwrap_or_else(|| job.image.path.to_string_lossy())
        .to_string();

    logger.verbose(&format!("Opening image: {}", filename));

    let reader = image::ImageReader::open(&job.image.path)
        .with_context(|| format!("Failed to open image: {}", job.image.path.display()))?
        .with_guessed_format()
        .context("Failed to guess image format")?;

    let mut img = reader.decode().context("Failed to decode image")?;

    logger.verbose(&format!("Image decoded: {}x{}", img.width(), img.height()));

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Apply EXIF rotation
    let rotation_deg = match job.image.rotation {
        crate::report::RotationDegrees::Deg0 => "0",
        crate::report::RotationDegrees::Deg90 => "90",
        crate::report::RotationDegrees::Deg180 => "180",
        crate::report::RotationDegrees::Deg270 => "270",
    };
    if rotation_deg != "0" {
        logger.verbose(&format!("Applying EXIF rotation: {}°", rotation_deg));
    }
    img = match job.image.rotation {
        crate::report::RotationDegrees::Deg0 => img,
        crate::report::RotationDegrees::Deg90 => img.rotate90(),
        crate::report::RotationDegrees::Deg180 => img.rotate180(),
        crate::report::RotationDegrees::Deg270 => img.rotate270(),
    };

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Convert to RGB for processing
    let img_rgb = img.to_rgb8();

    // Create single intermediate temporary file at the start - reuse for all operations
    let temp_dir = get_temp_dir()?;
    let mut temp_file = Builder::new()
        .prefix("pfproc_")
        .suffix(".png")
        .tempfile_in(&temp_dir)
        .context("Failed to create temporary file")?;

    // Detect faces if enabled
    let (detection, face_count, detection_time_ms): (Option<Vec<Face>>, Option<usize>, u128) =
        detect_faces(&job, logger, &img_rgb);

    if let Some(pb) = progress {
        pb.inc(1);
    }

    logger.verbose(&format!(
        "Computing crop area (target: {}x{})",
        job.target_size.width, job.target_size.height
    ));

    let (crop_x, crop_y, crop_width, crop_height) = compute_crop_area_faces(
        &img_rgb,
        job.target_size.width,
        job.target_size.height,
        detection.as_ref(),
    );
    logger.verbose(&format!(
        "Crop area: [{},{} - {}x{}]",
        crop_x, crop_y, crop_width, crop_height
    ));

    // Smart crop and resize with face detection awareness
    logger.verbose("Applying smart crop and resize with face awareness");
    let mut processing_image = smart_crop_and_resize_faces(
        &img_rgb,
        job.target_size.width,
        job.target_size.height,
        detection.as_ref(),
    )
    .context("Failed to crop and resize image")?;
    let dims = processing_image.dimensions();
    logger.verbose(&format!("Crop and resize complete: {}x{}", dims.0, dims.1));

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Apply color correction
    let mut active = Vec::new();
    if job.auto_color_correct {
        active.push("auto-color".to_string());
    }
    if job.brightness != 0 {
        active.push(format!("brightness:{}", job.brightness));
    }
    if job.contrast != 0 {
        active.push(format!("contrast:{}", job.contrast));
    }
    if job.saturation != 100 {
        active.push(format!("saturation:{}", job.saturation));
    }
    if !active.is_empty() {
        logger.verbose(&format!("Applying color correction: {}", active.join(", ")));
    }

    processing_image = apply_color_correction(
        &processing_image,
        job.auto_color_correct,
        job.brightness,
        job.contrast,
        job.saturation,
    )
    .context("Failed to apply color correction")?;

    // Debug visualization: draw detection boxes AFTER color correction
    // This ensures boxes are visible in the final saved image
    if job.debug && job.detect_people && detection.is_some() {
        logger.verbose("Drawing debug visualization (detection boxes)");
        let faces = detection.as_ref().unwrap();
        if !faces.is_empty() && crop_width > 0 && crop_height > 0 {
            let scale_x = job.target_size.width as f32 / crop_width as f32;
            let scale_y = job.target_size.height as f32 / crop_height as f32;
            let max_out_x = job.target_size.width.saturating_sub(1);
            let max_out_y = job.target_size.height.saturating_sub(1);
            let max_crop_x = crop_width.saturating_sub(1);
            let max_crop_y = crop_height.saturating_sub(1);

            let mut mapped = Vec::new();

            for face in faces {
                let det_x_min = face.x1;
                let det_y_min = face.y1;
                let det_x_max = face.x2;
                let det_y_max = face.y2;
                let confidence = face.confidence;

                let mut x_min = det_x_min.saturating_sub(crop_x).min(max_crop_x);
                let mut y_min = det_y_min.saturating_sub(crop_y).min(max_crop_y);
                let mut x_max = det_x_max.saturating_sub(crop_x).min(max_crop_x);
                let mut y_max = det_y_max.saturating_sub(crop_y).min(max_crop_y);

                if x_max <= x_min || y_max <= y_min {
                    continue;
                }

                x_min = (x_min as f32 * scale_x).round() as u32;
                y_min = (y_min as f32 * scale_y).round() as u32;
                x_max = (x_max as f32 * scale_x).round() as u32;
                y_max = (y_max as f32 * scale_y).round() as u32;

                x_min = x_min.min(max_out_x);
                y_min = y_min.min(max_out_y);
                x_max = x_max.min(max_out_x);
                y_max = y_max.min(max_out_y);

                if x_max > x_min && y_max > y_min {
                    mapped.push(Face {
                        x1: x_min,
                        y1: y_min,
                        x2: x_max,
                        y2: y_max,
                        confidence,
                    });
                }
            }

            if !mapped.is_empty() {
                processing_image = draw_detection_boxes(&processing_image, &mapped, false)
                    .unwrap_or(processing_image);
            }
        }
    }

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Add date annotation if enabled
    if job.annotate {
        logger.verbose(&format!("Adding date annotation (font: {})", job.font_name));
        processing_image = add_date_annotation(
            &processing_image,
            &job.image.path,
            &job.font_name,
            job.font_size,
            &job.annotation_background,
            logger,
        )
        .unwrap_or(processing_image);
    }

    logger.verbose(&format!(
        "Applying dithering: {:?} (strength: {})",
        job.dithering_method, job.dither_strength
    ));

    processing_image = apply_dithering(
        &processing_image,
        job.dithering_method,
        job.processing_type.into(),
        job.dither_strength,
    )
    .unwrap_or(processing_image);

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Save single intermediate file with all processing applied
    processing_image
        .save(&mut temp_file)
        .context("Failed to write intermediate image")?;

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Keep the file so it doesn't get deleted when the temp file is dropped
    let (_file, temp_path) = temp_file.keep().context("Failed to keep temporary file")?;

    let processing_time_ms = processing_start.elapsed().as_millis();

    Ok(ProcessedImage {
        source: job.image.path.clone(),
        temp_path,
        target_size: job.target_size,
        paired: job.paired,
        pair_id: job.pair_id,
        pair_index: job.pair_index,
        paired_source: None,
        orientation: job.image.orientation,
        people_count: face_count,
        width: dims.0,
        height: dims.1,
        processing_time_ms,
        detection_time_ms,
    })
}

#[cfg(feature = "ai")]
fn detect_faces(
    job: &&ProcessingJob,
    logger: &Logger,
    img_rgb: &RgbImage,
) -> (Option<Vec<Face>>, Option<usize>, u128) {
    if job.detect_people {
        logger.verbose(&format!(
            "Running face detection (confidence threshold: {:.2})",
            job.confidence_threshold
        ));

        let detection_start = Instant::now();
        match face_detection::detection::detect_faces(&img_rgb, job.confidence_threshold) {
            Ok(faces) => {
                let detection_elapsed = detection_start.elapsed().as_millis();
                let count = faces.len();
                if logger.is_verbose() {
                    logger.verbose(&format!(
                        "Face detection completed: {} face(s) detected",
                        count
                    ));
                    if !faces.is_empty() {
                        for (i, face) in faces.iter().enumerate() {
                            logger.verbose(&format!(
                                "  Face {}: [{},{} - {},{}] (confidence: {:.2})",
                                i + 1,
                                face.x1,
                                face.y1,
                                face.x2,
                                face.y2,
                                face.confidence
                            ));
                        }
                    }
                }
                (Some(faces), Some(count), detection_elapsed)
            }
            Err(err) => {
                logger.verbose(&format!("Face detection error: {}", err));
                (None, None, detection_start.elapsed().as_millis())
            }
        }
    } else {
        (None, None, 0)
    }
}

#[cfg(not(feature = "ai"))]
fn detect_faces(
    _job: &&ProcessingJob,
    _logger: &Logger,
    _img_rgb: &RgbImage,
) -> (Option<Vec<Face>>, Option<usize>, u128) {
    (None, None, 0)
}

/// Compute crop area based on target dimensions and face detections
/// Returns (crop_x, crop_y, crop_width, crop_height)
fn compute_crop_area_faces(
    img: &image::RgbImage,
    target_width: u32,
    target_height: u32,
    faces: Option<&Vec<Face>>,
) -> (u32, u32, u32, u32) {
    let (src_width, src_height) = img.dimensions();

    // Calculate crop dimensions to maintain aspect ratio
    let target_aspect = target_width as f64 / target_height as f64;
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

    // Determine crop position
    let (crop_x, crop_y) = if let Some(face_list) = faces {
        if !face_list.is_empty() {
            // Use smart cropping based on largest face
            let largest_face = face_list.first().unwrap(); // Already sorted by confidence
            calculate_crop_offset_from_face(
                src_width,
                src_height,
                crop_width,
                crop_height,
                largest_face,
            )
        } else {
            // No faces detected, use center crop
            standard_crop_offset(src_width, src_height, crop_width, crop_height)
        }
    } else {
        // No detection available, use center crop
        standard_crop_offset(src_width, src_height, crop_width, crop_height)
    };

    (crop_x, crop_y, crop_width, crop_height)
}

/// Calculate crop offset based on face position
fn calculate_crop_offset_from_face(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
    face: &Face,
) -> (u32, u32) {
    // Get face bounding box
    let face_x_min = face.x1 as u32;
    let face_y_min = face.y1 as u32;
    let face_x_max = face.x2 as u32;
    let face_y_max = face.y2 as u32;

    // Calculate face center
    let face_center_x = (face_x_min + face_x_max) / 2;
    let face_center_y = (face_y_min + face_y_max) / 2;

    // Try to keep the entire face within the crop
    let ideal_left = face_center_x.saturating_sub(crop_width / 2);
    let ideal_top = face_center_y.saturating_sub(crop_height / 2);

    let crop_left = ideal_left.min(src_width.saturating_sub(crop_width));
    let crop_top = ideal_top.min(src_height.saturating_sub(crop_height));

    (crop_left, crop_top)
}

/// Standard center crop offset
fn standard_crop_offset(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
) -> (u32, u32) {
    let crop_x = src_width.saturating_sub(crop_width) / 2;
    let crop_y = src_height.saturating_sub(crop_height) / 2;
    (crop_x, crop_y)
}

/// Smart crop and resize with face detection awareness
fn smart_crop_and_resize_faces(
    img: &image::RgbImage,
    target_width: u32,
    target_height: u32,
    faces: Option<&Vec<Face>>,
) -> Result<image::RgbImage> {
    let (crop_x, crop_y, crop_width, crop_height) =
        compute_crop_area_faces(img, target_width, target_height, faces);

    // Crop the image
    let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height)?;

    // Resize to exact target dimensions if needed
    if cropped.width() != target_width || cropped.height() != target_height {
        resize_image(&cropped, target_width, target_height)
    } else {
        Ok(cropped)
    }
}
