use crate::logging::Logger;
use crate::types::HexColor;
use ab_glyph::{Font, FontRef, PxScale, ScaleFont};
use anyhow::{Context, Result};
use image::{Rgb, RgbImage};
use imageproc::drawing::{draw_text_mut, text_size};
use regex::Regex;
use std::path::Path;

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
    let mut annotated_img = img.clone();

    // Extract date from EXIF data only
    if let Ok(date_text) = extract_date_from_image(input_path) {
        logger.verbose(&format!(
            "Adding date annotation '{}' to image '{}'",
            date_text,
            input_path.display()
        ));

        let bg_r = background_color.red();
        let bg_g = background_color.green();
        let bg_b = background_color.blue();
        let bg_a = background_color.alpha();

        // Load font
        let font = load_font(font_name)?;
        let scale = PxScale::from(font_size as f32);

        // Calculate text dimensions using imageproc
        let (text_width, _text_height) = text_size(scale, &font, &date_text);

        // Get font metrics for baseline positioning
        let scaled_font = font.as_scaled(scale);
        let ascent = scaled_font.ascent();
        let descent = scaled_font.descent();
        let actual_text_height = (ascent + descent.abs()).ceil() as u32;

        // Calculate background rectangle with reasonable padding
        let padding_x = 8;
        let padding_y = 4;
        let bg_width = text_width as u32 + (padding_x * 2);
        let bg_height = actual_text_height + (padding_y * 2);

        let (img_width, img_height) = annotated_img.dimensions();

        // Position in bottom-right corner with margin from image edge
        let margin = 15;
        let rect_x = img_width.saturating_sub(bg_width + margin);
        let rect_y = img_height.saturating_sub(bg_height + margin);

        // Draw semi-transparent background rectangle
        draw_background_rect(
            &mut annotated_img,
            rect_x,
            rect_y,
            bg_width,
            bg_height,
            bg_r,
            bg_g,
            bg_b,
            bg_a,
        );

        // Calculate properly centered text position within the background rectangle
        let text_x = rect_x + padding_x;
        let bg_center_y = rect_y + (bg_height / 2);
        let text_visual_center_offset = ascent * 0.6;
        let text_y = bg_center_y as f32 - text_visual_center_offset;

        // Draw text using imageproc with proper font rendering
        draw_text_mut(
            &mut annotated_img,
            Rgb([255u8, 255u8, 255u8]), // White text
            text_x as i32,
            text_y as i32,
            scale,
            &font,
            &date_text,
        );
    }

    Ok(annotated_img)
}

/// Load font based on font specification with smart detection
///
/// Supports three formats:
/// 1. Font name: "Arial" -> searches system font directories
/// 2. Font filename: "Arial.ttf" -> searches in common font directories
/// 3. Full path: "/System/Library/Fonts/Supplemental/Arial.ttf" -> loads directly
fn load_font(font_spec: &str) -> Result<FontRef<'static>> {
    // Strategy 1: Check if it's a full path (absolute path)
    if is_absolute_path(font_spec) {
        if let Ok(font) = load_font_from_path(font_spec) {
            return Ok(font);
        }
        return Err(anyhow::anyhow!(
            "Font file not found at path: {}",
            font_spec
        ));
    }

    // Strategy 2: Check if it's a font filename (ends with .ttf, .otf, .ttc, etc.)
    if is_font_filename(font_spec) {
        if let Ok(font) = load_font_by_filename(font_spec) {
            return Ok(font);
        }
        // Continue to name-based search as fallback
    }

    // Strategy 3: Treat as font name and search system directories
    if let Ok(font) = load_system_font(font_spec) {
        return Ok(font);
    }

    Err(anyhow::anyhow!(
        "No suitable fonts found for '{}'. Please ensure system fonts are available or specify a valid font path.",
        font_spec
    ))
}

/// Check if the input is an absolute path
fn is_absolute_path(path: &str) -> bool {
    path.starts_with('/') ||                           // Unix/Linux/macOS absolute path
    path.starts_with('\\') ||                          // Windows UNC path
    (path.len() > 2 && path.chars().nth(1) == Some(':')) // Windows drive path (C:, D:, etc.)
}

/// Check if the input looks like a font filename
fn is_font_filename(filename: &str) -> bool {
    let lower = filename.to_lowercase();
    lower.ends_with(".ttf")
        || lower.ends_with(".otf")
        || lower.ends_with(".ttc")
        || lower.ends_with(".woff")
        || lower.ends_with(".woff2")
}

/// Load font directly from a file path
fn load_font_from_path(font_path: &str) -> Result<FontRef<'static>> {
    let font_data = std::fs::read(font_path)
        .with_context(|| format!("Failed to read font file: {}", font_path))?;

    let font = FontRef::try_from_slice(Box::leak(font_data.into_boxed_slice()))
        .with_context(|| format!("Failed to parse font file: {}", font_path))?;

    Ok(font)
}

/// Load font by searching for filename in common font directories
fn load_font_by_filename(filename: &str) -> Result<FontRef<'static>> {
    for font_path in get_font_search_paths(filename, true) {
        let font_path = expand_path(&font_path);
        if std::path::Path::new(&font_path).exists() {
            if let Ok(font) = load_font_from_path(&font_path) {
                return Ok(font);
            }
        }
    }

    Err(anyhow::anyhow!(
        "Font filename '{}' not found in system directories",
        filename
    ))
}

/// Expand paths with ~ to home directory
fn expand_path(path: &str) -> String {
    if path.starts_with("~/") {
        if let Some(home) = std::env::var("HOME").ok() {
            return path.replacen("~", &home, 1);
        }
    }
    path.to_string()
}

/// Get common system font directories for different platforms
#[cfg(target_os = "macos")]
fn get_platform_font_dirs() -> Vec<String> {
    vec![
        "/System/Library/Fonts".to_string(),
        "/System/Library/Fonts/Supplemental".to_string(),
        "~/Library/Fonts/NerdFonts/".to_string(),
        "/Library/Fonts".to_string(),
        "~/Library/Fonts".to_string(),
    ]
}

#[cfg(target_os = "linux")]
fn get_platform_font_dirs() -> Vec<String> {
    vec![
        "/usr/share/fonts".to_string(),
        "/usr/share/fonts/truetype".to_string(),
        "/usr/share/fonts/TTF".to_string(),
        "/usr/share/fonts/opentype".to_string(),
        "/usr/local/share/fonts".to_string(),
        "~/.fonts".to_string(),
        "~/.local/share/fonts".to_string(),
    ]
}

#[cfg(target_os = "windows")]
fn get_platform_font_dirs() -> Vec<String> {
    vec!["C:\\Windows\\Fonts".to_string()]
}

/// Attempt to load a system font by name
fn load_system_font(font_name: &str) -> Result<FontRef<'static>> {
    for path in get_font_search_paths(font_name, false) {
        let expanded_path = expand_path(&path);
        if let Ok(font_data) = std::fs::read(&expanded_path) {
            if let Ok(font) = FontRef::try_from_slice(Box::leak(font_data.into_boxed_slice())) {
                return Ok(font);
            }
        }
    }

    Err(anyhow::anyhow!("System font '{}' not found", font_name))
}

/// Get potential system font paths for different platforms
fn get_font_search_paths(font_spec: &str, is_filename: bool) -> Vec<String> {
    let dirs = get_platform_font_dirs();
    let sep = std::path::MAIN_SEPARATOR;

    if is_filename {
        return dirs
            .into_iter()
            .map(|dir| format!("{}{}{}", dir, sep, font_spec))
            .collect();
    }

    let normalized_name = font_spec.to_lowercase().replace(" ", "").replace("-", "");
    let candidates = [font_spec.to_string(), normalized_name];
    let exts = ["ttf", "otf", "ttc"];

    let mut paths = Vec::new();
    for dir in dirs {
        for name in &candidates {
            for ext in &exts {
                paths.push(format!("{}{}{}.{}", dir, sep, name, ext));
            }
        }
    }

    paths
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

/// Draw a semi-transparent background rectangle
fn draw_background_rect(
    img: &mut RgbImage,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    bg_r: u8,
    bg_g: u8,
    bg_b: u8,
    bg_a: u8,
) {
    let (img_width, img_height) = img.dimensions();
    let alpha = bg_a as f32 / 255.0;
    let inv_alpha = 1.0 - alpha;

    for dy in 0..height {
        for dx in 0..width {
            let px = x + dx;
            let py = y + dy;

            if px < img_width && py < img_height {
                let current_pixel = img.get_pixel(px, py);

                // Alpha blend background color with existing pixel
                let blended_r =
                    ((bg_r as f32 * alpha) + (current_pixel[0] as f32 * inv_alpha)) as u8;
                let blended_g =
                    ((bg_g as f32 * alpha) + (current_pixel[1] as f32 * inv_alpha)) as u8;
                let blended_b =
                    ((bg_b as f32 * alpha) + (current_pixel[2] as f32 * inv_alpha)) as u8;

                img.put_pixel(px, py, Rgb([blended_r, blended_g, blended_b]));
            }
        }
    }
}
