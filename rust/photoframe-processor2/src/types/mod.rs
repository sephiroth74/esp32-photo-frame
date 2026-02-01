mod impls;

use clap::ValueEnum;

/// Color type used by the original project
#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum ColorType {
    #[value(name = "bw")]
    BlackWhite,
    #[value(name = "6c")]
    SixColor,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Orientation {
    Landscape,
    Portrait,
    LandscapeReverse,
    PortraitReverse,
}

/// Report output format options
#[derive(Debug, Clone, ValueEnum, PartialEq, Eq)]
pub enum ReportFormat {
    #[value(name = "plain")]
    Plain,
    #[value(name = "rich")]
    Rich,
    #[value(name = "json")]
    Json,
}

#[derive(Debug, Clone, ValueEnum, PartialEq, Eq, Hash)]
pub enum OutputType {
    /// Generate only BMP files
    #[value(name = "bmp")]
    Bmp,
    /// Generate only binary files for ESP32
    #[value(name = "pfr1")]
    Pfr1,
    /// Generate only JPG files
    #[value(name = "jpg")]
    Jpg,
    /// Generate only PNG files
    #[value(name = "png")]
    Png,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
enum DitheringMethod {
    #[value(name = "none")]
    None,
    #[value(name = "floyd_steinberg")]
    FloydSteinberg,
    #[value(name = "ordered")]
    Ordered,
    #[value(name = "error_diffusion")]
    ErrorDiffusion,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct HexColor(pub u8, pub u8, pub u8, pub u8);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Size {
    pub width: u32,
    pub height: u32,
}
