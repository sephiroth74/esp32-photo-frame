use anyhow::Result;
use image::{Rgb, RgbImage};

/// Enhanced Floyd-Steinberg dithering that simulates a full color range
/// using only the limited e-paper palette
///
/// The dither_strength parameter (0.0-2.0) controls error diffusion strength:
/// - 1.0 = normal (default)
/// - <1.0 = subtle dithering
/// - >1.0 = pronounced dithering
pub fn apply_enhanced_floyd_steinberg_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
    dither_strength: f32,
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Create working buffers - keep original image values
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    // Process each pixel
    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            // Get current pixel values (with accumulated error)
            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            // Find closest color in the limited palette
            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            // Set output pixel to palette color
            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            // Calculate error (difference between desired and actual color) multiplied by strength
            let error_r = (current_r - (palette_r as f32)) * dither_strength;
            let error_g = (current_g - (palette_g as f32)) * dither_strength;
            let error_b = (current_b - (palette_b as f32)) * dither_strength;

            // Distribute error to neighboring pixels using Floyd-Steinberg weights
            distribute_error_floyd_steinberg(
                &mut working_r,
                &mut working_g,
                &mut working_b,
                x,
                y,
                width,
                height,
                error_r,
                error_g,
                error_b,
            );
        }
    }

    Ok(output)
}

/// Enhanced color matching using perceptual color distance
/// This uses weighted Euclidean distance based on human color perception
pub fn find_closest_color_weighted(r: u8, g: u8, b: u8, palette: &[(u8, u8, u8)]) -> (u8, u8, u8) {
    let mut min_distance = f32::MAX;
    let mut closest_color = palette[0];

    for &(pr, pg, pb) in palette {
        // Use weighted distance based on human color perception
        // Red: 30%, Green: 59%, Blue: 11% (similar to luminance weights)
        let dr = (r as f32) - (pr as f32);
        let dg = (g as f32) - (pg as f32);
        let db = (b as f32) - (pb as f32);

        // Weighted Euclidean distance
        let distance = (dr * dr * 0.3 + dg * dg * 0.59 + db * db * 0.11).sqrt();

        if distance < min_distance {
            min_distance = distance;
            closest_color = (pr, pg, pb);
        }
    }

    closest_color
}

/// Create working buffers from the original image
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

/// Apply inverse gamma correction
#[allow(dead_code)]
fn apply_inverse_gamma(value: f32) -> f32 {
    let normalized = value / 255.0;
    let corrected = normalized.powf(1.0 / 2.2);
    corrected * 255.0
}

/// Distribute error using Floyd-Steinberg algorithm
fn distribute_error_floyd_steinberg(
    working_r: &mut [Vec<f32>],
    working_g: &mut [Vec<f32>],
    working_b: &mut [Vec<f32>],
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    error_r: f32,
    error_g: f32,
    error_b: f32,
) {
    // Floyd-Steinberg error distribution matrix:
    //     * 7/16
    // 3/16 5/16 1/16

    // Right pixel (7/16 of error)
    if x + 1 < width {
        let next_x = (x + 1) as usize;
        let curr_y = y as usize;
        working_r[curr_y][next_x] += error_r * 7.0 / 16.0;
        working_g[curr_y][next_x] += error_g * 7.0 / 16.0;
        working_b[curr_y][next_x] += error_b * 7.0 / 16.0;
    }

    if y + 1 < height {
        let next_y = (y + 1) as usize;
        let curr_x = x as usize;

        // Bottom-left pixel (3/16 of error)
        if x > 0 {
            let prev_x = (x - 1) as usize;
            working_r[next_y][prev_x] += error_r * 3.0 / 16.0;
            working_g[next_y][prev_x] += error_g * 3.0 / 16.0;
            working_b[next_y][prev_x] += error_b * 3.0 / 16.0;
        }

        // Bottom pixel (5/16 of error)
        working_r[next_y][curr_x] += error_r * 5.0 / 16.0;
        working_g[next_y][curr_x] += error_g * 5.0 / 16.0;
        working_b[next_y][curr_x] += error_b * 5.0 / 16.0;

        // Bottom-right pixel (1/16 of error)
        if x + 1 < width {
            let next_x = (x + 1) as usize;
            working_r[next_y][next_x] += error_r * 1.0 / 16.0;
            working_g[next_y][next_x] += error_g * 1.0 / 16.0;
            working_b[next_y][next_x] += error_b * 1.0 / 16.0;
        }
    }
}

/// Alternative: Ordered dithering (Bayer matrix) for comparison
/// This can produce less "wormy" patterns than Floyd-Steinberg in some cases
pub fn apply_ordered_dithering(img: &RgbImage, palette: &[(u8, u8, u8)]) -> Result<RgbImage> {
    // 8x8 Bayer dithering matrix
    const BAYER_8X8: [[f32; 8]; 8] = [
        [
            0.0 / 64.0,
            48.0 / 64.0,
            12.0 / 64.0,
            60.0 / 64.0,
            3.0 / 64.0,
            51.0 / 64.0,
            15.0 / 64.0,
            63.0 / 64.0,
        ],
        [
            32.0 / 64.0,
            16.0 / 64.0,
            44.0 / 64.0,
            28.0 / 64.0,
            35.0 / 64.0,
            19.0 / 64.0,
            47.0 / 64.0,
            31.0 / 64.0,
        ],
        [
            8.0 / 64.0,
            56.0 / 64.0,
            4.0 / 64.0,
            52.0 / 64.0,
            11.0 / 64.0,
            59.0 / 64.0,
            7.0 / 64.0,
            55.0 / 64.0,
        ],
        [
            40.0 / 64.0,
            24.0 / 64.0,
            36.0 / 64.0,
            20.0 / 64.0,
            43.0 / 64.0,
            27.0 / 64.0,
            39.0 / 64.0,
            23.0 / 64.0,
        ],
        [
            2.0 / 64.0,
            50.0 / 64.0,
            14.0 / 64.0,
            62.0 / 64.0,
            1.0 / 64.0,
            49.0 / 64.0,
            13.0 / 64.0,
            61.0 / 64.0,
        ],
        [
            34.0 / 64.0,
            18.0 / 64.0,
            46.0 / 64.0,
            30.0 / 64.0,
            33.0 / 64.0,
            17.0 / 64.0,
            45.0 / 64.0,
            29.0 / 64.0,
        ],
        [
            10.0 / 64.0,
            58.0 / 64.0,
            6.0 / 64.0,
            54.0 / 64.0,
            9.0 / 64.0,
            57.0 / 64.0,
            5.0 / 64.0,
            53.0 / 64.0,
        ],
        [
            42.0 / 64.0,
            26.0 / 64.0,
            38.0 / 64.0,
            22.0 / 64.0,
            41.0 / 64.0,
            25.0 / 64.0,
            37.0 / 64.0,
            21.0 / 64.0,
        ],
    ];

    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let pixel = img.get_pixel(x, y);

            // Get threshold from Bayer matrix
            let threshold = BAYER_8X8[(y % 8) as usize][(x % 8) as usize];

            // Add threshold to pixel values (centered around 0.5)
            let r = (pixel[0] as f32 / 255.0 + threshold - 0.5).clamp(0.0, 1.0) * 255.0;
            let g = (pixel[1] as f32 / 255.0 + threshold - 0.5).clamp(0.0, 1.0) * 255.0;
            let b = (pixel[2] as f32 / 255.0 + threshold - 0.5).clamp(0.0, 1.0) * 255.0;

            // Find closest color in palette
            let (new_r, new_g, new_b) =
                find_closest_color_weighted(r as u8, g as u8, b as u8, palette);

            output.put_pixel(x, y, Rgb([new_r, new_g, new_b]));
        }
    }

    Ok(output)
}

/// Apply pre-processing to enhance image before dithering
#[allow(dead_code)]
pub fn preprocess_for_dithering(img: &RgbImage) -> RgbImage {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let pixel = img.get_pixel(x, y);

            // Increase saturation slightly to compensate for dithering
            let (h, s, v) = rgb_to_hsv(pixel[0], pixel[1], pixel[2]);
            let enhanced_s = (s * 1.2).min(1.0); // Boost saturation by 20%
            let (r, g, b) = hsv_to_rgb(h, enhanced_s, v);

            output.put_pixel(x, y, Rgb([r, g, b]));
        }
    }

    output
}

/// Convert RGB to HSV color space
#[allow(dead_code)]
fn rgb_to_hsv(r: u8, g: u8, b: u8) -> (f32, f32, f32) {
    let r = r as f32 / 255.0;
    let g = g as f32 / 255.0;
    let b = b as f32 / 255.0;

    let max = r.max(g).max(b);
    let min = r.min(g).min(b);
    let delta = max - min;

    let h = if delta == 0.0 {
        0.0
    } else if max == r {
        60.0 * (((g - b) / delta) % 6.0)
    } else if max == g {
        60.0 * (((b - r) / delta) + 2.0)
    } else {
        60.0 * (((r - g) / delta) + 4.0)
    };

    let s = if max == 0.0 { 0.0 } else { delta / max };
    let v = max;

    (h, s, v)
}

/// Convert HSV back to RGB
#[allow(dead_code)]
fn hsv_to_rgb(h: f32, s: f32, v: f32) -> (u8, u8, u8) {
    let c = v * s;
    let x = c * (1.0 - ((h / 60.0) % 2.0 - 1.0).abs());
    let m = v - c;

    let (r, g, b) = if h < 60.0 {
        (c, x, 0.0)
    } else if h < 120.0 {
        (x, c, 0.0)
    } else if h < 180.0 {
        (0.0, c, x)
    } else if h < 240.0 {
        (0.0, x, c)
    } else if h < 300.0 {
        (x, 0.0, c)
    } else {
        (c, 0.0, x)
    };

    (
        ((r + m) * 255.0) as u8,
        ((g + m) * 255.0) as u8,
        ((b + m) * 255.0) as u8,
    )
}

/// 6-color palette for e-paper displays
#[allow(dead_code)]
pub const SIX_COLOR_PALETTE: [(u8, u8, u8); 6] = [
    (0, 0, 0),       // Black
    (255, 255, 255), // White
    (252, 0, 0),     // Red
    (0, 252, 0),     // Green
    (0, 0, 255),     // Blue
    (252, 252, 0),   // Yellow
];
