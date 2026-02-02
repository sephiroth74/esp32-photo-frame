use super::{ProcessedImage, get_temp_dir};
use crate::types::{HexColor, Orientation, Size};
use anyhow::{Context, Result};
use image::RgbImage;
use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use rayon::ThreadPoolBuilder;
use rayon::prelude::*;
use std::collections::HashMap;
use tempfile::Builder;

/// Combine paired images with divider
pub fn combine_paired_images(
    processed: &[ProcessedImage],
    multi: &MultiProgress,
    json_progress: bool,
    verbose: bool,
    jobs: usize,
    target_orientation: Orientation,
    divider_width: u32,
    divider_color: HexColor,
    logger: &crate::logging::Logger,
) -> Result<Vec<ProcessedImage>> {
    // Group paired images by pair_id
    let mut pairs: HashMap<usize, [Option<&ProcessedImage>; 2]> = HashMap::new();
    let mut singles = Vec::new();

    for img in processed {
        if let (Some(pair_id), Some(pair_index)) = (img.pair_id, img.pair_index) {
            let entry = pairs.entry(pair_id).or_insert([None, None]);
            entry[pair_index] = Some(img);
        } else {
            singles.push(img.clone());
        }
    }

    if pairs.is_empty() {
        // No pairs to combine, return original list
        return Ok(processed.to_vec());
    }

    // Prepare combining jobs
    let combine_jobs: Vec<_> = pairs
        .into_iter()
        .filter_map(|(pair_id, [first, second])| {
            if let (Some(first), Some(second)) = (first, second) {
                Some((pair_id, first.clone(), second.clone()))
            } else {
                None
            }
        })
        .collect();

    if combine_jobs.is_empty() {
        return Ok(processed.to_vec());
    }

    if !json_progress {
        logger.info(&format!(
            "Combining {} paired images...",
            combine_jobs.len()
        ));
    }

    let global_bar = if json_progress || verbose {
        None
    } else {
        let pb = multi.add(ProgressBar::new(combine_jobs.len() as u64));
        pb.set_style(
            ProgressStyle::with_template("Combining [{bar:40.cyan/blue}] {pos}/{len} {eta}")
                .unwrap()
                .progress_chars("=>-"),
        );
        pb.set_message("Combining paired images");
        Some(pb)
    };

    let thread_count = if jobs == 0 { num_cpus::get() } else { jobs };

    let pool = ThreadPoolBuilder::new()
        .num_threads(thread_count.max(1))
        .build()
        .context("Failed to create thread pool for combining")?;

    let is_landscape = matches!(
        target_orientation,
        Orientation::Landscape | Orientation::LandscapeReverse
    );

    let combined_results: Vec<Result<ProcessedImage>> = pool.install(|| {
        combine_jobs
            .into_par_iter()
            .map(|(pair_id, first, second)| {
                let result = combine_two_images(
                    &first,
                    &second,
                    is_landscape,
                    divider_width,
                    &divider_color,
                    pair_id,
                    first.target_size,
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

    let mut result = singles;
    for combined in combined_results {
        match combined {
            Ok(img) => result.push(img),
            Err(e) => {
                if !json_progress {
                    logger.warning(&format!("Failed to combine pair: {}", e));
                }
            }
        }
    }

    Ok(result)
}

/// Combine two processed images with a divider
fn combine_two_images(
    first: &ProcessedImage,
    second: &ProcessedImage,
    is_landscape: bool,
    divider_width: u32,
    divider_color: &HexColor,
    pair_id: usize,
    target_size: Size,
) -> Result<ProcessedImage> {
    // Load both images
    let img1 = image::open(&first.temp_path)
        .with_context(|| format!("Failed to open first image: {}", first.temp_path.display()))?
        .to_rgb8();

    let img2 = image::open(&second.temp_path)
        .with_context(|| {
            format!(
                "Failed to open second image: {}",
                second.temp_path.display()
            )
        })?
        .to_rgb8();

    let (width1, height1) = img1.dimensions();
    let (width2, height2) = img2.dimensions();

    // Create combined image
    let (combined_width, combined_height) = if is_landscape {
        // Horizontal combination: [img1 | divider | img2]
        (width1 + width2, height1.max(height2))
    } else {
        // Vertical combination: [img1] [divider] [img2]
        (width1.max(width2), height1 + height2)
    };

    let mut combined = RgbImage::new(combined_width, combined_height);

    // Fill with divider color
    let div_r = divider_color.red();
    let div_g = divider_color.green();
    let div_b = divider_color.blue();
    for pixel in combined.pixels_mut() {
        *pixel = image::Rgb([div_r, div_g, div_b]);
    }

    if is_landscape {
        // Copy first image to left side
        image::imageops::replace(&mut combined, &img1, 0, 0);
        // Copy second image to right side (after divider)
        image::imageops::replace(&mut combined, &img2, (width1 + divider_width) as i64, 0);
    } else {
        // Copy first image to top
        image::imageops::replace(&mut combined, &img1, 0, 0);
        // Copy second image to bottom (after divider)
        image::imageops::replace(&mut combined, &img2, 0, (height1 + divider_width) as i64);
    }

    // Save combined image to temp file
    let temp_dir = get_temp_dir()?;
    let mut temp_file = Builder::new()
        .prefix(&format!("pfproc_combined_{}_", pair_id))
        .suffix(".png")
        .tempfile_in(&temp_dir)
        .context("Failed to create temporary file for combined image")?;

    combined
        .save(&mut temp_file)
        .context("Failed to save combined image")?;

    let (_file, temp_path) = temp_file
        .keep()
        .context("Failed to keep combined temporary file")?;

    // Return ProcessedImage with exact target size
    Ok(ProcessedImage {
        source: first.source.clone(), // Use first image as source reference
        temp_path,
        target_size,
        paired: true, // Mark as paired so we know it's a combined image
        pair_id: Some(pair_id),
        pair_index: None,
        paired_source: Some(second.source.clone()), // Store second image source for naming
        orientation: first.orientation,
        people_count: None,
        width: combined_width,
        height: combined_height,
        processing_time_ms: first.processing_time_ms + second.processing_time_ms,
        detection_time_ms: first.detection_time_ms + second.detection_time_ms,
    })
}
