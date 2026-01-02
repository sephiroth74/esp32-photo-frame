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
    /// 7-color processing (matches auto.sh -t 7c)
    #[value(name = "7c")]
    SevenColor,
}

#[derive(Debug, Clone, ValueEnum, PartialEq, Eq, Hash)]
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

#[derive(Debug, Clone, ValueEnum, PartialEq, Eq)]
pub enum DitherMethod {
    /// Floyd-Steinberg with perceptual color weighting (best for gradients)
    #[value(name = "floyd-steinberg")]
    FloydSteinberg,
    /// Atkinson dithering (preserves brightness, good for photos)
    #[value(name = "atkinson")]
    Atkinson,
    /// Stucki dithering (similar to Floyd-Steinberg but more diffused)
    #[value(name = "stucki")]
    Stucki,
    /// Jarvis-Judice-Ninke dithering (very diffused, best for photos)
    #[value(name = "jarvis")]
    JarvisJudiceNinke,
    /// Ordered dithering with Bayer matrix (less noise, good for text)
    #[value(name = "ordered")]
    Ordered,
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

  # Multiple output formats (creates subdirectories: bmp/, bin/, jpg/)
  photoframe-processor -i ~/Photos -o ~/processed --output-format bmp,bin,jpg --auto

  # PNG output format only
  photoframe-processor -i ~/Photos -o ~/processed --output-format png --auto

  # With people detection for smart cropping (requires find_subject.py script)
  photoframe-processor -i ~/Photos -o ~/processed --detect-people \\
    --python-script ./scripts/private/find_subject.py --auto --verbose

  # Process images without filename annotations
  photoframe-processor -i ~/Photos -o ~/processed --auto

  # Process images with filename annotations enabled
  photoframe-processor -i ~/Photos -o ~/processed --auto --annotate

  # Find original filename from an 8-character hash
  photoframe-processor --find-hash a1b2c3d4

  # Decode combined portrait filename to show original filenames
  photoframe-processor --find-original combined_bw_aW1hZ2Ux_aW1hZ2Uy.bin

  # Dry run mode: simulate processing without creating files
  photoframe-processor -i ~/Photos -o ~/processed --auto --dry-run --verbose"
)]
pub struct Args {
    /// Input directories or single image files (can be specified multiple times)
    #[arg(
        short = 'i',
        long = "input",
        required_unless_present_any = ["find_original", "find_hash"],
        value_name = "DIR|FILE"
    )]
    pub input_paths: Vec<PathBuf>,

    /// Output directory for processed images
    #[arg(
        short = 'o',
        long = "output",
        required_unless_present_any = ["find_original", "find_hash"],
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

    /// Output formats: comma-separated list of bmp, bin, jpg, png (e.g., "bmp,bin" or "jpg")
    #[arg(long = "output-format", default_value = "bmp")]
    pub output_formats_str: String,

    /// Comma-separated list of image extensions to process
    #[arg(long = "extensions", default_value = "jpg,jpeg,png,heic,webp,tiff")]
    pub extensions_str: String,

    /// Enable automatic orientation handling (swaps dimensions when needed)
    #[arg(long = "auto-orientation")]
    pub auto_orientation: bool,

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

    /// Width of the divider line between combined portrait images in pixels
    #[arg(long = "divider-width", default_value = "3", value_name = "WIDTH")]
    pub divider_width: u32,

    /// Color of the divider line between combined portrait images (hex RGB, e.g., #FFFFFF for white)
    #[arg(
        long = "divider-color",
        default_value = "#FFFFFF",
        value_name = "COLOR"
    )]
    pub divider_color: String,

    /// Dithering method for color quantization
    #[arg(
        long = "dithering",
        default_value = "floyd-steinberg",
        value_name = "METHOD",
        help = "Dithering algorithm: floyd-steinberg (best gradients), atkinson (bright), stucki (diffused), jarvis (photos), ordered (text)"
    )]
    pub dithering_method: DitherMethod,

    /// Dithering strength (0.0-2.0, default 1.0). Higher values = stronger dithering effect
    #[arg(
        long = "dither-strength",
        default_value = "1.0",
        value_name = "STRENGTH",
        help = "Dithering strength multiplier. 1.0 = normal, <1.0 = subtle, >1.0 = pronounced"
    )]
    pub dither_strength: f32,

    /// Contrast adjustment (-1.0 to 1.0, default 0.0). Positive = increase, negative = decrease
    #[arg(
        long = "contrast",
        default_value = "0.0",
        value_name = "ADJUSTMENT",
        help = "Contrast adjustment: -1.0 (low contrast) to 1.0 (high contrast), 0.0 = no change"
    )]
    pub contrast: f32,

    /// Enable automatic per-image parameter optimization (overrides manual dithering/contrast settings)
    #[arg(
        long = "auto-optimize",
        help = "Automatically select optimal dithering, strength, and contrast for each image based on content analysis"
    )]
    pub auto_optimize: bool,

    /// Generate processing report table at the end
    #[arg(
        long = "report",
        help = "Display formatted table with processing parameters for all images"
    )]
    pub report: bool,

    /// Number of parallel processing jobs (0 = auto-detect CPU cores)
    #[arg(short = 'j', long = "jobs", default_value = "0", value_name = "N")]
    pub jobs: usize,

    /// Enable verbose output with detailed progress information
    #[arg(short = 'v', long = "verbose")]
    pub verbose: bool,

    /// Skip processing and only validate input files
    #[arg(long = "validate-only")]
    pub validate_only: bool,

    /// Enable people detection for smart cropping using find_subject.py script
    #[arg(long = "detect-people")]
    pub detect_people: bool,

    /// Path to find_subject.py script for people detection
    #[arg(
        long = "python-script",
        value_name = "FILE",
        help = "Path to find_subject.py script for people detection (uses system Python)"
    )]
    pub python_script_path: Option<PathBuf>,

    /// Force processing even if output files already exist (bypass duplicate check)
    #[arg(long = "force")]
    pub force: bool,

    /// Find original filenames from a combined filename (e.g., 'combined_bw_BASE64_BASE64.bin')
    #[arg(long = "find-original", value_name = "FILENAME")]
    pub find_original: Option<String>,

    /// Find original filename for given hash (8-character hex hash)
    #[arg(long = "find-hash", value_name = "HASH")]
    pub find_hash: Option<String>,

    /// Enable debug mode: visualize detection boxes and crop area with correct orientation
    #[arg(long = "debug")]
    pub debug: bool,

    /// Enable filename annotations on processed images (default: false)
    #[arg(long = "annotate")]
    pub annotate: bool,

    /// Enable automatic color correction before processing (uses ImageMagick if available)
    #[arg(long = "auto-color")]
    pub auto_color_correct: bool,

    /// Perform a dry run: simulate processing and show what would be generated without creating files
    #[arg(long = "dry-run")]
    pub dry_run: bool,

    /// Confidence threshold for people detection (0.0-1.0, requires --detect-people)
    #[arg(long = "confidence", default_value = "0.6", value_name = "THRESHOLD")]
    pub confidence_threshold: f32,
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

    /// Parse the output formats string into a vector of OutputType
    pub fn parse_output_formats(&self) -> Result<Vec<OutputType>, String> {
        let mut formats = Vec::new();

        for format_str in self.output_formats_str.split(',') {
            let format_str = format_str.trim().to_lowercase();
            if format_str.is_empty() {
                continue;
            }

            match format_str.as_str() {
                "bmp" => formats.push(OutputType::Bmp),
                "bin" => formats.push(OutputType::Bin),
                "jpg" | "jpeg" => formats.push(OutputType::Jpg),
                "png" => formats.push(OutputType::Png),
                _ => {
                    return Err(format!(
                        "Invalid output format '{}'. Valid formats: bmp, bin, jpg, png",
                        format_str
                    ))
                }
            }
        }

        if formats.is_empty() {
            return Err("No valid output formats specified".to_string());
        }

        // Remove duplicates while preserving order
        let mut unique_formats = Vec::new();
        for format in formats {
            if !unique_formats.contains(&format) {
                unique_formats.push(format);
            }
        }

        Ok(unique_formats)
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

    pub fn output_formats(&self) -> Vec<OutputType> {
        self.parse_output_formats()
            .unwrap_or_else(|_| vec![OutputType::Bmp])
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
            output_formats_str: "bmp".to_string(),
            extensions_str: "jpg,png".to_string(),
            auto_orientation: false,
            pointsize: 24,
            font: "Arial".to_string(),
            annotate_background: "#00000040".to_string(),
            jobs: 0,
            verbose: false,
            validate_only: false,
            detect_people: false,
            python_script_path: None,
            force: false,
            find_original: None,
            find_hash: None,
            debug: false,
            annotate: false,
            auto_color_correct: false,
            dry_run: false,
            confidence_threshold: 0.6,
            divider_width: 3,
            divider_color: "#FFFFFF".to_string(),
        }
    }
}
