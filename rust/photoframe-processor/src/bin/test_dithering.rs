use anyhow::Result;
use clap::Parser;
use image::{self, RgbImage};
use std::path::PathBuf;

// Import the modules we need
mod convert_improved {
    include!("../image_processing/convert_improved.rs");
}

#[derive(Parser, Debug)]
#[command(name = "test_dithering")]
#[command(about = "Test and compare different dithering methods")]
struct Args {
    /// Input image path
    #[arg(short, long)]
    input: PathBuf,

    /// Output directory
    #[arg(short, long)]
    output: PathBuf,

    /// Method: "original" or "enhanced" or "ordered" or "all"
    #[arg(short, long, default_value = "all")]
    method: String,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Load the input image
    println!("Loading image: {:?}", args.input);
    let img = image::open(&args.input)?.to_rgb8();

    // Ensure output directory exists
    std::fs::create_dir_all(&args.output)?;

    // Get the base filename
    let stem = args
        .input
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image");

    match args.method.as_str() {
        "original" => process_original(&img, &args.output, stem)?,
        "enhanced" => process_enhanced(&img, &args.output, stem)?,
        "ordered" => process_ordered(&img, &args.output, stem)?,
        "all" => {
            process_original(&img, &args.output, stem)?;
            process_enhanced(&img, &args.output, stem)?;
            process_ordered(&img, &args.output, stem)?;
            process_preprocessed(&img, &args.output, stem)?;
        }
        _ => println!("Unknown method: {}", args.method),
    }

    println!("✅ Processing complete!");
    Ok(())
}

fn process_original(img: &RgbImage, output_dir: &PathBuf, stem: &str) -> Result<()> {
    println!("Processing with ORIGINAL Floyd-Steinberg method...");

    // This simulates the current implementation
    let result = apply_original_floyd_steinberg(img)?;

    let output_path = output_dir.join(format!("{}_original.bmp", stem));
    result.save(&output_path)?;
    println!("  Saved: {:?}", output_path);

    Ok(())
}

fn process_enhanced(img: &RgbImage, output_dir: &PathBuf, stem: &str) -> Result<()> {
    println!("Processing with ENHANCED Floyd-Steinberg method...");

    let result = convert_improved::apply_enhanced_floyd_steinberg_dithering(
        img,
        &convert_improved::SIX_COLOR_PALETTE,
        1.0, // Default dither strength
    )?;

    let output_path = output_dir.join(format!("{}_enhanced.bmp", stem));
    result.save(&output_path)?;
    println!("  Saved: {:?}", output_path);

    Ok(())
}

fn process_ordered(img: &RgbImage, output_dir: &PathBuf, stem: &str) -> Result<()> {
    println!("Processing with ORDERED dithering (Bayer matrix)...");

    let result =
        convert_improved::apply_ordered_dithering(img, &convert_improved::SIX_COLOR_PALETTE)?;

    let output_path = output_dir.join(format!("{}_ordered.bmp", stem));
    result.save(&output_path)?;
    println!("  Saved: {:?}", output_path);

    Ok(())
}

fn process_preprocessed(img: &RgbImage, output_dir: &PathBuf, stem: &str) -> Result<()> {
    println!("Processing with ENHANCED + preprocessing...");

    // Apply preprocessing first
    let preprocessed = convert_improved::preprocess_for_dithering(img);

    // Then apply enhanced dithering
    let result = convert_improved::apply_enhanced_floyd_steinberg_dithering(
        &preprocessed,
        &convert_improved::SIX_COLOR_PALETTE,
        1.0, // Default dither strength
    )?;

    let output_path = output_dir.join(format!("{}_preprocessed.bmp", stem));
    result.save(&output_path)?;
    println!("  Saved: {:?}", output_path);

    Ok(())
}

// Simulated original method for comparison
fn apply_original_floyd_steinberg(img: &RgbImage) -> Result<RgbImage> {
    use image::Rgb;

    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);

    // Create working buffers
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

    // Apply Floyd-Steinberg dithering with direct palette mapping
    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let old_r = working_r[y_idx][x_idx];
            let old_g = working_g[y_idx][x_idx];
            let old_b = working_b[y_idx][x_idx];

            // Find closest color in palette (simple Euclidean distance)
            let (new_r, new_g, new_b) = find_closest_color_simple(
                old_r.clamp(0.0, 255.0) as u8,
                old_g.clamp(0.0, 255.0) as u8,
                old_b.clamp(0.0, 255.0) as u8,
            );

            // Set the new pixel values
            working_r[y_idx][x_idx] = new_r as f32;
            working_g[y_idx][x_idx] = new_g as f32;
            working_b[y_idx][x_idx] = new_b as f32;

            // Calculate error
            let error_r = old_r - (new_r as f32);
            let error_g = old_g - (new_g as f32);
            let error_b = old_b - (new_b as f32);

            // Distribute error using Floyd-Steinberg weights
            if x + 1 < width {
                working_r[y_idx][(x + 1) as usize] += error_r * 7.0 / 16.0;
                working_g[y_idx][(x + 1) as usize] += error_g * 7.0 / 16.0;
                working_b[y_idx][(x + 1) as usize] += error_b * 7.0 / 16.0;
            }

            if y + 1 < height {
                let next_y = (y + 1) as usize;

                if x > 0 {
                    working_r[next_y][(x - 1) as usize] += error_r * 3.0 / 16.0;
                    working_g[next_y][(x - 1) as usize] += error_g * 3.0 / 16.0;
                    working_b[next_y][(x - 1) as usize] += error_b * 3.0 / 16.0;
                }

                working_r[next_y][x_idx] += error_r * 5.0 / 16.0;
                working_g[next_y][x_idx] += error_g * 5.0 / 16.0;
                working_b[next_y][x_idx] += error_b * 5.0 / 16.0;

                if x + 1 < width {
                    working_r[next_y][(x + 1) as usize] += error_r * 1.0 / 16.0;
                    working_g[next_y][(x + 1) as usize] += error_g * 1.0 / 16.0;
                    working_b[next_y][(x + 1) as usize] += error_b * 1.0 / 16.0;
                }
            }
        }
    }

    // Convert back to image
    for y in 0..height {
        for x in 0..width {
            let r = working_r[y as usize][x as usize].clamp(0.0, 255.0) as u8;
            let g = working_g[y as usize][x as usize].clamp(0.0, 255.0) as u8;
            let b = working_b[y as usize][x as usize].clamp(0.0, 255.0) as u8;

            output.put_pixel(x, y, Rgb([r, g, b]));
        }
    }

    Ok(output)
}

fn find_closest_color_simple(r: u8, g: u8, b: u8) -> (u8, u8, u8) {
    let palette = convert_improved::SIX_COLOR_PALETTE;
    let mut min_distance = f32::MAX;
    let mut closest_color = palette[0];

    for &(pr, pg, pb) in &palette {
        // Simple Euclidean distance
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
