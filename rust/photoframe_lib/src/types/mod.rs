use std::fmt::Display;
use clap::ValueEnum;

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[repr(C)]
pub struct Size {
    pub width: u32,
    pub height: u32,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[repr(C)]
pub enum Orientation {
    #[serde(rename = "landscape")]
    Landscape,
    #[serde(rename = "portrait")]
    Portrait,
    #[serde(rename = "landscape-reverse")]
    LandscapeReverse,
    #[serde(rename = "portrait-reverse")]
    PortraitReverse,
}

/// Target display palette selection used by dithering and conversion helpers.
///
/// - `SixColors`: limit output to the predefined 6-color e-paper palette.
/// - `BlackAndWhite`: pure black-and-white output.
#[derive(Debug, Clone, Copy, ValueEnum, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[repr(C)]
pub enum DisplayType {
    #[serde(rename = "black-and-white")]
    BlackAndWhite = 0,
    #[serde(rename = "six-colors")]
    SixColors = 1,
}

// Color mode used in the output .pfr1 binary file header.
/// - `SixColors`: indicates 6-color mode (color_mode = 1).
/// - `BlackAndWhite`: indicates black-and-white mode (color_mode = 0).
#[derive(Debug, Clone, Copy, ValueEnum, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[repr(C)]
pub enum ColorMode {
    #[serde(rename = "black-and-white")]
    BlackAndWhite = 0,
    #[serde(rename = "six-colors")]
    SixColors = 1,
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
#[repr(C)]
pub enum DitheringMethod {
    #[serde(rename = "floyd-steinberg")]
    FloydSteinberg,
    #[serde(rename = "atkinson")]
    Atkinson,
    #[serde(rename = "stucki")]
    Stucki,
    #[serde(rename = "jarvis-judice-ninke")]
    JarvisJudiceNinke,
    #[serde(rename = "ordered")]
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
            "blackandwhite" | "black_and_white" | "black-and-white" | "bw" => {
                ColorMode::BlackAndWhite
            }
            "sixcolors" | "six_colors" | "six-colors" | "6c" => ColorMode::SixColors,
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

impl Into<Size> for DisplayType {
    fn into(self) -> Size {
        // Default to landscape orientation size
        match self {
            DisplayType::BlackAndWhite | DisplayType::SixColors => Size::new(800, 480),
        }
    }
}

impl Into<Size> for ColorMode {
    fn into(self) -> Size {
        let display_type: DisplayType = self.into();
        display_type.into()
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

impl Display for DisplayType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            DisplayType::BlackAndWhite => "Black and White",
            DisplayType::SixColors => "Six Colors",
        };
        write!(f, "{}", s)
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

impl Size {
    pub fn new(width: u32, height: u32) -> Self {
        Self { width, height }
    }

    pub fn rotate(self, orientation: Orientation) -> Size {
        match orientation {
            Orientation::Landscape | Orientation::LandscapeReverse => self,
            Orientation::Portrait | Orientation::PortraitReverse => Size {
                width: self.height,
                height: self.width,
            },
        }
    }
}

impl From<&str> for Orientation {
    fn from(value: &str) -> Self {
        match value {
            "0" | "landscape" | "0°" => Orientation::Landscape,
            "1" | "portrait" | "90°" => Orientation::Portrait,
            "2" | "landscape-reverse" | "180°" => Orientation::LandscapeReverse,
            "3" | "portrait-reverse" | "270°" => Orientation::PortraitReverse,
            _ => Orientation::Landscape, // Default case
        }
    }
}

impl Into<u8> for Orientation {
    fn into(self) -> u8 {
        match self {
            Orientation::Landscape => 0,
            Orientation::Portrait => 1,
            Orientation::LandscapeReverse => 2,
            Orientation::PortraitReverse => 3,
        }
    }
}

impl From<u8> for Orientation {
    fn from(value: u8) -> Self {
        match value {
            0 => Orientation::Landscape,
            1 => Orientation::Portrait,
            2 => Orientation::LandscapeReverse,
            3 => Orientation::PortraitReverse,
            _ => Orientation::Landscape, // Default case
        }
    }
}
