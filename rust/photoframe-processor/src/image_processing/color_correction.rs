use anyhow::Result;
use image::{imageops, Rgb, RgbImage};
use std::fs;
use std::process::Command;

/// Apply automatic color correction to improve image quality before dithering
///
/// This function will try ImageMagick first (if available), then fall back to
/// custom implementation. ImageMagick provides superior auto-color correction.
pub fn apply_auto_color_correction(img: &RgbImage) -> Result<RgbImage> {
    // Try ImageMagick first for superior color correction
    if let Ok(corrected) = apply_imagemagick_auto_color(img) {
        return Ok(corrected);
    }

    // Fall back to custom implementation
    let mut corrected = img.clone();

    // Apply corrections in sequence
    corrected = apply_auto_levels(&corrected)?;
    corrected = apply_white_balance_correction(&corrected)?;
    corrected = apply_saturation_adjustment(&corrected, 1.1)?; // 10% saturation boost

    Ok(corrected)
}

/// Check if ImageMagick is available on the system
pub fn is_imagemagick_available() -> bool {
    // Try 'magick' first (ImageMagick v7), then fall back to 'convert' (v6)
    Command::new("magick")
        .arg("-version")
        .output()
        .map(|output| output.status.success())
        .unwrap_or_else(|_| {
            Command::new("convert")
                .arg("-version")
                .output()
                .map(|output| output.status.success())
                .unwrap_or(false)
        })
}

/// Get the appropriate ImageMagick command ('magick' for v7, 'convert' for v6)
fn get_imagemagick_command() -> &'static str {
    // Try 'magick' first (ImageMagick v7)
    if Command::new("magick")
        .arg("-version")
        .output()
        .map(|output| output.status.success())
        .unwrap_or(false)
    {
        "magick"
    } else {
        "convert"
    }
}

/// Apply automatic color correction using ImageMagick
/// This provides superior results compared to custom implementation
fn apply_imagemagick_auto_color(img: &RgbImage) -> Result<RgbImage> {
    // Create unique temporary files to avoid conflicts in multi-threaded environments
    let temp_dir = std::env::temp_dir();
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let thread_id = std::thread::current().id();

    let input_path = temp_dir.join(format!(
        "photoframe_input_{}_{:?}.png",
        timestamp, thread_id
    ));
    let output_path = temp_dir.join(format!(
        "photoframe_output_{}_{:?}.png",
        timestamp, thread_id
    ));

    // Save input image
    img.save(&input_path)?;

    // Get the correct ImageMagick command
    let magick_cmd = get_imagemagick_command();

    // Run ImageMagick with auto-color correction
    let output = Command::new(magick_cmd)
        .arg(&input_path)
        .arg("-auto-color") // Remove color casts
        .arg("-auto-level") // Stretch histogram
        .arg("-auto-gamma") // Adjust gamma
        .arg("-modulate") // Enhance saturation slightly
        .arg("100,120,100") // brightness,saturation,hue (20% more saturation)
        .arg(&output_path)
        .output()?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow::anyhow!("ImageMagick failed: {}", stderr));
    }

    // Load the corrected image
    let corrected_img = image::open(&output_path)?.to_rgb8();

    // Clean up temporary files
    let _ = fs::remove_file(&input_path);
    let _ = fs::remove_file(&output_path);

    Ok(corrected_img)
}

/// Apply automatic levels correction to stretch histogram
/// This ensures the image uses the full 0-255 range for better contrast
fn apply_auto_levels(img: &RgbImage) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Calculate min/max values for each channel
    let mut min_r = 255u8;
    let mut max_r = 0u8;
    let mut min_g = 255u8;
    let mut max_g = 0u8;
    let mut min_b = 255u8;
    let mut max_b = 0u8;

    for pixel in img.pixels() {
        min_r = min_r.min(pixel[0]);
        max_r = max_r.max(pixel[0]);
        min_g = min_g.min(pixel[1]);
        max_g = max_g.max(pixel[1]);
        min_b = min_b.min(pixel[2]);
        max_b = max_b.max(pixel[2]);
    }

    // Calculate scaling factors for each channel
    let range_r = max_r.saturating_sub(min_r) as f32;
    let range_g = max_g.saturating_sub(min_g) as f32;
    let range_b = max_b.saturating_sub(min_b) as f32;

    // Avoid division by zero
    let scale_r = if range_r > 0.0 { 255.0 / range_r } else { 1.0 };
    let scale_g = if range_g > 0.0 { 255.0 / range_g } else { 1.0 };
    let scale_b = if range_b > 0.0 { 255.0 / range_b } else { 1.0 };

    // Apply levels correction
    for (x, y, pixel) in img.enumerate_pixels() {
        let new_r = ((pixel[0].saturating_sub(min_r) as f32 * scale_r).clamp(0.0, 255.0)) as u8;
        let new_g = ((pixel[1].saturating_sub(min_g) as f32 * scale_g).clamp(0.0, 255.0)) as u8;
        let new_b = ((pixel[2].saturating_sub(min_b) as f32 * scale_b).clamp(0.0, 255.0)) as u8;

        output.put_pixel(x, y, Rgb([new_r, new_g, new_b]));
    }

    Ok(output)
}

/// Apply white balance correction using gray world assumption
/// This removes color casts by balancing the average color to neutral gray
fn apply_white_balance_correction(img: &RgbImage) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Calculate average values for each channel
    let mut sum_r = 0u64;
    let mut sum_g = 0u64;
    let mut sum_b = 0u64;
    let pixel_count = (width * height) as u64;

    for pixel in img.pixels() {
        sum_r += pixel[0] as u64;
        sum_g += pixel[1] as u64;
        sum_b += pixel[2] as u64;
    }

    let avg_r = sum_r as f32 / pixel_count as f32;
    let avg_g = sum_g as f32 / pixel_count as f32;
    let avg_b = sum_b as f32 / pixel_count as f32;

    // Calculate target gray level (overall average)
    let gray_target = (avg_r + avg_g + avg_b) / 3.0;

    // Calculate correction factors
    let correction_r = if avg_r > 0.0 {
        gray_target / avg_r
    } else {
        1.0
    };
    let correction_g = if avg_g > 0.0 {
        gray_target / avg_g
    } else {
        1.0
    };
    let correction_b = if avg_b > 0.0 {
        gray_target / avg_b
    } else {
        1.0
    };

    // Limit correction strength to avoid overcorrection
    let max_correction = 1.5;
    let min_correction = 0.5;
    let correction_r = correction_r.clamp(min_correction, max_correction);
    let correction_g = correction_g.clamp(min_correction, max_correction);
    let correction_b = correction_b.clamp(min_correction, max_correction);

    // Apply white balance correction
    for (x, y, pixel) in img.enumerate_pixels() {
        let new_r = ((pixel[0] as f32 * correction_r).clamp(0.0, 255.0)) as u8;
        let new_g = ((pixel[1] as f32 * correction_g).clamp(0.0, 255.0)) as u8;
        let new_b = ((pixel[2] as f32 * correction_b).clamp(0.0, 255.0)) as u8;

        output.put_pixel(x, y, Rgb([new_r, new_g, new_b]));
    }

    Ok(output)
}

/// Apply saturation adjustment to enhance or reduce color intensity
/// factor > 1.0 increases saturation, factor < 1.0 decreases it
fn apply_saturation_adjustment(img: &RgbImage, factor: f32) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    for (x, y, pixel) in img.enumerate_pixels() {
        let r = pixel[0] as f32;
        let g = pixel[1] as f32;
        let b = pixel[2] as f32;

        // Convert to HSV for saturation adjustment
        let (h, s, v) = rgb_to_hsv(r, g, b);

        // Adjust saturation
        let new_s = (s * factor).clamp(0.0, 1.0);

        // Convert back to RGB
        let (new_r, new_g, new_b) = hsv_to_rgb(h, new_s, v);

        output.put_pixel(
            x,
            y,
            Rgb([
                new_r.clamp(0.0, 255.0) as u8,
                new_g.clamp(0.0, 255.0) as u8,
                new_b.clamp(0.0, 255.0) as u8,
            ]),
        );
    }

    Ok(output)
}

/// Convert RGB to HSV color space
fn rgb_to_hsv(r: f32, g: f32, b: f32) -> (f32, f32, f32) {
    let r = r / 255.0;
    let g = g / 255.0;
    let b = b / 255.0;

    let max = r.max(g).max(b);
    let min = r.min(g).min(b);
    let delta = max - min;

    let h = if delta == 0.0 {
        0.0
    } else if max == r {
        60.0 * (((g - b) / delta) % 6.0)
    } else if max == g {
        60.0 * (((b - r) / delta) + 2.0)
    } else {
        60.0 * (((r - g) / delta) + 4.0)
    };

    let s = if max == 0.0 { 0.0 } else { delta / max };
    let v = max;

    (h, s, v)
}

/// Convert HSV to RGB color space
fn hsv_to_rgb(h: f32, s: f32, v: f32) -> (f32, f32, f32) {
    let c = v * s;
    let x = c * (1.0 - ((h / 60.0) % 2.0 - 1.0).abs());
    let m = v - c;

    let (r_prime, g_prime, b_prime) = match h as u32 {
        0..=59 => (c, x, 0.0),
        60..=119 => (x, c, 0.0),
        120..=179 => (0.0, c, x),
        180..=239 => (0.0, x, c),
        240..=299 => (x, 0.0, c),
        _ => (c, 0.0, x),
    };

    (
        (r_prime + m) * 255.0,
        (g_prime + m) * 255.0,
        (b_prime + m) * 255.0,
    )
}

/// Apply brightness and contrast adjustments using ImageMagick
fn apply_imagemagick_brightness_contrast(
    img: &RgbImage,
    brightness_adjustment: f32,
    contrast_adjustment: f32,
) -> Result<RgbImage> {
    // Create unique temporary files
    let temp_dir = std::env::temp_dir();
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let thread_id = std::thread::current().id();

    let input_path = temp_dir.join(format!(
        "photoframe_input_{}_{:?}.png",
        timestamp, thread_id
    ));
    let output_path = temp_dir.join(format!(
        "photoframe_output_{}_{:?}.png",
        timestamp, thread_id
    ));

    // Save input image
    img.save(&input_path)?;

    // Get the correct ImageMagick command
    let magick_cmd = get_imagemagick_command();

    // Convert adjustments to ImageMagick brightness-contrast format
    // ImageMagick expects brightness/contrast as percentages
    // brightness: -100 to +100 (we have -1.0 to +1.0, so multiply by 100)
    // contrast: -100 to +100 (same conversion)
    let brightness_percent = (brightness_adjustment * 100.0).round() as i32;
    let contrast_percent = (contrast_adjustment * 100.0).round() as i32;

    // Run ImageMagick with brightness-contrast adjustment
    let output = Command::new(magick_cmd)
        .arg(&input_path)
        .arg("-brightness-contrast")
        .arg(format!("{}x{}", brightness_percent, contrast_percent))
        .arg(&output_path)
        .output()?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow::anyhow!(
            "ImageMagick brightness-contrast failed: {}",
            stderr
        ));
    }

    // Load the adjusted image
    let adjusted_img = image::open(&output_path)?.to_rgb8();

    // Clean up temporary files
    let _ = fs::remove_file(&input_path);
    let _ = fs::remove_file(&output_path);

    Ok(adjusted_img)
}

/// Apply contrast adjustment to enhance or reduce image contrast
/// contrast_adjustment: -1.0 to 1.0
///   - Positive values increase contrast (makes darks darker, lights lighter)
///   - Negative values decrease contrast (flattens the image)
///   - 0.0 = no change
pub fn apply_contrast_adjustment(img: &RgbImage, contrast_adjustment: f32) -> Result<RgbImage> {
    if contrast_adjustment == 0.0 {
        // No adjustment needed
        return Ok(img.clone());
    }

    // Try ImageMagick first for superior contrast adjustment
    if is_imagemagick_available() {
        if let Ok(adjusted) = apply_imagemagick_brightness_contrast(img, 0.0, contrast_adjustment) {
            return Ok(adjusted);
        }
    }

    // Fall back to custom implementation
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Contrast formula: adjusted = ((pixel - 128) * (1 + contrast)) + 128
    // This pivots around middle gray (128)
    let factor = 1.0 + contrast_adjustment;

    for (x, y, pixel) in img.enumerate_pixels() {
        let r = ((pixel[0] as f32 - 128.0) * factor + 128.0).clamp(0.0, 255.0) as u8;
        let g = ((pixel[1] as f32 - 128.0) * factor + 128.0).clamp(0.0, 255.0) as u8;
        let b = ((pixel[2] as f32 - 128.0) * factor + 128.0).clamp(0.0, 255.0) as u8;

        output.put_pixel(x, y, Rgb([r, g, b]));
    }

    Ok(output)
}

/// Apply brightness adjustment to make image lighter or darker
/// brightness_adjustment: -1.0 to 1.0
///   - Positive values increase brightness (makes image lighter)
///   - Negative values decrease brightness (makes image darker)
///   - 0.0 = no change
pub fn apply_brightness_adjustment(img: &RgbImage, brightness_adjustment: f32) -> Result<RgbImage> {
    if brightness_adjustment == 0.0 {
        // No adjustment needed
        return Ok(img.clone());
    }

    // Try ImageMagick first for superior brightness adjustment
    if is_imagemagick_available() {
        if let Ok(adjusted) = apply_imagemagick_brightness_contrast(img, brightness_adjustment, 0.0) {
            return Ok(adjusted);
        }
    }

    // Fall back to custom implementation
    // Convert adjustment range from -1.0..1.0 to brightness delta in 0-255 range
    // brightness_adjustment of 1.0 adds 255 (max brightness)
    // brightness_adjustment of -1.0 subtracts 255 (max darkness)
    let delta = (brightness_adjustment * 255.0) as i32;

    // Use image::imageops::brighten function
    let output = imageops::brighten(img, delta);

    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;

    fn create_test_image() -> RgbImage {
        ImageBuffer::from_fn(4, 4, |x, y| {
            match (x % 2, y % 2) {
                (0, 0) => Rgb([100, 50, 50]),   // Dark red with color cast
                (1, 0) => Rgb([50, 100, 50]),   // Dark green with color cast
                (0, 1) => Rgb([50, 50, 100]),   // Dark blue with color cast
                (1, 1) => Rgb([150, 150, 150]), // Gray
                _ => unreachable!(),
            }
        })
    }

    #[test]
    fn test_auto_levels() {
        let img = create_test_image();
        let corrected = apply_auto_levels(&img).unwrap();

        // Check that the output uses a wider range of values
        let mut min_val = 255u8;
        let mut max_val = 0u8;

        for pixel in corrected.pixels() {
            for &channel in pixel.0.iter() {
                min_val = min_val.min(channel);
                max_val = max_val.max(channel);
            }
        }

        // Should have expanded the range
        assert!(max_val > 150, "Max value should be increased: {}", max_val);
    }

    #[test]
    fn test_white_balance_correction() {
        let img = create_test_image();
        let corrected = apply_white_balance_correction(&img).unwrap();

        // Calculate average values to verify balance
        let mut sum_r = 0u64;
        let mut sum_g = 0u64;
        let mut sum_b = 0u64;
        let pixel_count = (corrected.width() * corrected.height()) as u64;

        for pixel in corrected.pixels() {
            sum_r += pixel[0] as u64;
            sum_g += pixel[1] as u64;
            sum_b += pixel[2] as u64;
        }

        let avg_r = sum_r as f32 / pixel_count as f32;
        let avg_g = sum_g as f32 / pixel_count as f32;
        let avg_b = sum_b as f32 / pixel_count as f32;

        // The averages should be closer to each other after white balance
        let max_avg = avg_r.max(avg_g).max(avg_b);
        let min_avg = avg_r.min(avg_g).min(avg_b);
        let range = max_avg - min_avg;

        // Should have reduced the color cast (smaller range between channels)
        assert!(range < 50.0, "Color balance range too high: {}", range);
    }

    #[test]
    fn test_saturation_adjustment() {
        let img = create_test_image();
        let enhanced = apply_saturation_adjustment(&img, 1.5).unwrap();
        let reduced = apply_saturation_adjustment(&img, 0.5).unwrap();

        // Enhanced version should have more vibrant colors
        // Reduced version should be more muted
        // This is a basic test - more sophisticated tests could check actual saturation values
        assert_ne!(img.get_pixel(0, 0), enhanced.get_pixel(0, 0));
        assert_ne!(img.get_pixel(0, 0), reduced.get_pixel(0, 0));
    }

    #[test]
    fn test_rgb_hsv_conversion() {
        // Test pure red
        let (h, s, v) = rgb_to_hsv(255.0, 0.0, 0.0);
        assert!((h - 0.0).abs() < 1.0); // Hue should be ~0 for red
        assert!((s - 1.0).abs() < 0.01); // Should be fully saturated
        assert!((v - 1.0).abs() < 0.01); // Should be full value

        let (r, g, b) = hsv_to_rgb(h, s, v);
        assert!((r - 255.0).abs() < 1.0);
        assert!(g.abs() < 1.0);
        assert!(b.abs() < 1.0);
    }

    #[test]
    fn test_auto_color_correction() {
        let img = create_test_image();
        let corrected = apply_auto_color_correction(&img).unwrap();

        // Should have modified the image
        assert_ne!(img.get_pixel(0, 0), corrected.get_pixel(0, 0));

        // Image dimensions should remain the same
        assert_eq!(img.dimensions(), corrected.dimensions());
    }
}
