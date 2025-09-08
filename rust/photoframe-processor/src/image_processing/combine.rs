use anyhow::Result;
use image::{ImageBuffer, Rgb, RgbImage};

use super::resize::resize_image;

/// Combine two already-processed portrait images side by side
/// 
/// This function assumes both images are already at half-width size (e.g., 400x480)
/// and just places them side by side without any resizing
pub fn combine_processed_portraits(
    left_img: &RgbImage,
    right_img: &RgbImage,
    target_width: u32,
    target_height: u32,
) -> Result<RgbImage> {
    let mut combined = ImageBuffer::new(target_width, target_height);
    let half_width = target_width / 2;
    
    // Left image should already be half_width x target_height
    let left_width = left_img.width().min(half_width);
    let left_height = left_img.height().min(target_height);
    
    // Right image should already be half_width x target_height  
    let right_width = right_img.width().min(half_width);
    let right_height = right_img.height().min(target_height);
    
    // Copy left image to left half
    for y in 0..left_height {
        for x in 0..left_width {
            if let Some(pixel) = left_img.get_pixel_checked(x, y) {
                combined.put_pixel(x, y, *pixel);
            }
        }
    }
    
    // Copy right image to right half
    for y in 0..right_height {
        for x in 0..right_width {
            let dst_x = half_width + x;
            if dst_x < target_width {
                if let Some(pixel) = right_img.get_pixel_checked(x, y) {
                    combined.put_pixel(dst_x, y, *pixel);
                }
            }
        }
    }
    
    // Draw white 3px vertical divider line between the two images
    let divider_width = 3;
    let divider_start = half_width.saturating_sub(divider_width / 2);
    let divider_end = (half_width + divider_width / 2).min(target_width);
    
    for y in 0..target_height {
        for x in divider_start..divider_end {
            if x < target_width {
                combined.put_pixel(x, y, Rgb([255, 255, 255])); // White divider
            }
        }
    }
    
    Ok(combined)
}

/// Combine two portrait images side by side into a landscape image
/// 
/// This creates a single landscape image by placing two portrait images
/// side by side with an optional divider line between them.
#[allow(dead_code)]
pub fn combine_portraits(
    left_img: &RgbImage,
    right_img: &RgbImage,
    target_width: u32,
    target_height: u32,
    divider_color: Option<(u8, u8, u8)>,
    divider_width: u32,
) -> Result<RgbImage> {
    let mut combined = ImageBuffer::new(target_width, target_height);
    
    let half_width = target_width / 2;
    
    // Resize both images to fit within half width while preserving aspect ratio
    let left_resized = resize_to_fit(left_img, half_width, target_height)?;
    let right_resized = resize_to_fit(right_img, half_width, target_height)?;
    
    // Center and copy left image to left half
    let left_offset_x = (half_width.saturating_sub(left_resized.width())) / 2;
    let left_offset_y = (target_height.saturating_sub(left_resized.height())) / 2;
    
    for y in 0..left_resized.height() {
        for x in 0..left_resized.width() {
            let dst_x = left_offset_x + x;
            let dst_y = left_offset_y + y;
            if dst_x < half_width && dst_y < target_height {
                if let Some(pixel) = left_resized.get_pixel_checked(x, y) {
                    combined.put_pixel(dst_x, dst_y, *pixel);
                }
            }
        }
    }
    
    // Center and copy right image to right half
    let right_offset_x = half_width + (half_width.saturating_sub(right_resized.width())) / 2;
    let right_offset_y = (target_height.saturating_sub(right_resized.height())) / 2;
    
    for y in 0..right_resized.height() {
        for x in 0..right_resized.width() {
            let dst_x = right_offset_x + x;
            let dst_y = right_offset_y + y;
            if dst_x < target_width && dst_y < target_height {
                if let Some(pixel) = right_resized.get_pixel_checked(x, y) {
                    combined.put_pixel(dst_x, dst_y, *pixel);
                }
            }
        }
    }
    
    // Draw divider line if specified
    if let Some((r, g, b)) = divider_color {
        let line_start = half_width.saturating_sub(divider_width / 2);
        let line_end = (half_width + divider_width / 2).min(target_width);
        
        for y in 0..target_height {
            for x in line_start..line_end {
                combined.put_pixel(x, y, Rgb([r, g, b]));
            }
        }
    }
    
    Ok(combined)
}

/// Resize image to fit within given dimensions while preserving aspect ratio
fn resize_to_fit(img: &RgbImage, max_width: u32, max_height: u32) -> Result<RgbImage> {
    let (current_width, current_height) = img.dimensions();
    
    // Calculate scaling factor to fit within bounds while preserving aspect ratio
    let width_scale = max_width as f64 / current_width as f64;
    let height_scale = max_height as f64 / current_height as f64;
    let scale = width_scale.min(height_scale);
    
    // Calculate new dimensions
    let new_width = (current_width as f64 * scale) as u32;
    let new_height = (current_height as f64 * scale) as u32;
    
    // Ensure minimum size of 1x1
    let final_width = new_width.max(1);
    let final_height = new_height.max(1);
    
    // Use the same high-quality resizing function from resize module
    resize_image(img, final_width, final_height)
}

/// Resize image to exact dimensions using high-quality algorithm (legacy)
#[allow(dead_code)]
fn resize_to_exact(img: &RgbImage, width: u32, height: u32) -> Result<RgbImage> {
    // Use the same high-quality resizing function from resize module
    resize_image(img, width, height)
}

/// Pair portrait images for combination
/// 
/// This function groups portrait images into pairs, handling odd numbers
/// by duplicating the last image if necessary
#[allow(dead_code)]
pub fn pair_portrait_images(portraits: Vec<RgbImage>) -> Vec<(RgbImage, RgbImage)> {
    let mut pairs = Vec::new();
    let mut iter = portraits.into_iter();
    
    while let Some(first) = iter.next() {
        let second = iter.next().unwrap_or_else(|| first.clone());
        pairs.push((first, second));
    }
    
    pairs
}

#[cfg(test)]
mod tests {
    use super::*;

    fn create_test_image(width: u32, height: u32, color: [u8; 3]) -> RgbImage {
        ImageBuffer::from_pixel(width, height, Rgb(color))
    }

    #[test]
    fn test_combine_portraits() {
        let left = create_test_image(100, 200, [255, 0, 0]); // Red
        let right = create_test_image(100, 200, [0, 255, 0]); // Green
        
        let combined = combine_portraits(&left, &right, 200, 200, None, 0).unwrap();
        
        assert_eq!(combined.dimensions(), (200, 200));
        
        // Check left half is red-ish and right half is green-ish
        let left_pixel = combined.get_pixel(50, 100);
        let right_pixel = combined.get_pixel(150, 100);
        
        assert!(left_pixel[0] > left_pixel[1]); // More red than green
        assert!(right_pixel[1] > right_pixel[0]); // More green than red
    }

    #[test]
    fn test_combine_with_divider() {
        let left = create_test_image(50, 100, [255, 0, 0]);
        let right = create_test_image(50, 100, [0, 255, 0]);
        
        let combined = combine_portraits(
            &left, &right, 100, 100, 
            Some((255, 255, 255)), // White divider
            2 // 2 pixels wide
        ).unwrap();
        
        // Check that there's a white divider in the middle
        let divider_pixel = combined.get_pixel(50, 50);
        assert_eq!(divider_pixel, &Rgb([255, 255, 255]));
    }

    #[test]
    fn test_pair_portrait_images() {
        let images = vec![
            create_test_image(10, 10, [255, 0, 0]),
            create_test_image(10, 10, [0, 255, 0]),
            create_test_image(10, 10, [0, 0, 255]),
        ];
        
        let pairs = pair_portrait_images(images);
        
        assert_eq!(pairs.len(), 2); // 3 images -> 2 pairs (last one duplicated)
        
        // Last pair should have the same image twice
        let last_pair = &pairs[1];
        assert_eq!(last_pair.0.dimensions(), last_pair.1.dimensions());
    }

    #[test]
    fn test_pair_even_number() {
        let images = vec![
            create_test_image(10, 10, [255, 0, 0]),
            create_test_image(10, 10, [0, 255, 0]),
        ];
        
        let pairs = pair_portrait_images(images);
        
        assert_eq!(pairs.len(), 1); // 2 images -> 1 pair
    }
}