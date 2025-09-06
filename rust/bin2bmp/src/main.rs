use anyhow::{Context, Result};
use clap::{Parser, ValueEnum};
use image::{ImageBuffer, Rgb, RgbImage};
use indicatif::{ProgressBar, ProgressStyle};
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};

#[derive(Parser)]
#[command(
    name = "bin2bmp",
    about = "Convert ESP32 photo frame .bin files back to images",
    long_about = "Convert binary image files used by ESP32 photo frame back to viewable BMP, JPEG, or PNG images.\n\nThe output filename is automatically generated based on the input filename and format.\nExample: input.bin + BMP format = input.bmp\n\nThe binary format stores one byte per pixel with specific color mappings based on display type.\nDefault image size is 800x480 pixels (common photo frame resolution)."
)]
struct Args {
    /// Input .bin file
    #[arg(short, long, value_name = "FILE")]
    input: String,

    /// Output directory (filename will be auto-generated)
    #[arg(short, long, value_name = "DIR")]
    output: Option<String>,

    /// Image size in pixels (format: WIDTHxHEIGHT)
    #[arg(short, long, default_value = "800x480")]
    size: String,

    /// Display type
    #[arg(short = 't', long, default_value = "bw")]
    display_type: DisplayType,

    /// Output format
    #[arg(short, long, default_value = "bmp")]
    format: OutputFormat,

    /// Enable verbose output
    #[arg(short, long)]
    verbose: bool,

    /// Only validate the .bin file, don't convert
    #[arg(long)]
    validate_only: bool,
}

#[derive(Clone, ValueEnum)]
enum DisplayType {
    /// Black & White display (DISP_BW_V2)
    Bw,
    /// 6-color display (DISP_6C)
    #[value(name = "6c")]
    SixColor,
}

#[derive(Debug, Clone, ValueEnum)]
enum OutputFormat {
    /// BMP format
    Bmp,
    /// JPEG format  
    Jpeg,
    /// PNG format
    Png,
}

impl DisplayType {
    fn description(&self) -> &'static str {
        match self {
            DisplayType::Bw => "Black & White",
            DisplayType::SixColor => "6-color",
        }
    }
}

impl OutputFormat {
    fn extension(&self) -> &'static str {
        match self {
            OutputFormat::Bmp => "bmp",
            OutputFormat::Jpeg => "jpg", 
            OutputFormat::Png => "png",
        }
    }
}

fn pixel_to_rgb(pixel: u8, display_type: &DisplayType) -> [u8; 3] {
    match display_type {
        DisplayType::Bw => {
            // Black & White mapping (based on renderer.cpp:1378-1382)
            if pixel > 127 {
                [255, 255, 255] // White
            } else {
                [0, 0, 0] // Black
            }
        }
        DisplayType::SixColor => {
            // 6-color mapping (based on renderer.cpp:1296-1320)
            match pixel {
                255 => [255, 255, 255], // White
                0 => [0, 0, 0],          // Black
                224 => [255, 0, 0],      // Red
                28 => [0, 255, 0],       // Green
                252 => [255, 255, 0],    // Yellow
                3 => [0, 0, 255],        // Blue
                _ => {
                    // Fallback for dithered or unexpected values
                    [pixel, pixel, pixel] // Grayscale
                }
            }
        }
    }
}

fn parse_size(size_str: &str) -> Result<(u32, u32)> {
    let parts: Vec<&str> = size_str.split('x').collect();
    if parts.len() != 2 {
        return Err(anyhow::anyhow!(
            "Invalid size format '{}'. Expected format: WIDTHxHEIGHT (e.g., 800x480)", 
            size_str
        ));
    }
    
    let width: u32 = parts[0].parse()
        .with_context(|| format!("Invalid width '{}' in size '{}'", parts[0], size_str))?;
    let height: u32 = parts[1].parse()
        .with_context(|| format!("Invalid height '{}' in size '{}'", parts[1], size_str))?;
    
    if width == 0 || height == 0 {
        return Err(anyhow::anyhow!("Width and height must be greater than 0"));
    }
    
    Ok((width, height))
}

fn construct_output_path(input_path: &str, output_dir: Option<&str>, format: &OutputFormat) -> Result<PathBuf> {
    // Extract the input filename without extension
    let input_path = Path::new(input_path);
    let input_stem = input_path.file_stem()
        .ok_or_else(|| anyhow::anyhow!("Cannot extract filename from input path"))?
        .to_string_lossy();
    
    // Determine output directory
    let output_dir = match output_dir {
        Some(dir) => PathBuf::from(dir),
        None => std::env::current_dir()
            .with_context(|| "Cannot determine current directory")?,
    };
    
    // Create output directory if it doesn't exist
    if !output_dir.exists() {
        fs::create_dir_all(&output_dir)
            .with_context(|| format!("Failed to create output directory '{}'", output_dir.display()))?;
    }
    
    // Construct the output filename
    let output_filename = format!("{}.{}", input_stem, format.extension());
    let output_path = output_dir.join(output_filename);
    
    Ok(output_path)
}

fn validate_file(args: &Args, width: u32, height: u32) -> Result<Vec<u8>> {
    let path = Path::new(&args.input);
    if !path.exists() {
        return Err(anyhow::anyhow!("Input file '{}' does not exist", args.input));
    }

    let mut file = File::open(path)
        .with_context(|| format!("Failed to open input file '{}'", args.input))?;

    let mut data = Vec::new();
    file.read_to_end(&mut data)
        .with_context(|| "Failed to read input file")?;

    let expected_size = (width * height) as usize;

    if args.verbose {
        println!("=== Binary File Analysis ===");
        println!("Input file: {}", args.input);
        println!("Specified dimensions: {}x{}", width, height);
        println!("Display type: {}", args.display_type.description());
        println!("Expected size: {} bytes", expected_size);
        println!("Actual size: {} bytes", data.len());
        println!();
    }

    if data.len() != expected_size {
        let mut error_lines = vec![
            format!("File size does not match specified dimensions!"),
            format!("  Specified: {}x{} (expected {} bytes)", width, height, expected_size),
            format!("  Actual file size: {} bytes", data.len()),
            String::new(),
            format!("Possible correct dimensions for this file ({} bytes):", data.len()),
        ];

        // Common dimensions
        let common_dims = vec![
            (800, 480), (640, 384), (480, 800), (384, 640),
            (1200, 825), (825, 1200), (1448, 1072), (1072, 1448),
        ];

        let mut found_match = false;
        for (w, h) in common_dims {
            if (w * h) as usize == data.len() {
                error_lines.push(format!("  {}x{}", w, h));
                found_match = true;
            }
        }

        if !found_match {
            // Calculate some reasonable dimensions
            for w in [100, 200, 300, 400, 480, 600, 640, 800, 1024, 1200, 1448] {
                if data.len() % w == 0 {
                    let h = data.len() / w;
                    if h > 100 && h < 2000 {
                        error_lines.push(format!("  {}x{}", w, h));
                    }
                }
            }
        }

        return Err(anyhow::anyhow!("{}", error_lines.join("\n")));
    }

    println!("✓ File size validation passed ({}x{} = {} bytes)", width, height, expected_size);

    // Analyze pixel distribution
    if args.verbose {
        let sample_size = std::cmp::min(1000, data.len());
        let sample = &data[..sample_size];

        let pb = ProgressBar::new(sample_size as u64);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("{spinner:.green} Analyzing pixel distribution [{wide_bar:.cyan/blue}] {pos}/{len} bytes")
                .unwrap_or_else(|_| ProgressStyle::default_bar())
                .progress_chars("#>-")
        );

        let mut unique_values: Vec<u8> = Vec::new();
        for (i, &byte) in sample.iter().enumerate() {
            if !unique_values.contains(&byte) {
                unique_values.push(byte);
            }
            if i % 50 == 0 {
                pb.set_position(i as u64);
            }
        }
        
        pb.finish_and_clear();
        unique_values.sort();

        println!("Found {} unique pixel values in sample of {} bytes", unique_values.len(), sample_size);

        let show_count = std::cmp::min(20, unique_values.len());
        print!("Sample values: ");
        for (i, val) in unique_values.iter().take(show_count).enumerate() {
            if i > 0 { print!(", "); }
            print!("{}", val);
        }
        if unique_values.len() > show_count {
            print!(" ... and {} more", unique_values.len() - show_count);
        }
        println!();

        // Validate based on display type
        match args.display_type {
            DisplayType::Bw => {
                println!("✓ Binary validation for B&W display");
            }
            DisplayType::SixColor => {
                let expected_6c_values = vec![0, 3, 28, 224, 252, 255];
                println!("Expected 6-color values: {:?}", expected_6c_values);

                let unexpected: Vec<u8> = unique_values.iter()
                    .filter(|&val| !expected_6c_values.contains(val))
                    .cloned()
                    .collect();

                if !unexpected.is_empty() {
                    let show_count = std::cmp::min(10, unexpected.len());
                    print!("⚠ Warning: Found unexpected values for 6-color display: ");
                    for (i, val) in unexpected.iter().take(show_count).enumerate() {
                        if i > 0 { print!(", "); }
                        print!("{}", val);
                    }
                    if unexpected.len() > show_count {
                        print!(" ... and {} more", unexpected.len() - show_count);
                    }
                    println!("\n   This might indicate dithering or incorrect display type");
                } else {
                    println!("✓ All sampled values match expected 6-color palette");
                }
            }
        }
        println!();
    }

    Ok(data)
}

fn convert_to_image(data: &[u8], args: &Args, width: u32, height: u32) -> Result<RgbImage> {
    let mut img = ImageBuffer::new(width, height);

    // Create progress bar
    let pb = if args.verbose {
        let pb = ProgressBar::new(data.len() as u64);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("{spinner:.green} [{elapsed_precise}] [{wide_bar:.cyan/blue}] {pos}/{len} pixels ({percent}%) {eta}")
                .unwrap_or_else(|_| ProgressStyle::default_bar())
                .progress_chars("#>-")
        );
        pb.set_message("Processing pixels");
        Some(pb)
    } else {
        println!("Processing {} pixels...", data.len());
        None
    };

    for (i, &pixel) in data.iter().enumerate() {
        let x = (i as u32) % width;
        let y = (i as u32) / width;
        
        let rgb = pixel_to_rgb(pixel, &args.display_type);
        img.put_pixel(x, y, Rgb(rgb));

        // Update progress bar every 1000 pixels for better performance
        if let Some(ref pb) = pb {
            if i % 1000 == 0 {
                pb.set_position(i as u64);
            }
        }
    }

    // Finish progress bar
    if let Some(pb) = pb {
        pb.set_position(data.len() as u64);
        pb.finish_with_message("✓ Pixel conversion completed");
    } else {
        println!("✓ Pixel conversion completed");
    }

    Ok(img)
}

fn save_image(img: &RgbImage, output_path: &str, format: &OutputFormat, verbose: bool) -> Result<()> {
    let spinner = if verbose {
        let spinner = ProgressBar::new_spinner();
        spinner.set_style(
            ProgressStyle::default_spinner()
                .template("{spinner:.green} {msg}")
                .unwrap_or_else(|_| ProgressStyle::default_spinner())
        );
        spinner.set_message(format!("Saving to {} format...", format.extension().to_uppercase()));
        spinner.enable_steady_tick(std::time::Duration::from_millis(100));
        Some(spinner)
    } else {
        None
    };

    let result = match format {
        OutputFormat::Bmp => {
            img.save_with_format(output_path, image::ImageFormat::Bmp)
        }
        OutputFormat::Jpeg => {
            img.save_with_format(output_path, image::ImageFormat::Jpeg)
        }
        OutputFormat::Png => {
            img.save_with_format(output_path, image::ImageFormat::Png)
        }
    }.with_context(|| format!("Failed to save image to '{}'", output_path));

    if let Some(spinner) = spinner {
        if result.is_ok() {
            spinner.finish_with_message("✓ Image saved successfully");
        } else {
            spinner.finish_with_message("✗ Failed to save image");
        }
    }

    result
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Parse the size argument
    let (width, height) = parse_size(&args.size)?;

    // Validate and read the binary file
    let data = validate_file(&args, width, height)?;

    if args.validate_only {
        println!("✓ Binary file validation completed successfully");
        println!("  File: {}", args.input);
        println!("  Size: {} bytes", data.len());
        println!("  Dimensions: {}x{}", width, height);
        println!("  Display type: {}", args.display_type.description());
        return Ok(());
    }

    // Construct the output file path
    let output_path = construct_output_path(&args.input, args.output.as_deref(), &args.format)?;
    let output_file = output_path.to_string_lossy();

    if args.verbose {
        println!("=== Converting Binary to Image ===");
        println!("Output file: {}", output_file);
        println!("Output format: {:?}", args.format);
        println!();
    }

    // Convert to image
    let img = convert_to_image(&data, &args, width, height)?;

    // Save image
    save_image(&img, &output_file, &args.format, args.verbose)?;

    // Verify output file was created
    if !output_path.exists() {
        return Err(anyhow::anyhow!("Failed to create output file '{}'", output_file));
    }

    let output_size = std::fs::metadata(&output_path)
        .map(|m| m.len())
        .unwrap_or(0);

    println!("✓ Conversion completed successfully!");
    println!("  Input: {} ({} bytes)", args.input, data.len());
    println!("  Output: {} ({} bytes)", output_file, output_size);
    println!("  Format: {}", args.format.extension().to_uppercase());
    println!("  Dimensions: {}x{}", width, height);
    println!("  Display type: {}", args.display_type.description());

    if args.verbose {
        println!();
        println!("You can view the image with:");
        println!("  open '{}'  # macOS", output_file);
        println!("  xdg-open '{}'  # Linux", output_file);
    }

    Ok(())
}