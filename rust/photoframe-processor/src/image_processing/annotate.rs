use anyhow::Result;
use image::RgbImage;
use std::path::Path;

/// Add filename annotation to an image
/// 
/// This is a placeholder implementation. In the full version, this would:
/// 1. Parse the hex color background 
/// 2. Load the specified font
/// 3. Render text with background rectangle
/// 4. Position text in bottom-right corner
/// 
/// For now, it just returns the image unchanged
pub fn add_filename_annotation(
    img: &RgbImage,
    input_path: &Path,
    _font_name: &str,
    _font_size: f32,
    _background_color: &str,
) -> Result<RgbImage> {
    // TODO: Implement text rendering with fontdue or ab_glyph
    // For now, just return the original image
    let _ = input_path; // Silence unused parameter warning
    Ok(img.clone())
}

/// Parse hex color string to RGB values
/// 
/// Supports formats: #RGB, #RRGGBB, #RRGGBBAA
fn _parse_hex_color(color_str: &str) -> Result<(u8, u8, u8, u8)> {
    if !color_str.starts_with('#') {
        return Err(anyhow::anyhow!("Color must start with #"));
    }
    
    let hex = &color_str[1..];
    
    match hex.len() {
        3 => {
            // #RGB -> #RRGGBB
            let r = u8::from_str_radix(&hex[0..1].repeat(2), 16)?;
            let g = u8::from_str_radix(&hex[1..2].repeat(2), 16)?;
            let b = u8::from_str_radix(&hex[2..3].repeat(2), 16)?;
            Ok((r, g, b, 255))
        }
        6 => {
            // #RRGGBB
            let r = u8::from_str_radix(&hex[0..2], 16)?;
            let g = u8::from_str_radix(&hex[2..4], 16)?;
            let b = u8::from_str_radix(&hex[4..6], 16)?;
            Ok((r, g, b, 255))
        }
        8 => {
            // #RRGGBBAA
            let r = u8::from_str_radix(&hex[0..2], 16)?;
            let g = u8::from_str_radix(&hex[2..4], 16)?;
            let b = u8::from_str_radix(&hex[4..6], 16)?;
            let a = u8::from_str_radix(&hex[6..8], 16)?;
            Ok((r, g, b, a))
        }
        _ => Err(anyhow::anyhow!("Invalid hex color format")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_hex_color() {
        assert_eq!(_parse_hex_color("#000").unwrap(), (0, 0, 0, 255));
        assert_eq!(_parse_hex_color("#FFF").unwrap(), (255, 255, 255, 255));
        assert_eq!(_parse_hex_color("#FF0000").unwrap(), (255, 0, 0, 255));
        assert_eq!(_parse_hex_color("#00FF0080").unwrap(), (0, 255, 0, 128));
        
        assert!(_parse_hex_color("FF0000").is_err()); // Missing #
        assert!(_parse_hex_color("#GG0000").is_err()); // Invalid hex
    }
}