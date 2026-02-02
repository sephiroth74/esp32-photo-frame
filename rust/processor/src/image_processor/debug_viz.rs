use crate::image_processor::face_detection::Face;
use ab_glyph::{FontRef, PxScale};
/// Debug visualization module for drawing detection boxes
/// Used when --debug flag is active to visualize face detection results
use anyhow::Result;
use image::{Rgb, RgbImage};
use imageproc::drawing::{draw_hollow_rect_mut, draw_text_mut};
use imageproc::rect::Rect;

/// Draw detection bounding boxes on an image with confidence scores
///
/// This draws green rectangles around detected faces with confidence scores.
/// Uses embedded DejaVu Sans Mono font for text rendering.
///
/// # Arguments
/// * `img` - The image to annotate
/// * `faces` - List of detected faces with bounding boxes and confidence scores
/// * `_show_confidence` - Unused, kept for API compatibility
pub fn draw_detection_boxes(
    img: &RgbImage,
    faces: &[Face],
    _show_confidence: bool,
) -> Result<RgbImage> {
    let mut annotated = img.clone();
    let (img_width, img_height) = img.dimensions();

    // Load font for score text rendering
    // Try to load from insightface-rs assets first, fallback if not available
    let font_data =
        include_bytes!("../../../insightface_rs/assets/DejaVuSansMNerdFont-Regular.ttf");
    let font = match FontRef::try_from_slice(font_data) {
        Ok(f) => f,
        Err(_) => {
            return Ok(annotated);
        }
    };

    // Green color for detection boxes (bright green)
    let box_color = Rgb([0u8, 255u8, 0u8]);
    let scale = PxScale::from(16.0);

    for face in faces.iter() {
        let x1 = face.x1 as i32;
        let y1 = face.y1 as i32;
        let x2 = face.x2 as i32;
        let y2 = face.y2 as i32;

        // Clamp coordinates to image bounds
        let x1 = x1.max(0) as u32;
        let y1 = y1.max(0) as u32;
        let x2 = (x2 as u32).min(img_width);
        let y2 = (y2 as u32).min(img_height);

        if x2 > x1 && y2 > y1 {
            // Draw bounding box (2 pixels thick)
            let width = x2 - x1;
            let height = y2 - y1;
            let rect = Rect::at(x1 as i32, y1 as i32).of_size(width, height);

            draw_hollow_rect_mut(&mut annotated, rect, box_color);

            // Draw again for thickness
            if x1 + 1 < x2 && y1 + 1 < y2 {
                let rect_inner = Rect::at((x1 + 1) as i32, (y1 + 1) as i32)
                    .of_size(width.saturating_sub(2), height.saturating_sub(2));
                draw_hollow_rect_mut(&mut annotated, rect_inner, box_color);
            }

            // Draw confidence score
            let score_text = format!("{:.2}", face.confidence);

            // Determine text position based on available space
            let text_height = 20;
            let text_width = score_text.len() as i32 * 10;

            let (text_x, text_y) = if y1 >= text_height as u32 {
                // Above the box
                (x1 as i32, (y1 as i32) - text_height)
            } else if (y2 as i32 + text_height) < img_height as i32 {
                // Below the box
                (x1 as i32, (y2 as i32) + 5)
            } else if (x2 as i32 + text_width) < img_width as i32 {
                // Right of the box
                ((x2 as i32) + 5, y1 as i32)
            } else if (x1 as i32) >= text_width {
                // Left of the box
                ((x1 as i32) - text_width, y1 as i32)
            } else {
                // Inside the box at top
                ((x1 as i32) + 5, (y1 as i32) + 5)
            };

            draw_text_mut(
                &mut annotated,
                box_color,
                text_x.max(0),
                text_y.max(0),
                scale,
                &font,
                &score_text,
            );
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
        let faces = vec![
            Face {
                x1: 100,
                y1: 100,
                x2: 300,
                y2: 400,
                confidence: 0.95,
            },
            Face {
                x1: 500,
                y1: 200,
                x2: 700,
                y2: 500,
                confidence: 0.87,
            },
        ];

        let result = draw_detection_boxes(&img, &faces, true);
        assert!(result.is_ok());

        let annotated = result.unwrap();
        assert_eq!(annotated.dimensions(), (800, 600));
    }
}
