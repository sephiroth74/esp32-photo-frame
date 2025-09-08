use anyhow::Result;
use fast_image_resize::{images::Image, Resizer, ResizeOptions};
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

    // Calculate crop offset - use people detection if available
    let (crop_x, crop_y) = if let Some(detection) = people_detection {
        if detection.person_count > 0 {
            // Use people-aware cropping
            calculate_people_aware_crop_offset(
                src_width, src_height,
                crop_width, crop_height,
                detection
            )
        } else {
            // No people detected, use standard center cropping
            standard_crop_offset(src_width, src_height, crop_width, crop_height)
        }
    } else {
        // No detection available, use standard center cropping
        standard_crop_offset(src_width, src_height, crop_width, crop_height)
    };

    // Try to crop the image, fall back to center crop if people-aware cropping fails
    let cropped = match crop_image(img, crop_x, crop_y, crop_width, crop_height, verbose) {
        Ok(cropped) => cropped,
        Err(_) => {
            // People-aware cropping resulted in invalid dimensions, fall back to center crop
            verbose_println(verbose, "⚠ People-aware cropping failed, falling back to center crop");
            let (center_x, center_y) = standard_crop_offset(src_width, src_height, crop_width, crop_height);
            crop_image(img, center_x, center_y, crop_width, crop_height, verbose)?
        }
    };

    // Resize to exact target dimensions
    resize_image(&cropped, eff_width, eff_height)
}

/// Calculate crop offset using people detection results
/// 
/// Given a center point (x, y), calculates crop box (left, top) that:
/// 1. Centers the crop box on the subject as much as possible
/// 2. Maintains exact crop dimensions (crop_width x crop_height) 
/// 3. Clamps to image boundaries with proper adjustment
///
/// Example: image 1100x1000, center (900,900), crop 800x480
/// - Ideal: left=500, top=660, right=1300, bottom=1140
/// - Clamped: left=300, top=520, right=1100, bottom=1000 (maintains 800x480)
fn calculate_people_aware_crop_offset(
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
fn standard_crop_offset(src_width: u32, src_height: u32, crop_width: u32, crop_height: u32) -> (u32, u32) {
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
            adjusted_width, adjusted_height, MIN_CROP_SIZE, MIN_CROP_SIZE
        ));
    }
    
    // Log adjustment if bounds were exceeded (for debugging) 
    if x != adjusted_x || y != adjusted_y || width != adjusted_width || height != adjusted_height {
        verbose_println(verbose, &format!("⚠ Adjusted crop bounds: crop({},{},{}x{}) → crop({},{},{}x{}) on {}x{} image",
            x, y, width, height, adjusted_x, adjusted_y, adjusted_width, adjusted_height, img_width, img_height));
    }
    
    let (final_x, final_y, final_width, final_height) = (adjusted_x, adjusted_y, adjusted_width, adjusted_height);

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
    let src_pixels: Vec<u8> = img.pixels()
        .flat_map(|p| [p[0], p[1], p[2]])
        .collect();

    // Create source image for fast_image_resize
    let src_width_nz = NonZeroU32::new(src_width).ok_or_else(|| anyhow::anyhow!("Source width is zero"))?;
    let src_height_nz = NonZeroU32::new(src_height).ok_or_else(|| anyhow::anyhow!("Source height is zero"))?;
    let dst_width_nz = NonZeroU32::new(width).ok_or_else(|| anyhow::anyhow!("Target width is zero"))?;
    let dst_height_nz = NonZeroU32::new(height).ok_or_else(|| anyhow::anyhow!("Target height is zero"))?;

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
            Rgb([
                (x % 256) as u8,
                (y % 256) as u8,
                ((x + y) % 256) as u8,
            ])
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
            confidence: 0.9,
            person_count: 1,
        };
        
        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1100, // src_width
            1000, // src_height  
            800,  // crop_width
            480,  // crop_height
            &detection
        );
        
        // Verify the result matches expected values
        assert_eq!(crop_left, 300, "Left should be 300 to maintain 800px width within 1100px image");
        assert_eq!(crop_top, 520, "Top should be 520 to maintain 480px height within 1000px image");
        
        // Verify the crop box dimensions are exactly as requested
        assert_eq!(crop_left + 800, 1100, "Right edge should be at image boundary");
        assert_eq!(crop_top + 480, 1000, "Bottom edge should be at image boundary");
    }

    #[test]
    fn test_center_crop_no_clamping_needed() {
        // Test case where center point allows crop without boundary issues
        let detection = SubjectDetectionResult {
            center: (400, 300), // Center that should work without clamping
            offset_from_center: (0, 0),
            confidence: 0.9,
            person_count: 1,
        };
        
        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            800,  // src_width
            600,  // src_height
            400,  // crop_width  
            200,  // crop_height
            &detection
        );
        
        // Should center perfectly: left = 400-200=200, top = 300-100=200
        assert_eq!(crop_left, 200);
        assert_eq!(crop_top, 200);
    }

    #[test]
    fn test_center_at_image_edges() {
        // Test when center is near the edges of the image
        let detection = SubjectDetectionResult {
            center: (50, 50), // Very close to top-left corner
            offset_from_center: (0, 0),
            confidence: 0.9,
            person_count: 1,
        };
        
        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            1000, // src_height
            200,  // crop_width
            200,  // crop_height
            &detection
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
            confidence: 0.9,
            person_count: 1,
        };
        
        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            1000, // src_height
            200,  // crop_width
            200,  // crop_height
            &detection
        );
        
        // Should be clamped to maintain 200x200 crop within 1000x1000 image
        assert_eq!(crop_left, 800); // 1000 - 200 = 800
        assert_eq!(crop_top, 800);  // 1000 - 200 = 800
    }

    #[test]
    fn test_no_people_detected_fallback() {
        // Test fallback to standard center crop when no people are detected
        let detection = SubjectDetectionResult {
            center: (0, 0), // This should be ignored
            offset_from_center: (0, 0),
            confidence: 0.0,
            person_count: 0, // No people detected
        };
        
        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            600,  // src_height
            400,  // crop_width
            200,  // crop_height
            &detection
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
            confidence: 0.9,
            person_count: 1,
        };
        
        let (crop_left, crop_top) = calculate_people_aware_crop_offset(
            1000, // src_width
            1000, // src_height
            1000, // crop_width - same as image
            1000, // crop_height - same as image
            &detection
        );
        
        // Should be (0, 0) since crop is same size as image
        assert_eq!(crop_left, 0);
        assert_eq!(crop_top, 0);
    }
}