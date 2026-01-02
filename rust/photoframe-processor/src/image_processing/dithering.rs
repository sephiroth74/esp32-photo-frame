/// Additional dithering algorithms for e-paper displays
///
/// This module implements Atkinson, Stucki, and Jarvis-Judice-Ninke dithering algorithms
/// as alternatives to the standard Floyd-Steinberg dithering.
use anyhow::Result;
use image::{Rgb, RgbImage};

use super::convert_improved::{create_working_buffers, find_closest_color_weighted};

/// Atkinson dithering - preserves brightness, good for photos
///
/// Diffuses only 75% of error (6/8), creating lighter results that preserve
/// the original image brightness better than Floyd-Steinberg.
///
/// Error distribution pattern:
/// ```text
///         *   1/8  1/8
///     1/8 1/8  1/8
///         1/8
/// ```
///
/// The dither_strength parameter (0.0-2.0) controls error diffusion strength.
pub fn apply_atkinson_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
    dither_strength: f32,
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            let error_r = (current_r - (palette_r as f32)) * dither_strength;
            let error_g = (current_g - (palette_g as f32)) * dither_strength;
            let error_b = (current_b - (palette_b as f32)) * dither_strength;

            distribute_atkinson_error(
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

fn distribute_atkinson_error(
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
    let factor = 1.0 / 8.0;
    let x_idx = x as usize;
    let y_idx = y as usize;

    // Current row: x+1, x+2
    if x + 1 < width {
        working_r[y_idx][x_idx + 1] += error_r * factor;
        working_g[y_idx][x_idx + 1] += error_g * factor;
        working_b[y_idx][x_idx + 1] += error_b * factor;
    }
    if x + 2 < width {
        working_r[y_idx][x_idx + 2] += error_r * factor;
        working_g[y_idx][x_idx + 2] += error_g * factor;
        working_b[y_idx][x_idx + 2] += error_b * factor;
    }

    // Next row: x-1, x, x+1
    if y + 1 < height {
        if x > 0 {
            working_r[y_idx + 1][x_idx - 1] += error_r * factor;
            working_g[y_idx + 1][x_idx - 1] += error_g * factor;
            working_b[y_idx + 1][x_idx - 1] += error_b * factor;
        }
        working_r[y_idx + 1][x_idx] += error_r * factor;
        working_g[y_idx + 1][x_idx] += error_g * factor;
        working_b[y_idx + 1][x_idx] += error_b * factor;
        if x + 1 < width {
            working_r[y_idx + 1][x_idx + 1] += error_r * factor;
            working_g[y_idx + 1][x_idx + 1] += error_g * factor;
            working_b[y_idx + 1][x_idx + 1] += error_b * factor;
        }
    }

    // Row y+2: x only
    if y + 2 < height {
        working_r[y_idx + 2][x_idx] += error_r * factor;
        working_g[y_idx + 2][x_idx] += error_g * factor;
        working_b[y_idx + 2][x_idx] += error_b * factor;
    }
}

/// Stucki dithering - reduces visible patterns with wide diffusion
///
/// Uses a 5×3 matrix for error diffusion, distributing error over a wider area
/// than Floyd-Steinberg, which helps reduce visible dithering patterns.
///
/// Error distribution pattern:
/// ```text
///             *   8/42  4/42
///     2/42  4/42  8/42  4/42  2/42
///     1/42  2/42  4/42  2/42  1/42
/// ```
///
/// The dither_strength parameter (0.0-2.0) controls error diffusion strength.
pub fn apply_stucki_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
    dither_strength: f32,
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            let error_r = (current_r - (palette_r as f32)) * dither_strength;
            let error_g = (current_g - (palette_g as f32)) * dither_strength;
            let error_b = (current_b - (palette_b as f32)) * dither_strength;

            distribute_stucki_error(
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

#[allow(clippy::too_many_arguments)]
fn distribute_stucki_error(
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
    let x_idx = x as usize;
    let y_idx = y as usize;
    let divisor = 42.0;

    // Current row
    if x + 1 < width {
        let factor = 8.0 / divisor;
        working_r[y_idx][x_idx + 1] += error_r * factor;
        working_g[y_idx][x_idx + 1] += error_g * factor;
        working_b[y_idx][x_idx + 1] += error_b * factor;
    }
    if x + 2 < width {
        let factor = 4.0 / divisor;
        working_r[y_idx][x_idx + 2] += error_r * factor;
        working_g[y_idx][x_idx + 2] += error_g * factor;
        working_b[y_idx][x_idx + 2] += error_b * factor;
    }

    // Next row
    if y + 1 < height {
        let offsets_and_factors = [
            (-2, 2.0 / divisor),
            (-1, 4.0 / divisor),
            (0, 8.0 / divisor),
            (1, 4.0 / divisor),
            (2, 2.0 / divisor),
        ];

        for (offset, factor) in offsets_and_factors {
            let target_x = x as i32 + offset;
            if target_x >= 0 && (target_x as u32) < width {
                let target_x_idx = target_x as usize;
                working_r[y_idx + 1][target_x_idx] += error_r * factor;
                working_g[y_idx + 1][target_x_idx] += error_g * factor;
                working_b[y_idx + 1][target_x_idx] += error_b * factor;
            }
        }
    }

    // Row y+2
    if y + 2 < height {
        let offsets_and_factors = [
            (-2, 1.0 / divisor),
            (-1, 2.0 / divisor),
            (0, 4.0 / divisor),
            (1, 2.0 / divisor),
            (2, 1.0 / divisor),
        ];

        for (offset, factor) in offsets_and_factors {
            let target_x = x as i32 + offset;
            if target_x >= 0 && (target_x as u32) < width {
                let target_x_idx = target_x as usize;
                working_r[y_idx + 2][target_x_idx] += error_r * factor;
                working_g[y_idx + 2][target_x_idx] += error_g * factor;
                working_b[y_idx + 2][target_x_idx] += error_b * factor;
            }
        }
    }
}

/// Jarvis-Judice-Ninke dithering - best for detailed photographs
///
/// Uses a 5×3 matrix with different weights than Stucki, creating very smooth
/// results that are particularly well-suited for photographic images with lots of detail.
///
/// Error distribution pattern:
/// ```text
///             *   7/48  5/48
///     3/48  5/48  7/48  5/48  3/48
///     1/48  3/48  5/48  3/48  1/48
/// ```
///
/// The dither_strength parameter (0.0-2.0) controls error diffusion strength.
pub fn apply_jarvis_judice_ninke_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
    dither_strength: f32,
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            let error_r = (current_r - (palette_r as f32)) * dither_strength;
            let error_g = (current_g - (palette_g as f32)) * dither_strength;
            let error_b = (current_b - (palette_b as f32)) * dither_strength;

            distribute_jjn_error(
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

#[allow(clippy::too_many_arguments)]
fn distribute_jjn_error(
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
    let x_idx = x as usize;
    let y_idx = y as usize;
    let divisor = 48.0;

    // Current row
    if x + 1 < width {
        let factor = 7.0 / divisor;
        working_r[y_idx][x_idx + 1] += error_r * factor;
        working_g[y_idx][x_idx + 1] += error_g * factor;
        working_b[y_idx][x_idx + 1] += error_b * factor;
    }
    if x + 2 < width {
        let factor = 5.0 / divisor;
        working_r[y_idx][x_idx + 2] += error_r * factor;
        working_g[y_idx][x_idx + 2] += error_g * factor;
        working_b[y_idx][x_idx + 2] += error_b * factor;
    }

    // Next row
    if y + 1 < height {
        let offsets_and_factors = [
            (-2, 3.0 / divisor),
            (-1, 5.0 / divisor),
            (0, 7.0 / divisor),
            (1, 5.0 / divisor),
            (2, 3.0 / divisor),
        ];

        for (offset, factor) in offsets_and_factors {
            let target_x = x as i32 + offset;
            if target_x >= 0 && (target_x as u32) < width {
                let target_x_idx = target_x as usize;
                working_r[y_idx + 1][target_x_idx] += error_r * factor;
                working_g[y_idx + 1][target_x_idx] += error_g * factor;
                working_b[y_idx + 1][target_x_idx] += error_b * factor;
            }
        }
    }

    // Row y+2
    if y + 2 < height {
        let offsets_and_factors = [
            (-2, 1.0 / divisor),
            (-1, 3.0 / divisor),
            (0, 5.0 / divisor),
            (1, 3.0 / divisor),
            (2, 1.0 / divisor),
        ];

        for (offset, factor) in offsets_and_factors {
            let target_x = x as i32 + offset;
            if target_x >= 0 && (target_x as u32) < width {
                let target_x_idx = target_x as usize;
                working_r[y_idx + 2][target_x_idx] += error_r * factor;
                working_g[y_idx + 2][target_x_idx] += error_g * factor;
                working_b[y_idx + 2][target_x_idx] += error_b * factor;
            }
        }
    }
}
