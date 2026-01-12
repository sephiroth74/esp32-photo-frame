use crate::cli::{Args, ColorType, DitherMethod, TargetOrientation};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

/// Represents the Flutter config file format
#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ConfigFile {
    pub name: Option<String>,
    pub file_path: Option<String>,
    pub last_modified: Option<String>,
    pub config: ProcessingConfigJson,
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ProcessingConfigJson {
    pub input_path: Option<String>,
    pub output_path: Option<String>,
    pub display_type: Option<String>,
    pub orientation: Option<String>,
    pub auto_color_correct: Option<bool>,
    pub dither_method: Option<String>,
    pub dither_strength: Option<f32>,
    pub contrast: Option<i32>,
    pub brightness: Option<i32>,
    pub saturation_boost: Option<f32>,
    pub auto_optimize: Option<bool>,
    pub output_bmp: Option<bool>,
    pub output_bin: Option<bool>,
    pub output_jpg: Option<bool>,
    pub output_png: Option<bool>,
    pub detect_people: Option<bool>,
    pub confidence_threshold: Option<f32>,
    pub annotate: Option<bool>,
    pub font: Option<String>,
    pub pointsize: Option<u32>,
    pub annotate_background: Option<String>,
    pub divider_width: Option<u32>,
    pub divider_color: Option<String>,
    pub force: Option<bool>,
    pub dry_run: Option<bool>,
    pub debug: Option<bool>,
    pub report: Option<bool>,
    pub jobs: Option<usize>,
    pub extensions: Option<String>,
    pub processor_binary_path: Option<String>,
}

impl Args {
    /// Load configuration from a JSON file and merge with command-line arguments
    /// Command-line arguments take precedence over config file values
    pub fn load_and_merge_config(&mut self) -> Result<()> {
        if let Some(config_path) = self.config_file.clone() {
            let contents = fs::read_to_string(&config_path)
                .with_context(|| format!("Failed to read config file: {:?}", config_path))?;

            let config: ConfigFile = serde_json::from_str(&contents)
                .with_context(|| format!("Failed to parse config file: {:?}", config_path))?;

            self.merge_from_config(config.config)?;

            if self.verbose {
                eprintln!("Loaded configuration from: {:?}", config_path);
            }
        }
        Ok(())
    }

    fn merge_from_config(&mut self, config: ProcessingConfigJson) -> Result<()> {
        // We check if arguments were explicitly provided on the command line
        let args_from_cli = std::env::args().collect::<Vec<_>>();

        // Input/output paths - only apply if not specified on CLI
        if !args_from_cli.iter().any(|a| a == "-i" || a == "--input") {
            if let Some(input) = config.input_path {
                self.input_paths = vec![PathBuf::from(input)];
            }
        }

        if !args_from_cli.iter().any(|a| a == "-o" || a == "--output") {
            if let Some(output) = config.output_path {
                self.output_dir = PathBuf::from(output);
            }
        }

        // Display type - only apply if not specified on CLI
        if !args_from_cli.iter().any(|a| a == "-t" || a == "--type") {
            if let Some(display_type) = config.display_type {
                self.processing_type = match display_type.as_str() {
                    "bw" | "blackWhite" => ColorType::BlackWhite,
                    "6c" | "sixColor" => ColorType::SixColor,
                    _ => self.processing_type.clone(),
                };
            }
        }

        // Orientation
        if !args_from_cli.iter().any(|a| a == "--orientation") {
            if let Some(orientation) = config.orientation {
                self.target_orientation = match orientation.as_str() {
                    "landscape" => TargetOrientation::Landscape,
                    "portrait" => TargetOrientation::Portrait,
                    _ => self.target_orientation.clone(),
                };
            }
        }

        // Output formats - build from individual booleans if using default
        if self.output_formats_str == "bmp" {
            let mut formats = Vec::new();
            if config.output_bmp.unwrap_or(false) {
                formats.push("bmp");
            }
            if config.output_bin.unwrap_or(false) {
                formats.push("bin");
            }
            if config.output_jpg.unwrap_or(false) {
                formats.push("jpg");
            }
            if config.output_png.unwrap_or(false) {
                formats.push("png");
            }
            if !formats.is_empty() {
                self.output_formats_str = formats.join(",");
            }
        }

        // Dithering method
        if !args_from_cli.iter().any(|a| a == "--dithering") {
            if let Some(method) = config.dither_method {
                self.dithering_method = match method.as_str() {
                    "floyd-steinberg" | "floydSteinberg" => DitherMethod::FloydSteinberg,
                    "atkinson" => DitherMethod::Atkinson,
                    "stucki" => DitherMethod::Stucki,
                    "jarvis" | "jarvis-judice-ninke" | "jarvisJudiceNinke" => {
                        DitherMethod::JarvisJudiceNinke
                    }
                    "ordered" => DitherMethod::Ordered,
                    _ => self.dithering_method.clone(),
                };
            }
        }

        // Numeric parameters - only apply if using defaults
        if self.dither_strength == 1.0 {
            if let Some(strength) = config.dither_strength {
                self.dither_strength = strength;
            }
        }

        if self.contrast == 0 {
            if let Some(contrast) = config.contrast {
                self.contrast = contrast;
            }
        }

        if self.brightness == 0 {
            if let Some(brightness) = config.brightness {
                self.brightness = brightness;
            }
        }

        if self.saturation_boost == 1.1 {
            if let Some(boost) = config.saturation_boost {
                self.saturation_boost = boost;
            }
        }

        // Boolean flags - only apply if currently false (default)
        if !self.auto_optimize {
            self.auto_optimize = config.auto_optimize.unwrap_or(false);
        }

        if !self.auto_color_correct {
            self.auto_color_correct = config.auto_color_correct.unwrap_or(false);
        }

        #[cfg(feature = "ai")]
        {
            if !self.detect_people {
                self.detect_people = config.detect_people.unwrap_or(false);
            }
        }

        if !self.annotate {
            self.annotate = config.annotate.unwrap_or(false);
        }

        if !self.force {
            self.force = config.force.unwrap_or(false);
        }

        if !self.dry_run {
            self.dry_run = config.dry_run.unwrap_or(false);
        }

        if !self.debug {
            self.debug = config.debug.unwrap_or(false);
        }

        if !self.report {
            self.report = config.report.unwrap_or(false);
        }

        // String parameters - only apply if using defaults
        if self.font == "Arial" {
            if let Some(font) = config.font {
                self.font = font;
            }
        }

        if !args_from_cli.iter().any(|a| a == "--pointsize") {
            if let Some(size) = config.pointsize {
                self.pointsize = size;
            }
        }

        if self.annotate_background == "#00000040" {
            if let Some(bg) = config.annotate_background {
                self.annotate_background = bg;
            }
        }

        if self.divider_width == 3 {
            if let Some(width) = config.divider_width {
                self.divider_width = width;
            }
        }

        if self.divider_color == "#FFFFFF" {
            if let Some(color) = config.divider_color {
                self.divider_color = color;
            }
        }

        if self.extensions_str == "jpg,jpeg,png,heic,webp,tiff" {
            if let Some(ext) = config.extensions {
                self.extensions_str = ext;
            }
        }

        if self.jobs == 0 {
            if let Some(jobs) = config.jobs {
                self.jobs = jobs;
            }
        }

        #[cfg(feature = "ai")]
        {
            if self.confidence_threshold == 0.6 {
                if let Some(threshold) = config.confidence_threshold {
                    self.confidence_threshold = threshold;
                }
            }
        }

        Ok(())
    }
}
