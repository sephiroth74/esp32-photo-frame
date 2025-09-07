use anyhow::Result;
use fast_image_resize::{images::Image, Resizer, ResizeOptions};
use image::{ImageBuffer, Rgb, RgbImage};
use std::num::NonZeroU32;

use super::orientation::get_effective_target_dimensions;
use super::subject_detection::SubjectDetectionResult;

/// Smart resize function that handles aspect ratio and cropping
/// 
/// This function implements crop-to-fill behavior similar to ImageMagick's geometry operators
/// used in the bash scripts. It will crop the image to fit the target aspect ratio perfectly.
pub fn smart_resize(
    img: &RgbImage,
    target_width: u32,
    target_height: u32,
    auto_process: bool,
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

    // Calculate crop offset to center the crop
    let crop_x = (src_width.saturating_sub(crop_width)) / 2;
    let crop_y = (src_height.saturating_sub(crop_height)) / 2;

    // Crop the image
    let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height)?;

    // Resize to exact target dimensions
    resize_image(&cropped, eff_width, eff_height)
}

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

    // Crop the image
    let cropped = crop_image(img, crop_x, crop_y, crop_width, crop_height)?;

    // Resize to exact target dimensions
    resize_image(&cropped, eff_width, eff_height)
}

/// Calculate crop offset using people detection results
fn calculate_people_aware_crop_offset(
    src_width: u32,
    src_height: u32,
    crop_width: u32,
    crop_height: u32,
    detection: &SubjectDetectionResult,
) -> (u32, u32) {
    let people_center_x = detection.center.0 as i32;
    let people_center_y = detection.center.1 as i32;
    
    // Calculate ideal crop position to center on detected people
    let ideal_crop_x = people_center_x - (crop_width as i32 / 2);
    let ideal_crop_y = people_center_y - (crop_height as i32 / 2);
    
    // Constrain to image boundaries
    let crop_x = ideal_crop_x
        .max(0)
        .min(src_width as i32 - crop_width as i32)
        as u32;
    let crop_y = ideal_crop_y
        .max(0)
        .min(src_height as i32 - crop_height as i32)
        as u32;
    
    // If people are detected in bounding box, apply some bias towards keeping them in frame
    if let Some((bbox_x, bbox_y, bbox_w, bbox_h)) = detection.bounding_box {
        // Calculate overlap between crop area and people bounding box
        let crop_right = crop_x + crop_width;
        let crop_bottom = crop_y + crop_height;
        let bbox_right = bbox_x + bbox_w;
        let bbox_bottom = bbox_y + bbox_h;
        
        // Check if bounding box is well within crop area
        let overlap_x = crop_x.max(bbox_x).min(crop_right.min(bbox_right));
        let overlap_y = crop_y.max(bbox_y).min(crop_bottom.min(bbox_bottom));
        let overlap_right = crop_right.min(bbox_right).max(crop_x.max(bbox_x));
        let overlap_bottom = crop_bottom.min(bbox_bottom).max(crop_y.max(bbox_y));
        
        if overlap_right > overlap_x && overlap_bottom > overlap_y {
            let overlap_area = (overlap_right - overlap_x) * (overlap_bottom - overlap_y);
            let bbox_area = bbox_w * bbox_h;
            let overlap_ratio = overlap_area as f32 / bbox_area as f32;
            
            // If less than 80% of people are in frame, try to adjust
            if overlap_ratio < 0.8 {
                // Prefer to keep people fully in frame if possible
                let adjusted_x = if bbox_x + bbox_w > crop_right {
                    // People extend beyond right edge, shift left
                    (bbox_x + bbox_w).saturating_sub(crop_width).max(0)
                } else if bbox_x < crop_x {
                    // People extend beyond left edge, shift right
                    bbox_x.min(src_width.saturating_sub(crop_width))
                } else {
                    crop_x
                };
                
                let adjusted_y = if bbox_y + bbox_h > crop_bottom {
                    // People extend beyond bottom edge, shift up
                    (bbox_y + bbox_h).saturating_sub(crop_height).max(0)
                } else if bbox_y < crop_y {
                    // People extend beyond top edge, shift down
                    bbox_y.min(src_height.saturating_sub(crop_height))
                } else {
                    crop_y
                };
                
                return (adjusted_x, adjusted_y);
            }
        }
    }
    
    (crop_x, crop_y)
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
) -> Result<RgbImage> {
    let (img_width, img_height) = img.dimensions();
    
    // Validate crop parameters
    if x + width > img_width || y + height > img_height {
        return Err(anyhow::anyhow!(
            "Crop dimensions exceed image bounds: crop({},{},{}x{}) on {}x{} image",
            x, y, width, height, img_width, img_height
        ));
    }

    let mut output = ImageBuffer::new(width, height);
    
    for (out_x, out_y, pixel) in output.enumerate_pixels_mut() {
        let src_x = x + out_x;
        let src_y = y + out_y;
        
        if let Some(src_pixel) = img.get_pixel_checked(src_x, src_y) {
            *pixel = *src_pixel;
        }
    }

    Ok(output)
}

/// Resize an image to exact dimensions using high-quality algorithm
fn resize_image(img: &RgbImage, width: u32, height: u32) -> Result<RgbImage> {
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
        let cropped = crop_image(&img, 10, 10, 50, 50).unwrap();
        
        assert_eq!(cropped.dimensions(), (50, 50));
        
        // Verify pixel values match the source region
        let original_pixel = img.get_pixel(15, 15);
        let cropped_pixel = cropped.get_pixel(5, 5); // 15-10, 15-10
        assert_eq!(original_pixel, cropped_pixel);
    }

    #[test]
    fn test_crop_bounds_validation() {
        let img = create_test_image(50, 50);
        
        // This should fail - crop exceeds bounds
        let result = crop_image(&img, 10, 10, 50, 50);
        assert!(result.is_err());
        
        // This should succeed
        let result = crop_image(&img, 10, 10, 40, 40);
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
        let result = smart_resize(&img, 100, 50, false).unwrap();
        
        assert_eq!(result.dimensions(), (100, 50));
    }

    #[test]
    fn test_smart_resize_with_auto_process() {
        // 100x200 portrait image to 800x480 landscape target with auto
        let img = create_test_image(100, 200);
        let result = smart_resize(&img, 800, 480, true).unwrap();
        
        // With auto-process, dimensions should be swapped to 480x800
        assert_eq!(result.dimensions(), (480, 800));
    }
}