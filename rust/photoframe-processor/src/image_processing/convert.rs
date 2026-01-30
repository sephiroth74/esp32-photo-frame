use anyhow::Result;
use image::{Rgb, RgbImage};
use photoframe_lib::dithering::apply_floyd_steinberg_dithering;
use photoframe_lib::{DisplayType, DitheringMethod, apply_dithering};

/// Process an image with color conversion and dithering
///
/// This applies the appropriate color processing based on the processing type:
/// - BlackWhite: Convert to grayscale and apply Floyd-Steinberg dithering
/// - SixColor: Reduce to 6-color palette and apply dithering
///
/// The dither_strength parameter (0.0-2.0) controls the strength of error diffusion:
/// - 1.0 = normal strength (default)
/// - <1.0 = subtle dithering
/// - >1.0 = pronounced dithering
pub fn process_image(
    img: &RgbImage,
    processing_type: &DisplayType,
    dithering_method: &DitheringMethod,
    dither_strength: f32,
) -> Result<RgbImage> {
    match processing_type {
        DisplayType::BlackAndWhite => apply_bw_processing(img, dither_strength),
        DisplayType::SixColors => apply_6c_processing(img, dithering_method, dither_strength),
    }
}

/// Apply black & white processing with Floyd-Steinberg dithering
fn apply_bw_processing(img: &RgbImage, dither_strength: f32) -> Result<RgbImage> {
    // Convert to grayscale first
    let grayscale = convert_to_grayscale(img);
    // Apply Floyd-Steinberg dithering with strength
    apply_floyd_steinberg_dithering(&grayscale, dither_strength)
}

/// Apply 6-color processing with selectable dithering method
fn apply_6c_processing(
    img: &RgbImage,
    dithering_method: &DitheringMethod,
    dither_strength: f32,
) -> Result<RgbImage> {
    apply_dithering(
        img,
        *dithering_method,
        DisplayType::SixColors,
        dither_strength,
    )
    //match dithering_method {
    //    DitheringMethod::FloydSteinberg => {
    //        super::convert_improved::apply_enhanced_floyd_steinberg_dithering(
    //            img,
    //            &SIX_COLOR_PALETTE,
    //            dither_strength,
    //        )
    //    }
    //    DitheringMethod::Atkinson => {
    //        super::dithering::apply_atkinson_dithering(img, &SIX_COLOR_PALETTE, dither_strength)
    //    }
    //    DitheringMethod::Stucki => {
    //        super::dithering::apply_stucki_dithering(img, &SIX_COLOR_PALETTE, dither_strength)
    //    }
    //    DitheringMethod::JarvisJudiceNinke => super::dithering::apply_jarvis_judice_ninke_dithering(
    //        img,
    //        &SIX_COLOR_PALETTE,
    //        dither_strength,
    //    ),
    //    DitheringMethod::Ordered => {
    //        // Ordered dithering doesn't use error diffusion, so strength doesn't apply
    //        super::convert_improved::apply_ordered_dithering(img, &SIX_COLOR_PALETTE)
    //    }
    //}
}

/// Convert RGB image to grayscale using luminance weights
fn convert_to_grayscale(img: &RgbImage) -> RgbImage {
    let (width, height) = img.dimensions();
    let mut grayscale = RgbImage::new(width, height);

    for (x, y, pixel) in img.enumerate_pixels() {
        // Use standard luminance weights: 0.299*R + 0.587*G + 0.114*B
        let r = pixel[0] as f32;
        let g = pixel[1] as f32;
        let b = pixel[2] as f32;

        let gray = (0.299 * r + 0.587 * g + 0.114 * b) as u8;

        grayscale.put_pixel(x, y, Rgb([gray, gray, gray]));
    }

    grayscale
}

/// Generic function to find the closest color in any palette
fn find_closest_color(r: u8, g: u8, b: u8, palette: &[(u8, u8, u8)]) -> (u8, u8, u8) {
    let mut min_distance = f32::MAX;
    let mut closest_color = palette[0];

    for &(pr, pg, pb) in palette {
        // Calculate Euclidean distance in RGB space
        let dr = (r as f32) - (pr as f32);
        let dg = (g as f32) - (pg as f32);
        let db = (b as f32) - (pb as f32);

        let distance = (dr * dr + dg * dg + db * db).sqrt();

        if distance < min_distance {
            min_distance = distance;
            closest_color = (pr, pg, pb);
        }
    }

    closest_color
}

/// Create working buffers for Floyd-Steinberg dithering
fn create_working_buffers(img: &RgbImage) -> (Vec<Vec<f32>>, Vec<Vec<f32>>, Vec<Vec<f32>>) {
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
            row_r.push(pixel[0] as f32);
            row_g.push(pixel[1] as f32);
            row_b.push(pixel[2] as f32);
        }

        working_r.push(row_r);
        working_g.push(row_g);
        working_b.push(row_b);
    }

    (working_r, working_g, working_b)
}

/// Apply Floyd-Steinberg error distribution to neighboring pixels
#[allow(dead_code)]
fn distribute_floyd_steinberg_error(
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
    // Floyd-Steinberg error distribution:
    //     * 7/16
    // 3/16 5/16 1/16

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

        if x > 0 {
            let prev_x = (x - 1) as usize;
            working_r[next_y][prev_x] += error_r * 3.0 / 16.0;
            working_g[next_y][prev_x] += error_g * 3.0 / 16.0;
            working_b[next_y][prev_x] += error_b * 3.0 / 16.0;
        }

        working_r[next_y][curr_x] += error_r * 5.0 / 16.0;
        working_g[next_y][curr_x] += error_g * 5.0 / 16.0;
        working_b[next_y][curr_x] += error_b * 5.0 / 16.0;

        if x + 1 < width {
            let next_x = (x + 1) as usize;
            working_r[next_y][next_x] += error_r * 1.0 / 16.0;
            working_g[next_y][next_x] += error_g * 1.0 / 16.0;
            working_b[next_y][next_x] += error_b * 1.0 / 16.0;
        }
    }
}

/// Generic Floyd-Steinberg dithering with any color palette
///
/// This implements Floyd-Steinberg error diffusion for any color palette
#[allow(dead_code)]
fn apply_floyd_steinberg_dithering_with_palette(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Create working buffers with error accumulation for each color channel
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    // Apply Floyd-Steinberg dithering with the given palette
    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let old_r = working_r[y_idx][x_idx];
            let old_g = working_g[y_idx][x_idx];
            let old_b = working_b[y_idx][x_idx];

            // Find closest color in palette
            let (new_r, new_g, new_b) = find_closest_color(
                old_r.clamp(0.0, 255.0) as u8,
                old_g.clamp(0.0, 255.0) as u8,
                old_b.clamp(0.0, 255.0) as u8,
                palette,
            );

            // Set the new pixel values
            working_r[y_idx][x_idx] = new_r as f32;
            working_g[y_idx][x_idx] = new_g as f32;
            working_b[y_idx][x_idx] = new_b as f32;

            // Calculate error for each channel
            let error_r = old_r - (new_r as f32);
            let error_g = old_g - (new_g as f32);
            let error_b = old_b - (new_b as f32);

            // Distribute error to neighboring pixels
            distribute_floyd_steinberg_error(
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

    // Convert back to image
    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;
            let r = working_r[y_idx][x_idx].clamp(0.0, 255.0) as u8;
            let g = working_g[y_idx][x_idx].clamp(0.0, 255.0) as u8;
            let b = working_b[y_idx][x_idx].clamp(0.0, 255.0) as u8;

            output.put_pixel(x, y, Rgb([r, g, b]));
        }
    }

    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;

    fn create_gradient_image(width: u32, height: u32) -> RgbImage {
        ImageBuffer::from_fn(width, height, |x, _| {
            let gray = (x * 255 / width) as u8;
            Rgb([gray, gray, gray])
        })
    }

    #[test]
    fn test_convert_to_grayscale() {
        let img = ImageBuffer::from_fn(2, 2, |x, y| {
            match (x, y) {
                (0, 0) => Rgb([255, 0, 0]),     // Red
                (1, 0) => Rgb([0, 255, 0]),     // Green
                (0, 1) => Rgb([0, 0, 255]),     // Blue
                (1, 1) => Rgb([255, 255, 255]), // White
                _ => unreachable!(),
            }
        });

        let grayscale = convert_to_grayscale(&img);

        // Check that all pixels are grayscale (R=G=B)
        for pixel in grayscale.pixels() {
            assert_eq!(pixel[0], pixel[1]);
            assert_eq!(pixel[1], pixel[2]);
        }
    }

    #[test]
    fn test_floyd_steinberg_dithering() {
        let img = create_gradient_image(10, 10);
        let dithered = apply_floyd_steinberg_dithering(&img, 1.0).unwrap();

        // Check that output only contains black (0) or white (255)
        for pixel in dithered.pixels() {
            assert!(pixel[0] == 0 || pixel[0] == 255);
            assert_eq!(pixel[0], pixel[1]);
            assert_eq!(pixel[1], pixel[2]);
        }
    }

    #[test]
    fn test_process_image_bw() {
        let img = create_gradient_image(10, 10);
        let result = process_image(
            &img,
            &DisplayType::BlackAndWhite,
            &DitheringMethod::FloydSteinberg,
            1.0,
        )
        .unwrap();

        // Should be dithered black and white
        for pixel in result.pixels() {
            assert!(pixel[0] == 0 || pixel[0] == 255);
        }
    }
}
