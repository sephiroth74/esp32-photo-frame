use std::path::PathBuf;

use crate::cli::Args;
use crate::logging::Logger;
use crate::types::{OutputType, ReportFormat};
use serde::Serialize;
use tabled::{Table, settings::Style};

#[derive(Debug, Serialize)]
struct JsonReport {
    config: JsonConfig,
    summary: JsonSummary,
    processed_images: Vec<JsonProcessingDetail>,
    paired_images: Vec<JsonPairedDetail>,
}

#[derive(Debug, Serialize)]
struct JsonConfig {
    input_paths: Vec<String>,
    output_dir: String,
    extensions: String,
    output_formats: Vec<String>,
    no_pairing: bool,
}

#[derive(Debug, Serialize)]
struct JsonSummary {
    total_files_discovered: usize,
    invalid_files: usize,
    unpaired_images: usize,
    images_processed: usize,
    image_pairs_created: usize,
    total_output_images: usize,
    failed_images: usize,
}

#[derive(Debug, Serialize)]
struct JsonProcessingDetail {
    source: String,
    output: String,
    size: String,
    faces: usize,
    total_ms: u128,
    detect_ms: u128,
}

#[derive(Debug, Serialize)]
struct JsonPairedDetail {
    source: String,
    output: String,
    size: String,
    faces: usize,
    total_ms: u128,
    detect_ms: u128,
}

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
    // Detailed processing information
    pub processed_details: Vec<ProcessingDetail>,
    pub paired_details: Vec<PairedImageDetail>,
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
}

/// Detailed information about a processed image
#[derive(Debug, Clone, tabled::Tabled)]
pub struct ProcessingDetail {
    #[tabled(rename = "Source")]
    pub source_path: String,
    #[tabled(rename = "Output")]
    pub output_path: String,
    #[tabled(rename = "Size")]
    pub size: String,
    #[tabled(rename = "Faces")]
    pub faces_detected: usize,
    #[tabled(rename = "Total (ms)")]
    pub processing_time_ms: u128,
    #[tabled(rename = "Detect (ms)")]
    pub detection_time_ms: u128,
}

/// Detailed information about a paired image output
#[derive(Debug, Clone, tabled::Tabled)]
pub struct PairedImageDetail {
    #[tabled(rename = "Source")]
    pub source_path: String,
    #[tabled(rename = "Output")]
    pub output_path: String,
    #[tabled(rename = "Size")]
    pub size: String,
    #[tabled(rename = "Faces")]
    pub faces_detected: usize,
    #[tabled(rename = "Total (ms)")]
    pub processing_time_ms: u128,
    #[tabled(rename = "Detect (ms)")]
    pub detection_time_ms: u128,
}

impl ProcessingDetail {
    pub fn new(
        source_path: PathBuf,
        output_path: PathBuf,
        width: u32,
        height: u32,
        faces_detected: usize,
        processing_time_ms: u128,
        detection_time_ms: u128,
    ) -> Self {
        let size = format!("{}x{}", width, height);
        let source_name = source_path
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("?")
            .to_string();
        let output_name = output_path
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("?")
            .to_string();
        Self {
            source_path: source_name,
            output_path: output_name,
            size,
            faces_detected,
            processing_time_ms,
            detection_time_ms,
        }
    }
}

impl PairedImageDetail {
    pub fn new(
        output_path: Option<PathBuf>,
        source_path: PathBuf,
        width: u32,
        height: u32,
        faces_detected: usize,
        processing_time_ms: u128,
        detection_time_ms: u128,
    ) -> Self {
        let size = format!("{}x{}", width, height);
        let output_name = output_path
            .as_ref()
            .and_then(|p| p.file_name())
            .and_then(|n| n.to_str())
            .unwrap_or("")
            .to_string();
        let source_name = source_path
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("?")
            .to_string();
        Self {
            source_path: source_name,
            output_path: output_name,
            size,
            faces_detected,
            processing_time_ms,
            detection_time_ms,
        }
    }
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
            processed_details: Vec::new(),
            paired_details: Vec::new(),
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

    pub fn set_processing_details(
        &mut self,
        processed_details: Vec<ProcessingDetail>,
        paired_details: Vec<PairedImageDetail>,
    ) {
        self.processed_details = processed_details;
        self.paired_details = paired_details;
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

        self.print_config(logger);

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

    /// Generate full report with detailed table
    fn generate_full(&self, logger: &Logger) {
        logger.divider();
        logger.info("DETAILED PROCESSING REPORT");
        logger.divider();
        logger.info("");

        self.print_config(logger);

        if !self.processed_details.is_empty() {
            logger.section("PROCESSED IMAGES");
            self.print_processed_table(logger);
            logger.info("");
        }

        if !self.paired_details.is_empty() {
            logger.section("PAIRED IMAGES");
            self.print_paired_table(logger);
            logger.info("");
        }

        // Summary
        logger.section("SUMMARY");
        let total_output = self.processed_count + self.paired_count;
        logger.config_item(
            "Total files discovered",
            &self.discovered_files.len().to_string(),
        );
        logger.config_item("Invalid files", &self.invalid_images.len().to_string());
        logger.config_item("Images processed", &self.processed_count.to_string());
        logger.config_item("Image pairs created", &self.paired_count.to_string());
        logger.config_item("Total output images", &total_output.to_string());
        logger.config_item("Failed images", &self.failed_count.to_string());
        logger.info("");
        logger.divider();
    }

    /// Print detailed table of processed images
    fn print_processed_table(&self, logger: &Logger) {
        if self.processed_details.is_empty() {
            return;
        }

        let table = Table::new(&self.processed_details)
            .with(Style::rounded())
            .to_string();

        for line in table.lines() {
            logger.info(line);
        }
    }

    /// Print detailed table of paired images
    fn print_paired_table(&self, logger: &Logger) {
        if self.paired_details.is_empty() {
            return;
        }

        let table = Table::new(&self.paired_details)
            .with(Style::rounded())
            .to_string();

        for line in table.lines() {
            logger.info(line);
        }
    }

    /// Generate JSON report (to be implemented)
    fn generate_json(&self, logger: &Logger) {
        let report = self.build_json_report();
        match serde_json::to_string_pretty(&report) {
            Ok(json) => {
                for line in json.lines() {
                    logger.info(line);
                }
            }
            Err(err) => logger.error(&format!("Failed to generate JSON report: {}", err)),
        }
    }

    pub fn to_json_value(&self) -> serde_json::Value {
        serde_json::to_value(self.build_json_report()).unwrap_or_else(|_| serde_json::Value::Null)
    }

    fn build_json_report(&self) -> JsonReport {
        let output_formats = self
            .config
            .output_formats
            .iter()
            .map(|f| match f {
                OutputType::Bmp => "bmp".to_string(),
                OutputType::Pfr1 => "pfr1".to_string(),
                OutputType::Jpg => "jpg".to_string(),
                OutputType::Png => "png".to_string(),
            })
            .collect::<Vec<_>>();

        let total_output = self.processed_count + self.paired_count;

        JsonReport {
            config: JsonConfig {
                input_paths: self
                    .config
                    .input_paths
                    .iter()
                    .map(|p| p.display().to_string())
                    .collect(),
                output_dir: self.config.output_dir.display().to_string(),
                extensions: self.config.extensions.clone(),
                output_formats,
                no_pairing: self.config.no_pairing,
            },
            summary: JsonSummary {
                total_files_discovered: self.discovered_files.len(),
                invalid_files: self.invalid_images.len(),
                unpaired_images: self.unpaired_images.len(),
                images_processed: self.processed_count,
                image_pairs_created: self.paired_count,
                total_output_images: total_output,
                failed_images: self.failed_count,
            },
            processed_images: self
                .processed_details
                .iter()
                .map(|d| JsonProcessingDetail {
                    source: d.source_path.clone(),
                    output: d.output_path.clone(),
                    size: d.size.clone(),
                    faces: d.faces_detected,
                    total_ms: d.processing_time_ms,
                    detect_ms: d.detection_time_ms,
                })
                .collect(),
            paired_images: self
                .paired_details
                .iter()
                .map(|d| JsonPairedDetail {
                    source: d.source_path.clone(),
                    output: d.output_path.clone(),
                    size: d.size.clone(),
                    faces: d.faces_detected,
                    total_ms: d.processing_time_ms,
                    detect_ms: d.detection_time_ms,
                })
                .collect(),
        }
    }

    fn print_config(&self, logger: &Logger) {
        logger.section("CONFIG");

        let input_paths = if self.config.input_paths.is_empty() {
            "-".to_string()
        } else {
            self.config
                .input_paths
                .iter()
                .map(|p| p.display().to_string())
                .collect::<Vec<_>>()
                .join(", ")
        };

        let output_formats = if self.config.output_formats.is_empty() {
            "-".to_string()
        } else {
            self.config
                .output_formats
                .iter()
                .map(|f| match f {
                    OutputType::Bmp => "bmp",
                    OutputType::Pfr1 => "pfr1",
                    OutputType::Jpg => "jpg",
                    OutputType::Png => "png",
                })
                .collect::<Vec<_>>()
                .join(", ")
        };

        logger.config_item("Input paths", &input_paths);
        logger.config_item("Output dir", &self.config.output_dir.display().to_string());
        logger.config_item("Extensions", &self.config.extensions);
        logger.config_item("Output formats", &output_formats);
        logger.config_item("No pairing", &self.config.no_pairing.to_string());
        logger.info("");
    }
}
