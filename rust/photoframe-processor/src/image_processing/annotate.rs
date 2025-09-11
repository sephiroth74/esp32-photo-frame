use ab_glyph::{Font, FontRef, PxScale, ScaleFont};
use anyhow::{Context, Result};
use image::{Rgb, RgbImage};
use imageproc::drawing::{draw_text_mut, text_size};
use std::path::Path;

/// Add date annotation to an image (extracted from EXIF data)
///
/// This adds a date label in the bottom-right corner with a semi-transparent background,
/// matching the format seen in the original bash script output (YYYY/MM/DD).
///
/// Uses imageproc crate for proper font rendering with support for various font families.
pub fn add_filename_annotation(
    img: &RgbImage,
    input_path: &Path,
    font_name: &str,
    font_size: f32,
    background_color: &str,
) -> Result<RgbImage> {
    let mut annotated_img = img.clone();

    // Extract date from EXIF data, fallback to filename if no date found
    let display_text = extract_date_from_image(input_path)?;

    // Parse background color with alpha
    let (bg_r, bg_g, bg_b, bg_a) = parse_hex_color(background_color)?;

    // Load font
    let font = load_font(font_name)?;
    let scale = PxScale::from(font_size);

    // Calculate text dimensions using imageproc
    let (text_width, _text_height) = text_size(scale, &font, &display_text);

    // Get font metrics for baseline positioning
    let scaled_font = font.as_scaled(scale);
    let ascent = scaled_font.ascent();

    // Use a more conservative approach for background height
    // Use font ascent + descent for more accurate height calculation
    let descent = scaled_font.descent();
    let actual_text_height = (ascent + descent.abs()).ceil() as u32;

    // Calculate background rectangle with reasonable padding
    let padding_x = 4;
    let padding_y = 2;
    let bg_width = text_width as u32 + (padding_x * 2);
    let bg_height = actual_text_height + (padding_y * 2);

    let (img_width, img_height) = annotated_img.dimensions();

    // Position in bottom-right corner with margin from image edge
    let margin = 4;
    let rect_x = img_width.saturating_sub(bg_width + margin);
    let rect_y = img_height.saturating_sub(bg_height + margin);

    // Draw subtle semi-transparent background rectangle
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

    // More direct centering approach using actual text dimensions
    // Position the text so its visual center aligns with the background center
    let bg_center_y = rect_y + (bg_height / 2);

    // Calculate text center position: the baseline should be positioned so that
    // the visual center of the text aligns with the background center
    // Fine-tuning: need to move text down to reduce top padding
    let text_visual_center_offset = ascent * 0.6; // About 50% of ascent above baseline
    let text_y = bg_center_y as f32 - text_visual_center_offset;

    // Draw text using imageproc with proper font rendering
    draw_text_mut(
        &mut annotated_img,
        Rgb([255u8, 255u8, 255u8]), // White text
        text_x as i32,
        text_y as i32,
        scale,
        &font,
        &display_text,
    );

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

    // Strategy 4: Fallback to common system fonts
    if let Ok(font) = load_embedded_font() {
        return Ok(font);
    }

    // Final fallback: try to load any available system font
    let default_fonts = [
        "/System/Library/Fonts/Helvetica.ttc", // macOS
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", // Linux
        "/mnt/c/Windows/Fonts/arial.ttf",      // Windows
    ];

    for font_path in &default_fonts {
        if let Ok(font_data) = std::fs::read(font_path) {
            if let Ok(font) = FontRef::try_from_slice(Box::leak(font_data.into_boxed_slice())) {
                return Ok(font);
            }
        }
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
    let font_directories = get_system_font_directories();

    for dir in font_directories {
        let expanded_dir = expand_path(dir);
        let font_path = format!("{}/{}", expanded_dir, filename);
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
fn get_system_font_directories() -> Vec<&'static str> {
    vec![
        // macOS
        "/System/Library/Fonts",
        "/System/Library/Fonts/Supplemental",
        "/Library/Fonts",
        "~/Library/Fonts",
        // Linux
        "/usr/share/fonts",
        "/usr/share/fonts/truetype",
        "/usr/share/fonts/TTF",
        "/usr/share/fonts/opentype",
        "/usr/local/share/fonts",
        "~/.fonts",
        "~/.local/share/fonts",
        // Windows (via WSL)
        "/mnt/c/Windows/Fonts",
        "/mnt/c/Windows/System32/Fonts",
    ]
}

/// Attempt to load a system font by name
fn load_system_font(font_name: &str) -> Result<FontRef<'static>> {
    // Common system font paths on different platforms
    let font_paths = get_system_font_paths(font_name);

    for path in font_paths {
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
fn get_system_font_paths(font_name: &str) -> Vec<String> {
    let mut paths = Vec::new();

    // Normalize font name (remove spaces, make lowercase for file matching)
    let normalized_name = font_name.to_lowercase().replace(" ", "").replace("-", "");

    // macOS font paths
    paths.push(format!("/System/Library/Fonts/{}.ttf", font_name));
    paths.push(format!("/System/Library/Fonts/{}.otf", font_name));
    paths.push(format!(
        "/System/Library/Fonts/Supplemental/{}.ttf",
        font_name
    ));
    paths.push(format!(
        "/System/Library/Fonts/Supplemental/{}.otf",
        font_name
    ));
    paths.push(format!("/Library/Fonts/{}.ttf", font_name));
    paths.push(format!("/Library/Fonts/{}.otf", font_name));
    paths.push(format!("~/Library/Fonts/{}.otf", font_name));
    paths.push(format!("~/Library/Fonts/{}.ttf", font_name));

    // Linux font paths
    paths.push(format!(
        "/usr/share/fonts/truetype/{}/{}.ttf",
        normalized_name, normalized_name
    ));
    paths.push(format!("/usr/share/fonts/TTF/{}.ttf", font_name));
    paths.push(format!(
        "/usr/share/fonts/opentype/{}/{}.otf",
        normalized_name, normalized_name
    ));

    // Windows font paths (if running under WSL or similar)
    paths.push(format!("/mnt/c/Windows/Fonts/{}.ttf", font_name));
    paths.push(format!("/mnt/c/Windows/Fonts/{}.otf", font_name));

    // Common font name mappings
    match font_name.to_lowercase().as_str() {
        "arial" | "arial-bold" => {
            paths.push("/System/Library/Fonts/Arial.ttf".to_string());
            paths.push("/mnt/c/Windows/Fonts/arial.ttf".to_string());
        }
        "helvetica" | "helvetica-bold" => {
            paths.push("/System/Library/Fonts/Helvetica.ttc".to_string());
        }
        "times" | "times-bold" | "times new roman" => {
            paths.push("/System/Library/Fonts/Times.ttc".to_string());
            paths.push("/mnt/c/Windows/Fonts/times.ttf".to_string());
        }
        "courier" | "courier-bold" | "courier new" => {
            paths.push("/System/Library/Fonts/Courier New.ttf".to_string());
            paths.push("/mnt/c/Windows/Fonts/cour.ttf".to_string());
        }
        "ubuntumono-nerd-font-bold" | "ubuntu mono" => {
            paths.push("/usr/share/fonts/truetype/ubuntu/UbuntuMono-Bold.ttf".to_string());
            paths.push("/System/Library/Fonts/Monaco.ttf".to_string()); // macOS fallback
        }
        _ => {}
    }

    paths
}

/// Load embedded default font (fallback to system monospace)
fn load_embedded_font() -> Result<FontRef<'static>> {
    // Try common monospace fonts as fallbacks
    let fallback_fonts = [
        "/System/Library/Fonts/Monaco.ttf",                    // macOS
        "/System/Library/Fonts/Menlo.ttc",                     // macOS
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", // Linux
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",             // Linux alt
        "/mnt/c/Windows/Fonts/consola.ttf",                    // Windows Consolas
    ];

    for font_path in &fallback_fonts {
        if let Ok(font_data) = std::fs::read(font_path) {
            if let Ok(font) = FontRef::try_from_slice(Box::leak(font_data.into_boxed_slice())) {
                return Ok(font);
            }
        }
    }

    // If no system fonts are available, create a minimal font using imageproc's built-in capabilities
    // This is a fallback that uses system defaults
    Err(anyhow::anyhow!(
        "No suitable fonts found. Text rendering may fall back to system default."
    ))
}

/// Extract date from EXIF data, format as YYYY/MM/DD
/// Falls back to filename if no EXIF date is available
fn extract_date_from_image(image_path: &Path) -> Result<String> {
    // Try to read EXIF date first
    if let Ok(exif_date) = read_exif_date(image_path) {
        return Ok(exif_date);
    }

    // Fallback: try to extract date from filename patterns
    if let Some(filename) = image_path.file_stem().and_then(|s| s.to_str()) {
        // Common patterns: IMG_20220513, 20220513_IMG, IMG20220513WA0000
        if let Some(date) = extract_date_from_filename(filename) {
            return Ok(date);
        }
    }

    // Last resort: use filename without extension, truncated
    let fallback = image_path
        .file_stem()
        .and_then(|name| name.to_str())
        .unwrap_or("image");

    if fallback.len() > 10 {
        Ok(format!("{}...", &fallback[..7]))
    } else {
        Ok(fallback.to_string())
    }
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
    if let Some(caps) = regex::Regex::new(r"(?i)img(\d{8})")
        .ok()?
        .captures(filename)
    {
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

    // Pattern 2: Just 8 digits anywhere in filename
    if let Some(caps) = regex::Regex::new(r"(\d{8})").ok()?.captures(filename) {
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

    None
}

/// Parse hex color string to RGB values
///
/// Supports formats: #RGB, #RRGGBB, #RRGGBBAA
fn parse_hex_color(color_str: &str) -> Result<(u8, u8, u8, u8)> {
    if !color_str.starts_with('#') {
        return Err(anyhow::anyhow!("Color must start with #"));
    }

    let hex = &color_str[1..];

    match hex.len() {
        3 => {
            // #RGB -> #RRGGBB
            let r = u8::from_str_radix(&hex[0..1].repeat(2), 16)?;
            let g = u8::from_str_radix(&hex[1..2].repeat(2), 16)?;
            let b = u8::from_str_radix(&hex[2..3].repeat(2), 16)?;
            Ok((r, g, b, 255))
        }
        6 => {
            // #RRGGBB
            let r = u8::from_str_radix(&hex[0..2], 16)?;
            let g = u8::from_str_radix(&hex[2..4], 16)?;
            let b = u8::from_str_radix(&hex[4..6], 16)?;
            Ok((r, g, b, 255))
        }
        8 => {
            // #RRGGBBAA
            let r = u8::from_str_radix(&hex[0..2], 16)?;
            let g = u8::from_str_radix(&hex[2..4], 16)?;
            let b = u8::from_str_radix(&hex[4..6], 16)?;
            let a = u8::from_str_radix(&hex[6..8], 16)?;
            Ok((r, g, b, a))
        }
        _ => Err(anyhow::anyhow!("Invalid hex color format")),
    }
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_hex_color() {
        assert_eq!(parse_hex_color("#000").unwrap(), (0, 0, 0, 255));
        assert_eq!(parse_hex_color("#FFF").unwrap(), (255, 255, 255, 255));
        assert_eq!(parse_hex_color("#FF0000").unwrap(), (255, 0, 0, 255));
        assert_eq!(parse_hex_color("#00FF0080").unwrap(), (0, 255, 0, 128));

        assert!(parse_hex_color("FF0000").is_err()); // Missing #
        assert!(parse_hex_color("#GG0000").is_err()); // Invalid hex
    }

    #[test]
    fn test_is_absolute_path() {
        // Unix/Linux/macOS paths
        assert!(is_absolute_path("/System/Library/Fonts/Arial.ttf"));
        assert!(is_absolute_path("/usr/share/fonts/font.ttf"));

        // Windows paths
        assert!(is_absolute_path("C:\\Windows\\Fonts\\arial.ttf"));
        assert!(is_absolute_path("D:\\Fonts\\font.ttf"));
        assert!(is_absolute_path("\\\\server\\share\\fonts\\font.ttf"));

        // Relative paths
        assert!(!is_absolute_path("Arial.ttf"));
        assert!(!is_absolute_path("fonts/Arial.ttf"));
        assert!(!is_absolute_path("./Arial.ttf"));
        assert!(!is_absolute_path("../fonts/Arial.ttf"));
    }

    #[test]
    fn test_is_font_filename() {
        assert!(is_font_filename("Arial.ttf"));
        assert!(is_font_filename("Arial.TTF")); // Case insensitive
        assert!(is_font_filename("font.otf"));
        assert!(is_font_filename("font.ttc"));
        assert!(is_font_filename("font.woff"));
        assert!(is_font_filename("font.woff2"));

        assert!(!is_font_filename("Arial")); // Just name
        assert!(!is_font_filename("Arial.txt")); // Wrong extension
        assert!(!is_font_filename("/path/to/Arial")); // Path without extension
    }

    #[test]
    fn test_expand_path() {
        // Mock HOME environment for testing
        std::env::set_var("HOME", "/Users/testuser");

        assert_eq!(
            expand_path("~/Library/Fonts"),
            "/Users/testuser/Library/Fonts"
        );
        assert_eq!(
            expand_path("/System/Library/Fonts"),
            "/System/Library/Fonts"
        );
        assert_eq!(expand_path("relative/path"), "relative/path");

        // Clean up
        std::env::remove_var("HOME");
    }
}
