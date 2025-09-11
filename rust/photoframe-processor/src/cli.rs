use clap::{Parser, ValueEnum};
use std::path::PathBuf;

#[derive(Debug, Clone, ValueEnum)]
pub enum ColorType {
    /// Black & White processing (matches auto.sh -t bw)
    #[value(name = "bw")]
    BlackWhite,
    /// 6-color processing (matches auto.sh -t 6c)
    #[value(name = "6c")]
    SixColor,
    /// 6-color processing (matches auto.sh -t 7c)
    #[value(name = "7c")]
    SevenColor,
}

#[derive(Debug, Clone, ValueEnum)]
pub enum OutputType {
    /// Generate only BMP files
    #[value(name = "bmp")]
    Bmp,
    /// Generate only binary files for ESP32
    #[value(name = "bin")]
    Bin,
    /// Generate only JPG files
    #[value(name = "jpg")]
    Jpg,
    /// Generate only PNG files
    #[value(name = "png")]
    Png,
}

#[derive(Parser, Debug)]
#[command(
    name = "photoframe-processor",
    about = "High-performance image processor for ESP32 Photo Frame project",
    long_about = "
ESP32 Photo Frame - Image Processor (Rust Implementation)

This tool processes photos for ESP32-based e-paper photo frames with significant performance
improvements over the bash/ImageMagick pipeline. It handles smart orientation detection,
portrait image pairing, text annotations, and generates optimized binary files.

Key Features:
• 5-10x faster processing than bash scripts
• Parallel batch processing with progress tracking  
• Smart portrait image combination
• EXIF orientation handling
• Floyd-Steinberg dithering for e-paper displays
• ESP32-optimized binary format generation
• People detection for smart cropping via find_subject.py

Example Usage:
  # Basic black & white processing (default 800x480, both BMP and binary files)
  photoframe-processor -i ~/Photos -o ~/processed --auto

  # Process single image file
  photoframe-processor -i ~/Photos/IMG_001.jpg -o ~/processed

  # 6-color processing with only binary output and custom size
  photoframe-processor -i ~/Photos -o ~/processed -t 6c -s 1024x768 \\
    --output-format bin --font \"Arial-Bold\" --pointsize 22 --auto --verbose

  # Multiple input directories and files with JPG-only output
  photoframe-processor -i ~/Photos/2023 -i ~/Photos/2024 -i ~/single.jpg -o ~/processed \\
    -t bw --output-format jpg --auto --jobs 8

  # PNG output format
  photoframe-processor -i ~/Photos -o ~/processed --output-format png --auto

  # With people detection for smart cropping (requires find_subject.py script)
  photoframe-processor -i ~/Photos -o ~/processed --detect-people \\
    --python-script ./scripts/private/find_subject.py --auto --verbose

  # Process images without filename annotations
  photoframe-processor -i ~/Photos -o ~/processed --auto

  # Process images with filename annotations enabled
  photoframe-processor -i ~/Photos -o ~/processed --auto --annotate"
)]
pub struct Args {
    /// Input directories or single image files (can be specified multiple times)
    #[arg(
        short = 'i',
        long = "input",
        required_unless_present = "find_hash",
        value_name = "DIR|FILE"
    )]
    pub input_paths: Vec<PathBuf>,

    /// Output directory for processed images
    #[arg(
        short = 'o',
        long = "output",
        required_unless_present = "find_hash",
        value_name = "DIR",
        default_value = "."
    )]
    pub output_dir: PathBuf,

    /// Target display size (format: WIDTHxHEIGHT, e.g., 800x480)
    #[arg(
        short = 's',
        long = "size",
        default_value = "800x480",
        value_name = "WIDTHxHEIGHT"
    )]
    pub size: String,

    /// Processing type: bw (black & white) or 6c (6-color)
    #[arg(short = 't', long = "type", default_value = "bw")]
    pub processing_type: ColorType,

    /// Output format: bmp (BMP only), bin (binary only), jpg (JPG only), or png (PNG only)
    #[arg(long = "output-format", default_value = "bmp")]
    pub output_format: OutputType,

    /// Comma-separated list of image extensions to process
    #[arg(long = "extensions", default_value = "jpg,jpeg,png,heic,webp,tiff")]
    pub extensions_str: String,

    /// Enable automatic orientation handling (swaps dimensions when needed)
    #[arg(long = "auto")]
    pub auto_process: bool,

    /// Font size for filename annotations
    #[arg(long = "pointsize", default_value = "22", value_name = "SIZE")]
    pub pointsize: u32,

    /// Font specification for annotations. Supports three formats:
    /// - Font name: "Arial" (searches system fonts)
    /// - Font filename: "Arial.ttf" (searches in font directories)  
    /// - Full path: "/System/Library/Fonts/Arial.ttf" (loads directly)
    #[arg(long = "font", default_value = "Arial", value_name = "FONT")]
    pub font: String,

    /// Background color for text annotations (hex with alpha, e.g., #00000040)
    #[arg(
        long = "annotate_background",
        default_value = "#00000040",
        value_name = "COLOR"
    )]
    pub annotate_background: String,

    /// Number of parallel processing jobs (0 = auto-detect CPU cores)
    #[arg(short = 'j', long = "jobs", default_value = "0", value_name = "N")]
    pub jobs: usize,

    /// Enable verbose output with detailed progress information
    #[arg(short = 'v', long = "verbose")]
    pub verbose: bool,

    /// Skip processing and only validate input files
    #[arg(long = "validate-only")]
    pub validate_only: bool,

    /// Benchmark mode: compare processing time against bash auto.sh
    #[arg(long = "benchmark")]
    pub benchmark: bool,

    /// Enable people detection for smart cropping using find_subject.py script
    #[arg(long = "detect-people")]
    pub detect_people: bool,

    /// Path to find_subject.py script for people detection
    #[arg(
        long = "python-script",
        value_name = "FILE",
        help = "Path to find_subject.py script for people detection"
    )]
    pub python_script_path: Option<PathBuf>,

    /// Force processing even if output files already exist (bypass duplicate check)
    #[arg(long = "force")]
    pub force: bool,

    /// Find original filename that matches the given hash (format: 8 hex characters like '7af9ecca')
    #[arg(long = "find-hash", value_name = "HASH")]
    pub find_hash: Option<String>,

    /// Enable debug mode: visualize detection boxes and crop area without processing
    #[arg(long = "debug")]
    pub debug: bool,

    /// Enable filename annotations on processed images (default: false)
    #[arg(long = "annotate")]
    pub annotate: bool,

    /// Enable automatic color correction before processing (uses ImageMagick if available)
    #[arg(long = "auto-color")]
    pub auto_color_correct: bool,
}

impl Args {
    /// Parse the size string into width and height
    pub fn parse_size(&self) -> Result<(u32, u32), String> {
        let parts: Vec<&str> = self.size.split('x').collect();
        if parts.len() != 2 {
            return Err(format!(
                "Invalid size format '{}'. Use WIDTHxHEIGHT (e.g., 800x480)",
                self.size
            ));
        }

        let width = parts[0]
            .parse::<u32>()
            .map_err(|_| format!("Invalid width: '{}'", parts[0]))?;
        let height = parts[1]
            .parse::<u32>()
            .map_err(|_| format!("Invalid height: '{}'", parts[1]))?;

        if width == 0 || height == 0 {
            return Err("Width and height must be greater than 0".to_string());
        }

        if width > 4000 || height > 4000 {
            return Err("Width and height must be less than 4000 pixels".to_string());
        }

        Ok((width, height))
    }

    /// Parse the extensions string into a vector
    pub fn parse_extensions(&self) -> Vec<String> {
        self.extensions_str
            .split(',')
            .map(|s| s.trim().to_lowercase())
            .filter(|s| !s.is_empty())
            .collect()
    }

    // Getters that match the expected interface
    pub fn target_width(&self) -> u32 {
        self.parse_size().unwrap().0
    }

    pub fn target_height(&self) -> u32 {
        self.parse_size().unwrap().1
    }

    pub fn extensions(&self) -> Vec<String> {
        self.parse_extensions()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_size() {
        let args = Args {
            size: "800x480".to_string(),
            ..Default::default()
        };
        assert_eq!(args.parse_size().unwrap(), (800, 480));

        let args = Args {
            size: "1920x1080".to_string(),
            ..Default::default()
        };
        assert_eq!(args.parse_size().unwrap(), (1920, 1080));
    }

    #[test]
    fn test_parse_size_invalid() {
        let args = Args {
            size: "invalid".to_string(),
            ..Default::default()
        };
        assert!(args.parse_size().is_err());

        let args = Args {
            size: "800".to_string(),
            ..Default::default()
        };
        assert!(args.parse_size().is_err());

        let args = Args {
            size: "0x480".to_string(),
            ..Default::default()
        };
        assert!(args.parse_size().is_err());
    }

    #[test]
    fn test_parse_extensions() {
        let args = Args {
            extensions_str: "jpg,png,heic".to_string(),
            ..Default::default()
        };
        assert_eq!(args.parse_extensions(), vec!["jpg", "png", "heic"]);

        let args = Args {
            extensions_str: "JPG, PNG , HEIC ".to_string(),
            ..Default::default()
        };
        assert_eq!(args.parse_extensions(), vec!["jpg", "png", "heic"]);
    }
}

// Default implementation for tests
#[cfg(test)]
impl Default for Args {
    fn default() -> Self {
        Self {
            input_paths: vec![],
            output_dir: PathBuf::new(),
            size: "800x480".to_string(),
            processing_type: ColorType::BlackWhite,
            output_format: OutputType::Bmp,
            extensions_str: "jpg,png".to_string(),
            auto_process: false,
            pointsize: 24,
            font: "Arial".to_string(),
            annotate_background: "#00000040".to_string(),
            jobs: 0,
            verbose: false,
            validate_only: false,
            benchmark: false,
            detect_people: false,
            python_script_path: None,
            force: false,
            find_hash: None,
            debug: false,
            annotate: false,
        }
    }
}
