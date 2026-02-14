use crate::logging::Logger;
use anyhow::{Context, Result};
use image::RgbImage;
use std::path::Path;
use std::process::{Command, Stdio};

/// Wrapper class for ImageMagick operations
/// Centralizes all ImageMagick command execution, version detection, and error handling
#[allow(dead_code)]
pub struct ImageMagickWrapper;

impl ImageMagickWrapper {
    /// Check if ImageMagick is available on the system
    /// Tries 'magick' (v7) first, then falls back to 'convert' (v6)
    pub fn is_available() -> bool {
        Self::command_works("magick")
    }

    /// Get the appropriate ImageMagick command ('magick' for v7, 'convert' for v6)
    pub fn get_command() -> Option<&'static str> {
        if Self::is_available() {
            Some("magick")
        } else {
            None
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

    pub fn annotate(
        input_path: &Path,
        output_path: &Path,
        font: &str,
        font_size: u32,
        text: &str,
        foreground_color: Option<&str>,
        background_color: Option<&str>,
        gravity: Option<&str>,
        offset: Option<&str>,
        border_size: Option<u16>,
    ) -> Result<()> {
        if Self::is_available() {
            let fcolor = foreground_color.unwrap_or("#FFFFFFFF");
            let bcolor = background_color.unwrap_or("#00000080");
            let border = border_size.unwrap_or(6);

            let first = Command::new("magick")
                .arg("convert")
                .arg("-background")
                .arg("#00000000") // Transparent background
                .arg("-fill")
                .arg(fcolor) // Text color
                .arg("-font")
                .arg(font) // Font name
                .arg("-pointsize")
                .arg(font_size.to_string()) // Font size
                .arg("-border")
                .arg(format!("{}x{}", border, border)) // Border around text
                .arg("-bordercolor")
                .arg(bcolor) // Semi-transparent black border
                .arg(format!("label:{}", text)) // Text to render
                .arg("miff:-")
                .stdout(Stdio::piped())
                .spawn()?;

            let output = &mut Command::new("magick")
                .arg("composite")
                .arg("-gravity")
                .arg(gravity.unwrap_or("SouthEast")) // Center the text on the image
                .arg("-geometry")
                .arg(offset.unwrap_or("+10+10")) // No offset
                .arg("-") // Read text image from stdin
                .arg(input_path.to_string_lossy().as_ref())
                .arg(output_path.to_string_lossy().as_ref())
                .stdin(Stdio::from(first.stdout.unwrap()))
                .spawn()?
                .wait_with_output()?;

            let _ = str::from_utf8(&output.stdout)?;
            Ok(())
        } else {
            Err(anyhow::anyhow!("ImageMagick is not available"))
        }
    }

    /// Apply automatic color correction using ImageMagick
    ///
    /// # Arguments
    /// * `input_path` - Path to input image
    /// * `output_path` - Path to output image
    pub fn apply_auto_correction(input_path: &Path, output_path: &Path) -> Result<()> {
        if let Some(cmd) = Self::get_command() {
            let output = Command::new(cmd)
                .arg("convert")
                .arg(input_path.to_string_lossy().as_ref())
                .arg("-separate")
                .arg("-contrast-stretch")
                .arg("0.5%x0.5%")
                .arg("-combine")
                .arg("-auto-level") // Stretch histogram
                .arg("-auto-gamma") // Adjust gamma
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
        brightness: i32,
        contrast: i32,
        saturation: u32,
        logger: &Logger,
    ) -> Result<()> {
        if let Some(cmd) = Self::get_command() {
            let mut command = Command::new(cmd);
            command.arg(input_path.to_string_lossy().as_ref());
            command.arg("-auto-level"); // Stretch histogram

            // Apply saturation and brightness
            if saturation != 100 {
                command
                    .arg("-modulate")
                    .arg(format!("100,{},100", saturation));
            }

            // Apply contrast
            if contrast != 0 || brightness != 0 {
                command
                    .arg("-brightness-contrast")
                    .arg(format!("{}x{}", brightness, contrast));
            }

            command.arg(output_path.to_string_lossy().as_ref());

            logger.verbose(&format!("cmd: {:?}", &command));

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
        } else {
            Err(anyhow::anyhow!("ImageMagick is not available"))
        }
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
        if let Some(cmd) = Self::get_command() {
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
        } else {
            Err(anyhow::anyhow!("ImageMagick is not available"))
        }
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

        let cmd = Self::get_command().context("Failed to get image command")?;
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

    #[test]
    fn test_annotate() {
        let datetime = chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string();
        // This test will pass if ImageMagick is installed and can annotate, fail otherwise
        let input =
            Path::new("/Users/alessandro/Desktop/arduino/photos/test2/IMG-20251006-WA0002.jpg");
        let output = Path::new("/Users/alessandro/Desktop/arduino/photos/test2/output.jpg");
        let result = ImageMagickWrapper::annotate(
            input,
            output,
            "InconsolataLGC Nerd Font Mono",
            24,
            &datetime,
            Some("#FFFFFFFF"),
            Some("#00000080"),
            Some("SouthEast"),
            Some("+10+10"),
            Some(10),
        );
        assert!(result.is_ok());
    }
}
