use super::ProcessedImage;
use crate::types::{ColorType, Orientation, OutputType};
use anyhow::{Context, Result};
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use photoframe_lib::{ColorMode, DisplayType, build_bin_file, process_image_with_display_type};
use rayon::ThreadPoolBuilder;
use rayon::prelude::*;
use sha2::{Digest, Sha256};
use std::fs;
use std::io::Read;
use std::path::Path;

/// Save processed images to output directory in requested formats
pub fn save_outputs(
    processed: &[ProcessedImage],
    output_dir: &Path,
    output_formats: &[OutputType],
    processing_type: ColorType,
    target_orientation: Orientation,
    multi: &MultiProgress,
    json_progress: bool,
    jobs: usize,
    logger: &crate::logging::Logger,
) -> Result<()> {
    if processed.is_empty() {
        return Ok(());
    }

    // Create output subdirectories for each format
    for format in output_formats {
        let format_dir = output_dir.join(format.as_str());
        fs::create_dir_all(&format_dir).with_context(|| {
            format!(
                "Failed to create output directory: {}",
                format_dir.display()
            )
        })?;
    }

    if !json_progress {
        logger.info(&format!(
            "Saving {} images in {} format(s)...",
            processed.len(),
            output_formats.len()
        ));
    }

    let total_jobs = processed.len() * output_formats.len();
    let global_bar = if json_progress {
        None
    } else {
        let pb = multi.add(ProgressBar::new(total_jobs as u64));
        pb.set_style(
            ProgressStyle::with_template("Saving [{bar:40.cyan/blue}] {pos}/{len} {eta}")
                .unwrap()
                .progress_chars("██▌ "),
        );
        pb.set_message("Saving output files");
        Some(pb)
    };

    let thread_count = if jobs == 0 { num_cpus::get() } else { jobs };

    let pool = ThreadPoolBuilder::new()
        .num_threads(thread_count.max(1))
        .build()
        .context("Failed to create thread pool for saving")?;

    // Create jobs: for each image, save in each output format
    let save_jobs: Vec<_> = processed
        .iter()
        .enumerate()
        .flat_map(|(idx, img)| output_formats.iter().map(move |format| (idx, img, *format)))
        .collect();

    let display_type = match processing_type {
        ColorType::BlackWhite => DisplayType::BlackAndWhite,
        ColorType::SixColor => DisplayType::SixColors,
    };

    let output_dir = output_dir.to_path_buf();

    let results: Vec<Result<()>> = pool.install(|| {
        save_jobs
            .into_par_iter()
            .map(|(idx, img, format)| {
                let result = save_single_output(
                    img,
                    &output_dir,
                    format,
                    display_type,
                    target_orientation,
                    idx,
                );

                if let Some(global) = &global_bar {
                    global.inc(1);
                }

                result
            })
            .collect()
    });

    if let Some(global) = global_bar {
        global.finish_and_clear();
    }

    // Check for errors
    let mut error_count = 0;
    for (i, result) in results.iter().enumerate() {
        if let Err(e) = result {
            error_count += 1;
            if !json_progress {
                logger.warning(&format!("Failed to save output {}: {}", i, e));
            }
        }
    }

    if error_count > 0 && !json_progress {
        logger.warning(&format!("{} output(s) failed to save", error_count));
    }

    Ok(())
}

/// Save a single image in a specific format
fn save_single_output(
    img: &ProcessedImage,
    output_dir: &Path,
    format: OutputType,
    display_type: DisplayType,
    target_orientation: Orientation,
    _index: usize,
) -> Result<()> {
    let format_dir = output_dir.join(format.as_str());

    // Generate output filename
    let ext = match format {
        OutputType::Bmp => "bmp",
        OutputType::Jpg => "jpg",
        OutputType::Png => "png",
        OutputType::Pfr1 => "pfr1",
    };

    let base_name = if img.paired {
        // For combined images, just use the source name with hash
        create_readable_filename(&img.source, ext)?
    } else {
        create_readable_filename(&img.source, ext)?
    };

    match format {
        OutputType::Bmp => {
            let output_path = format_dir.join(&base_name);
            save_as_bmp(&img.temp_path, &output_path)?;
        }
        OutputType::Jpg => {
            let output_path = format_dir.join(&base_name);
            save_as_jpg(&img.temp_path, &output_path)?;
        }
        OutputType::Png => {
            let output_path = format_dir.join(&base_name);
            save_as_png(&img.temp_path, &output_path)?;
        }
        OutputType::Pfr1 => {
            let output_path = format_dir.join(&base_name);
            save_as_pfr1(
                &img.temp_path,
                &output_path,
                display_type,
                target_orientation,
            )?
        }
    }

    Ok(())
}

/// Save image as BMP
fn save_as_bmp(temp_path: &Path, output_path: &Path) -> Result<()> {
    let img = image::open(temp_path)
        .with_context(|| format!("Failed to open temp image: {}", temp_path.display()))?;

    img.save_with_format(output_path, image::ImageFormat::Bmp)
        .with_context(|| format!("Failed to save BMP: {}", output_path.display()))?;

    Ok(())
}

/// Save image as JPG
fn save_as_jpg(temp_path: &Path, output_path: &Path) -> Result<()> {
    let img = image::open(temp_path)
        .with_context(|| format!("Failed to open temp image: {}", temp_path.display()))?;

    img.save_with_format(output_path, image::ImageFormat::Jpeg)
        .with_context(|| format!("Failed to save JPG: {}", output_path.display()))?;

    Ok(())
}

/// Save image as PNG
fn save_as_png(temp_path: &Path, output_path: &Path) -> Result<()> {
    let img = image::open(temp_path)
        .with_context(|| format!("Failed to open temp image: {}", temp_path.display()))?;

    img.save_with_format(output_path, image::ImageFormat::Png)
        .with_context(|| format!("Failed to save PNG: {}", output_path.display()))?;

    Ok(())
}

/// Save image as PFR1 binary format using photoframe-lib
fn save_as_pfr1(
    temp_path: &Path,
    output_path: &Path,
    display_type: DisplayType,
    target_orientation: Orientation,
) -> Result<()> {
    // Load image
    let img = image::open(temp_path)
        .with_context(|| format!("Failed to open temp image: {}", temp_path.display()))?
        .to_rgb8();

    // Process image to get indexed pixel data
    let dimensions = img.dimensions();
    let color_mode: ColorMode = display_type.into();
    let payload = process_image_with_display_type(&img, display_type)
        .context("Failed to process image for PFR1 format")?;

    // Map Orientation to rotation value (0-3)
    let rotation = match target_orientation {
        Orientation::Landscape => 0,
        Orientation::Portrait => 1,
        Orientation::LandscapeReverse => 2,
        Orientation::PortraitReverse => 3,
    };

    // Build binary file
    let bin_data = build_bin_file(
        &payload,
        dimensions.0 as u16,
        dimensions.1 as u16,
        rotation,
        color_mode,
        1,
    );

    // Write to file
    fs::write(output_path, bin_data)
        .with_context(|| format!("Failed to write PFR1 file: {}", output_path.display()))?;

    Ok(())
}

/// Generate an 8-character hash from file content for uniqueness
fn generate_content_hash(file_path: &Path) -> Result<String> {
    // Read first 4KB of file for hash (faster for large files)
    let mut file = fs::File::open(file_path)
        .with_context(|| format!("Failed to open file for hashing: {}", file_path.display()))?;
    let mut buffer = vec![0; 4096];
    let bytes_read = file
        .read(&mut buffer)
        .context("Failed to read file for hashing")?;
    buffer.truncate(bytes_read);

    // Also include file size and name for better uniqueness
    let metadata = fs::metadata(file_path)
        .with_context(|| format!("Failed to get file metadata: {}", file_path.display()))?;
    let file_size = metadata.len();
    let file_name = file_path
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("unknown");

    // Create hash from content + size + name
    let mut hasher = Sha256::new();
    hasher.update(&buffer);
    hasher.update(file_size.to_le_bytes());
    hasher.update(file_name.as_bytes());

    let result = hasher.finalize();
    let hex_hash = result
        .iter()
        .map(|b| format!("{:02x}", b))
        .collect::<String>();

    // Return first 8 characters of hash
    Ok(hex_hash[..8].to_string())
}

/// Sanitize filename for FAT32 compatibility
fn sanitize_filename(name: &str) -> String {
    // Replace invalid FAT32 characters
    let invalid_chars = ['<', '>', ':', '"', '/', '\\', '|', '?', '*'];
    let mut sanitized = String::new();

    for ch in name.chars() {
        if invalid_chars.contains(&ch) || (ch as u32) < 32 {
            sanitized.push('_');
        } else {
            sanitized.push(ch);
        }
    }

    // Replace consecutive underscores
    while sanitized.contains("__") {
        sanitized = sanitized.replace("__", "_");
    }

    // Remove leading/trailing underscores
    sanitized = sanitized.trim_matches('_').to_string();

    // Truncate to max length (leaving room for hash and extension)
    const MAX_NAME_LENGTH: usize = 100;
    if sanitized.len() > MAX_NAME_LENGTH {
        sanitized = sanitized[..MAX_NAME_LENGTH].to_string();
    }

    sanitized
}

/// Create a readable filename with hash for uniqueness
/// Format: {sanitized_name}_{hash8}.{ext}
fn create_readable_filename(input_path: &Path, extension: &str) -> Result<String> {
    let stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image");

    let sanitized = sanitize_filename(stem);
    let hash = generate_content_hash(input_path)?;

    Ok(format!("{}_{}.{}", sanitized, hash, extension))
}

impl OutputType {
    /// Get the string representation for directory name
    pub fn as_str(&self) -> &'static str {
        match self {
            OutputType::Bmp => "bmp",
            OutputType::Pfr1 => "pfr1",
            OutputType::Jpg => "jpg",
            OutputType::Png => "png",
        }
    }
}
