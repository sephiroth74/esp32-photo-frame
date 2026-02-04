use super::ProcessedImage;
use crate::fs_utils::get_format_extension;
use crate::json_output::JsonMessage;
use crate::types::{ColorType, Orientation, OutputType};
use anyhow::{Context, Result};
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use photoframe_lib::{ColorMode, DisplayType, build_bin_file, process_image_with_display_type};
use rayon::ThreadPoolBuilder;
use rayon::prelude::*;
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};

#[derive(Debug)]
pub struct SaveOutputsResult {
    pub output_paths: Vec<Option<PathBuf>>,
    pub failed_indices: Vec<usize>,
}

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
) -> Result<SaveOutputsResult> {
    if processed.is_empty() {
        return Ok(SaveOutputsResult {
            output_paths: Vec::new(),
            failed_indices: Vec::new(),
        });
    }

    let primary_format = output_formats.first().copied();
    let output_paths: Vec<Option<PathBuf>> = if let Some(format) = primary_format {
        processed
            .iter()
            .map(|img| build_output_path(img, output_dir, format).map(Some))
            .collect::<Result<Vec<_>>>()?
    } else {
        vec![None; processed.len()]
    };

    let output_paths_all: Vec<Vec<PathBuf>> = processed
        .iter()
        .map(|img| {
            output_formats
                .iter()
                .map(|format| build_output_path(img, output_dir, *format))
                .collect::<Result<Vec<_>>>()
        })
        .collect::<Result<Vec<_>>>()?;

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
                .progress_chars("=>-"),
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

    let json_counter = AtomicUsize::new(0);
    let results: Vec<(usize, Result<()>)> = pool.install(|| {
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

                if json_progress {
                    let current = json_counter.fetch_add(1, Ordering::Relaxed) + 1;
                    let message = format!("Saving {} ({})", img.source.display(), format.as_str());
                    JsonMessage::progress("saving", current, total_jobs, message);
                }

                (idx, result)
            })
            .collect()
    });

    if let Some(global) = global_bar {
        global.finish_and_clear();
    }

    // Check for errors
    let mut error_count = 0;
    let mut error_by_index: HashMap<usize, String> = HashMap::new();
    for (i, result) in results.iter() {
        if let Err(e) = result {
            error_count += 1;
            error_by_index.entry(*i).or_insert_with(|| e.to_string());
            if !json_progress {
                logger.warning(&format!("Failed to save output {}: {}", i, e));
            }
        }
    }

    if error_count > 0 && !json_progress {
        logger.warning(&format!("{} output(s) failed to save", error_count));
    }

    if json_progress {
        for (idx, img) in processed.iter().enumerate() {
            if let Some(err) = error_by_index.get(&idx) {
                JsonMessage::file_failed(&img.source, err.clone());
            } else {
                let outputs = output_paths_all.get(idx).cloned().unwrap_or_default();
                JsonMessage::file_completed(&img.source, &outputs, img.processing_time_ms);
            }
        }
    }

    let mut failed_indices = error_by_index.keys().copied().collect::<Vec<_>>();
    failed_indices.sort_unstable();

    Ok(SaveOutputsResult {
        output_paths,
        failed_indices,
    })
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
    let output_path = build_output_path(img, output_dir, format)?;

    match format {
        OutputType::Bmp => {
            save_as_bmp(&img.temp_path, &output_path)?;
        }
        OutputType::Jpg => {
            save_as_jpg(&img.temp_path, &output_path)?;
        }
        OutputType::Png => {
            save_as_png(&img.temp_path, &output_path)?;
        }
        OutputType::Pfr1 => save_as_pfr1(
            &img.temp_path,
            &output_path,
            display_type,
            target_orientation,
        )?,
    }

    Ok(())
}

/// Build output path for a processed image and format
fn build_output_path(
    img: &ProcessedImage,
    output_dir: &Path,
    format: OutputType,
) -> Result<PathBuf> {
    let format_dir = output_dir.join(format.as_str());

    let ext = get_format_extension(&format);
    let base_name = if img.paired && img.paired_source.is_some() {
        // For combined images, use format: combined_name1_name2_hash
        let paired_src = img.paired_source.as_ref().unwrap();
        create_combined_filename(&img.source, paired_src, ext)?
    } else {
        create_readable_filename(&img.source, ext)?
    };

    Ok(format_dir.join(base_name))
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
    let mut img = image::open(temp_path)
        .with_context(|| format!("Failed to open temp image: {}", temp_path.display()))?
        .to_rgb8();

    // Apply inverse rotation of target_orientation to normalize image to rotation 0
    // This accounts for any rotation that was applied during processing
    match target_orientation {
        Orientation::Landscape => {
            // No rotation needed, already at rotation 0
        }
        Orientation::Portrait => {
            // Inverse of 90° is 270° (rotate3 times)
            img = image::imageops::rotate90(&img);
        }
        Orientation::LandscapeReverse => {
            // Inverse of 180° is 180°
            img = image::imageops::rotate180(&img);
        }
        Orientation::PortraitReverse => {
            // Inverse of 270° is 90°
            img = image::imageops::rotate270(&img);
        }
    }

    // Ensure image is in landscape mode (width >= height)
    let (width, height) = if img.width() < img.height() {
        // Shouldn't happen if orientation was correct, but rotate to be safe
        img = image::imageops::rotate90(&img);
        (img.height(), img.width())
    } else {
        (img.width(), img.height())
    };

    // Process image to get indexed pixel data
    let color_mode: ColorMode = display_type.into();
    let payload = process_image_with_display_type(&img, display_type)
        .context("Failed to process image for PFR1 format")?;

    // Build binary file with landscape dimensions and rotation 0
    // The image is now normalized to rotation 0
    let bin_data = build_bin_file(
        &payload,
        width as u16,
        height as u16,
        target_orientation.into(),
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

/// Create a combined filename for paired images
/// Format: combined_{name1}_{name2}_{hash8}.{ext}
fn create_combined_filename(
    first_path: &Path,
    second_path: &Path,
    extension: &str,
) -> Result<String> {
    let stem1 = first_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image1");

    let stem2 = second_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image2");

    let sanitized1 = sanitize_filename(stem1);
    let sanitized2 = sanitize_filename(stem2);

    // Generate hash from both source files
    let hash1 = generate_content_hash(first_path)?;
    let hash2 = generate_content_hash(second_path)?;

    // Combine hashes by taking first 4 chars of each
    let combined_hash = format!(
        "{}{}",
        &hash1[..4.min(hash1.len())],
        &hash2[..4.min(hash2.len())]
    );
    let final_hash = &combined_hash[..8.min(combined_hash.len())];

    Ok(format!(
        "combined_{}_{}{}.{}",
        sanitized1, sanitized2, final_hash, extension
    ))
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
