use crate::types::{ColorType, HexColor, Orientation, ReportFormat, Size};
use image::{Rgb, Rgba};
use photoframe_lib::DisplayType;
use std::fmt::{Display, Formatter};

impl Display for ReportFormat {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            ReportFormat::Full => "rich",
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

impl TryFrom<&str> for HexColor {
    type Error = &'static str;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        let hex = if value.starts_with("0x") {
            &value[2..]
        } else if value.starts_with('#') {
            &value[1..]
        } else {
            &value
        };

        match hex.len() {
            3 => {
                // #RGB -> #RRGGBB
                let r = u8::from_str_radix(&hex[0..1].repeat(2), 16)
                    .map_err(|_| "hex color parse error")?;
                let g = u8::from_str_radix(&hex[1..2].repeat(2), 16)
                    .map_err(|_| "hex color parse error")?;
                let b = u8::from_str_radix(&hex[2..3].repeat(2), 16)
                    .map_err(|_| "hex color parse error")?;
                Ok(Self(255, r, g, b))
            }
            6 => {
                // #RRGGBB
                let r = u8::from_str_radix(&hex[0..2], 16).map_err(|_| "hex color parse error")?;
                let g = u8::from_str_radix(&hex[2..4], 16).map_err(|_| "hex color parse error")?;
                let b = u8::from_str_radix(&hex[4..6], 16).map_err(|_| "hex color parse error")?;
                Ok(Self(255, r, g, b))
            }
            8 => {
                // #AARRGGBB
                let a = u8::from_str_radix(&hex[0..2], 16).map_err(|_| "hex color parse error")?;
                let r = u8::from_str_radix(&hex[2..4], 16).map_err(|_| "hex color parse error")?;
                let g = u8::from_str_radix(&hex[4..6], 16).map_err(|_| "hex color parse error")?;
                let b = u8::from_str_radix(&hex[6..8], 16).map_err(|_| "hex color parse error")?;
                Ok(Self(a, r, g, b))
            }
            _ => Err("Invalid hex color format"),
        }
    }
}

impl HexColor {
    pub fn parse(value: &str) -> anyhow::Result<Self> {
        value
            .try_into()
            .map_err(|_| anyhow::anyhow!("HexColor parse error"))
    }

    #[allow(dead_code)]
    pub fn to_rgba(&self) -> Rgba<u8> {
        Rgba::from([self.1, self.2, self.3, self.0])
    }

    #[allow(dead_code)]
    pub fn to_rgb(&self) -> Rgb<u8> {
        Rgb::from([self.1, self.2, self.3])
    }

    pub fn alpha(&self) -> u8 {
        self.0
    }

    pub fn red(&self) -> u8 {
        self.1
    }

    pub fn green(&self) -> u8 {
        self.2
    }

    pub fn blue(&self) -> u8 {
        self.3
    }
}

impl Display for HexColor {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let mut rgb = self.0 as u32;
        rgb = (rgb << 8) + self.1 as u32;
        rgb = (rgb << 8) + self.2 as u32;
        rgb = (rgb << 8) + self.3 as u32;
        write!(f, "{:#06x}", rgb)
    }
}

impl Into<Rgba<u8>> for HexColor {
    fn into(self) -> Rgba<u8> {
        Rgba::from([self.1, self.2, self.3, self.0])
    }
}

impl Size {
    pub fn new(width: u32, height: u32) -> Self {
        Self { width, height }
    }
}

impl Into<DisplayType> for ColorType {
    fn into(self) -> DisplayType {
        match self {
            ColorType::SixColor => DisplayType::SixColors,
            ColorType::BlackWhite => DisplayType::BlackAndWhite,
        }
    }
}

impl ColorType {
    pub fn into_size(self, orientation: Orientation) -> Size {
        match orientation {
            Orientation::Landscape | Orientation::LandscapeReverse => match self {
                ColorType::BlackWhite | ColorType::SixColor => Size::new(800, 480),
            },

            Orientation::Portrait | Orientation::PortraitReverse => match self {
                ColorType::BlackWhite | ColorType::SixColor => Size::new(480, 800),
            },
        }
    }
}
