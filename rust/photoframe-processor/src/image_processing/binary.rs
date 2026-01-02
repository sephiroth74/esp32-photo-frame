use anyhow::Result;
use image::{Rgb, RgbImage};

/// Convert an RGB image to ESP32 binary format
///
/// This function converts RGB pixel data to the 8-bit format used by the ESP32 photo frame:
/// - 3 bits for red (values 0-7)
/// - 3 bits for green (values 0-7)
/// - 2 bits for blue (values 0-3)
///
/// The format is: RRRGGGBB (8 bits total per pixel)
///
/// This matches the logic from bmp2cpp/src/main.rs:217-218:
/// ```rust
/// let color8 = ((color.r() / 32) << 5) + ((color.g() / 32) << 2) + (color.b() / 64);
/// ```
pub fn convert_to_esp32_binary(img: &RgbImage) -> Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = (width * height) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    for pixel in img.pixels() {
        let color8 = rgb_to_esp32_color(pixel);
        binary_data.push(color8);
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Binary data size mismatch: expected {}, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}

/// Convert RGB pixel to 6-color native format (writeNative() input format)
///
/// This converts an RGB pixel to the format expected by GxEPD2's writeNative() method
/// for GDEP073E01 6-color displays. The library will call _convert_to_native() on these values.
///
/// Input format (3 bits per pixel, using lower nibble):
/// - 0x0 = BLACK   - RGB(0, 0, 0)
/// - 0x1 = WHITE   - RGB(255, 255, 255)
/// - 0x2 = GREEN   - RGB(0, 252, 0)
/// - 0x3 = BLUE    - RGB(0, 0, 255)
/// - 0x4 = RED     - RGB(252, 0, 0)
/// - 0x5 = YELLOW  - RGB(252, 252, 0)
///
/// The actual display uses different values but _convert_to_native() handles that conversion.
///
/// IMPORTANT: This function expects the image has already been dithered to the 6-color palette.
/// It simply maps the palette RGB values to their corresponding nibble values.
#[allow(dead_code)]
pub fn rgb_to_6c_native(pixel: &Rgb<u8>) -> u8 {
    let r = pixel[0];
    let g = pixel[1];
    let b = pixel[2];

    // Map from 6-color palette RGB values to native nibbles
    // These are the exact colors from convert.rs::SIX_COLOR_PALETTE
    match (r, g, b) {
        (0, 0, 0) => 0x0,       // BLACK
        (255, 255, 255) => 0x1, // WHITE
        (252, 0, 0) => 0x4,     // RED
        (0, 252, 0) => 0x2,     // GREEN
        (0, 0, 255) => 0x3,     // BLUE
        (252, 252, 0) => 0x5,   // YELLOW
        _ => {
            // Fallback: find closest color by distance
            // This shouldn't happen if image is properly dithered
            let black_dist = color_distance(r, g, b, 0, 0, 0);
            let white_dist = color_distance(r, g, b, 255, 255, 255);
            let red_dist = color_distance(r, g, b, 252, 0, 0);
            let green_dist = color_distance(r, g, b, 0, 252, 0);
            let blue_dist = color_distance(r, g, b, 0, 0, 255);
            let yellow_dist = color_distance(r, g, b, 252, 252, 0);

            let min_dist = black_dist
                .min(white_dist)
                .min(red_dist)
                .min(green_dist)
                .min(blue_dist)
                .min(yellow_dist);

            if min_dist == black_dist {
                0x0
            } else if min_dist == white_dist {
                0x1
            } else if min_dist == red_dist {
                0x4
            } else if min_dist == green_dist {
                0x2
            } else if min_dist == blue_dist {
                0x3
            } else {
                0x5
            } // yellow
        }
    }
}

/// Calculate color distance (squared Euclidean distance)
#[inline]
#[allow(dead_code)]
fn color_distance(r1: u8, g1: u8, b1: u8, r2: u8, g2: u8, b2: u8) -> u32 {
    let dr = (r1 as i32 - r2 as i32).abs() as u32;
    let dg = (g1 as i32 - g2 as i32).abs() as u32;
    let db = (b1 as i32 - b2 as i32).abs() as u32;
    dr * dr + dg * dg + db * db
}

/// Convert an RGB image to 6-color native binary format
///
/// This format uses 2 pixels per byte (4 bits per pixel) for the GDEP073E01 6-color display.
/// The output is compatible with GxEPD2's writeNative() method.
///
/// File size is 50% smaller than the standard format: (width * height) / 2 bytes
///
/// Format: 2 pixels packed into each byte
/// - High nibble: left pixel (bits 4-7)
/// - Low nibble: right pixel (bits 0-3)
#[allow(dead_code)]
pub fn convert_to_6c_native_binary(img: &RgbImage) -> Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = ((width * height) / 2) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    let pixels: Vec<&Rgb<u8>> = img.pixels().collect();

    // Process pixels in pairs
    for i in (0..pixels.len()).step_by(2) {
        let nibble1 = rgb_to_6c_native(pixels[i]);
        let nibble2 = if i + 1 < pixels.len() {
            rgb_to_6c_native(pixels[i + 1])
        } else {
            0x1 // Default to WHITE for odd pixel count
        };

        // Pack two pixels into one byte
        let packed = (nibble1 << 4) | nibble2;
        binary_data.push(packed);
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Native binary data size mismatch: expected {} bytes, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}

/// Convert RGB pixel to ESP32 color format using bmp2cpp approach
///
/// This uses the same RGB compression format as the bmp2cpp project:
/// - Red: 3 bits (0-7) - divide by 32, shift left 5 positions  
/// - Green: 3 bits (0-7) - divide by 32, shift left 2 positions
/// - Blue: 2 bits (0-3) - divide by 64, no shift
///
/// Final format: RRRGGGBB
///
/// This matches exactly what the 6-color e-paper display expects and ensures
/// compatibility with existing bmp2cpp generated files.
pub fn rgb_to_esp32_color(pixel: &Rgb<u8>) -> u8 {
    let r = pixel[0];
    let g = pixel[1];
    let b = pixel[2];

    // Use the exact same formula as bmp2cpp project
    ((r / 32) << 5) + ((g / 32) << 2) + (b / 64)
}

/// Convert ESP32 compressed color back to RGB (for validation/debugging)
///
/// This is the reverse operation of rgb_to_esp32_color()
/// Extracts the RRRGGGBB format back to full 8-bit RGB values
/// Used primarily for testing and validation
#[allow(dead_code)]
pub fn esp32_color_to_rgb(color8: u8) -> [u8; 3] {
    // Extract the compressed components
    let r3 = (color8 >> 5) & 0x07; // 3 bits for red (0-7)
    let g3 = (color8 >> 2) & 0x07; // 3 bits for green (0-7)
    let b2 = color8 & 0x03; // 2 bits for blue (0-3)

    // Scale back to 8-bit values using proper scaling
    // For 3-bit values (0-7): multiply by 36.43 ≈ (255/7) = 36
    // For 2-bit values (0-3): multiply by 85 = (255/3) = 85
    let r8 = if r3 == 7 { 255 } else { r3 * 36 };
    let g8 = if g3 == 7 { 255 } else { g3 * 36 };
    let b8 = if b2 == 3 { 255 } else { b2 * 85 };

    [r8, g8, b8]
}

/// Validate that a binary file has the correct size for given dimensions
#[allow(dead_code)]
pub fn validate_binary_size(binary_data: &[u8], width: u32, height: u32) -> Result<()> {
    let expected_size = (width * height) as usize;
    let actual_size = binary_data.len();

    if actual_size != expected_size {
        return Err(anyhow::anyhow!(
            "Binary size mismatch for {}x{}: expected {} bytes, got {} bytes",
            width,
            height,
            expected_size,
            actual_size
        ));
    }

    Ok(())
}

/// Generate a C header file with the binary data as an array
///
/// This matches the OutputFormat::CArray functionality from bmp2cpp
#[allow(dead_code)]
pub fn generate_c_header(
    binary_data: &[u8],
    variable_name: &str,
    source_filename: &str,
    width: u32,
    height: u32,
) -> String {
    let header_guard = format!("_{}_H_", variable_name.to_uppercase());

    // Split data into rows for better readability
    let chunks: Vec<String> = binary_data
        .chunks(width as usize)
        .map(|chunk| {
            chunk
                .iter()
                .map(|b| format!("0x{:02X}", b))
                .collect::<Vec<String>>()
                .join(", ")
        })
        .collect();

    let pixel_data = chunks.join(",\n");

    format!(
        r#"
#ifndef {header_guard}
#define {header_guard}

// This file is automatically generated by photoframe-processor
// from the image file: {source_filename}
// Image dimensions: {width}x{height} pixels

#if defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

const unsigned char {variable_name}[{size}] PROGMEM = {{
{pixel_data}
}};

#endif"#,
        header_guard = header_guard,
        source_filename = source_filename,
        width = width,
        height = height,
        variable_name = variable_name,
        size = binary_data.len(),
        pixel_data = pixel_data
    )
}

/// Statistics about color usage in the binary data
#[allow(dead_code)]
#[derive(Debug)]
pub struct ColorStats {
    pub unique_colors: std::collections::HashMap<u8, u32>,
    pub total_pixels: usize,
    pub most_common_color: Option<(u8, u32)>,
}

impl ColorStats {
    #[allow(dead_code)]
    pub fn analyze(binary_data: &[u8]) -> Self {
        let mut color_counts = std::collections::HashMap::new();

        for &color in binary_data {
            *color_counts.entry(color).or_insert(0) += 1;
        }

        let most_common_color = color_counts
            .iter()
            .max_by_key(|(_, &count)| count)
            .map(|(&color, &count)| (color, count));

        Self {
            unique_colors: color_counts,
            total_pixels: binary_data.len(),
            most_common_color,
        }
    }

    #[allow(dead_code)]
    pub fn unique_color_count(&self) -> usize {
        self.unique_colors.len()
    }

    #[allow(dead_code)]
    pub fn color_usage_percent(&self, color: u8) -> f64 {
        if let Some(&count) = self.unique_colors.get(&color) {
            (count as f64 / self.total_pixels as f64) * 100.0
        } else {
            0.0
        }
    }
}

/// Convert RGB pixel to drawDemoBitmap format (mode 1) for GDEP073E01
///
/// drawDemoBitmap() with mode=1 expects these specific byte values:
/// - 0x00 = BLACK   - RGB(0, 0, 0)
/// - 0xFF = WHITE   - RGB(255, 255, 255)
/// - 0xFC = YELLOW  - RGB(252, 252, 0)
/// - 0xE0 = RED     - RGB(252, 0, 0)
/// - 0x03 = BLUE    - RGB(0, 0, 255)
/// - 0x1C = GREEN   - RGB(0, 252, 0)
///
/// This function maps the 6-color palette RGB values to these byte values.
/// Any unrecognized color defaults to BLACK (0x00).
pub fn rgb_to_demo_bitmap_mode1(pixel: &Rgb<u8>) -> u8 {
    let r = pixel[0];
    let g = pixel[1];
    let b = pixel[2];

    // Match against the 6-color palette with tolerance
    // White: RGB(255, 255, 255)
    if r >= 250 && g >= 250 && b >= 250 {
        return 0xFF;
    }

    // Yellow: RGB(252, 252, 0) or similar
    if r >= 250 && g >= 250 && b < 10 {
        return 0xFC;
    }

    // Red: RGB(252, 0, 0) or similar
    if r >= 250 && g < 10 && b < 10 {
        return 0xE0;
    }

    // Green: RGB(0, 252, 0) or similar
    if r < 10 && g >= 250 && b < 10 {
        return 0x1C;
    }

    // Blue: RGB(0, 0, 255) or similar
    if r < 10 && g < 10 && b >= 250 {
        return 0x03;
    }

    // Black: RGB(0, 0, 0)
    if r < 10 && g < 10 && b < 10 {
        return 0x00;
    }

    // Default to black for any other color
    0x00
}

/// Convert image to drawDemoBitmap format (mode 1) - 1 byte per pixel
///
/// This format is specifically for GDEP073E01 6-color displays using drawDemoBitmap()
/// with mode=1. The output is 384000 bytes for an 800x480 image (1 byte per pixel).
///
/// # Arguments
/// * `img` - Input RGB image (must be already dithered to 6-color palette)
///
/// # Returns
/// * `Result<Vec<u8>>` - Binary data in mode 1 format
pub fn convert_to_demo_bitmap_mode1(img: &RgbImage) -> Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = (width * height) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    for pixel in img.pixels() {
        let color_byte = rgb_to_demo_bitmap_mode1(pixel);
        binary_data.push(color_byte);
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Binary data size mismatch: expected {}, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::{ImageBuffer, Rgb};

    #[test]
    fn test_rgb_to_esp32_color() {
        // Test pure colors
        assert_eq!(rgb_to_esp32_color(&Rgb([255, 255, 255])), 0xFF); // White: 11111111
        assert_eq!(rgb_to_esp32_color(&Rgb([0, 0, 0])), 0x00); // Black: 00000000
        assert_eq!(rgb_to_esp32_color(&Rgb([255, 0, 0])), 0xE0); // Red:   11100000
        assert_eq!(rgb_to_esp32_color(&Rgb([0, 255, 0])), 0x1C); // Green: 00011100
        assert_eq!(rgb_to_esp32_color(&Rgb([0, 0, 255])), 0x03); // Blue:  00000011
    }

    #[test]
    fn test_esp32_color_to_rgb() {
        // Test round-trip conversion (note: some precision is lost)
        let white_rgb = esp32_color_to_rgb(0xFF);
        assert_eq!(white_rgb, [255, 255, 255]);

        let black_rgb = esp32_color_to_rgb(0x00);
        assert_eq!(black_rgb, [0, 0, 0]);

        // Red should be approximately preserved
        let red_rgb = esp32_color_to_rgb(0xE0);
        assert!(red_rgb[0] > 200); // Should be close to 255
        assert!(red_rgb[1] < 50); // Should be close to 0
        assert!(red_rgb[2] < 50); // Should be close to 0
    }

    #[test]
    fn test_convert_to_esp32_binary() {
        // Create a small test image
        let img: RgbImage = ImageBuffer::new(2, 2);
        let binary_data = convert_to_esp32_binary(&img).unwrap();

        assert_eq!(binary_data.len(), 4); // 2x2 = 4 pixels

        // All pixels should be black (0x00) since we created an empty image
        assert!(binary_data.iter().all(|&b| b == 0x00));
    }

    #[test]
    fn test_validate_binary_size() {
        let data = vec![0u8; 100];

        // Valid size
        assert!(validate_binary_size(&data, 10, 10).is_ok());

        // Invalid size
        assert!(validate_binary_size(&data, 5, 5).is_err());
        assert!(validate_binary_size(&data, 20, 20).is_err());
    }

    #[test]
    fn test_color_stats() {
        let data = vec![0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xE0];
        let stats = ColorStats::analyze(&data);

        assert_eq!(stats.total_pixels, 6);
        assert_eq!(stats.unique_color_count(), 3);
        assert_eq!(stats.most_common_color, Some((0xFF, 3)));

        // Test percentages
        assert!((stats.color_usage_percent(0xFF) - 50.0).abs() < 0.1);
        assert!((stats.color_usage_percent(0x00) - 33.33).abs() < 0.1);
    }

    #[test]
    fn test_generate_c_header() {
        let data = vec![0x00, 0xFF, 0xE0, 0x1C];
        let header = generate_c_header(&data, "TEST_IMAGE", "test.bmp", 2, 2);

        assert!(header.contains("#ifndef _TEST_IMAGE_H_"));
        assert!(header.contains("const unsigned char TEST_IMAGE[4]"));
        assert!(header.contains("0x00, 0xFF"));
        assert!(header.contains("0xE0, 0x1C"));
        assert!(header.contains("Image dimensions: 2x2"));
    }
}
