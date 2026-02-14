use crate::image_processor::imagemagick::ImageMagickWrapper;
use crate::logging::Logger;
use crate::types::HexColor;
use anyhow::{Context, Result};
use image::RgbImage;
use regex::Regex;
use std::path::Path;
use tempfile::NamedTempFile;

/// Add date annotation to an image (extracted from EXIF data)
///
/// This adds a date label in the bottom-right corner with a semi-transparent background,
/// matching the format seen in the original bash script output (YYYY/MM/DD).
///
/// Uses ab_glyph and imageproc for proper font rendering with background box and padding.
pub fn add_date_annotation(
    img: &RgbImage,
    input_path: &Path,
    font_name: &str,
    font_size: u32,
    background_color: &HexColor,
    logger: &Logger,
) -> Result<RgbImage> {
    //let mut annotated_img = img.clone();

    // Extract date from EXIF data only
    if let Ok(date_text) = extract_date_from_image(input_path) {
        let thread_id = std::thread::current().id();
        let input_path = NamedTempFile::with_suffix(format!("annotation_{:?}.png", thread_id))
            .context("Failed to create temporary input image file")?;
        img.save(input_path.path())?;

        return match ImageMagickWrapper::annotate(
            input_path.path(),
            input_path.path(),
            font_name,
            font_size,
            &date_text,
            Some("white"),
            Some(&background_color.to_magick_color()),
            None,
            None,
            None,
        )
        .and_then(|_| {
            // Load the annotated image back into memory
            image::open(input_path.path())
                .context("Failed to open annotated image")
                .map(|img| img.to_rgb8())
        }) {
            Ok(annotated) => Ok(annotated),
            Err(e) => {
                logger.warning(&format!(
                    "ImageMagick annotation failed: {}. Falling back to Rust-based annotation.",
                    e
                ));
                // Fall back to Rust-based annotation if ImageMagick fails
                Err(e)
            }
        };

        //
        //
        //let bg_r = background_color.red();
        //let bg_g = background_color.green();
        //let bg_b = background_color.blue();
        //let bg_a = background_color.alpha();
        //
        //// Load font
        //let font = load_font(font_name)?;
        //let scale = PxScale::from(font_size as f32);
        //
        //// Calculate text dimensions using imageproc
        //let (text_width, _text_height) = text_size(scale, &font, &date_text);
        //
        //// Get font metrics for baseline positioning
        //let scaled_font = font.as_scaled(scale);
        //let ascent = scaled_font.ascent();
        //let descent = scaled_font.descent();
        //let actual_text_height = (ascent + descent.abs()).ceil() as u32;
        //
        //// Calculate background rectangle with reasonable padding
        //let padding_x = 8;
        //let padding_y = 4;
        //let bg_width = text_width as u32 + (padding_x * 2);
        //let bg_height = actual_text_height + (padding_y * 2);
        //
        //let (img_width, img_height) = annotated_img.dimensions();
        //
        //// Position in bottom-right corner with margin from image edge
        //let margin = 15;
        //let rect_x = img_width.saturating_sub(bg_width + margin);
        //let rect_y = img_height.saturating_sub(bg_height + margin);
        //
        //// Draw semi-transparent background rectangle
        //draw_background_rect(
        //    &mut annotated_img,
        //    rect_x,
        //    rect_y,
        //    bg_width,
        //    bg_height,
        //    bg_r,
        //    bg_g,
        //    bg_b,
        //    bg_a,
        //);
        //
        //// Calculate properly centered text position within the background rectangle
        //let text_x = rect_x + padding_x;
        //let bg_center_y = rect_y + (bg_height / 2);
        //let text_visual_center_offset = ascent * 0.6;
        //let text_y = bg_center_y as f32 - text_visual_center_offset;
        //
        //// Draw text using imageproc with proper font rendering
        //draw_text_mut(
        //    &mut annotated_img,
        //    Rgb([255u8, 255u8, 255u8]), // White text
        //    text_x as i32,
        //    text_y as i32,
        //    scale,
        //    &font,
        //    &date_text,
        //);
    }

    Ok(img.clone())
}

/// Extract date from EXIF data, format as YYYY/MM/DD
fn extract_date_from_image(image_path: &Path) -> Result<String> {
    // Try to read EXIF date first
    if let Ok(exif_date) = read_exif_date(image_path) {
        return Ok(exif_date);
    }

    // Fallback: try to extract date from filename patterns
    if let Some(filename) = image_path.file_stem().and_then(|s| s.to_str()) {
        if let Some(date) = extract_date_from_filename(filename) {
            return Ok(date);
        }
    }

    Err(anyhow::anyhow!("No EXIF date found"))
}

/// Read EXIF date and format as YYYY/MM/DD
fn read_exif_date(image_path: &Path) -> Result<String> {
    use exif::{In, Reader, Tag, Value};

    let file = std::fs::File::open(image_path)?;
    let mut buf_reader = std::io::BufReader::new(file);
    let exif_reader = Reader::new();
    let exif = exif_reader.read_from_container(&mut buf_reader)?;

    // Try DateTime first, then DateTimeOriginal, then DateTimeDigitized
    let date_tags = [Tag::DateTime, Tag::DateTimeOriginal, Tag::DateTimeDigitized];

    for &tag in &date_tags {
        if let Some(field) = exif.get_field(tag, In::PRIMARY) {
            if let Value::Ascii(values) = &field.value {
                if let Some(date_bytes) = values.first() {
                    if let Ok(date_str) = std::str::from_utf8(date_bytes) {
                        // EXIF date format: "YYYY:MM:DD HH:MM:SS"
                        if date_str.len() >= 10 {
                            let date_part = &date_str[..10];
                            // Convert "YYYY:MM:DD" to "YYYY/MM/DD"
                            let formatted = date_part.replace(':', "/");
                            return Ok(formatted);
                        }
                    }
                }
            }
        }
    }

    Err(anyhow::anyhow!("No EXIF date found"))
}

/// Extract date from filename patterns like IMG20220513, IMG_20220513, etc.
fn extract_date_from_filename(filename: &str) -> Option<String> {
    // Pattern 1: IMG20220513WA0000 -> 2022/05/13
    if let Ok(img_regex) = Regex::new(r"(?i)img(\d{8})") {
        if let Some(caps) = img_regex.captures(filename) {
            if let Some(date_match) = caps.get(1) {
                let date_str = date_match.as_str();
                if date_str.len() == 8 {
                    let year = &date_str[0..4];
                    let month = &date_str[4..6];
                    let day = &date_str[6..8];
                    return Some(format!("{}/{}/{}", year, month, day));
                }
            }
        }
    }

    // Pattern 2: Just 8 digits anywhere in filename
    if let Ok(date_regex) = Regex::new(r"(\d{8})") {
        if let Some(caps) = date_regex.captures(filename) {
            if let Some(date_match) = caps.get(1) {
                let date_str = date_match.as_str();
                let year = &date_str[0..4];
                let month = &date_str[4..6];
                let day = &date_str[6..8];

                // Basic validation
                if year >= "2000"
                    && year <= "2030"
                    && month >= "01"
                    && month <= "12"
                    && day >= "01"
                    && day <= "31"
                {
                    return Some(format!("{}/{}/{}", year, month, day));
                }
            }
        }
    }

    None
}
