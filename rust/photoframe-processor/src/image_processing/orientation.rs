use anyhow::{Context, Result};
use exif::{In, Reader, Tag, Value};
use image::{imageops, RgbImage};
use std::path::Path;

/// EXIF orientation values
/// Based on the EXIF specification and matching utils.sh:15-30
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExifOrientation {
    /// No orientation specified or undefined
    Undefined = 0,
    /// Normal orientation (0 degrees)
    TopLeft = 1,
    /// Horizontally flipped
    TopRight = 2,
    /// Rotated 180 degrees
    BottomRight = 3,
    /// Vertically flipped
    BottomLeft = 4,
    /// Rotated 90 degrees CCW + horizontally flipped
    LeftTop = 5,
    /// Rotated 90 degrees CW (portrait)
    RightTop = 6,
    /// Rotated 90 degrees CW + horizontally flipped
    RightBottom = 7,
    /// Rotated 90 degrees CCW (portrait)
    LeftBottom = 8,
}

impl From<u32> for ExifOrientation {
    fn from(value: u32) -> Self {
        match value {
            1 => ExifOrientation::TopLeft,
            2 => ExifOrientation::TopRight,
            3 => ExifOrientation::BottomRight,
            4 => ExifOrientation::BottomLeft,
            5 => ExifOrientation::LeftTop,
            6 => ExifOrientation::RightTop,
            7 => ExifOrientation::RightBottom,
            8 => ExifOrientation::LeftBottom,
            _ => ExifOrientation::Undefined,
        }
    }
}

impl ExifOrientation {
    /// Get a human-readable description of the orientation
    #[allow(dead_code)]
    pub fn description(&self) -> &'static str {
        match self {
            ExifOrientation::Undefined => "Undefined",
            ExifOrientation::TopLeft => "Normal",
            ExifOrientation::TopRight => "Horizontally flipped",
            ExifOrientation::BottomRight => "Rotated 180°",
            ExifOrientation::BottomLeft => "Vertically flipped",
            ExifOrientation::LeftTop => "Rotated 90° CCW + flipped",
            ExifOrientation::RightTop => "Rotated 90° CW (portrait)",
            ExifOrientation::RightBottom => "Rotated 90° CW + flipped",
            ExifOrientation::LeftBottom => "Rotated 90° CCW (portrait)",
        }
    }

    /// Check if this orientation represents a portrait image
    /// Based on utils.sh:24-28 logic for RightTop and LeftBottom
    pub fn is_rotated_portrait(&self) -> bool {
        matches!(
            self,
            ExifOrientation::RightTop | ExifOrientation::LeftBottom
        )
    }
}

/// Information about an image's orientation
#[derive(Debug)]
pub struct OrientationInfo {
    pub exif_orientation: ExifOrientation,
    pub is_portrait: bool,
    #[allow(dead_code)]
    pub original_width: u32,
    #[allow(dead_code)]
    pub original_height: u32,
    #[allow(dead_code)]
    pub effective_width: u32,
    #[allow(dead_code)]
    pub effective_height: u32,
}

/// Fast orientation detection using only image headers (no full image loading)
///
/// This is much faster than detect_orientation() for batch processing
pub fn detect_orientation_fast(image_path: &Path) -> Result<OrientationInfo> {
    // Get image dimensions without loading the full image
    let image_reader = image::ImageReader::open(image_path)?;
    let (original_width, original_height) = image_reader.into_dimensions()?;

    // Try to read EXIF orientation
    let exif_orientation = read_exif_orientation(image_path).unwrap_or(ExifOrientation::Undefined);

    // Calculate effective dimensions after EXIF rotation
    let (effective_width, effective_height) = if exif_orientation.is_rotated_portrait() {
        // Image is rotated 90°, so dimensions are swapped in EXIF
        (original_height, original_width)
    } else {
        (original_width, original_height)
    };

    // Determine if the image is portrait based on effective dimensions
    let is_portrait = if exif_orientation.is_rotated_portrait() {
        // For 90° rotated images, check the effective dimensions
        effective_height > effective_width // Taller than wide after rotation
    } else {
        // For non-rotated images, check original dimensions
        effective_height > effective_width // Taller than wide
    };

    Ok(OrientationInfo {
        exif_orientation,
        is_portrait,
        original_width,
        original_height,
        effective_width,
        effective_height,
    })
}

/// Detect image orientation from EXIF data and image dimensions
///
/// This matches the logic from utils.sh:extract_image_info()
pub fn detect_orientation(image_path: &Path, img: &RgbImage) -> Result<OrientationInfo> {
    let (original_width, original_height) = img.dimensions();

    // Try to read EXIF orientation
    let exif_orientation = read_exif_orientation(image_path).unwrap_or(ExifOrientation::Undefined);

    // Calculate effective dimensions after EXIF rotation
    let (effective_width, effective_height) = if exif_orientation.is_rotated_portrait() {
        // Image is rotated 90°, so dimensions are swapped in EXIF
        (original_height, original_width)
    } else {
        (original_width, original_height)
    };

    // Determine if the image is portrait based on effective dimensions
    // This matches utils.sh:30-35 logic
    let is_portrait = if exif_orientation.is_rotated_portrait() {
        true // Explicitly rotated to portrait
    } else {
        effective_height > effective_width // Taller than wide
    };

    Ok(OrientationInfo {
        exif_orientation,
        is_portrait,
        original_width,
        original_height,
        effective_width,
        effective_height,
    })
}

/// Read EXIF orientation tag from an image file
fn read_exif_orientation(image_path: &Path) -> Result<ExifOrientation> {
    let file = std::fs::File::open(image_path).with_context(|| {
        format!(
            "Failed to open image for EXIF reading: {}",
            image_path.display()
        )
    })?;

    let mut buf_reader = std::io::BufReader::new(file);
    let exif_reader = Reader::new();

    let exif = exif_reader
        .read_from_container(&mut buf_reader)
        .context("Failed to read EXIF data")?;

    // Look for orientation tag
    if let Some(field) = exif.get_field(Tag::Orientation, In::PRIMARY) {
        if let Value::Short(values) = &field.value {
            if let Some(&orientation_value) = values.first() {
                return Ok(ExifOrientation::from(orientation_value as u32));
            }
        }
    }

    Ok(ExifOrientation::Undefined)
}

/// Apply EXIF rotation to an image
///
/// This function handles all 8 possible EXIF orientations by applying
/// the appropriate combination of rotations and flips
pub fn apply_rotation(img: &RgbImage, orientation: ExifOrientation) -> Result<RgbImage> {
    match orientation {
        ExifOrientation::Undefined | ExifOrientation::TopLeft => {
            // No rotation needed
            Ok(img.clone())
        }
        ExifOrientation::TopRight => {
            // Horizontally flipped
            Ok(imageops::flip_horizontal(img))
        }
        ExifOrientation::BottomRight => {
            // Rotated 180 degrees
            Ok(imageops::rotate180(img))
        }
        ExifOrientation::BottomLeft => {
            // Vertically flipped
            Ok(imageops::flip_vertical(img))
        }
        ExifOrientation::LeftTop => {
            // Rotated 90° CCW + horizontally flipped
            let rotated = imageops::rotate270(img);
            Ok(imageops::flip_horizontal(&rotated))
        }
        ExifOrientation::RightTop => {
            // Rotated 90° CW (most common portrait orientation)
            Ok(imageops::rotate90(img))
        }
        ExifOrientation::RightBottom => {
            // Rotated 90° CW + horizontally flipped
            let rotated = imageops::rotate90(img);
            Ok(imageops::flip_horizontal(&rotated))
        }
        ExifOrientation::LeftBottom => {
            // Rotated 90° CCW (alternative portrait orientation)
            Ok(imageops::rotate270(img))
        }
    }
}

/// Check if dimensions should be swapped based on auto-process mode and orientation
///
/// This implements the logic from auto_resize_and_annotate.sh where it swaps
/// target dimensions if auto mode is enabled and the orientation doesn't match
pub fn should_swap_dimensions(
    image_is_portrait: bool,
    target_width: u32,
    target_height: u32,
    auto_process: bool,
) -> bool {
    if !auto_process {
        return false;
    }

    let target_is_portrait = target_height > target_width;

    // Swap if image orientation doesn't match target orientation
    image_is_portrait != target_is_portrait
}

/// Get the effective target dimensions after considering auto-process swapping
pub fn get_effective_target_dimensions(
    image_is_portrait: bool,
    target_width: u32,
    target_height: u32,
    auto_process: bool,
) -> (u32, u32) {
    if should_swap_dimensions(image_is_portrait, target_width, target_height, auto_process) {
        (target_height, target_width) // Swap dimensions
    } else {
        (target_width, target_height) // Keep original dimensions
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_exif_orientation_from_u32() {
        assert_eq!(ExifOrientation::from(1), ExifOrientation::TopLeft);
        assert_eq!(ExifOrientation::from(6), ExifOrientation::RightTop);
        assert_eq!(ExifOrientation::from(8), ExifOrientation::LeftBottom);
        assert_eq!(ExifOrientation::from(99), ExifOrientation::Undefined);
    }

    #[test]
    fn test_is_rotated_portrait() {
        assert!(!ExifOrientation::TopLeft.is_rotated_portrait());
        assert!(!ExifOrientation::BottomRight.is_rotated_portrait());
        assert!(ExifOrientation::RightTop.is_rotated_portrait());
        assert!(ExifOrientation::LeftBottom.is_rotated_portrait());
    }

    #[test]
    fn test_should_swap_dimensions() {
        // Auto-process disabled - never swap
        assert!(!should_swap_dimensions(true, 800, 480, false));
        assert!(!should_swap_dimensions(false, 800, 480, false));

        // Auto-process enabled
        // Portrait image (true) with landscape target (800x480) -> swap
        assert!(should_swap_dimensions(true, 800, 480, true));

        // Landscape image (false) with landscape target (800x480) -> no swap
        assert!(!should_swap_dimensions(false, 800, 480, true));

        // Portrait image (true) with portrait target (480x800) -> no swap
        assert!(!should_swap_dimensions(true, 480, 800, true));

        // Landscape image (false) with portrait target (480x800) -> swap
        assert!(should_swap_dimensions(false, 480, 800, true));
    }

    #[test]
    fn test_get_effective_target_dimensions() {
        // No auto-process - dimensions unchanged
        assert_eq!(
            get_effective_target_dimensions(true, 800, 480, false),
            (800, 480)
        );

        // Auto-process enabled, portrait image with landscape target -> swap
        assert_eq!(
            get_effective_target_dimensions(true, 800, 480, true),
            (480, 800)
        );

        // Auto-process enabled, landscape image with landscape target -> no swap
        assert_eq!(
            get_effective_target_dimensions(false, 800, 480, true),
            (800, 480)
        );
    }

    #[test]
    fn test_orientation_descriptions() {
        assert_eq!(ExifOrientation::TopLeft.description(), "Normal");
        assert_eq!(
            ExifOrientation::RightTop.description(),
            "Rotated 90° CW (portrait)"
        );
        assert_eq!(ExifOrientation::BottomRight.description(), "Rotated 180°");
    }
}
