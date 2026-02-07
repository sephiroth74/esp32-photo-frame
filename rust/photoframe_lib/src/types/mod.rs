use clap::ValueEnum;

/// Target display palette selection used by dithering and conversion helpers.
///
/// - `SixColors`: limit output to the predefined 6-color e-paper palette.
/// - `BlackAndWhite`: pure black-and-white output.
#[derive(Debug, Clone, Copy, ValueEnum, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum DisplayType {
    SixColors,
    BlackAndWhite,
}

// Color mode used in the output .pfr1 binary file header.
/// - `SixColors`: indicates 6-color mode (color_mode = 1).
/// - `BlackAndWhite`: indicates black-and-white mode (color_mode = 0).
#[derive(Debug, Clone, Copy, ValueEnum, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[repr(u8)]
pub enum ColorMode {
    SixColors,
    BlackAndWhite,
}

/// Available dithering algorithms implemented in this crate.
///
/// Choose a method depending on the image characteristics and desired result:
/// - `FloydSteinberg`: balanced, general-purpose error diffusion.
/// - `Atkinson`: lighter diffusion, preserves perceived brightness.
/// - `Stucki`: wide diffusion, smooth gradients.
/// - `JarvisJudiceNinke`: high-quality for photographic detail.
/// - `Ordered`: fast thresholding using Bayer matrix, may show patterning.
#[derive(Debug, Clone, Copy, ValueEnum, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum DitheringMethod {
    FloydSteinberg,
    Atkinson,
    Stucki,
    JarvisJudiceNinke,
    Ordered,
}

impl Into<u8> for ColorMode {
    fn into(self) -> u8 {
        match self {
            ColorMode::BlackAndWhite => 0,
            ColorMode::SixColors => 1,
        }
    }
}

impl From<u8> for ColorMode {
    fn from(value: u8) -> Self {
        match value {
            0 => ColorMode::BlackAndWhite,
            1 => ColorMode::SixColors,
            _ => ColorMode::BlackAndWhite, // Default case
        }
    }
}

impl From<&str> for ColorMode {
    fn from(value: &str) -> Self {
        match value.to_lowercase().as_str() {
            "blackandwhite" | "black_and_white" | "bw" => ColorMode::BlackAndWhite,
            "sixcolors" | "six_colors" | "6c" => ColorMode::SixColors,
            _ => ColorMode::BlackAndWhite, // Default case
        }
    }
}

impl Into<ColorMode> for DisplayType {
    fn into(self) -> ColorMode {
        match self {
            DisplayType::BlackAndWhite => ColorMode::BlackAndWhite,
            DisplayType::SixColors => ColorMode::SixColors,
        }
    }
}

impl Into<DisplayType> for ColorMode {
    fn into(self) -> DisplayType {
        match self {
            ColorMode::BlackAndWhite => DisplayType::BlackAndWhite,
            ColorMode::SixColors => DisplayType::SixColors,
        }
    }
}

impl DisplayType {
    /// Get the filename prefix for this display type (used in examples/tests).
    pub fn get_prefix(&self) -> &'static str {
        match self {
            DisplayType::BlackAndWhite => "bw",
            DisplayType::SixColors => "6c",
        }
    }

    /// Parse prefix back to DisplayType
    #[allow(dead_code)]
    pub fn from_prefix(prefix: &str) -> Option<Self> {
        match prefix {
            "bw" => Some(DisplayType::BlackAndWhite),
            "6c" => Some(DisplayType::SixColors),
            _ => None,
        }
    }
}
