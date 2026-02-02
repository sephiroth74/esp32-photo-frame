/// Smart crop module - image cropping and resizing utilities
use anyhow::Result;
use image::RgbImage;

/// Crop an image to specified dimensions
pub fn crop_image(img: &RgbImage, x: u32, y: u32, width: u32, height: u32) -> Result<RgbImage> {
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
pub fn resize_image(img: &RgbImage, width: u32, height: u32) -> Result<RgbImage> {
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
