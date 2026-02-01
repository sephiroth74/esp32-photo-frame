mod color_correction;
mod pairing;
mod resize;
mod types;

use color_correction::apply_color_correction;
use resize::resize_to_cover;
pub use types::{ProcessingPlan, SingleImage};

use crate::cli::Args;
use crate::logging::Logger;
use crate::report::{ImageInfo, ImageOrientation, Report};
use crate::types::{Orientation, Size};
use anyhow::{Context, Result};
use image::imageops::FilterType;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use rayon::ThreadPoolBuilder;
use rayon::prelude::*;
use std::io;
use std::path::PathBuf;
use std::time::Duration;
use tempfile::{Builder, TempPath};

pub struct ImageProcessor<'a> {
    args: &'a Args,
    logger: &'a Logger,
}

impl<'a> ImageProcessor<'a> {
    pub fn new(args: &'a Args, logger: &'a Logger) -> Self {
        Self { args, logger }
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
                    let pb = multi.add(ProgressBar::new(5));
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

                    let result = process_job(&job, pb.as_ref());

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

        if let Some(global) = global_bar {
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

        Ok(ProcessingResult { processed, failed })
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
                auto_color_correct: self.args.auto_color_correct,
                brightness: self.args.brightness,
                contrast: self.args.contrast,
                saturation: self.args.saturation,
            });
        }

        for pair in &plan.paired_images {
            jobs.push(ProcessingJob {
                image: pair.first.clone(),
                target_size: paired_size,
                paired: true,
                auto_color_correct: self.args.auto_color_correct,
                brightness: self.args.brightness,
                contrast: self.args.contrast,
                saturation: self.args.saturation,
            });
            jobs.push(ProcessingJob {
                image: pair.second.clone(),
                target_size: paired_size,
                paired: true,
                auto_color_correct: self.args.auto_color_correct,
                brightness: self.args.brightness,
                contrast: self.args.contrast,
                saturation: self.args.saturation,
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
    auto_color_correct: bool,
    brightness: i32,
    contrast: i32,
    saturation: u32,
}

#[derive(Debug)]
pub struct ProcessingResult {
    pub processed: Vec<ProcessedImage>,
    pub failed: Vec<PathBuf>,
}

#[derive(Debug)]
pub struct ProcessedImage {
    pub source: PathBuf,
    pub temp_path: TempPath,
    pub target_size: Size,
    pub paired: bool,
    pub orientation: ImageOrientation,
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

fn process_job(job: &ProcessingJob, progress: Option<&ProgressBar>) -> Result<ProcessedImage> {
    let reader = image::ImageReader::open(&job.image.path)
        .with_context(|| format!("Failed to open image: {}", job.image.path.display()))?
        .with_guessed_format()
        .context("Failed to guess image format")?;

    let mut img = reader.decode().context("Failed to decode image")?;

    if let Some(pb) = progress {
        pb.inc(1);
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

    let resized = resize_to_cover(&img, job.target_size, FilterType::Lanczos3);

    if let Some(pb) = progress {
        pb.inc(1);
    }

    // Apply color correction
    let resized_rgb = resized.to_rgb8();
    let corrected = apply_color_correction(
        &resized_rgb,
        job.auto_color_correct,
        job.brightness,
        job.contrast,
        job.saturation,
    )
    .context("Failed to apply color correction")?;

    if let Some(pb) = progress {
        pb.inc(1);
    }

    let temp_dir = std::env::temp_dir();
    let mut temp_file = Builder::new()
        .prefix("pfproc_")
        .suffix(".png")
        .tempfile_in(&temp_dir)
        .context("Failed to create temporary file")?;

    corrected
        .save(&mut temp_file)
        .context("Failed to write temporary image")?;

    if let Some(pb) = progress {
        pb.inc(1);
    }

    Ok(ProcessedImage {
        source: job.image.path.clone(),
        temp_path: temp_file.into_temp_path(),
        target_size: job.target_size,
        paired: job.paired,
        orientation: job.image.orientation,
    })
}
