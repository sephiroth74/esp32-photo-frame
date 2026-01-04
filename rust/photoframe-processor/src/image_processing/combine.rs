use anyhow::Result;
use image::{ImageBuffer, Rgb, RgbImage};

/// Combine two already-processed portrait images side by side
///
/// This function assumes both images are already at half-width size (e.g., 400x480)
/// and just places them side by side without any resizing.
///
/// # Arguments
/// * `left_img` - The left portrait image (already resized to half-width)
/// * `right_img` - The right portrait image (already resized to half-width)
/// * `target_width` - The total width of the combined image
/// * `target_height` - The height of the combined image
/// * `divider_width` - Width of the divider line between images (typically 3 pixels)
/// * `divider_color` - RGB color of the divider line (e.g., Rgb([255, 255, 255]) for white)
pub fn combine_processed_portraits(
    left_img: &RgbImage,
    right_img: &RgbImage,
    target_width: u32,
    target_height: u32,
    divider_width: u32,
    divider_color: Rgb<u8>,
) -> Result<RgbImage> {
    let mut combined = ImageBuffer::new(target_width, target_height);
    let half_width = target_width / 2;

    // Left image should already be half_width x target_height
    let left_width = left_img.width().min(half_width);
    let left_height = left_img.height().min(target_height);

    // Right image should already be half_width x target_height
    let right_width = right_img.width().min(half_width);
    let right_height = right_img.height().min(target_height);

    // Copy left image to the left half
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

    if divider_width > 0 {
        // Draw vertical divider line between the two images
        let divider_start = half_width.saturating_sub(divider_width / 2);
        let divider_end = (half_width + divider_width / 2).min(target_width);

        for y in 0..target_height {
            for x in divider_start..divider_end {
                if x < target_width {
                    combined.put_pixel(x, y, divider_color);
                }
            }
        }
    }
    Ok(combined)
}

/// Combine two already-processed landscape images vertically (top-bottom)
///
/// This function assumes both images are already at half-height size (e.g., 480x400)
/// and stacks them vertically with a horizontal divider.
///
/// # Arguments
/// * `top_img` - The top landscape image (already resized to half-height)
/// * `bottom_img` - The bottom landscape image (already resized to half-height)
/// * `target_width` - The width of the combined image
/// * `target_height` - The total height of the combined image
/// * `divider_width` - Width of the horizontal divider line (typically 3 pixels)
/// * `divider_color` - RGB color of the divider line (e.g., Rgb([255, 255, 255]) for white)
pub fn combine_processed_landscapes(
    top_img: &RgbImage,
    bottom_img: &RgbImage,
    target_width: u32,
    target_height: u32,
    divider_width: u32,
    divider_color: Rgb<u8>,
) -> Result<RgbImage> {
    let mut combined = ImageBuffer::new(target_width, target_height);
    let half_height = target_height / 2;

    // Top image should already be target_width x half_height
    let top_width = top_img.width().min(target_width);
    let top_height = top_img.height().min(half_height);

    // Bottom image should already be target_width x half_height
    let bottom_width = bottom_img.width().min(target_width);
    let bottom_height = bottom_img.height().min(half_height);

    // Copy top image to the upper half
    for y in 0..top_height {
        for x in 0..top_width {
            if let Some(pixel) = top_img.get_pixel_checked(x, y) {
                combined.put_pixel(x, y, *pixel);
            }
        }
    }

    // Copy bottom image to lower half
    for y in 0..bottom_height {
        for x in 0..bottom_width {
            let dst_y = half_height + y;
            if dst_y < target_height {
                if let Some(pixel) = bottom_img.get_pixel_checked(x, y) {
                    combined.put_pixel(x, dst_y, *pixel);
                }
            }
        }
    }

    if divider_width > 0 {
        // Draw horizontal divider line between the two images
        let divider_start = half_height.saturating_sub(divider_width / 2);
        let divider_end = (half_height + divider_width / 2).min(target_height);

        for y in divider_start..divider_end {
            for x in 0..target_width {
                if y < target_height {
                    combined.put_pixel(x, y, divider_color);
                }
            }
        }
    }

    Ok(combined)
}

#[cfg(test)]
mod tests {}
