mod impls;

use clap::ValueEnum;

/// Report output format options
#[derive(Debug, Clone, ValueEnum, PartialEq, Eq)]
pub enum ReportFormat {
    #[value(name = "plain")]
    Plain,
    #[value(name = "full")]
    Full,
    #[value(name = "json")]
    Json,
}

#[derive(Debug, Clone, Copy, ValueEnum, PartialEq, Eq, Hash)]
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct HexColor(pub u8, pub u8, pub u8, pub u8);
