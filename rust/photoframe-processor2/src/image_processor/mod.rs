mod pairing;
mod types;

pub use types::{ProcessingPlan, SingleImage};

use crate::cli::Args;
use crate::logging::Logger;
use crate::report::{ImageInfo, ImageOrientation, Report};
use crate::types::Orientation;
use std::io;

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
}
