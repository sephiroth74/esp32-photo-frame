use crate::types::Size;
use image::imageops::FilterType;
use imageproc::drawing::Canvas;

/// Resize an image to cover the target dimensions while preserving aspect ratio.
/// The image will be scaled so that both dimensions are at least as large as the target,
/// using the maximum of the width and height scale factors.
/// This ensures no dimension is smaller than the target (cover strategy).
pub fn resize_to_cover(
    img: &image::DynamicImage,
    target: Size,
    filter: FilterType,
) -> image::DynamicImage {
    let (width, height) = img.dimensions();
    let target_w = target.width as f32;
    let target_h = target.height as f32;

    let scale_w = target_w / width as f32;
    let scale_h = target_h / height as f32;
    let scale = scale_w.max(scale_h);

    let new_w = (width as f32 * scale).ceil() as u32;
    let new_h = (height as f32 * scale).ceil() as u32;

    img.resize_exact(new_w, new_h, filter)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::{DynamicImage, RgbImage};

    #[test]
    fn test_resize_to_cover_landscape() {
        // 400x200 -> target 300x300 should scale to 600x300
        let img = DynamicImage::ImageRgb8(RgbImage::new(400, 200));
        let target = Size {
            width: 300,
            height: 300,
        };

        let resized = resize_to_cover(&img, target, FilterType::Nearest);
        let (w, h) = resized.dimensions();

        assert!(w >= target.width);
        assert!(h >= target.height);
        assert_eq!((w, h), (600, 300));
    }

    #[test]
    fn test_resize_to_cover_portrait() {
        // 200x400 -> target 300x300 should scale to 300x600
        let img = DynamicImage::ImageRgb8(RgbImage::new(200, 400));
        let target = Size {
            width: 300,
            height: 300,
        };

        let resized = resize_to_cover(&img, target, FilterType::Nearest);
        let (w, h) = resized.dimensions();

        assert!(w >= target.width);
        assert!(h >= target.height);
        assert_eq!((w, h), (300, 600));
    }
}
