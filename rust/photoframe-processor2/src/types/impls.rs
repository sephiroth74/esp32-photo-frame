use crate::types::{Orientation, ReportFormat};
use std::fmt::Display;

impl Display for ReportFormat {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            ReportFormat::Rich => "rich",
            ReportFormat::Json => "json",
            ReportFormat::Plain => "plain",
        };
        write!(f, "{}", s)
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
