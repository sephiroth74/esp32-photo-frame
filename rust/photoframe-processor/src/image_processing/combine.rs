use anyhow::Result;
use image::{ImageBuffer, Rgb, RgbImage};

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
    
    // Resize both images to half width if needed
    let left_resized = if left_img.width() != half_width || left_img.height() != target_height {
        resize_to_exact(left_img, half_width, target_height)?
    } else {
        left_img.clone()
    };
    
    let right_resized = if right_img.width() != half_width || right_img.height() != target_height {
        resize_to_exact(right_img, half_width, target_height)?
    } else {
        right_img.clone()
    };
    
    // Copy left image to left half
    for y in 0..target_height {
        for x in 0..half_width {
            if let Some(pixel) = left_resized.get_pixel_checked(x, y) {
                combined.put_pixel(x, y, *pixel);
            }
        }
    }
    
    // Copy right image to right half
    for y in 0..target_height {
        for x in 0..half_width {
            let dst_x = half_width + x;
            if dst_x < target_width {
                if let Some(pixel) = right_resized.get_pixel_checked(x, y) {
                    combined.put_pixel(dst_x, y, *pixel);
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

/// Resize image to exact dimensions (placeholder implementation)
#[allow(dead_code)]
fn resize_to_exact(img: &RgbImage, width: u32, height: u32) -> Result<RgbImage> {
    // TODO: Use proper high-quality resizing
    // For now, use simple nearest neighbor
    let (src_width, src_height) = img.dimensions();
    let mut resized = ImageBuffer::new(width, height);
    
    for (x, y, pixel) in resized.enumerate_pixels_mut() {
        let src_x = (x * src_width / width).min(src_width - 1);
        let src_y = (y * src_height / height).min(src_height - 1);
        
        *pixel = *img.get_pixel(src_x, src_y);
    }
    
    Ok(resized)
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