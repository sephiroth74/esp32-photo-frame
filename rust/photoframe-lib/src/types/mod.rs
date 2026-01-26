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
