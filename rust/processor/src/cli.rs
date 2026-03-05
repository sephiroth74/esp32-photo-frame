// rust
use crate::types::{HexColor, OutputType, ReportFormat};
use clap::Parser;
use photoframe_lib::{ColorMode, DitheringMethod, Orientation};
use std::path::PathBuf;

fn get_long_about() -> String {
    let features_str = if cfg!(feature = "ai") {
        "✓ AI people detection (YOLO11)"
    } else {
        "None"
    };

    let ai_feature_desc = if cfg!(feature = "ai") {
        "\n• AI-powered people detection for smart cropping (embedded YOLO11)"
    } else {
        ""
    };

    format!(
        "ESP32 Photo Frame - Image Processor (Rust Implementation)

This tool processes photos for ESP32-based e-paper photo frames. It handles smart orientation detection,
portrait image pairing, text annotations, and generates optimized binary files.

Compiled features: {}

Example Usage:
    # Basic black & white processing (800x480 landscape by default)
    processor -i ~/Photos -o ~/processed -t black-and-white --output-format pfr1

    # Process single image file
    processor -i ~/Photos/IMG_001.jpg -o ~/processed -t black-and-white --output-format pfr1

    # 6-color processing with only binary output (hardware 800x480)
    processor -i ~/Photos -o ~/processed -t 6c --output-format pfr1 --verbose

    # Multiple output formats (creates subdirectories: bmp/, pfr1/, jpg/)
    processor -i ~/Photos -o ~/processed --output-format bmp,pfr1,jpg

    # PNG output format only
    processor -i ~/Photos -o ~/processed --output-format png
{}
    # Process images with filename annotations enabled
    processor -i ~/Photos -o ~/processed --annotate",
        features_str,
        ai_feature_desc,
    )
}

#[derive(Parser, Debug)]
#[command(
    name = "processor",
    version,
    about = "Image processor for ESP32 Photo Frame project",
    long_about = get_long_about()
)]
pub struct Args {
    /// Input directories or single image files (can be specified multiple times)
    #[arg(
        short = 'i',
        long = "input",
        required_unless_present_any = ["validate"],
        conflicts_with = "validate",
        value_name = "DIR|FILE",
        help = "Input directories or image files to process (can be specified multiple times)"
    )]
    pub input: Vec<PathBuf>,

    /// Output directory for processed images
    #[arg(
        short = 'o',
        long = "output",
        required_unless_present_any = ["validate"],
        conflicts_with = "validate",
        value_name = "DIR",
        default_value = ".",
        help = "Output directory for processed images"
    )]
    pub output: PathBuf,

    /// Display orientation (how the physical display is mounted)
    /// - landscape or 0: Display mounted horizontally (800×480 visual)
    /// - portrait or 1: Display mounted vertically (480×800 visual, images pre-rotated for 6c)
    /// - landscape-reverse or 2: Upside-down horizontal (display handles reverse)
    /// - portrait-reverse or 3: Upside-down vertical (display handles reverse)
    #[arg(
        long = "orientation",
        default_value = "0",
        value_name = "ORIENTATION",
        conflicts_with = "validate",
        help = "Display mounting orientation: 0 = landscape, 1 = portrait, 2 = landscape-reverse, 3 = portrait-reverse"
    )]
    pub target_orientation: Orientation,

    /// Display type: black-and-white (black & white) or six-colors (6-color)
    /// This determines both the processing type and output dimensions
    #[arg(
        short = 't',
        long = "type",
        default_value = "black-and-white",
        conflicts_with = "validate",
        help = "Display type: 'black-and-white' for black & white, 'six-colors' for 6-color"
    )]
    pub processing_type: ColorMode,

    /// Output formats: comma-separated list of bmp, pfr1, jpg, png (e.g., "bmp,pfr1" or "jpg")
    #[arg(
        short = 'f',
        long = "output-format",
        default_value = "pfr1,jpg",
        conflicts_with = "validate",
        num_args = 1,
        value_delimiter = ',',
        help = "Output formats: bmp, pfr1, jpg, png (comma-separated list)"
    )]
    pub output_formats: Vec<OutputType>,

    #[arg(
        long = "no-pairing",
        conflicts_with_all = ["validate", "divider_width", "divider_color"],
        help = "Disable automatic pairing of images into combined files"
    )]
    pub no_pairing: bool,

    /// Comma-separated list of image extensions to process
    #[arg(
        long = "extensions",
        default_value = "jpg,jpeg,png,webp,tiff,heic",
        conflicts_with = "validate",
        help = "Comma-separated list of image file extensions to process (e.g., 'jpg,png,bmp,tiff')"
    )]
    pub extensions: String,

    /// Enable filename annotations on processed images (default: false)
    #[arg(
        long = "annotate",
        conflicts_with = "validate",
        help = "Enable annotations on processed images (it will look for the EXIF image creation date by default)"
    )]
    pub annotate: bool,

    /// Font size for filename annotations
    #[arg(
        long = "font-size",
        default_value = "22",
        value_name = "SIZE",
        conflicts_with = "validate",
        requires = "annotate",
        help = "Font size for text annotations in pixels"
    )]
    pub font_size: u32,

    /// Font specification for annotations. Supports three formats:
    /// - Font name: "Arial" (searches system fonts)
    /// - Font filename: "Arial.ttf" (searches in font directories)
    /// - Full path: "/System/Library/Fonts/Arial.ttf" (loads directly)
    #[arg(
        long = "font",
        default_value = "Arial",
        value_name = "FONT",
        conflicts_with = "validate",
        requires = "annotate",
        help = "Font for text annotations (name, filename, or full path"
    )]
    pub font: String,

    /// Background color for text annotations (ARGB hex, e.g., #40000000 for semi-transparent black)
    #[arg(
        long = "annotation_background",
        default_value = "#40000000",
        value_name = "COLOR",
        value_parser = HexColor::parse,
        conflicts_with = "validate",
        requires = "annotate",
        help = "Background color for text annotations (hex ARGB format, e.g., #40000000 (semi-transparent black)"
    )]
    pub annotation_background: HexColor,

    /// Width of the divider line between combined portrait images in pixels
    #[arg(
        long = "divider-width",
        default_value = "3",
        value_name = "WIDTH",
        conflicts_with_all = ["validate", "no_pairing"],
        help = "Width of the divider line between combined images in pixels"
    )]
    pub divider_width: u32,

    /// Color of the divider line between combined portrait images (hex ARGB, e.g., #FFFFFF for white)
    #[arg(
        long = "divider-color",
        default_value = "#FFFFFFFF",
        value_name = "COLOR",
        value_parser = HexColor::parse,
        conflicts_with_all = ["validate", "no_pairing"],
        help = "Color of the divider line between combined images (hex ARGB format, e.g., #FFFFFFFF for white"
    )]
    pub divider_color: HexColor,

    /// Dithering method for color quantization
    #[arg(
        short = 'd',
        long = "dithering",
        default_value = "floyd-steinberg",
        value_name = "METHOD",
        conflicts_with = "validate",
        help = "Dithering algorithm: floyd-steinberg (best gradients), atkinson (bright), stucki (diffused), jarvis (photos), ordered (text)"
    )]
    pub dithering_method: DitheringMethod,

    #[arg(
        long = "dither-strength",
        default_value = "100",
        value_name = "STRENGTH",
        value_parser = clap::value_parser!(u32).range(0..=200),
        conflicts_with = "validate",
        help = "Dithering strength multiplier. 100 = normal, <100 = subtle, >100 = pronounced"
    )]
    pub dither_strength: u32,

    /// Contrast adjustment (-100 to 100, default 0). Positive = increase, negative = decrease
    #[arg(
        short = 'c',
        long = "contrast",
        default_value = "0",
        value_name = "ADJUSTMENT",
        value_parser = clap::value_parser!(i32).range(-100..=100),
        conflicts_with = "validate",
        help = "Contrast adjustment: 0 (no change), <0 = decrease contrast, >0 = increase contrast (e.g., 50 = 50% more contrast, -50 = 50% less contrast)"
    )]
    pub contrast: i32,

    /// Brightness adjustment (-100 to 100, default 0). Positive = lighter, negative = darker
    #[arg(
        short = 'b',
        long = "brightness",
        default_value = "0",
        value_name = "ADJUSTMENT",
        value_parser = clap::value_parser!(i32).range(-100..=100),
        conflicts_with = "validate",
        help = "Brightness adjustment: 0 (no change), <0 = darker, >0 = lighter (e.g., 50 = 50% brighter, -50 = 50% darker)"
    )]
    pub brightness: i32,

    /// Saturation boost multiplier (0.5 to 2.0, default 1.0). >1.0 = more vibrant, <1.0 = less vibrant
    #[arg(
        short = 's',
        long = "saturation",
        default_value = "100",
        value_name = "MULTIPLIER",
        value_parser = clap::value_parser!(u32).range(0..=1000),
        conflicts_with = "validate",
        help = "Saturation: 100 (no change), <100 = less vibrant, >100 = more vibrant (e.g., 150 = 50% boost, 50 = 50% reduction)"
    )]
    pub saturation: u32,

    /// Enable automatic color correction before processing (uses ImageMagick if available)
    #[arg(long = "auto-color",
        conflicts_with_all = ["validate"],
        help = "Automatically enhance colors using ImageMagick's auto-level and auto-gamma functions (requires ImageMagick installed)"
    )]
    pub auto_color: bool,

    /// Enable automatic per-image parameter optimization (overrides manual dithering/contrast settings)
    #[arg(
        short = 'a',
        long = "auto-optimize",
        conflicts_with_all = ["dithering_method", "dither_strength", "contrast", "brightness", "saturation", "auto_color", "validate"],
        help = "Automatically select optimal dithering, strength, and contrast for each image based on content analysis"
    )]
    pub auto_optimize: bool,

    /// Output format for the report
    #[arg(
        short = 'r',
        long = "report",
        default_value = "plain",
        value_name = "FORMAT",
        conflicts_with = "validate",
        help = "Report output format: 'plain', 'full', 'json'"
    )]
    pub report: ReportFormat,

    #[arg(
        long = "report-output",
        value_name = "FILE",
        conflicts_with = "validate",
        help = "Optional file path to save the processing report (if not specified, report will be printed to console)"
    )]
    pub report_output: Option<PathBuf>,

    /// Number of parallel processing jobs (0 = auto-detect CPU cores)
    #[arg(
        short = 'j',
        long = "jobs",
        default_value = "0",
        value_name = "N",
        help = "Number of parallel processing jobs (0 = auto-detect CPU cores)"
    )]
    pub jobs: usize,

    /// Enable verbose output with detailed progress information
    #[arg(
        short = 'v',
        long = "verbose",
        help = "Enable verbose output with detailed progress information",
        conflicts_with_all = ["json_progress", "validate"]
    )]
    pub verbose: bool,

    /// Output progress as JSON lines (for GUI integration, suppresses all other output)
    #[arg(
        short = 'J',
        long = "json-progress",
        conflicts_with_all = ["verbose", "validate"],
        help = "Output progress as JSON lines for GUI integration (suppresses all other output)"
    )]
    pub json_progress: bool,

    /// Enable people detection for smart cropping using embedded YOLO11 model
    #[cfg(feature = "ai")]
    #[arg(
        long = "detect-people",
        conflicts_with = "validate",
        help = "Enable AI-powered people detection for smart cropping (uses downloaded model)"
    )]
    pub detect_people: bool,

    #[cfg(not(feature = "ai"))]
    #[arg(long = "detect-people", hide = true, default_value_t = false)]
    pub detect_people: bool,

    /// Confidence threshold for people detection (0.0-1.0, requires --detect-people)
    #[cfg(feature = "ai")]
    #[arg(
        long = "confidence",
        default_value = "0.50",
        value_name = "THRESHOLD",
        conflicts_with = "validate",
        requires = "detect_people",
        help = "Confidence threshold for people detection (0.0 to 1.0)"
    )]
    pub confidence_threshold: f32,

    #[cfg(not(feature = "ai"))]
    #[arg(long = "confidence", hide = true, default_value_t = 0.50)]
    pub confidence_threshold: f32,

    /// Enable debug mode: visualize detection boxes and crop area with correct orientation
    #[arg(
        long = "debug",
        conflicts_with = "validate",
        help = "Will draw detection boxes and crop areas on output images for debugging purposes"
    )]
    pub debug: bool,

    /// Validate a .pfr1 file and exit (bypasses normal processing)
    #[arg(long = "validate", value_name = "FILE")]
    pub validate: Option<PathBuf>,
}
