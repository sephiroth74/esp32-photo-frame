use std::path::PathBuf;

use crate::cli::Args;
use crate::logging::Logger;
use crate::types::{OutputType, ReportFormat};

#[derive(Debug, Clone)]
#[allow(dead_code)]
pub struct ReportConfig {
    pub input_paths: Vec<PathBuf>,
    pub output_dir: PathBuf,
    pub extensions: String,
    pub output_formats: Vec<OutputType>,
    pub no_pairing: bool,
}

#[derive(Debug, Clone)]
#[allow(dead_code)]
pub struct Report {
    pub config: ReportConfig,
    pub discovered_files: Vec<PathBuf>,
    pub valid_images: Vec<ImageInfo>,
    pub invalid_images: Vec<PathBuf>,
    pub unpaired_images: Vec<ImageInfo>,
    // Processing results
    pub processed_count: usize,
    pub failed_count: usize,
    pub paired_count: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RotationDegrees {
    Deg0,
    Deg90,
    Deg180,
    Deg270,
}

/// Represents the orientation of an image
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageOrientation {
    Landscape,
    Portrait,
}

#[derive(Debug, Clone)]
pub struct ImageInfo {
    pub path: PathBuf,
    pub rotation: RotationDegrees,
    pub orientation: ImageOrientation,
    #[allow(dead_code)]
    pub faces_detected: Option<usize>,
}

impl Report {
    pub fn new(args: &Args, discovered_files: Vec<PathBuf>) -> Self {
        Self {
            config: ReportConfig {
                input_paths: args.input.clone(),
                output_dir: args.output.clone(),
                extensions: args.extensions.clone(),
                output_formats: args.output_formats.clone(),
                no_pairing: args.no_pairing,
            },
            discovered_files,
            valid_images: Vec::new(),
            invalid_images: Vec::new(),
            unpaired_images: Vec::new(),
            processed_count: 0,
            failed_count: 0,
            paired_count: 0,
        }
    }

    pub fn set_image_results(&mut self, valid: Vec<ImageInfo>, invalid: Vec<PathBuf>) {
        self.valid_images = valid;
        self.invalid_images = invalid;
    }

    pub fn set_processing_results(&mut self, processed: usize, failed: usize, paired: usize) {
        self.processed_count = processed;
        self.failed_count = failed;
        self.paired_count = paired;
    }

    /// Generate and display report based on format
    pub fn generate(&self, logger: &Logger, format: ReportFormat) {
        match format {
            ReportFormat::Plain => self.generate_plain(logger),
            ReportFormat::Full => self.generate_full(logger),
            ReportFormat::Json => self.generate_json(logger),
        }
    }

    /// Generate plain text report
    fn generate_plain(&self, logger: &Logger) {
        logger.divider();
        logger.info("PROCESSING REPORT");
        logger.divider();
        logger.info("");

        // Calculate total output images
        let total_output = self.processed_count + self.paired_count;

        logger.config_item(
            "Total files discovered",
            &self.discovered_files.len().to_string(),
        );
        logger.config_item("Invalid files", &self.invalid_images.len().to_string());
        logger.config_item("Unpaired images", &self.unpaired_images.len().to_string());
        logger.config_item("Images processed", &self.processed_count.to_string());
        logger.config_item("Image pairs created", &self.paired_count.to_string());
        logger.config_item("Total output images", &total_output.to_string());
        logger.config_item("Failed images", &self.failed_count.to_string());

        logger.info("");
        logger.divider();
    }

    /// Generate full report (to be implemented)
    fn generate_full(&self, logger: &Logger) {
        logger.info("Full report format not yet implemented");
    }

    /// Generate JSON report (to be implemented)
    fn generate_json(&self, logger: &Logger) {
        logger.info("JSON report format not yet implemented");
    }
}
