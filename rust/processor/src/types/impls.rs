use crate::types::{HexColor, ReportFormat};
use image::{Rgb, Rgba};
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
