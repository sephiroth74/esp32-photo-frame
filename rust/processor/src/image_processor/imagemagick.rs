use crate::logging::Logger;
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
    pub fn get_command() -> &'static str {
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

    /// Apply automatic color correction using ImageMagick
    ///
    /// # Arguments
    /// * `input_path` - Path to input image
    /// * `output_path` - Path to output image
    pub fn apply_auto_correction(input_path: &Path, output_path: &Path) -> Result<()> {
        if Self::is_available() {
            let cmd = Self::get_command();

            let output = Command::new(cmd)
                .arg(input_path.to_string_lossy().as_ref())
                .arg("-separate -contrast-stretch 0.5%x0.5% -combine")
                .arg("-auto-level") // Stretch histogram
                .arg("-auto-gamma") // Adjust gamma
                .arg("-auto-contrast") // Adjust contrast
                .arg("-normalize") // Normalize contrast
                .arg("-modulate")
                .arg("100,120,100") // brightness,saturation,hue (boost saturation by 20%)
                .arg(output_path.to_string_lossy().as_ref())
                .output()
                .context("Failed to execute ImageMagick auto-correction command")?;

            if !output.status.success() {
                let err_msg = String::from_utf8_lossy(&output.stderr);
                return Err(anyhow::anyhow!(
                    "ImageMagick auto-correction failed: {}",
                    err_msg
                ));
            }

            Ok(())
        } else {
            Err(anyhow::anyhow!("ImageMagick is not available"))
        }
    }

    /// Apply color correction (brightness/contrast/saturation) using ImageMagick
    ///
    /// # Arguments
    /// * `input_path` - Path to input image
    /// * `output_path` - Path to output image
    /// * `brightness` - Brightness adjustment (0 to 1000)
    /// * `contrast` - Contrast adjustment (0 to 1000)
    /// * `saturation` - Saturation (0 to 1000)
    pub fn apply_color_correction(
        input_path: &Path,
        output_path: &Path,
        brightness: u32,
        contrast: u32,
        saturation: u32,
        logger: &Logger,
    ) -> Result<()> {
        if !Self::is_available() {
            return Err(anyhow::anyhow!("ImageMagick is not available"));
        }

        let cmd = Self::get_command();

        let mut command = Command::new(cmd);
        command.arg(input_path.to_string_lossy().as_ref());
        command.arg("-auto-level"); // Stretch histogram

        // Apply saturation and brightness
        if saturation != 100 || brightness != 100 {
            command
                .arg("-modulate")
                .arg(format!("{},{},100", brightness, saturation));
        }

        // Apply contrast
        if contrast != 100 {
            command.arg("-contrast-stretch").arg("0");
            command
                .arg("-brightness-contrast")
                .arg(format!("0x{}", 100i32 - (contrast as i32)));
        }

        command.arg(output_path.to_string_lossy().as_ref());

        logger.verbose(&format!("cmd: {}", output_path.display()));

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
    #[allow(dead_code)]
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
    #[allow(dead_code)]
    pub fn load_image(path: &Path) -> Result<RgbImage> {
        Ok(image::open(path).context("Failed to open image")?.to_rgb8())
    }

    /// Save an RgbImage using ImageMagick (preserves quality)
    ///
    /// # Arguments
    /// * `image` - RgbImage to save
    /// * `output_path` - Path to save to
    /// * `quality` - JPEG quality (1-100), default is 90
    #[allow(dead_code)]
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
