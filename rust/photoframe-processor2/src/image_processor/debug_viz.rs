/// Debug visualization module for drawing detection boxes
/// Used when --debug flag is active to visualize people detection results
use anyhow::Result;
use image::{Rgb, RgbImage};
use imageproc::drawing::draw_hollow_rect_mut;
use imageproc::rect::Rect;

/// Draw detection bounding boxes on an image
///
/// This draws green rectangles around detected people.
///
/// # Arguments
/// * `img` - The image to annotate
/// * `detections` - List of bounding boxes: [(x_min, y_min, x_max, y_max, confidence)]
/// * `_show_confidence` - Unused, kept for API compatibility
pub fn draw_detection_boxes(
    img: &RgbImage,
    detections: &[(u32, u32, u32, u32, f32)],
    _show_confidence: bool,
) -> Result<RgbImage> {
    let mut annotated = img.clone();

    // Green color for detection boxes (bright green)
    let box_color = Rgb([0u8, 255u8, 0u8]);

    for &(x_min, y_min, x_max, y_max, _confidence) in detections.iter() {
        let width = x_max.saturating_sub(x_min);
        let height = y_max.saturating_sub(y_min);

        // Draw the bounding box (3 pixels thick for visibility)
        let rect = Rect::at(x_min as i32, y_min as i32).of_size(width, height);

        // Draw multiple times for thickness (3 pixels)
        for offset in 0..3 {
            let thick_rect = Rect::at(rect.left() - offset, rect.top() - offset).of_size(
                rect.width() + (offset * 2) as u32,
                rect.height() + (offset * 2) as u32,
            );
            draw_hollow_rect_mut(&mut annotated, thick_rect, box_color);
        }
    }

    Ok(annotated)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_draw_detection_boxes() {
        let img = RgbImage::new(800, 600);
        let detections = vec![(100, 100, 300, 400, 0.95), (500, 200, 700, 500, 0.87)];

        let result = draw_detection_boxes(&img, &detections, true);
        assert!(result.is_ok());

        let annotated = result.unwrap();
        assert_eq!(annotated.dimensions(), (800, 600));
    }
}
