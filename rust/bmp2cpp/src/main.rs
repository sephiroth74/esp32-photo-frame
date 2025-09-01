mod images;

#[allow(unused_imports)]
use crate::images::{IMAGE_BW, IMAGE_BW2};
use bmp_rust::bmp::BMP;
use clap::{Parser, ValueEnum};
use embedded_graphics::{pixelcolor::Rgb888, prelude::*};
use std::collections::HashMap;
use std::fmt::Display;
use std::path::PathBuf;
use tinybmp::Bmp;

#[derive(Debug, Clone)]
struct PixelsMap {
    unique: HashMap<u8, UniquePixel>,
    count: usize,
}

impl Display for PixelsMap {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Total pixels: {}\n", self.count)?;

        // Sort the unique pixels by count for better readability
        let mut sorted_pixels: Vec<&UniquePixel> = self.unique.values().collect();
        sorted_pixels.sort_by(|a, b| b.count.cmp(&a.count)); // Sort by count descending
        for pixel in sorted_pixels {
            writeln!(f, "{}", pixel)?;
        }

        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct UniquePixel {
    color: u8,
    count: usize,
}

impl Display for UniquePixel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let r8 = (self.color >> 5) * (255 / 7); // 3 bits for red
        let g8 = ((self.color >> 2) & 0x07) * (255 / 7); // 3 bits for green
        let b8 = (self.color & 0x03) * (255 / 3); // 2 bits for blue

        write!(
            f,
            "[{:6}] Color: 0x{:02X}, RGB: {:3},{:3},{:3} ",
            self.count, self.color, r8, g8, b8
        )

        //write!(f, "Color: 0x{:02X}, Count: {}", self.color, self.count)
    }
}

#[derive(Debug, Clone, ValueEnum, Copy, PartialEq, Eq)]
enum OutputFormat {
    Binary,
    CArray,
}

#[allow(dead_code)]
impl OutputFormat {
    fn extension(&self) -> &str {
        match self {
            OutputFormat::Binary => "bin",
            OutputFormat::CArray => "h",
        }
    }

    fn content_type(&self) -> &str {
        match self {
            OutputFormat::Binary => "application/octet-stream",
            OutputFormat::CArray => "text/plain",
        }
    }
}

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    #[arg(short, long)]
    output_format: OutputFormat,

    // array_name is optional and cano only be set when output_format is CArray
    #[arg(short, long, value_name = "ARRAY_NAME")]
    array_name: Option<String>,

    #[arg(required = true)]
    input: PathBuf,

    #[arg(required = true)]
    output: PathBuf,
}

#[allow(dead_code)]
fn pixels_to_bmp_bw(
    code: &[u8],
    width: u16,
    height: u16,
    output: &PathBuf,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut bmp_from_scratch = BMP::new(height as i32, width as u32, None);

    let byte_width: u16 = (width + 7) / 8; // Bitmap scanline pad = whole byte
    let mut b: u8 = 0;
    let x: u16 = 0;
    let mut y: u16 = 0;

    for j in 0..height - 1 {
        y += 1;
        for i in 0..width - 1 {
            if i & 7 != 0 {
                // Shift left to make room for the next bit
                b <<= 1;
            } else {
                // Read a new byte from the bitmap
                b = code[(j * byte_width + i / 8) as usize];
            }
            if b & 0x80 != 0 {
                // let color = code[(j * byte_width + i / 8) as usize];

                bmp_from_scratch
                    .change_color_of_pixel(x + i, y, [0, 0, 0, 255])
                    .map_err(|e| {
                        format!("Error changing pixel color at ({}, {}): {}", x + i, y, e)
                    })?;
            }
        }
    }

    let path = output.to_str().ok_or("Invalid output path")?;
    bmp_from_scratch.save_to_new(&path).unwrap();

    Ok(())
}

#[allow(dead_code)]
fn map_pixels(code: &[u8]) -> PixelsMap {
    // this function will return a list of unique pixels in the code given
    let mut unique_pixels: HashMap<u8, UniquePixel> = HashMap::new();
    for &pixel in code {
        // check if the pixel is already in the map
        if let Some(unique_pixel) = unique_pixels.get_mut(&pixel) {
            // if it is, increment the count
            unique_pixel.count += 1;
        } else {
            // if it is not, insert a new UniquePixel with count 1
            unique_pixels.insert(
                pixel,
                UniquePixel {
                    color: pixel,
                    count: 1,
                },
            );
        }
    }

    PixelsMap {
        unique: unique_pixels.into(),
        count: code.len(),
    }
}

#[allow(dead_code)]
fn pixels_to_bmp8(
    code: &[u8],
    width: u16,
    height: u16,
    output: &PathBuf,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut bmp_from_scratch = BMP::new(height as i32, width as u32, None);

    for y in 0..height {
        for x in 0..width {
            let pixel_index = (y * width + x) as usize;
            let color = code[pixel_index];

            // 3 bits for red, 3 bits for green, 2 bits for blue
            let r = (color >> 5) * (255 / 7); // 3 bits for red
            let g = ((color >> 2) & 0x07) * (255 / 7); // 3 bits for green
            let b = (color & 0x03) * (255 / 3); // 2 bits for blue

            bmp_from_scratch
                .change_color_of_pixel(x, y, [r, g, b, 255])
                .unwrap(); // RGBA format
        }
    }

    let path = output.to_str().ok_or("Invalid output path")?;
    bmp_from_scratch.save_to_new(&path).unwrap();

    Ok(())
}

/// Converts a BMP file to an array of pixels in a specific format.
/// # Arguments
/// * `input` - The path to the input BMP file.
/// # Returns
/// A vector of bytes representing the pixel data in the specified format.
/// # Errors
/// Returns an error if the BMP file cannot be read or parsed.
/// # Example
/// ```
/// let pixels = bmp_to_array(&PathBuf::from("path/to/image.bmp"))?;
/// ```
fn bmp_to_array(input: &PathBuf) -> Result<(Size, Vec<u8>), Box<dyn std::error::Error>> {
    let bmp_data = std::fs::read(input)?;
    let bmp =
        Bmp::<Rgb888>::from_slice(&bmp_data).map_err(|_e| "Failed to parse BMP".to_string())?;

    let image_size = bmp.size();

    let pixels: Vec<u8> = bmp
        .pixels()
        .map(|p| {
            let color = p.1;
            let color8 = ((color.r() / 32) << 5) + ((color.g() / 32) << 2) + (color.b() / 64);
            color8
        })
        .collect();
    Ok((image_size, pixels))
}

/// Converts a BMP file to a specific output format (binary or C array).
/// # Arguments
/// * `input` - The path to the input BMP file.
/// * `output` - The path to the output file.
/// * `format` - The output format (binary or C array).
/// * `array_name` - Optional name for the C array variable.
/// # Returns
/// A result indicating success or failure.
fn bmp2lcd(
    input: &PathBuf,
    output: &PathBuf,
    format: OutputFormat,
    array_name: Option<&str>,
) -> Result<(), Box<dyn std::error::Error>> {
    // read the bmp file
    let (size, pixels) = bmp_to_array(input)?;

    let variable_name = array_name.unwrap_or("IMAGE");
    let header_name = format!("_{}_H_", variable_name);

    let content: Vec<u8> = match format {
        OutputFormat::Binary => pixels,
        OutputFormat::CArray => {
            // convert pixels to C array format
            // split the pixel string into chunks of size.width for better readability
            let chunks = pixels.chunks(size.width as usize);

            let pixel_string = chunks
                .map(|chunk| {
                    // convert each chunk into a string of hex values
                    let chunk_string: String = chunk
                        .iter()
                        .map(|p| format!("0x{:02X}", p))
                        .collect::<Vec<String>>()
                        .join(", ");

                    chunk_string
                })
                .collect::<Vec<String>>()
                .join(",\n");

            let final_string = format!(
                "
#ifndef {}
#define {}

// This file is automatically generated by bmp2lcd
// from the BMP file: {}

#if defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

const unsigned char {}[{}] PROGMEM = {{
{}
}};

#endif",
                header_name,
                header_name,
                input.display(),
                variable_name,
                pixels.len(),
                pixel_string
            );
            let bytes = final_string.as_str().as_bytes();
            bytes.to_vec()
        }
    };

    std::fs::write(output, content)?;
    Ok(())
}

#[allow(unreachable_code)]
fn main() {
    //let colors = map_pixels(IMAGE_BW);
    //println!("pixel map: {:#}", colors);
    //return;

    // pixels_to_bmp_bw(
    //     IMAGE_BW2,
    //     800,
    //     480,
    //     &PathBuf::from("/Users/alessandro/Documents/Projects/Rust/bmp2cpp/output.bmp"),
    // )
    // .expect("Failed to create BMP");
    // return;

    //pixels_to_bmp8();
    //return;

    let args = Args::parse();

    if args.array_name.is_some() && args.output_format != OutputFormat::CArray {
        eprintln!("Error: array_name can only be set when output_format is CArray");
        return;
    }

    bmp2lcd(
        &args.input,
        &args.output,
        args.output_format,
        args.array_name.as_deref(),
    )
    .unwrap_or_else(|e| eprintln!("Error: {}", e));
}
