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
}

#[derive(Debug, Clone, ValueEnum)]
pub enum OutputType {
    /// Generate only BMP files
    #[value(name = "bmp")]
    Bmp,
    /// Generate only binary files for ESP32
    #[value(name = "bin")]
    Bin,
    /// Generate both BMP and binary files
    #[value(name = "both")]
    Both,
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

Example Usage:
  # Basic black & white processing (default 800x480, both BMP and binary files)
  photoframe-processor -i ~/Photos -o ~/processed --auto

  # Process single image file
  photoframe-processor -i ~/Photos/IMG_001.jpg -o ~/processed

  # 6-color processing with only binary output and custom size
  photoframe-processor -i ~/Photos -o ~/processed -t 6c -s 1024x768 \\
    --output-format bin --font \"Arial-Bold\" --pointsize 32 --auto --verbose

  # Multiple input directories and files with BMP-only output
  photoframe-processor -i ~/Photos/2023 -i ~/Photos/2024 -i ~/single.jpg -o ~/processed \\
    -t bw --output-format bmp --auto --jobs 8"
)]
#[cfg_attr(feature = "ai", command(
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
• YOLO-based people detection for smart cropping (AI feature enabled)

Example Usage:
  # Basic black & white processing (default 800x480, both BMP and binary files)
  photoframe-processor -i ~/Photos -o ~/processed --auto

  # Process single image file
  photoframe-processor -i ~/Photos/IMG_001.jpg -o ~/processed

  # 6-color processing with only binary output and custom size
  photoframe-processor -i ~/Photos -o ~/processed -t 6c -s 1024x768 \\
    --output-format bin --font \"Arial-Bold\" --pointsize 32 --auto --verbose

  # Multiple input directories and files with BMP-only output
  photoframe-processor -i ~/Photos/2023 -i ~/Photos/2024 -i ~/single.jpg -o ~/processed \\
    -t bw --output-format bmp --auto --jobs 8

  # With YOLO people detection for smart cropping (requires ai feature and YOLO models)
  photoframe-processor -i ~/Photos -o ~/processed --detect-people --yolo-assets ./assets \\
    --yolo-confidence 0.6 --yolo-nms 0.4 --auto --verbose

For complete documentation, see: https://github.com/sephiroth74/arduino/esp32-photo-frame
"
))]
pub struct Args {
    /// Input directories or single image files (can be specified multiple times)
    #[arg(short = 'i', long = "input", required = true, value_name = "DIR|FILE")]
    pub input_paths: Vec<PathBuf>,

    /// Output directory for processed images
    #[arg(short = 'o', long = "output", required = true, value_name = "DIR")]
    pub output_dir: PathBuf,

    /// Target display size (format: WIDTHxHEIGHT, e.g., 800x480)
    #[arg(short = 's', long = "size", default_value = "800x480", value_name = "WIDTHxHEIGHT")]
    pub size: String,

    /// Processing type: bw (black & white) or 6c (6-color)
    #[arg(short = 't', long = "type", default_value = "bw")]
    pub processing_type: ColorType,

    /// Output format: bmp (BMP only), bin (binary only), or both
    #[arg(long = "output-format", default_value = "both")]
    pub output_format: OutputType,

    /// Custom palette file for color processing (optional)
    #[arg(short = 'p', long = "palette", value_name = "FILE")]
    pub palette: Option<PathBuf>,

    /// Comma-separated list of image extensions to process
    #[arg(long = "extensions", default_value = "jpg,jpeg,png,heic,webp,tiff")]
    pub extensions_str: String,

    /// Enable automatic orientation handling (swaps dimensions when needed)
    #[arg(long = "auto")]
    pub auto_process: bool,

    /// Font size for filename annotations
    #[arg(long = "pointsize", default_value = "24", value_name = "SIZE")]
    pub pointsize: u32,

    /// Font family for annotations (Note: Currently uses built-in bitmap font, custom fonts not yet supported)
    #[arg(long = "font", default_value = "UbuntuMono-Nerd-Font-Bold", value_name = "FONT")]
    pub font: String,

    /// Background color for text annotations (hex with alpha, e.g., #00000040)
    #[arg(long = "annotate_background", default_value = "#00000040", value_name = "COLOR")]
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

    /// Enable YOLO-based people detection for smart cropping (requires ai feature)
    #[cfg(feature = "ai")]
    #[arg(long = "detect-people")]
    pub detect_people: bool,

    /// Path to YOLO assets directory containing yolov3.cfg and yolov3.weights
    #[cfg(feature = "ai")]
    #[arg(long = "yolo-assets", value_name = "DIR", 
          help = "Directory containing yolov3.cfg and yolov3.weights files")]
    pub yolo_assets_dir: Option<PathBuf>,

    /// YOLO confidence threshold for people detection (0.0-1.0)
    #[cfg(feature = "ai")]
    #[arg(long = "yolo-confidence", default_value = "0.5", value_name = "THRESHOLD")]
    pub yolo_confidence: f32,

    /// YOLO NMS (Non-Maximum Suppression) threshold (0.0-1.0)
    #[cfg(feature = "ai")]
    #[arg(long = "yolo-nms", default_value = "0.4", value_name = "THRESHOLD")]
    pub yolo_nms_threshold: f32,
}

impl Args {
    /// Parse the size string into width and height
    pub fn parse_size(&self) -> Result<(u32, u32), String> {
        let parts: Vec<&str> = self.size.split('x').collect();
        if parts.len() != 2 {
            return Err(format!("Invalid size format '{}'. Use WIDTHxHEIGHT (e.g., 800x480)", self.size));
        }

        let width = parts[0].parse::<u32>()
            .map_err(|_| format!("Invalid width: '{}'", parts[0]))?;
        let height = parts[1].parse::<u32>()
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
            output_format: OutputType::Both,
            palette: None,
            extensions_str: "jpg,png".to_string(),
            auto_process: false,
            pointsize: 24,
            font: "Arial".to_string(),
            annotate_background: "#00000040".to_string(),
            jobs: 0,
            verbose: false,
            validate_only: false,
            benchmark: false,
            #[cfg(feature = "ai")]
            detect_people: false,
            #[cfg(feature = "ai")]
            yolo_assets_dir: None,
            #[cfg(feature = "ai")]
            yolo_confidence: 0.5,
            #[cfg(feature = "ai")]
            yolo_nms_threshold: 0.4,
        }
    }
}