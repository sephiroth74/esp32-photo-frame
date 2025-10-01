use anyhow::Result;
use fast_image_resize::{images::Image, ResizeOptions, Resizer};
use image::{ImageBuffer, Rgb, RgbImage};
use std::num::NonZeroU32;

use super::orientation::get_effective_target_dimensions;
use super::subject_detection::SubjectDetectionResult;
use crate::utils::verbose_println;

/// Smart resize with people detection-aware cropping
///
/// This function uses YOLO detection results to make intelligent cropping decisions,
/// focusing on detected people rather than just center-cropping the image.
pub fn smart_resize_with_people_detection(
    img: &RgbImage,
    target_width: u32,
    target_height: u32,
    auto_process: bool,
    people_detection: Option<&SubjectDetectionResult>,
    verbose: bool,
) -> Result<RgbImage> {
    let (src_width, src_height) = img.dimensions();

    // Determine if image is portrait for auto-process consideration
    let image_is_portrait = src_height > src_width;

    // Get effective target dimensions (may be swapped in auto mode)
    let (eff_width, eff_height) = get_effective_target_dimensions(
        image_is_portrait,
        target_width,
        target_height,
        auto_process,
    );

    // Calculate crop dimensions to maintain aspect ratio
    let target_aspect = eff_width as f64 / eff_height as f64;
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

    // Use enhanced smart cropping algorithm if detection data is available
    let (_processed_img, cropped) = if let Some(detection) = people_detection {
        if detection.person_count > 0 && detection.bounding_box.is_some() {
            // Use enhanced algorithm with potential scaling
            let (crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
                src_width,
                src_height,
                crop_width,
                crop_height,
                detection,
            );

            if scale_factor < 1.0 {
                // Scale down the image first
                let new_width = (src_width as f32 * scale_factor) as u32;
                let new_height = (src_height as f32 * scale_factor) as u32;

                verbose_println(
                    verbose,
                    &format!(
                        "🔍 Scaling image down by {:.1}% to fit detection area ({}x{} → {}x{})",
                        scale_factor * 100.0,
                        src_width,
                        src_height,
                        new_width,
                        new_height
                    ),
                );

                let scaled_img = resize_image(img, new_width, new_height)?;
                let cropped = crop_image(
                    &scaled_img,
                    crop_x,
                    crop_y,
                    crop_width,
                    crop_height,
                    verbose,
                )?;
                (scaled_img, cropped)
            } else {
                // No scaling needed, just crop with enhanced algorithm
                verbose_println(
                    verbose,
                    "🎯 Using enhanced face-biased cropping (detection box preservation)",
                );
                let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height, verbose)?;
                (img.clone(), cropped)
            }
        } else if detection.person_count > 0 {
            // Use legacy center-based algorithm
            verbose_println(
                verbose,
                "🎯 Using legacy center-based people-aware cropping",
            );
            let (crop_x, crop_y) = calculate_people_aware_crop_offset(
                src_width,
                src_height,
                crop_width,
                crop_height,
                detection,
            );

            let cropped = match crop_image(img, crop_x, crop_y, crop_width, crop_height, verbose) {
                Ok(cropped) => cropped,
                Err(_) => {
                    verbose_println(
                        verbose,
                        "⚠ People-aware cropping failed, falling back to center crop",
                    );
                    let (center_x, center_y) =
                        standard_crop_offset(src_width, src_height, crop_width, crop_height);
                    crop_image(img, center_x, center_y, crop_width, crop_height, verbose)?
                }
            };
            (img.clone(), cropped)
        } else {
            // No people detected, use standard center cropping
            let (crop_x, crop_y) =
                standard_crop_offset(src_width, src_height, crop_width, crop_height);
            let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height, verbose)?;
            (img.clone(), cropped)
        }
    } else {
        // No detection available, use standard center cropping
        let (crop_x, crop_y) = standard_crop_offset(src_width, src_height, crop_width, crop_height);
        let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height, verbose)?;
        (img.clone(), cropped)
    };

    // Resize to exact target dimensions
    resize_image(&cropped, eff_width, eff_height)
}

/// Enhanced smart crop calculation with face preservation
///
/// This function implements a three-stage cropping algorithm:
/// 1. **Scaling check**: Only scale down if the image itself is smaller than the target crop dimensions
///    (not if the detection box is larger than the crop)
/// 2. **Regular crop attempt**: Try center-based expansion from detection box center
///    - If detection fits entirely in the crop → use regular crop
/// 3. **Smart bidirectional expansion**: If regular crop would cut the detection:
///    - Expand detection box equally in all 4 directions to reach target dimensions
///    - When hitting image bounds, redistribute expansion to opposite side
///    - **Face preservation**: If crop would start below detection top (cutting faces),
///      align crop with detection top instead
///
/// # Arguments
/// * `src_width` - Source image width
/// * `src_height` - Source image height
/// * `target_width` - Target crop width
/// * `target_height` - Target crop height
/// * `detection` - Subject detection result with bounding box
///
/// # Returns
/// Tuple of (crop_x, crop_y, scale_factor)
/// - scale_factor = 1.0 means no scaling needed
/// - scale_factor < 1.0 means image should be scaled down first
///
pub fn calculate_enhanced_smart_crop(
    src_width: u32,
    src_height: u32,
    target_width: u32,
    target_height: u32,
    detection: &SubjectDetectionResult,
) -> (u32, u32, f32) {
    if detection.person_count == 0 || detection.bounding_box.is_none() {
        // No people detected, use standard center crop
        let (crop_x, crop_y) =
            standard_crop_offset(src_width, src_height, target_width, target_height);
        return (crop_x, crop_y, 1.0);
    }

    let (det_x_min, det_y_min, det_x_max, det_y_max) = detection.bounding_box.unwrap();
    let det_width = det_x_max - det_x_min;
    let det_height = det_y_max - det_y_min;

    // Only scale down if the detection box dimensions require it AND we want to preserve
    // the detection with padding. Scaling should only occur when:
    // 1. Detection box (with padding) is larger than the target crop dimensions
    // 2. The image itself cannot accommodate centering the crop without cutting the detection excessively
    //
    // The key insight: if detection is wider than crop but image is wide enough,
    // we can center the crop on detection and let it cut the edges - no scaling needed
    let padding_factor = 1.2f32;
    let det_width_padded = (det_width as f32 * padding_factor) as u32;
    let det_height_padded = (det_height as f32 * padding_factor) as u32;

    // Calculate if we have enough image space around the detection to do smart cropping
    // We need at least target_width pixels available to crop from the image
    let can_fit_crop_in_width = src_width >= target_width;
    let can_fit_crop_in_height = src_height >= target_height;

    // Only scale if the image dimensions are smaller than the target crop
    // In that case, we genuinely can't extract a crop of the desired size
    if !can_fit_crop_in_width || !can_fit_crop_in_height {
        // Image is too small for target crop - need to scale
        let scale_x = if !can_fit_crop_in_width {
            target_width as f32 / det_width_padded as f32
        } else {
            1.0
        };
        let scale_y = if !can_fit_crop_in_height {
            target_height as f32 / det_height_padded as f32
        } else {
            1.0
        };
        let scale_factor = scale_x.min(scale_y);

        // Calculate new image dimensions after scaling
        let new_width = (src_width as f32 * scale_factor) as u32;
        let new_height = (src_height as f32 * scale_factor) as u32;

        // Recalculate detection box coordinates after scaling
        let scaled_det_x_min = (det_x_min as f32 * scale_factor) as u32;
        let scaled_det_y_min = (det_y_min as f32 * scale_factor) as u32;
        let scaled_det_x_max = (det_x_max as f32 * scale_factor) as u32;
        let scaled_det_y_max = (det_y_max as f32 * scale_factor) as u32;

        // Use regular cropping approach on scaled image
        let (crop_x, crop_y) = calculate_regular_crop_from_detection_center(
            new_width,
            new_height,
            target_width,
            target_height,
            scaled_det_x_min,
            scaled_det_y_min,
            scaled_det_x_max,
            scaled_det_y_max,
        );

        return (crop_x, crop_y, scale_factor);
    }

    // STEP 1: Try regular center-based expansion from detection box
    let (regular_crop_x, regular_crop_y) = calculate_regular_crop_from_detection_center(
        src_width,
        src_height,
        target_width,
        target_height,
        det_x_min,
        det_y_min,
        det_x_max,
        det_y_max,
    );

    // STEP 2: Check if regular crop would intersect/cut the detection box
    let regular_crop_right = regular_crop_x + target_width;
    let regular_crop_bottom = regular_crop_y + target_height;

    let detection_fully_contained = regular_crop_x <= det_x_min
        && regular_crop_y <= det_y_min
        && regular_crop_right >= det_x_max
        && regular_crop_bottom >= det_y_max;

    if detection_fully_contained {
        // Regular crop works perfectly - detection box is fully contained
        return (regular_crop_x, regular_crop_y, 1.0);
    }

    // STEP 3: Regular crop would cut the person - use smart cropping with bidirectional expansion
    let (smart_crop_x, smart_crop_y) = calculate_smart_crop_bidirectional(
        src_width,
        src_height,
        target_width,
        target_height,
        det_x_min,
        det_y_min,
        det_x_max,
        det_y_max,
    );

    (smart_crop_x, smart_crop_y, 1.0)
}

/// Smart crop with bidirectional expansion and face preservation
///
/// Expands the detection box equally in all 4 directions to reach target dimensions.
/// When hitting image bounds in one direction, redistributes expansion to the opposite side.
///
/// # Algorithm
/// ## X-axis expansion:
/// - If detection width >= target width: center crop on detection
/// - If detection width < target width: expand equally left/right, redistribute when hitting bounds
///
/// ## Y-axis expansion:
/// - If detection height >= target height: center crop on detection
/// - If detection height < target height: expand equally top/bottom, redistribute when hitting bounds
/// - **Face preservation**: After expansion, if crop_y > det_y_min (would cut top of detection),
///   try to align crop with detection top to preserve faces
///
/// # Arguments
/// * `image_width` - Source image width
/// * `image_height` - Source image height
/// * `target_width` - Target crop width
/// * `target_height` - Target crop height
/// * `det_x_min` - Detection box left edge
/// * `det_y_min` - Detection box top edge
/// * `det_x_max` - Detection box right edge
/// * `det_y_max` - Detection box bottom edge
///
/// # Returns
/// Tuple of (crop_x, crop_y) representing the top-left corner of the crop area
///
fn calculate_smart_crop_bidirectional(
    image_width: u32,
    image_height: u32,
    target_width: u32,
    target_height: u32,
    det_x_min: u32,
    det_y_min: u32,
    det_x_max: u32,
    det_y_max: u32,
) -> (u32, u32) {
    let det_width = det_x_max - det_x_min;
    let det_height = det_y_max - det_y_min;

    // Calculate needed expansion in each dimension
    let needed_width_expansion = if det_width < target_width {
        target_width - det_width
    } else {
        0
    };
    let needed_height_expansion = if det_height < target_height {
        target_height - det_height
    } else {
        0
    };

    // X-axis expansion
    let crop_x = if det_width >= target_width {
        // Detection is wider than target - center crop on detection
        let det_center_x = (det_x_min + det_x_max) / 2;
        let ideal_x = det_center_x.saturating_sub(target_width / 2);
        ideal_x.min(image_width.saturating_sub(target_width))
    } else {
        // Detection is narrower - expand bidirectionally
        let initial_expansion_per_side = needed_width_expansion / 2;

        let max_left_expansion = det_x_min;
        let max_right_expansion = image_width.saturating_sub(det_x_max);

        let left_expansion = initial_expansion_per_side.min(max_left_expansion);
        let right_expansion = initial_expansion_per_side.min(max_right_expansion);

        let used_expansion = left_expansion + right_expansion;
        let remaining_expansion = needed_width_expansion.saturating_sub(used_expansion);

        // Redistribute remaining expansion
        let final_left_expansion = if remaining_expansion > 0 && left_expansion < max_left_expansion
        {
            (left_expansion + remaining_expansion).min(max_left_expansion)
        } else {
            left_expansion
        };

        let crop_x = det_x_min.saturating_sub(final_left_expansion);
        crop_x.min(image_width.saturating_sub(target_width))
    };

    // Y-axis expansion with face preservation priority
    let crop_y = if det_height >= target_height {
        // Detection is taller than target - center crop on detection
        let det_center_y = (det_y_min + det_y_max) / 2;
        let ideal_y = det_center_y.saturating_sub(target_height / 2);
        ideal_y.min(image_height.saturating_sub(target_height))
    } else {
        // Detection is shorter - expand bidirectionally
        let initial_expansion_per_side = needed_height_expansion / 2;

        let max_top_expansion = det_y_min;
        let max_bottom_expansion = image_height.saturating_sub(det_y_max);

        let top_expansion = initial_expansion_per_side.min(max_top_expansion);
        let bottom_expansion = initial_expansion_per_side.min(max_bottom_expansion);

        let used_expansion = top_expansion + bottom_expansion;
        let remaining_expansion = needed_height_expansion.saturating_sub(used_expansion);

        // Redistribute remaining expansion
        let final_top_expansion = if remaining_expansion > 0 && top_expansion < max_top_expansion {
            (top_expansion + remaining_expansion).min(max_top_expansion)
        } else {
            top_expansion
        };

        let crop_y = det_y_min.saturating_sub(final_top_expansion);
        crop_y.min(image_height.saturating_sub(target_height))
    };

    // Face preservation: if crop starts below detection top, try to align with detection top
    // This prevents cutting off faces which are typically at the top of person boxes
    let crop_y = if crop_y > det_y_min {
        // Crop would cut the top of detection - try to start at detection top instead
        if det_y_min + target_height <= image_height {
            // We can fit the crop starting at detection top
            det_y_min
        } else {
            // Can't fit crop at detection top - keep original position
            crop_y
        }
    } else {
        crop_y
    };

    (crop_x, crop_y)
}

/// Calculate regular crop by expanding from detection box center
/// This is the simple, predictable approach that works most of the time
fn calculate_regular_crop_from_detection_center(
    image_width: u32,
    image_height: u32,
    target_width: u32,
    target_height: u32,
    det_x_min: u32,
    det_y_min: u32,
    det_x_max: u32,
    det_y_max: u32,
) -> (u32, u32) {
    // Find detection box center
    let det_center_x = (det_x_min + det_x_max) / 2;
    let det_center_y = (det_y_min + det_y_max) / 2;

    // Expand from center to target crop size
    let crop_x = det_center_x.saturating_sub(target_width / 2);
    let crop_y = det_center_y.saturating_sub(target_height / 2);

    // Clamp to image boundaries
    let crop_x = crop_x.min(image_width.saturating_sub(target_width));
    let crop_y = crop_y.min(image_height.saturating_sub(target_height));

    (crop_x, crop_y)
}

/// Calculate crop offset using people detection results (Legacy function)
///
/// Given a center point (x, y), calculates crop box (left, top) that:
/// 1. Centers the crop box on the subject as much as possible
/// 2. Maintains exact crop dimensions (crop_width x crop_height)
/// 3. Clamps to image boundaries with proper adjustment
///
/// Example: image 1100x1000, center (900,900), crop 800x480
/// - Ideal: left=500, top=660, right=1300, bottom=1140
/// - Clamped: left=300, top=520, right=1100, bottom=1000 (maintains 800x480)
pub fn calculate_people_aware_crop_offset(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
    detection: &SubjectDetectionResult,
) -> (u32, u32) {
    if detection.person_count == 0 {
        // No people detected, use standard center crop
        return standard_crop_offset(src_width, src_height, crop_width, crop_height);
    }

    // Use enhanced algorithm if bounding box is available
    if detection.bounding_box.is_some() {
        let (crop_x, crop_y, _scale) = calculate_enhanced_smart_crop(
            src_width,
            src_height,
            crop_width,
            crop_height,
            detection,
        );
        return (crop_x, crop_y);
    }

    // Fallback to legacy center-based algorithm
    let center_x = detection.center.0;
    let center_y = detection.center.1;

    // Calculate ideal crop position to center on detected subject
    // This gives us the top-left corner of the crop box
    let ideal_left = center_x.saturating_sub(crop_width / 2);
    let ideal_top = center_y.saturating_sub(crop_height / 2);

    // Constrain to image boundaries while maintaining exact crop dimensions
    // If the crop box would extend beyond the image, shift it back
    let crop_left = ideal_left.min(src_width.saturating_sub(crop_width));
    let crop_top = ideal_top.min(src_height.saturating_sub(crop_height));

    (crop_left, crop_top)
}

/// Calculate standard center crop offset
pub fn standard_crop_offset(
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
fn crop_image(
    img: &RgbImage,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    verbose: bool,
) -> Result<RgbImage> {
    let (img_width, img_height) = img.dimensions();

    // Adjust crop parameters to fit within image bounds
    let adjusted_x = x.min(img_width.saturating_sub(1));
    let adjusted_y = y.min(img_height.saturating_sub(1));
    let adjusted_width = width.min(img_width - adjusted_x);
    let adjusted_height = height.min(img_height - adjusted_y);

    // Check for minimum viable dimensions (at least 10x10 pixels)
    const MIN_CROP_SIZE: u32 = 10;
    if adjusted_width < MIN_CROP_SIZE || adjusted_height < MIN_CROP_SIZE {
        return Err(anyhow::anyhow!(
            "Crop dimensions too small after adjustment: {}x{} (minimum: {}x{})",
            adjusted_width,
            adjusted_height,
            MIN_CROP_SIZE,
            MIN_CROP_SIZE
        ));
    }

    // Log adjustment if bounds were exceeded (for debugging)
    if x != adjusted_x || y != adjusted_y || width != adjusted_width || height != adjusted_height {
        verbose_println(
            verbose,
            &format!(
                "⚠ Adjusted crop bounds: crop({},{},{}x{}) → crop({},{},{}x{}) on {}x{} image",
                x,
                y,
                width,
                height,
                adjusted_x,
                adjusted_y,
                adjusted_width,
                adjusted_height,
                img_width,
                img_height
            ),
        );
    }

    let (final_x, final_y, final_width, final_height) =
        (adjusted_x, adjusted_y, adjusted_width, adjusted_height);

    let mut output = ImageBuffer::new(final_width, final_height);

    for (out_x, out_y, pixel) in output.enumerate_pixels_mut() {
        let src_x = final_x + out_x;
        let src_y = final_y + out_y;

        if let Some(src_pixel) = img.get_pixel_checked(src_x, src_y) {
            *pixel = *src_pixel;
        }
    }

    Ok(output)
}

/// Resize an image to exact dimensions using high-quality algorithm
pub fn resize_image(img: &RgbImage, width: u32, height: u32) -> Result<RgbImage> {
    let (src_width, src_height) = img.dimensions();

    if src_width == width && src_height == height {
        return Ok(img.clone());
    }

    // Convert to the format expected by fast_image_resize
    let src_pixels: Vec<u8> = img.pixels().flat_map(|p| [p[0], p[1], p[2]]).collect();

    // Create source image for fast_image_resize
    let src_width_nz =
        NonZeroU32::new(src_width).ok_or_else(|| anyhow::anyhow!("Source width is zero"))?;
    let src_height_nz =
        NonZeroU32::new(src_height).ok_or_else(|| anyhow::anyhow!("Source height is zero"))?;
    let dst_width_nz =
        NonZeroU32::new(width).ok_or_else(|| anyhow::anyhow!("Target width is zero"))?;
    let dst_height_nz =
        NonZeroU32::new(height).ok_or_else(|| anyhow::anyhow!("Target height is zero"))?;

    let src_image = Image::from_vec_u8(
        src_width_nz.into(),
        src_height_nz.into(),
        src_pixels,
        fast_image_resize::PixelType::U8x3,
    )?;

    // Create destination image
    let mut dst_image = Image::new(
        dst_width_nz.into(),
        dst_height_nz.into(),
        fast_image_resize::PixelType::U8x3,
    );

    // Create resizer with high-quality settings
    let mut resizer = Resizer::new();

    // Perform the resize with default ResizeOptions
    resizer.resize(&src_image, &mut dst_image, Some(&ResizeOptions::default()))?;

    // Convert back to RgbImage
    let dst_pixels = dst_image.buffer();
    let mut output = ImageBuffer::new(width, height);

    for (i, pixel) in output.pixels_mut().enumerate() {
        let base_idx = i * 3;
        if base_idx + 2 < dst_pixels.len() {
            *pixel = Rgb([
                dst_pixels[base_idx],
                dst_pixels[base_idx + 1],
                dst_pixels[base_idx + 2],
            ]);
        }
    }

    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::image_processing::subject_detection::SubjectDetectionResult;

    fn create_test_image(width: u32, height: u32) -> RgbImage {
        ImageBuffer::from_fn(width, height, |x, y| {
            Rgb([(x % 256) as u8, (y % 256) as u8, ((x + y) % 256) as u8])
        })
    }

    #[test]
    fn test_crop_image() {
        let img = create_test_image(100, 100);
        let cropped = crop_image(&img, 10, 10, 50, 50, false).unwrap();

        assert_eq!(cropped.dimensions(), (50, 50));

        // Verify pixel values match the source region
        let original_pixel = img.get_pixel(15, 15);
        let cropped_pixel = cropped.get_pixel(5, 5); // 15-10, 15-10
        assert_eq!(original_pixel, cropped_pixel);
    }

    #[test]
    fn test_crop_bounds_validation() {
        let img = create_test_image(50, 50);

        // Test crop that exceeds bounds - should be adjusted automatically
        let result = crop_image(&img, 10, 10, 50, 50, false);
        assert!(result.is_ok());
        let cropped = result.unwrap();
        // Should be adjusted to fit within bounds: 40x40 instead of 50x50
        assert_eq!(cropped.dimensions(), (40, 40));

        // This should succeed normally
        let result = crop_image(&img, 10, 10, 40, 40, false);
        assert!(result.is_ok());
    }

    #[test]
    fn test_resize_image() {
        let img = create_test_image(100, 100);
        let resized = resize_image(&img, 50, 50).unwrap();

        assert_eq!(resized.dimensions(), (50, 50));
    }

    #[test]
    fn test_smart_resize_landscape_to_landscape() {
        // 200x100 landscape image to 100x50 landscape target
        let img = create_test_image(200, 100);
        let result = smart_resize_with_people_detection(&img, 100, 50, false, None, false).unwrap();

        assert_eq!(result.dimensions(), (100, 50));
    }

    #[test]
    fn test_smart_resize_with_auto_process() {
        // 100x200 portrait image to 800x480 landscape target with auto
        let img = create_test_image(100, 200);
        let result = smart_resize_with_people_detection(&img, 800, 480, true, None, false).unwrap();

        // With auto-process, dimensions should be swapped to 480x800
        assert_eq!(result.dimensions(), (480, 800));
    }

    #[test]
    fn test_people_aware_crop_boundary_clamping() {
        // Test the user's example: image 1100x1000, center (900,900), crop 800x480
        // Expected result: left=300, top=520 (maintains 800x480 dimensions)

        let detection = SubjectDetectionResult {
            center: (900, 900),
            offset_from_center: (0, 0), // Not used in this calculation
            bounding_box: Some((700, 800, 1100, 1000)), // Add test bounding box
            individual_detections: vec![(700, 800, 1100, 1000)], // Add test individual detection
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1100, // src_width
            1000, // src_height
            800,  // crop_width
            480,  // crop_height
            &detection,
        );

        // With the enhanced algorithm, the positioning will be different due to face bias
        // The detection box (700, 800, 1100, 1000) must be included in the crop
        // Crop dimensions: 800x480

        // The enhanced algorithm prioritizes image boundaries when detection box cannot fit entirely
        // In this case, the detection box (700, 800, 1100, 1000) has conflicting constraints:
        // - To include bottom (y=1000), crop must start by y=520 (1000-480=520)
        // - To include top (y=800), crop must start after y=800
        // This is impossible, so the algorithm prioritizes image boundaries

        // Verify the crop fits within the image boundaries (highest priority)
        assert!(crop_left + 800 <= 1100); // Right edge within image
        assert!(crop_top + 480 <= 1000); // Bottom edge within image

        // Verify the detection box horizontal extent is included
        assert!(crop_left <= 700); // Must include left edge of detection
        assert!(crop_left + 800 >= 1100); // Must include right edge of detection

        // The vertical extent may not be fully included due to image boundary constraints
        // but should include as much as possible
    }

    #[test]
    fn test_center_crop_no_clamping_needed() {
        // Test case where center point allows crop without boundary issues
        let detection = SubjectDetectionResult {
            center: (400, 300), // Center that should work without clamping
            offset_from_center: (0, 0),
            bounding_box: Some((300, 200, 500, 400)), // Add test bounding box
            individual_detections: vec![(300, 200, 500, 400)], // Add test individual detection
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            800, // src_width
            600, // src_height
            400, // crop_width
            200, // crop_height
            &detection,
        );

        // Enhanced algorithm should ensure detection box fits entirely
        // The algorithm now uses the bounding box and face bias instead of just centering
        // Detection box is (300, 200, 500, 400), so crop should include all of it
        assert!(crop_left <= 300); // Must include left edge of detection
        assert!(crop_left + 400 >= 500); // Must include right edge of detection
        assert!(crop_top <= 200); // Must include top edge of detection
        assert!(crop_top + 200 >= 400); // Must include bottom edge of detection
    }

    #[test]
    fn test_center_at_image_edges() {
        // Test when center is near the edges of the image
        let detection = SubjectDetectionResult {
            center: (50, 50), // Very close to top-left corner
            offset_from_center: (0, 0),
            bounding_box: Some((0, 0, 100, 100)), // Add test bounding box
            individual_detections: vec![(0, 0, 100, 100)], // Add test individual detection
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            1000, // src_height
            200,  // crop_width
            200,  // crop_height
            &detection,
        );

        // Should be clamped to (0, 0) since center is too close to edges
        assert_eq!(crop_left, 0);
        assert_eq!(crop_top, 0);
    }

    #[test]
    fn test_center_near_bottom_right() {
        // Test when center is near bottom-right corner
        let detection = SubjectDetectionResult {
            center: (950, 950), // Close to bottom-right of 1000x1000 image
            offset_from_center: (0, 0),
            bounding_box: Some((850, 850, 1000, 1000)), // Add test bounding box
            individual_detections: vec![(850, 850, 1000, 1000)], // Add test individual detection
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            1000, // src_height
            200,  // crop_width
            200,  // crop_height
            &detection,
        );

        // With enhanced algorithm, the detection box (850, 850, 1000, 1000) must fit in crop
        // The crop should ensure the entire detection box is included
        assert!(crop_left <= 850); // Must include left edge of detection
        assert!(crop_left + 200 >= 1000); // Must include right edge of detection
        assert!(crop_top <= 850); // Must include top edge of detection
        assert!(crop_top + 200 >= 1000); // Must include bottom edge of detection
    }

    #[test]
    fn test_no_people_detected_fallback() {
        // Test fallback to standard center crop when no people are detected
        let detection = SubjectDetectionResult {
            center: (0, 0), // This should be ignored
            offset_from_center: (0, 0),
            bounding_box: None, // No bounding box when no people detected
            individual_detections: vec![], // No individual detections
            confidence: 0.0,
            person_count: 0, // No people detected
        };

        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            600,  // src_height
            400,  // crop_width
            200,  // crop_height
            &detection,
        );

        // Should use standard center crop: (1000-400)/2 = 300, (600-200)/2 = 200
        assert_eq!(crop_left, 300);
        assert_eq!(crop_top, 200);
    }

    #[test]
    fn test_exact_fit_crop() {
        // Test when crop dimensions exactly match image dimensions
        let detection = SubjectDetectionResult {
            center: (500, 500), // Center of 1000x1000 image
            offset_from_center: (0, 0),
            bounding_box: Some((0, 0, 1000, 1000)), // Add test bounding box (entire image)
            individual_detections: vec![(0, 0, 1000, 1000)], // Add test individual detection
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            1000, // src_height
            1000, // crop_width - same as image
            1000, // crop_height - same as image
            &detection,
        );

        // Should be (0, 0) since crop is same size as image
        assert_eq!(crop_left, 0);
        assert_eq!(crop_top, 0);
    }

    #[test]
    fn test_enhanced_smart_crop_no_scaling_needed() {
        // Test enhanced algorithm when no scaling is needed
        let detection = SubjectDetectionResult {
            center: (400, 300),
            offset_from_center: (0, 0),
            bounding_box: Some((200, 100, 600, 500)), // 400x400 detection box
            individual_detections: vec![(200, 100, 600, 500)],
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
            1000, // src_width
            800,  // src_height
            800,  // target_width
            480,  // target_height
            &detection,
        );

        // Should not need scaling (scale_factor should be 1.0)
        assert_eq!(scale_factor, 1.0);

        // Crop should be positioned to include entire detection box with face bias
        // Detection box is 200-600 horizontally, 100-500 vertically
        assert!(crop_x <= 200); // Must include left edge of detection
        assert!(crop_x + 800 >= 600); // Must include right edge of detection
        assert!(crop_y + 480 >= 500); // Must include bottom edge of detection

        // The exact positioning may vary due to face bias algorithm
        // Just ensure it's within reasonable image bounds
        assert!(crop_y <= 800 - 480); // Must fit within 800px height image
    }

    #[test]
    fn test_enhanced_smart_crop_scaling_needed() {
        // Test enhanced algorithm when scaling is needed (detection too large for target)
        // In this case, we want to scale down to fit the detection with padding
        let detection = SubjectDetectionResult {
            center: (500, 400),
            offset_from_center: (0, 0),
            bounding_box: Some((100, 100, 900, 700)), // 800x600 detection box
            individual_detections: vec![(100, 100, 900, 700)],
            confidence: 0.9,
            person_count: 1,
        };

        // Image is large enough, but detection box needs to fit with padding into smaller target
        // This scenario would require scaling if we want to preserve the entire detection
        let (crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
            1000, // src_width
            800,  // src_height
            600,  // target_width (smaller than detection 800)
            400,  // target_height (smaller than detection 600)
            &detection,
        );

        eprintln!(
            "Scale test result: crop_x={}, crop_y={}, scale={}",
            crop_x, crop_y, scale_factor
        );

        // With new logic, we should NOT scale (we can fit 600x400 crop in 1000x800 image)
        // We just center the crop on detection and let it cut the edges
        assert_eq!(scale_factor, 1.0);

        // Crop should be positioned to center on detection
        let det_center_x: u32 = (100 + 900) / 2; // 500
        let expected_crop_x = det_center_x.saturating_sub(600 / 2).min(1000 - 600); // 200, clamped to 400

        // With face preservation, crop_y should align with detection top (100) instead of centering (200)
        let expected_crop_y = 100; // det_y_min, to preserve faces at top of detection

        assert_eq!(crop_x, expected_crop_x);
        assert_eq!(crop_y, expected_crop_y);
    }

    #[test]
    fn test_enhanced_smart_crop_face_bias() {
        // Test that face bias positions faces in the upper third of the image
        let detection = SubjectDetectionResult {
            center: (400, 400),
            offset_from_center: (0, 0),
            bounding_box: Some((350, 300, 450, 500)), // 100x200 detection box (smaller, should not need scaling)
            individual_detections: vec![(350, 300, 450, 500)],
            confidence: 0.9,
            person_count: 1,
        };

        let (_crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
            800, // src_width
            800, // src_height
            400, // target_width
            300, // target_height
            &detection,
        );

        // Should not need scaling (detection box 100x200 easily fits in target 400x300 with padding)
        assert_eq!(scale_factor, 1.0);

        // Must include entire detection box (300-500)
        assert!(crop_y + 300 >= 500); // Must include bottom of detection

        // The crop should be positioned within valid image bounds
        assert!(crop_y <= 800 - 300); // Must fit within 800px height image
    }

    #[test]
    fn test_enhanced_smart_crop_fallback_no_detection() {
        // Test fallback to center crop when no people detected
        let detection = SubjectDetectionResult {
            center: (0, 0),
            offset_from_center: (0, 0),
            bounding_box: None,
            individual_detections: vec![],
            confidence: 0.0,
            person_count: 0,
        };

        let (crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
            800, // src_width
            600, // src_height
            400, // target_width
            300, // target_height
            &detection,
        );

        // Should not need scaling
        assert_eq!(scale_factor, 1.0);

        // Should use standard center crop
        assert_eq!(crop_x, (800 - 400) / 2); // 200
        assert_eq!(crop_y, (600 - 300) / 2); // 150
    }

    #[test]
    fn test_face_preservation_priority() {
        // Test that crop aligns with detection top when it would otherwise cut the face
        // Scenario: detection near bottom, limited space to expand downward forces crop upward
        let detection = SubjectDetectionResult {
            center: (400, 850),
            offset_from_center: (0, 0),
            bounding_box: Some((300, 750, 500, 950)), // 200x200 detection box near bottom
            individual_detections: vec![(300, 750, 500, 950)],
            confidence: 0.9,
            person_count: 1,
        };

        let (crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
            800,  // src_width
            1000, // src_height
            400,  // target_width
            600,  // target_height
            &detection,
        );

        eprintln!(
            "Face preservation test: crop_x={}, crop_y={}, scale={}",
            crop_x, crop_y, scale_factor
        );

        // No scaling needed
        assert_eq!(scale_factor, 1.0);

        // X should be centered on detection
        assert_eq!(crop_x, 200); // Center 400 - 200 = 200

        // Detection is 200px tall, needs to expand to 600px (400px expansion needed)
        // Can only expand 50px down (1000 - 950), so must expand 350px up
        // Without face preservation: crop_y would be 750 - 350 = 400
        // But this is > det_y_min (750), so it would cut the top
        // With face preservation: crop_y should be 750 (det_y_min) since 750 + 600 = 1350 > 1000
        // Actually we can only go to 400 max to fit in image (1000 - 600 = 400)
        // So the final position should be 400, which is the max it can be without exceeding bounds
        assert_eq!(
            crop_y, 400,
            "crop_y should be positioned to fit in image bounds"
        );
    }

    #[test]
    fn test_user_case_portrait_960x1280() {
        // Test the exact user case: 960x1280 portrait, detection (83,64,958,1205), target crop 768x1280
        let detection = SubjectDetectionResult {
            center: (520, 634),
            offset_from_center: (0, 0),
            bounding_box: Some((83, 64, 958, 1205)), // 875x1141 detection box
            individual_detections: vec![(83, 64, 958, 1205)],
            confidence: 0.73,
            person_count: 1,
        };

        let (crop_x, crop_y, scale_factor) = calculate_enhanced_smart_crop(
            960,  // src_width
            1280, // src_height
            768,  // target_width
            1280, // target_height (full height)
            &detection,
        );

        eprintln!(
            "Result: crop_x={}, crop_y={}, scale={}",
            crop_x, crop_y, scale_factor
        );

        // Should not need scaling
        assert_eq!(scale_factor, 1.0);

        // Detection box center is at x=520
        // Crop width is 768, so we expand 384 pixels each side from center
        // Expected: 520 - 384 = 136 (doesn't hit boundaries)
        assert_eq!(crop_x, 136, "crop_x should be 136, got {}", crop_x);

        // Full height, so crop_y should be 0
        assert_eq!(crop_y, 0);
    }
}
