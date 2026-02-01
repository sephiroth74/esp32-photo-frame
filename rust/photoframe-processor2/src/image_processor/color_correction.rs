use super::imagemagick::ImageMagickWrapper;
use anyhow::{Context, Result};
use image::{Rgb, RgbImage};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

/// Get the project-relative temp directory
/// TODO: Change back to std::env::temp_dir() once development is complete
fn get_temp_dir() -> Result<PathBuf> {
    let temp_dir = Path::new("./temp");
    if !temp_dir.exists() {
        fs::create_dir_all(temp_dir).context("Failed to create temp directory")?;
    }
    Ok(temp_dir.to_path_buf())
}

/// Check if ImageMagick is available on the system
pub fn is_imagemagick_available() -> bool {
    ImageMagickWrapper::is_available()
}

/// Get the appropriate ImageMagick command ('magick' for v7, 'convert' for v6)
/// This is a helper that queries the wrapper
fn get_imagemagick_command() -> &'static str {
    if ImageMagickWrapper::is_available() {
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
    } else {
        "convert"
    }
}

/// Apply automatic color correction using ImageMagick
/// Applies: auto-white-balance, auto-level, auto-color, auto-saturation, auto-gamma
fn apply_imagemagick_auto_correction(img: &RgbImage) -> Result<RgbImage> {
    let temp_dir = get_temp_dir()?;
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let thread_id = std::thread::current().id();

    let input_path = temp_dir.join(format!("pfproc_color_in_{}_{:?}.png", timestamp, thread_id));
    let output_path = temp_dir.join(format!(
        "pfproc_color_out_{}_{:?}.png",
        timestamp, thread_id
    ));

    // Save input image
    img.save(&input_path)
        .context("Failed to save temporary input image")?;

    let magick_cmd = get_imagemagick_command();

    // Apply full auto-correction pipeline
    let output = Command::new(magick_cmd)
        .arg(&input_path)
        .arg("-separate -contrast-stretch 0.5%x0.5% -combine")
        .arg("-auto-level") // Stretch histogram
        .arg("-auto-gamma") // Adjust gamma
        .arg("-normalize") // Normalize contrast
        .arg("-modulate")
        .arg("100,120,100") // brightness,saturation,hue (boost saturation by 20%)
        .arg(&output_path)
        .output()
        .context("Failed to execute ImageMagick")?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow::anyhow!(
            "ImageMagick auto-correction failed: {}",
            stderr
        ));
    }

    // Load the corrected image
    let corrected_img = image::open(&output_path)
        .context("Failed to load corrected image")?
        .to_rgb8();

    // Clean up temporary files immediately
    let _ = fs::remove_file(&input_path);
    let _ = fs::remove_file(&output_path);

    Ok(corrected_img)
}

/// Apply manual color correction using ImageMagick
/// Applies: auto-white-balance, auto-level, then custom brightness/contrast/saturation
fn apply_imagemagick_manual_correction(
    img: &RgbImage,
    brightness: i32,
    contrast: i32,
    saturation: u32,
) -> Result<RgbImage> {
    let temp_dir = get_temp_dir()?;
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let thread_id = std::thread::current().id();

    let input_path = temp_dir.join(format!("pfproc_color_in_{}_{:?}.png", timestamp, thread_id));
    let output_path = temp_dir.join(format!(
        "pfproc_color_out_{}_{:?}.png",
        timestamp, thread_id
    ));

    // Save input image
    img.save(&input_path)
        .context("Failed to save temporary input image")?;

    let magick_cmd = get_imagemagick_command();

    // Apply auto white balance and levels, then manual adjustments
    let output = Command::new(magick_cmd)
        .arg(&input_path)
        .arg("-auto-level") // Stretch histogram
        .arg("-brightness-contrast")
        .arg(format!("{}x{}", brightness, contrast))
        .arg("-modulate")
        .arg(format!("100,{},100", saturation)) // brightness,saturation,hue
        .arg(&output_path)
        .output()
        .context("Failed to execute ImageMagick")?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(anyhow::anyhow!(
            "ImageMagick manual correction failed: {}",
            stderr
        ));
    }

    // Load the corrected image
    let corrected_img = image::open(&output_path)
        .context("Failed to load corrected image")?
        .to_rgb8();

    // Clean up temporary files immediately
    let _ = fs::remove_file(&input_path);
    let _ = fs::remove_file(&output_path);

    Ok(corrected_img)
}

/// Fallback: Apply automatic color correction using photoframe-lib
fn apply_fallback_auto_correction(img: &RgbImage) -> Result<RgbImage> {
    let mut corrected = img.clone();

    // Apply auto-levels (stretch histogram)
    corrected = apply_auto_levels(&corrected)?;

    // Apply white balance correction
    corrected = apply_white_balance(&corrected)?;

    // Apply saturation boost (1.2 = 20% increase)
    corrected = photoframe_lib::apply_color_adjustments(&corrected, 1.2, 1.0, 1.0);

    Ok(corrected)
}

/// Fallback: Apply manual color correction using photoframe-lib
fn apply_fallback_manual_correction(
    img: &RgbImage,
    brightness: i32,
    contrast: i32,
    saturation: u32,
) -> Result<RgbImage> {
    let mut corrected = img.clone();

    // Apply auto-levels (stretch histogram)
    corrected = apply_auto_levels(&corrected)?;

    // Apply white balance correction
    corrected = apply_white_balance(&corrected)?;

    // Convert CLI parameters to photoframe-lib format
    // brightness: -100..100 -> 0.0..2.0 (100 = 1.0)
    let brightness_factor = 1.0 + (brightness as f32 / 100.0);

    // contrast: -100..100 -> 0.0..2.0 (100 = 1.0)
    let contrast_factor = 1.0 + (contrast as f32 / 100.0);

    // saturation: 50..200 -> 0.5..2.0 (100 = 1.0)
    let saturation_factor = saturation as f32 / 100.0;

    corrected = photoframe_lib::apply_color_adjustments(
        &corrected,
        saturation_factor,
        contrast_factor,
        brightness_factor,
    );

    Ok(corrected)
}

/// Apply color correction based on auto_color_correct flag
/// If auto_color_correct is true: applies full auto-correction
/// Otherwise: applies auto white balance and levels, then manual brightness/contrast/saturation
pub fn apply_color_correction(
    img: &RgbImage,
    auto_color_correct: bool,
    brightness: i32,
    contrast: i32,
    saturation: u32,
) -> Result<RgbImage> {
    let processed_image = if auto_color_correct {
        // Try ImageMagick first for full auto color correction
        if is_imagemagick_available() {
            match apply_imagemagick_auto_correction(img) {
                Ok(corrected) => return Ok(corrected),
                Err(_) => apply_fallback_auto_correction(img),
            }?
        } else {
            // Fall back to photoframe-lib
            apply_fallback_auto_correction(img)?
        }
    } else {
        img.clone()
    };

    if brightness == 0 && contrast == 0 && saturation == 100 {
        // No manual adjustments needed
        eprintln!("No manual adjustments specified, skipping further color correction.");
        return Ok(processed_image);
    }

    // Try ImageMagick first for manual correction
    if is_imagemagick_available() {
        if let Ok(corrected) =
            apply_imagemagick_manual_correction(&processed_image, brightness, contrast, saturation)
        {
            return Ok(corrected);
        }
    }
    // Fall back to photoframe-lib
    apply_fallback_manual_correction(&processed_image, brightness, contrast, saturation)
}

/// Apply automatic levels correction to stretch histogram
fn apply_auto_levels(img: &RgbImage) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Find min and max values for each channel
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

    // Stretch each channel to full 0-255 range
    let range_r = (max_r - min_r) as f32;
    let range_g = (max_g - min_g) as f32;
    let range_b = (max_b - min_b) as f32;

    for (x, y, pixel) in img.enumerate_pixels() {
        let new_r = if range_r > 0.0 {
            (((pixel[0] - min_r) as f32 / range_r) * 255.0) as u8
        } else {
            pixel[0]
        };
        let new_g = if range_g > 0.0 {
            (((pixel[1] - min_g) as f32 / range_g) * 255.0) as u8
        } else {
            pixel[1]
        };
        let new_b = if range_b > 0.0 {
            (((pixel[2] - min_b) as f32 / range_b) * 255.0) as u8
        } else {
            pixel[2]
        };

        output.put_pixel(x, y, Rgb([new_r, new_g, new_b]));
    }

    Ok(output)
}

/// Apply white balance correction using gray world assumption
fn apply_white_balance(img: &RgbImage) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Calculate average color
    let mut sum_r = 0u64;
    let mut sum_g = 0u64;
    let mut sum_b = 0u64;
    let pixel_count = (width * height) as u64;

    for pixel in img.pixels() {
        sum_r += pixel[0] as u64;
        sum_g += pixel[1] as u64;
        sum_b += pixel[2] as u64;
    }

    let avg_r = (sum_r as f32) / (pixel_count as f32);
    let avg_g = (sum_g as f32) / (pixel_count as f32);
    let avg_b = (sum_b as f32) / (pixel_count as f32);

    // Calculate target gray level
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

    // Limit correction strength
    let max_correction = 1.5;
    let min_correction = 0.5;
    let correction_r = correction_r.clamp(min_correction, max_correction);
    let correction_g = correction_g.clamp(min_correction, max_correction);
    let correction_b = correction_b.clamp(min_correction, max_correction);

    // Apply white balance
    for (x, y, pixel) in img.enumerate_pixels() {
        let new_r = ((pixel[0] as f32 * correction_r).clamp(0.0, 255.0)) as u8;
        let new_g = ((pixel[1] as f32 * correction_g).clamp(0.0, 255.0)) as u8;
        let new_b = ((pixel[2] as f32 * correction_b).clamp(0.0, 255.0)) as u8;

        output.put_pixel(x, y, Rgb([new_r, new_g, new_b]));
    }

    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::RgbImage;

    #[test]
    fn test_imagemagick_available() {
        // Just verify the function doesn't panic
        let _ = is_imagemagick_available();
    }

    #[test]
    fn test_auto_levels() {
        let mut img = RgbImage::new(10, 10);
        // Create gradient image
        for (x, _y, pixel) in img.enumerate_pixels_mut() {
            let val = (x * 255 / 10) as u8;
            *pixel = Rgb([val, val, val]);
        }

        let result = apply_auto_levels(&img).unwrap();
        assert_eq!(result.dimensions(), img.dimensions());
    }

    #[test]
    fn test_white_balance() {
        let mut img = RgbImage::new(10, 10);
        // Create image with color cast
        for pixel in img.pixels_mut() {
            *pixel = Rgb([200, 150, 100]);
        }

        let result = apply_white_balance(&img).unwrap();
        assert_eq!(result.dimensions(), img.dimensions());
    }
}
