use std::path::PathBuf;

use crate::cli::Args;
use crate::types::OutputType;

#[derive(Debug, Clone)]
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
    pub people_detected: Option<usize>,
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
        }
    }

    pub fn set_image_results(&mut self, valid: Vec<ImageInfo>, invalid: Vec<PathBuf>) {
        self.valid_images = valid;
        self.invalid_images = invalid;
    }
}
