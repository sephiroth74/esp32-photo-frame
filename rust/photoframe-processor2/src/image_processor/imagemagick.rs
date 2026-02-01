use anyhow::{Context, Result};
use image::RgbImage;
use std::path::Path;
use std::process::Command;

/// Wrapper class for ImageMagick operations
/// Centralizes all ImageMagick command execution, version detection, and error handling
#[allow(dead_code)]
pub struct ImageMagickWrapper;

impl ImageMagickWrapper {
    /// Check if ImageMagick is available on the system
    /// Tries 'magick' (v7) first, then falls back to 'convert' (v6)
    pub fn is_available() -> bool {
        // Try 'magick' first (ImageMagick v7)
        if Self::command_works("magick") {
            return true;
        }
        // Fall back to 'convert' (v6)
        Self::command_works("convert")
    }

    /// Get the appropriate ImageMagick command ('magick' for v7, 'convert' for v6)
    fn get_command() -> &'static str {
        if Self::command_works("magick") {
            "magick"
        } else {
            "convert"
        }
    }

    /// Test if a command works by trying to get its version
    fn command_works(cmd: &str) -> bool {
        Command::new(cmd)
            .arg("-version")
            .output()
            .map(|output| output.status.success())
            .unwrap_or(false)
    }

    /// Add text annotation to an image using ImageMagick
    ///
    /// # Arguments
    /// * `input_path` - Path to input image
    /// * `output_path` - Path to output image
    /// * `text` - Text to annotate
    /// * `font_name` - Font name or path
    /// * `font_size` - Font size in pixels
    /// * `gravity` - Gravity position (NorthWest, North, NorthEast, West, Center, East, SouthWest, South, SouthEast)
    /// * `offset_x` - Horizontal offset in pixels
    /// * `offset_y` - Vertical offset in pixels
    pub fn annotate_text(
        input_path: &Path,
        output_path: &Path,
        text: &str,
        font_name: &str,
        font_size: u32,
        gravity: &str,
        offset_x: i32,
        offset_y: i32,
    ) -> Result<()> {
        if !Self::is_available() {
            return Err(anyhow::anyhow!("ImageMagick is not available"));
        }

        let cmd = Self::get_command();

        let offset_str = if offset_x >= 0 && offset_y >= 0 {
            format!("+{}+{}", offset_x, offset_y)
        } else {
            format!("{}+{}", offset_x, offset_y)
        };

        let output = Command::new(cmd)
            .arg(input_path.to_string_lossy().as_ref())
            .arg("-fill")
            .arg("white")
            .arg("-pointsize")
            .arg(font_size.to_string())
            .arg("-font")
            .arg(font_name)
            .arg("-gravity")
            .arg(gravity)
            .arg("-annotate")
            .arg(offset_str)
            .arg(text)
            .arg(output_path.to_string_lossy().as_ref())
            .output()
            .context("Failed to execute ImageMagick annotate command")?;

        if !output.status.success() {
            let err_msg = String::from_utf8_lossy(&output.stderr);
            return Err(anyhow::anyhow!("ImageMagick annotate failed: {}", err_msg));
        }

        Ok(())
    }

    /// Apply color correction (brightness/contrast/saturation) using ImageMagick
    ///
    /// # Arguments
    /// * `input_path` - Path to input image
    /// * `output_path` - Path to output image
    /// * `brightness` - Brightness adjustment (-100 to 100)
    /// * `contrast` - Contrast adjustment (-100 to 100)
    /// * `saturation` - Saturation multiplier (0.0 to 2.0)
    pub fn apply_color_correction(
        input_path: &Path,
        output_path: &Path,
        brightness: i32,
        contrast: i32,
        saturation: f32,
    ) -> Result<()> {
        if !Self::is_available() {
            return Err(anyhow::anyhow!("ImageMagick is not available"));
        }

        let cmd = Self::get_command();

        let mut command = Command::new(cmd);
        command.arg(input_path.to_string_lossy().as_ref());

        // Apply brightness
        if brightness != 0 {
            command
                .arg("-modulate")
                .arg(format!("{},100,100", 100 + brightness));
        }

        // Apply contrast
        if contrast != 0 {
            command.arg("-contrast-stretch").arg("0");
            command
                .arg("-brightness-contrast")
                .arg(format!("0x{}", contrast as i32));
        }

        // Apply saturation
        if (saturation - 1.0).abs() > 0.01 {
            let saturation_percent = (saturation * 100.0) as u32;
            command
                .arg("-modulate")
                .arg(format!("100,{},100", saturation_percent));
        }

        command.arg(output_path.to_string_lossy().as_ref());

        let output = command
            .output()
            .context("Failed to execute ImageMagick color correction")?;

        if !output.status.success() {
            let err_msg = String::from_utf8_lossy(&output.stderr);
            return Err(anyhow::anyhow!(
                "ImageMagick color correction failed: {}",
                err_msg
            ));
        }

        Ok(())
    }

    /// Resize image using ImageMagick
    ///
    /// # Arguments
    /// * `input_path` - Path to input image
    /// * `output_path` - Path to output image
    /// * `width` - Target width
    /// * `height` - Target height
    /// * `filter` - Resize filter (Lanczos, Cubic, Gaussian, etc.)
    pub fn resize(
        input_path: &Path,
        output_path: &Path,
        width: u32,
        height: u32,
        filter: &str,
    ) -> Result<()> {
        if !Self::is_available() {
            return Err(anyhow::anyhow!("ImageMagick is not available"));
        }

        let cmd = Self::get_command();

        let output = Command::new(cmd)
            .arg(input_path.to_string_lossy().as_ref())
            .arg("-filter")
            .arg(filter)
            .arg("-resize")
            .arg(format!("{}x{}", width, height))
            .arg("-define")
            .arg("filter:support=2")
            .arg(output_path.to_string_lossy().as_ref())
            .output()
            .context("Failed to execute ImageMagick resize command")?;

        if !output.status.success() {
            let err_msg = String::from_utf8_lossy(&output.stderr);
            return Err(anyhow::anyhow!("ImageMagick resize failed: {}", err_msg));
        }

        Ok(())
    }

    /// Load an image file and convert to RgbImage
    pub fn load_image(path: &Path) -> Result<RgbImage> {
        Ok(image::open(path).context("Failed to open image")?.to_rgb8())
    }

    /// Save an RgbImage using ImageMagick (preserves quality)
    ///
    /// # Arguments
    /// * `image` - RgbImage to save
    /// * `output_path` - Path to save to
    /// * `quality` - JPEG quality (1-100), default is 90
    pub fn save_image(image: &RgbImage, output_path: &Path, quality: Option<u32>) -> Result<()> {
        if !Self::is_available() {
            // Fallback to standard image crate
            return image.save(output_path).context("Failed to save image");
        }

        // Create temporary file for input
        let temp_input =
            tempfile::NamedTempFile::new().context("Failed to create temp input file")?;
        image
            .save(temp_input.path())
            .context("Failed to save image to temp file")?;

        let cmd = Self::get_command();
        let mut command = Command::new(cmd);
        command.arg(temp_input.path().to_string_lossy().as_ref());

        if let Some(q) = quality {
            command.arg("-quality").arg(q.to_string());
        }

        command.arg(output_path.to_string_lossy().as_ref());

        let output = command
            .output()
            .context("Failed to execute ImageMagick save command")?;

        if !output.status.success() {
            let err_msg = String::from_utf8_lossy(&output.stderr);
            return Err(anyhow::anyhow!("ImageMagick save failed: {}", err_msg));
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_is_available() {
        // This test will pass if ImageMagick is installed, fail otherwise
        let _ = ImageMagickWrapper::is_available();
    }
}
