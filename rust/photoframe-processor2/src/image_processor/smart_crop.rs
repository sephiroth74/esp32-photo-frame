/// Smart crop module - intelligent cropping based on subject detection
use anyhow::Result;
use image::RgbImage;

use super::subject_detection::SubjectDetectionResult;

/// Smart crop and resize with people detection awareness
/// Returns the final image at exact target dimensions
pub fn smart_crop_and_resize(
    img: &RgbImage,
    target_width: u32,
    target_height: u32,
    detection: Option<&SubjectDetectionResult>,
) -> Result<RgbImage> {
    let (crop_x, crop_y, crop_width, crop_height) =
        compute_crop_area(img, target_width, target_height, detection);

    // Crop the image
    let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height)?;

    // Resize to exact target dimensions if needed
    if cropped.width() != target_width || cropped.height() != target_height {
        resize_image(&cropped, target_width, target_height)
    } else {
        Ok(cropped)
    }
}

/// Compute crop area based on target dimensions and detection
/// Returns (crop_x, crop_y, crop_width, crop_height)
pub fn compute_crop_area(
    img: &RgbImage,
    target_width: u32,
    target_height: u32,
    detection: Option<&SubjectDetectionResult>,
) -> (u32, u32, u32, u32) {
    let (src_width, src_height) = img.dimensions();

    // Calculate crop dimensions to maintain aspect ratio
    let target_aspect = target_width as f64 / target_height as f64;
    let source_aspect = src_width as f64 / src_height as f64;

    let (crop_width, crop_height) = if source_aspect > target_aspect {
        // Source is wider - crop width
        let new_width = (src_height as f64 * target_aspect) as u32;
        (new_width.min(src_width), src_height)
    } else {
        // Source is taller - crop height
        let new_height = (src_width as f64 / target_aspect) as u32;
        (src_width, new_height.min(src_height))
    };

    // Determine crop position
    let (crop_x, crop_y) = if let Some(det) = detection {
        if det.person_count > 0 {
            // Use smart cropping based on detection
            calculate_smart_crop_offset(src_width, src_height, crop_width, crop_height, det)
        } else {
            // No people detected, use center crop
            standard_crop_offset(src_width, src_height, crop_width, crop_height)
        }
    } else {
        // No detection available, use center crop
        standard_crop_offset(src_width, src_height, crop_width, crop_height)
    };

    (crop_x, crop_y, crop_width, crop_height)
}

/// Calculate smart crop offset based on detection result
fn calculate_smart_crop_offset(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
    detection: &SubjectDetectionResult,
) -> (u32, u32) {
    // Use enhanced algorithm if bounding box is available
    if let Some((det_x_min, det_y_min, det_x_max, det_y_max)) = detection.bounding_box {
        // Try to keep the entire detection box within the crop
        calculate_enhanced_crop(
            src_width,
            src_height,
            crop_width,
            crop_height,
            det_x_min,
            det_y_min,
            det_x_max,
            det_y_max,
        )
    } else {
        // Fallback to center-based crop
        let center_x = detection.center.0;
        let center_y = detection.center.1;

        let ideal_left = center_x.saturating_sub(crop_width / 2);
        let ideal_top = center_y.saturating_sub(crop_height / 2);

        let crop_left = ideal_left.min(src_width.saturating_sub(crop_width));
        let crop_top = ideal_top.min(src_height.saturating_sub(crop_height));

        (crop_left, crop_top)
    }
}

/// Enhanced crop that ensures the detection box is fully contained
fn calculate_enhanced_crop(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
    det_x_min: u32,
    det_y_min: u32,
    det_x_max: u32,
    det_y_max: u32,
) -> (u32, u32) {
    // Calculate detection center
    let det_center_x = (det_x_min + det_x_max) / 2;
    let det_center_y = (det_y_min + det_y_max) / 2;

    // Start by centering on detection
    let mut crop_x = det_center_x.saturating_sub(crop_width / 2);
    let mut crop_y = det_center_y.saturating_sub(crop_height / 2);

    // Clamp to image boundaries
    crop_x = crop_x.min(src_width.saturating_sub(crop_width));
    crop_y = crop_y.min(src_height.saturating_sub(crop_height));

    // Verify the detection box is fully contained
    // If not, adjust to include it
    let crop_right = crop_x + crop_width;
    let crop_bottom = crop_y + crop_height;

    // Adjust if detection extends beyond crop
    if det_x_min < crop_x {
        crop_x = det_x_min;
    }
    if det_x_max > crop_right {
        crop_x = det_x_max.saturating_sub(crop_width);
    }
    if det_y_min < crop_y {
        crop_y = det_y_min;
    }
    if det_y_max > crop_bottom {
        crop_y = det_y_max.saturating_sub(crop_height);
    }

    // Final clamp to image boundaries
    crop_x = crop_x.min(src_width.saturating_sub(crop_width));
    crop_y = crop_y.min(src_height.saturating_sub(crop_height));

    (crop_x, crop_y)
}

/// Standard center crop offset
fn standard_crop_offset(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
) -> (u32, u32) {
    let crop_x = (src_width.saturating_sub(crop_width)) / 2;
    let crop_y = (src_height.saturating_sub(crop_height)) / 2;
    (crop_x, crop_y)
}

/// Crop an image to specified dimensions
fn crop_image(img: &RgbImage, x: u32, y: u32, width: u32, height: u32) -> Result<RgbImage> {
    let (img_width, img_height) = img.dimensions();

    // Validate crop bounds
    if x + width > img_width || y + height > img_height {
        return Err(anyhow::anyhow!(
            "Crop bounds exceed image dimensions: crop=({},{},{}x{}) image={}x{}",
            x,
            y,
            width,
            height,
            img_width,
            img_height
        ));
    }

    // Create new image and copy pixels
    let mut cropped = RgbImage::new(width, height);
    for (dx, dy, pixel) in cropped.enumerate_pixels_mut() {
        *pixel = *img.get_pixel(x + dx, y + dy);
    }

    Ok(cropped)
}

/// Resize image to exact dimensions using Lanczos3 filter
fn resize_image(img: &RgbImage, width: u32, height: u32) -> Result<RgbImage> {
    use image::imageops::FilterType;

    Ok(image::imageops::resize(
        img,
        width,
        height,
        FilterType::Lanczos3,
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::Rgb;

    #[test]
    fn test_standard_crop_offset() {
        let (crop_x, crop_y) = standard_crop_offset(1000, 800, 600, 400);
        assert_eq!(crop_x, 200); // (1000-600)/2
        assert_eq!(crop_y, 200); // (800-400)/2
    }

    #[test]
    fn test_enhanced_crop_centered() {
        // Detection in center, should fit easily
        let (crop_x, crop_y) = calculate_enhanced_crop(
            1000, // src_width
            800,  // src_height
            600,  // crop_width
            400,  // crop_height
            400,  // det_x_min
            300,  // det_y_min
            600,  // det_x_max
            500,  // det_y_max
        );

        // Should center on detection (500, 400)
        // Ideal: left=200, top=200
        assert_eq!(crop_x, 200);
        assert_eq!(crop_y, 200);
    }

    #[test]
    fn test_enhanced_crop_boundary() {
        // Detection near edge
        let (crop_x, crop_y) = calculate_enhanced_crop(
            1000, // src_width
            800,  // src_height
            600,  // crop_width
            400,  // crop_height
            800,  // det_x_min (near right edge)
            600,  // det_y_min (near bottom edge)
            950,  // det_x_max
            750,  // det_y_max
        );

        // Should be clamped to fit image boundaries
        assert!(crop_x + 600 <= 1000);
        assert!(crop_y + 400 <= 800);

        // Should include detection box
        assert!(crop_x <= 800);
        assert!(crop_x + 600 >= 950);
        assert!(crop_y <= 600);
        assert!(crop_y + 400 >= 750);
    }

    #[test]
    fn test_crop_image() {
        let img = RgbImage::from_fn(100, 100, |x, _y| {
            if x < 50 {
                Rgb([255, 0, 0]) // Red left half
            } else {
                Rgb([0, 0, 255]) // Blue right half
            }
        });

        let cropped = crop_image(&img, 25, 25, 50, 50).unwrap();
        assert_eq!(cropped.dimensions(), (50, 50));

        // Check that we got both colors
        let left_pixel = cropped.get_pixel(0, 0);
        let right_pixel = cropped.get_pixel(49, 0);
        assert_eq!(*left_pixel, Rgb([255, 0, 0]));
        assert_eq!(*right_pixel, Rgb([0, 0, 255]));
    }
}
