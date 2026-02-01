mod annotate;
mod color_correction;
mod debug_viz;
mod imagemagick;
mod pairing;
mod resize;
mod smart_crop;
mod subject_detection;
mod types;

use annotate::add_date_annotation;
use color_correction::apply_color_correction;
use debug_viz::draw_detection_boxes;
use smart_crop::{compute_crop_area, smart_crop_and_resize};
use subject_detection::{SubjectDetector, create_detector};
pub use types::{ProcessingPlan, SingleImage};

use crate::cli::Args;
use crate::logging::Logger;
use crate::report::{ImageInfo, ImageOrientation, Report};
use crate::types::{ColorType, HexColor, Orientation, Size};
use anyhow::{Context, Result};
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use photoframe_lib::{DitheringMethod, apply_dithering};
use rayon::ThreadPoolBuilder;
use rayon::prelude::*;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::time::Duration;
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
    detector: Option<SubjectDetector>,
}

impl<'a> ImageProcessor<'a> {
    pub fn new(args: &'a Args, logger: &'a Logger) -> Result<Self> {
        let detector = if args.detect_people {
            Some(create_detector(args.verbose).context("Failed to create subject detector")?)
        } else {
            None
        };

        Ok(Self {
            args,
            logger,
            detector,
        })
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
            let orientation = image_info.orientation;
            plan.single_images.push(SingleImage {
                info: image_info,
                orientation,
            });
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

        self.logger.info(message);

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
            plan.single_images.push(SingleImage {
                info: image_info,
                orientation: single_orientation,
            });
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
        let global_bar = if json_progress {
            None
        } else {
            let pb = multi.add(ProgressBar::new(total_jobs as u64));
            pb.set_style(
                ProgressStyle::with_template("Global [{bar:40.cyan/blue}] {pos}/{len} {eta}")
                    .unwrap()
                    .progress_chars("██▌ "),
            );
            pb.set_message("Processing images");
            Some(pb)
        };

        let thread_bars: Option<Vec<ProgressBar>> = if json_progress {
            None
        } else {
            let bars = (0..thread_count.max(1))
                .map(|idx| {
                    let pb = multi.add(ProgressBar::new(6));
                    pb.set_style(
                        ProgressStyle::with_template("{msg} [{bar:20.cyan/blue}] {pos}/{len}")
                            .unwrap()
                            .progress_chars("██▌ "),
                    );
                    pb.set_message(format!("Job {:2}: idle", idx + 1));
                    pb.enable_steady_tick(Duration::from_millis(120));
                    pb
                })
                .collect();
            Some(bars)
        };

        // Clone detector for thread pool (it's cheap - just Arc clones internally)
        let detector_opt = self.detector.as_ref().cloned();

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
                        pb.set_message(format!("Job {:2}: {:25}", thread_idx + 1, slice));
                        pb.set_position(0);
                    }

                    let result = process_job(&job, detector_opt.as_ref(), pb.as_ref());

                    if let Some(pb) = &pb {
                        match &result {
                            Ok(_) => pb.set_message(format!("Job {:2}: done", thread_idx + 1)),
                            Err(_) => pb.set_message(format!("Job {:2}: failed", thread_idx + 1)),
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
        let combined = self.combine_paired_images(&processed, &multi, json_progress)?;

        Ok(ProcessingResult {
            processed: combined,
            failed,
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

    /// Combine paired images with divider
    fn combine_paired_images(
        &self,
        processed: &[ProcessedImage],
        multi: &MultiProgress,
        json_progress: bool,
    ) -> Result<Vec<ProcessedImage>> {
        use std::collections::HashMap;

        // Group paired images by pair_id
        let mut pairs: HashMap<usize, [Option<&ProcessedImage>; 2]> = HashMap::new();
        let mut singles = Vec::new();

        for img in processed {
            if let (Some(pair_id), Some(pair_index)) = (img.pair_id, img.pair_index) {
                let entry = pairs.entry(pair_id).or_insert([None, None]);
                entry[pair_index] = Some(img);
            } else {
                singles.push(img.clone());
            }
        }

        if pairs.is_empty() {
            // No pairs to combine, return original list
            return Ok(processed.to_vec());
        }

        // Prepare combining jobs
        let combine_jobs: Vec<_> = pairs
            .into_iter()
            .filter_map(|(pair_id, [first, second])| {
                if let (Some(first), Some(second)) = (first, second) {
                    Some((pair_id, first.clone(), second.clone()))
                } else {
                    None
                }
            })
            .collect();

        if combine_jobs.is_empty() {
            return Ok(processed.to_vec());
        }

        if !json_progress {
            self.logger.info(&format!(
                "Combining {} paired images...",
                combine_jobs.len()
            ));
        }

        let global_bar = if json_progress {
            None
        } else {
            let pb = multi.add(ProgressBar::new(combine_jobs.len() as u64));
            pb.set_style(
                ProgressStyle::with_template("Combining [{bar:40.cyan/blue}] {pos}/{len} {eta}")
                    .unwrap()
                    .progress_chars("██▌ "),
            );
            pb.set_message("Combining paired images");
            Some(pb)
        };

        let thread_count = if self.args.jobs == 0 {
            num_cpus::get()
        } else {
            self.args.jobs
        };

        let pool = ThreadPoolBuilder::new()
            .num_threads(thread_count.max(1))
            .build()
            .context("Failed to create thread pool for combining")?;

        let is_landscape = matches!(
            self.args.target_orientation,
            Orientation::Landscape | Orientation::LandscapeReverse
        );
        let divider_width = self.args.divider_width;
        let divider_color = self.args.divider_color;

        let combined_results: Vec<Result<ProcessedImage>> = pool.install(|| {
            combine_jobs
                .into_par_iter()
                .map(|(pair_id, first, second)| {
                    let result = combine_two_images(
                        &first,
                        &second,
                        is_landscape,
                        divider_width,
                        &divider_color,
                        pair_id,
                    );

                    if let Some(global) = &global_bar {
                        global.inc(1);
                    }

                    result
                })
                .collect()
        });

        if let Some(global) = global_bar {
            global.finish_and_clear();
        }

        let mut result = singles;
        for combined in combined_results {
            match combined {
                Ok(img) => result.push(img),
                Err(e) => {
                    if !json_progress {
                        self.logger
                            .warning(&format!("Failed to combine pair: {}", e));
                    }
                }
            }
        }

        Ok(result)
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
    pub orientation: ImageOrientation,
    pub people_count: Option<usize>,
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
    detector: Option<&SubjectDetector>,
    progress: Option<&ProgressBar>,
) -> Result<ProcessedImage> {
    let reader = image::ImageReader::open(&job.image.path)
        .with_context(|| format!("Failed to open image: {}", job.image.path.display()))?
        .with_guessed_format()
        .context("Failed to guess image format")?;

    let mut img = reader.decode().context("Failed to decode image")?;

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Apply EXIF rotation
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

    // Detect people if enabled
    let (detection, people_count) = if job.detect_people {
        if let Some(det) = detector {
            match det.detect_people(&img_rgb, job.confidence_threshold) {
                Ok(result) => {
                    let count = result.person_count;
                    (Some(result), Some(count))
                }
                Err(_) => (None, None),
            }
        } else {
            (None, None)
        }
    } else {
        (None, None)
    };

    if let Some(pb) = progress {
        pb.inc(1);
    }

    let (crop_x, crop_y, crop_width, crop_height) = compute_crop_area(
        &img_rgb,
        job.target_size.width,
        job.target_size.height,
        detection.as_ref(),
    );

    // Smart crop and resize with people detection awareness
    let mut processing_image = smart_crop_and_resize(
        &img_rgb,
        job.target_size.width,
        job.target_size.height,
        detection.as_ref(),
    )
    .context("Failed to crop and resize image")?;

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Apply color correction
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
        let det = detection.as_ref().unwrap();
        if det.person_count > 0 && crop_width > 0 && crop_height > 0 {
            let scale_x = job.target_size.width as f32 / crop_width as f32;
            let scale_y = job.target_size.height as f32 / crop_height as f32;
            let max_out_x = job.target_size.width.saturating_sub(1);
            let max_out_y = job.target_size.height.saturating_sub(1);
            let max_crop_x = crop_width.saturating_sub(1);
            let max_crop_y = crop_height.saturating_sub(1);

            let mut mapped = Vec::new();

            for (det_x_min, det_y_min, det_x_max, det_y_max, confidence) in det.detection_boxes() {
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
                    mapped.push((x_min, y_min, x_max, y_max, confidence));
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
        processing_image = add_date_annotation(
            &processing_image,
            &job.image.path,
            &job.font_name,
            job.font_size,
            &job.annotation_background,
        )
        .unwrap_or(processing_image);
    }

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

    Ok(ProcessedImage {
        source: job.image.path.clone(),
        temp_path,
        target_size: job.target_size,
        paired: job.paired,
        pair_id: job.pair_id,
        pair_index: job.pair_index,
        orientation: job.image.orientation,
        people_count,
    })
}

/// Combine two processed images with a divider
fn combine_two_images(
    first: &ProcessedImage,
    second: &ProcessedImage,
    is_landscape: bool,
    divider_width: u32,
    divider_color: &HexColor,
    pair_id: usize,
) -> Result<ProcessedImage> {
    // Load both images
    let img1 = image::open(&first.temp_path)
        .with_context(|| format!("Failed to open first image: {}", first.temp_path.display()))?
        .to_rgb8();

    let img2 = image::open(&second.temp_path)
        .with_context(|| {
            format!(
                "Failed to open second image: {}",
                second.temp_path.display()
            )
        })?
        .to_rgb8();

    let (width1, height1) = img1.dimensions();
    let (width2, height2) = img2.dimensions();

    // Create combined image
    let (combined_width, combined_height) = if is_landscape {
        // Horizontal combination: [img1 | divider | img2]
        (width1 + divider_width + width2, height1.max(height2))
    } else {
        // Vertical combination: [img1] [divider] [img2]
        (width1.max(width2), height1 + divider_width + height2)
    };

    let mut combined = image::RgbImage::new(combined_width, combined_height);

    // Fill with divider color
    let div_r = divider_color.red();
    let div_g = divider_color.green();
    let div_b = divider_color.blue();
    for pixel in combined.pixels_mut() {
        *pixel = image::Rgb([div_r, div_g, div_b]);
    }

    if is_landscape {
        // Copy first image to left side
        image::imageops::replace(&mut combined, &img1, 0, 0);
        // Copy second image to right side (after divider)
        image::imageops::replace(&mut combined, &img2, (width1 + divider_width) as i64, 0);
    } else {
        // Copy first image to top
        image::imageops::replace(&mut combined, &img1, 0, 0);
        // Copy second image to bottom (after divider)
        image::imageops::replace(&mut combined, &img2, 0, (height1 + divider_width) as i64);
    }

    // Save combined image to temp file
    let temp_dir = get_temp_dir()?;
    let mut temp_file = Builder::new()
        .prefix(&format!("pfproc_combined_{}_", pair_id))
        .suffix(".png")
        .tempfile_in(&temp_dir)
        .context("Failed to create temporary file for combined image")?;

    combined
        .save(&mut temp_file)
        .context("Failed to save combined image")?;

    let (_file, temp_path) = temp_file
        .keep()
        .context("Failed to keep combined temporary file")?;

    // Return ProcessedImage with combined dimensions
    Ok(ProcessedImage {
        source: first.source.clone(), // Use first image as source reference
        temp_path,
        target_size: Size {
            width: combined_width,
            height: combined_height,
        },
        paired: false, // No longer paired, it's now a single combined image
        pair_id: None,
        pair_index: None,
        orientation: first.orientation,
        people_count: None,
    })
}
