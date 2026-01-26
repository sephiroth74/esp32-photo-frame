use crate::DisplayType;
use image::{Rgb, RgbImage};

/// Six-color palette used by e-paper displays and conversion helpers.
/// Order: Black, White, Red, Yellow, Green, Blue.
pub const SIX_COLOR_PALETTE: [(u8, u8, u8); 6] = [
    (0, 0, 0),       // black
    (255, 255, 255), // white
    (255, 0, 0),     // red
    (255, 255, 0),   // yellow
    (0, 255, 0),     // green
    (0, 0, 255),     // blue
];

/// Black-and-white palette.
pub const BW_PALETTE: [(u8, u8, u8); 2] = [(0, 0, 0), (255, 255, 255)];

/// Apply color adjustments (brightness, contrast, saturation) to an 8-bit RGB image.
///
/// - `img`: source `RgbImage` (8-bit per channel).
/// - `saturation`: multiplier for saturation (1.0 = unchanged).
/// - `contrast`: multiplier for contrast around midpoint (128.0).
/// - `brightness`: multiplicative brightness factor (1.0 = unchanged).
///
/// The function returns a new `RgbImage` with the same dimensions as `img`.
pub fn apply_color_adjustments(
    img: &image::RgbImage,
    saturation: f32,
    contrast: f32,
    brightness: f32,
) -> image::RgbImage {
    use image::{Rgb, RgbImage};

    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let pixel = img.get_pixel(x, y);
            let mut r = pixel[0] as f32;
            let mut g = pixel[1] as f32;
            let mut b = pixel[2] as f32;

            // Apply brightness
            r *= brightness;
            g *= brightness;
            b *= brightness;

            // Apply contrast around midpoint 128
            r = (r - 128.0) * contrast + 128.0;
            g = (g - 128.0) * contrast + 128.0;
            b = (b - 128.0) * contrast + 128.0;

            // Apply saturation toward luma
            let lum = 0.299 * r + 0.587 * g + 0.114 * b;
            r = lum + (r - lum) * saturation;
            g = lum + (g - lum) * saturation;
            b = lum + (b - lum) * saturation;

            output.put_pixel(
                x,
                y,
                Rgb([
                    r.clamp(0.0, 255.0) as u8,
                    g.clamp(0.0, 255.0) as u8,
                    b.clamp(0.0, 255.0) as u8,
                ]),
            );
        }
    }

    output
}

/// Enhanced color matching using perceptual color distance
/// Uses weighted Euclidean distance based on human color perception
pub fn find_closest_color_weighted(r: u8, g: u8, b: u8, palette: &[(u8, u8, u8)]) -> (u8, u8, u8) {
    let mut min_distance = f32::MAX;
    let mut closest_color = palette[0];

    for &(pr, pg, pb) in palette {
        // Weighted distance based on human color perception
        // Red: 30%, Green: 59%, Blue: 11%
        let dr = (r as f32) - (pr as f32);
        let dg = (g as f32) - (pg as f32);
        let db = (b as f32) - (pb as f32);

        let distance = (dr * dr * 0.3 + dg * dg * 0.59 + db * db * 0.11).sqrt();

        if distance < min_distance {
            min_distance = distance;
            closest_color = (pr, pg, pb);
        }
    }

    closest_color
}

/// Create working buffers from the original image with gamma correction
pub fn create_working_buffers(img: &RgbImage) -> (Vec<Vec<f32>>, Vec<Vec<f32>>, Vec<Vec<f32>>) {
    let (width, height) = img.dimensions();
    let mut working_r: Vec<Vec<f32>> = Vec::with_capacity(height as usize);
    let mut working_g: Vec<Vec<f32>> = Vec::with_capacity(height as usize);
    let mut working_b: Vec<Vec<f32>> = Vec::with_capacity(height as usize);

    for y in 0..height {
        let mut row_r = Vec::with_capacity(width as usize);
        let mut row_g = Vec::with_capacity(width as usize);
        let mut row_b = Vec::with_capacity(width as usize);

        for x in 0..width {
            let pixel = img.get_pixel(x, y);
            // Apply gamma correction for better perceptual results
            row_r.push(apply_gamma(pixel[0] as f32));
            row_g.push(apply_gamma(pixel[1] as f32));
            row_b.push(apply_gamma(pixel[2] as f32));
        }

        working_r.push(row_r);
        working_g.push(row_g);
        working_b.push(row_b);
    }

    (working_r, working_g, working_b)
}

/// Apply gamma correction for more perceptually uniform dithering
fn apply_gamma(value: f32) -> f32 {
    // Apply gamma 2.2 for better perceptual linearity
    let normalized = value / 255.0;
    let corrected = normalized.powf(2.2);
    corrected * 255.0
}

/// Convert RGB pixel to ESP32 color format using bmp2cpp approach
///
/// This uses the same RGB compression format as the bmp2cpp project:
/// - Red: 3 bits (0-7) - divide by 32, shift left 5 positions
/// - Green: 3 bits (0-7) - divide by 32, shift left 2 positions
/// - Blue: 2 bits (0-3) - divide by 64, no shift
///
/// Final format: RRRGGGBB
///
/// This matches exactly what the 6-color e-paper display expects and ensures
/// compatibility with existing bmp2cpp generated files.
pub fn rgb_to_esp32_color(pixel: &Rgb<u8>) -> u8 {
    let r = pixel[0];
    let g = pixel[1];
    let b = pixel[2];

    // Use the exact same formula as bmp2cpp project
    ((r / 32) << 5) + ((g / 32) << 2) + (b / 64)
}

/// This function converts RGB pixel data to the 8-bit format used by the ESP32 photo frame:
/// - 3 bits for red (values 0-7)
/// - 3 bits for green (values 0-7)
/// - 2 bits for blue (values 0-3)
///
/// The format is: RRRGGGBB (8 bits total per pixel)
///
/// This matches the logic from bmp2cpp/src/main.rs:217-218:
/// ```ignore
/// let color8 = ((color.r() / 32) << 5) + ((color.g() / 32) << 2) + (color.b() / 64);
/// ```
pub fn convert_to_esp32_binary(img: &RgbImage) -> anyhow::Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = (width * height) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    for pixel in img.pixels() {
        let color8 = rgb_to_esp32_color(pixel);
        binary_data.push(color8);
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Binary data size mismatch: expected {}, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}

/// Convert RGB pixel to drawDemoBitmap format (mode 1) for GDEP073E01
///
/// drawDemoBitmap() with mode=1 expects these specific byte values:
/// - 0x00 = BLACK   - RGB(0, 0, 0)
/// - 0xFF = WHITE   - RGB(255, 255, 255)
/// - 0xFC = YELLOW  - RGB(252, 252, 0)
/// - 0xE0 = RED     - RGB(252, 0, 0)
/// - 0x03 = BLUE    - RGB(0, 0, 255)
/// - 0x1C = GREEN   - RGB(0, 252, 0)
///
/// This function maps the 6-color palette RGB values to these byte values.
/// Any unrecognized color defaults to BLACK (0x00).
pub fn rgb_to_demo_bitmap_mode1(pixel: &Rgb<u8>) -> u8 {
    let r = pixel[0];
    let g = pixel[1];
    let b = pixel[2];

    // Match against the 6-color palette with tolerance
    // White: RGB(255, 255, 255)
    if r >= 250 && g >= 250 && b >= 250 {
        return 0xFF;
    }

    // Yellow: RGB(252, 252, 0) or similar
    if r >= 250 && g >= 250 && b < 10 {
        return 0xFC;
    }

    // Red: RGB(252, 0, 0) or similar
    if r >= 250 && g < 10 && b < 10 {
        return 0xE0;
    }

    // Green: RGB(0, 252, 0) or similar
    if r < 10 && g >= 250 && b < 10 {
        return 0x1C;
    }

    // Blue: RGB(0, 0, 255) or similar
    if r < 10 && g < 10 && b >= 250 {
        return 0x03;
    }

    // Black: RGB(0, 0, 0)
    if r < 10 && g < 10 && b < 10 {
        return 0x00;
    }

    // Default to black for any other color
    0x00
}

/// Convert image to drawDemoBitmap format (mode 1) - 1 byte per pixel
///
/// This format is specifically for GDEP073E01 6-color displays using drawDemoBitmap()
/// with mode=1. The output is 384000 bytes for a 800x480 image (1 byte per pixel).
///
/// # Arguments
/// * `img` - Input RGB image (must be already dithered to 6-color palette)
///
/// # Returns
/// * `Result<Vec<u8>>` - Binary data in mode 1 format
pub fn convert_to_demo_bitmap_mode1(img: &RgbImage) -> anyhow::Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = (width * height) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    for pixel in img.pixels() {
        let color_byte = rgb_to_demo_bitmap_mode1(pixel);
        binary_data.push(color_byte);
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Binary data size mismatch: expected {}, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}

/// Convert an image to demo bitmap mode 1 using only black/white codes.
/// This ensures BW output uses the same byte values as the 6-color path
/// (0x00 for black, 0xFF for white).
pub fn convert_bw_to_demo_bitmap_mode1(img: &RgbImage) -> anyhow::Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = (width * height) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    for pixel in img.pixels() {
        // Luminance threshold: treat mid-gray as white to match display defaults
        let r = pixel[0] as f32;
        let g = pixel[1] as f32;
        let b = pixel[2] as f32;
        let lum = 0.299 * r + 0.587 * g + 0.114 * b;
        binary_data.push(if lum >= 128.0 { 0xFF } else { 0x00 });
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Binary data size mismatch: expected {}, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}

/// Decode image bytes and convert using the given `ProcessingType`.
/// Returns (payload, width, height) on success, or `None` on decode error.
pub fn convert_image_from_bytes(
    image_bytes: &[u8],
    dtype: DisplayType,
) -> Option<(Vec<u8>, u32, u32)> {
    match image::load_from_memory(image_bytes) {
        Ok(img) => {
            let rgb = img.to_rgb8();
            let (w, h) = rgb.dimensions();
            match process_image_with_display_type(&rgb, dtype) {
                Ok(payload) => Some((payload, w, h)),
                Err(_) => None,
            }
        }
        Err(_) => None,
    }
}

/// Process an already-decoded `RgbImage` using the requested `DisplayType`.
///
/// - If `DisplayType::BlackAndWhite` is requested the function returns the
///   mode-1 demo bitmap payload (0x00 black, 0xFF white) to match the 6-color
///   demo format.
/// - If `DisplayType::SixColors` is requested the function returns the demo-mode
///   payload produced by `convert_to_demo_bitmap_mode1`.
pub fn process_image_with_display_type(
    img: &RgbImage,
    dtype: DisplayType,
) -> anyhow::Result<Vec<u8>> {
    match dtype {
        DisplayType::BlackAndWhite => convert_bw_to_demo_bitmap_mode1(img),
        DisplayType::SixColors => convert_to_demo_bitmap_mode1(img),
    }
}

