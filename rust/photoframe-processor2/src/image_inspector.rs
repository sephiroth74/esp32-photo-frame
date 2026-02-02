use anyhow::Result;
use exif::{In, Tag};
use indicatif::{ProgressBar, ProgressStyle};
use rayon::prelude::*;
use std::fs::File;
use std::io::BufReader;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};

use crate::json_output::JsonMessage;
use crate::logging::Logger;
use crate::report::{ImageInfo, ImageOrientation, RotationDegrees};
use image::GenericImageView;

#[derive(Debug, Clone)]
pub struct InspectionResult {
    pub valid: Vec<ImageInfo>,
    pub invalid: Vec<PathBuf>,
}

pub struct ImageInspector<'a> {
    logger: &'a Logger,
}

impl<'a> ImageInspector<'a> {
    pub fn new(logger: &'a Logger) -> Self {
        Self { logger }
    }

    pub fn inspect(&self, files: &[PathBuf], json_progress: bool) -> InspectionResult {
        if files.is_empty() {
            self.logger.warning("No files to inspect");
            return InspectionResult {
                valid: Vec::new(),
                invalid: Vec::new(),
            };
        }

        let progress = if json_progress {
            None
        } else {
            let pb = ProgressBar::new(files.len() as u64);
            pb.set_style(
                ProgressStyle::with_template(
                    "{msg}\n[{bar:40.cyan/blue}] {pos}/{len} {percent:>3}% {eta}",
                )
                .unwrap()
                .progress_chars("=>-"),
            );
            pb.set_message("Validating images");
            Some(pb)
        };

        let counter = AtomicUsize::new(0);

        let results: Vec<Result<ImageInfo, PathBuf>> = files
            .par_iter()
            .map(|path| {
                // Update progress bar with current file name
                if let Some(pb) = &progress {
                    if let Some(filename) = path.file_name() {
                        pb.set_message(format!("Processing: {}", filename.to_string_lossy()));
                    }
                }

                let result = match Self::inspect_one(path) {
                    Ok(info) => Ok(info),
                    Err(_) => Err(path.clone()),
                };

                if let Some(pb) = &progress {
                    pb.inc(1);
                } else if json_progress {
                    let current = counter.fetch_add(1, Ordering::Relaxed) + 1;
                    emit_json_progress(current, files.len(), path);
                }

                result
            })
            .collect();

        if let Some(pb) = progress {
            pb.finish_and_clear();
        }

        let mut valid = Vec::new();
        let mut invalid = Vec::new();

        for item in results {
            match item {
                Ok(info) => valid.push(info),
                Err(path) => invalid.push(path),
            }
        }

        valid.sort_by(|a, b| a.path.cmp(&b.path));
        invalid.sort();

        if !json_progress {
            let mut count_0 = 0usize;
            let mut count_90 = 0usize;
            let mut count_180 = 0usize;
            let mut count_270 = 0usize;

            for info in &valid {
                match info.rotation {
                    RotationDegrees::Deg0 => count_0 += 1,
                    RotationDegrees::Deg90 => count_90 += 1,
                    RotationDegrees::Deg180 => count_180 += 1,
                    RotationDegrees::Deg270 => count_270 += 1,
                }
            }

            self.logger.info("Rotation summary:");
            self.logger.config_item("0°", &count_0.to_string());
            self.logger.config_item("90°", &count_90.to_string());
            self.logger.config_item("180°", &count_180.to_string());
            self.logger.config_item("270°", &count_270.to_string());
            self.logger.info("");
        }

        InspectionResult { valid, invalid }
    }

    fn inspect_one(path: &Path) -> Result<ImageInfo> {
        let reader = image::ImageReader::open(path)?.with_guessed_format()?;
        let img = reader.decode()?;

        let rotation = read_exif_rotation(path).unwrap_or(RotationDegrees::Deg0);

        // Get image dimensions
        let (width, height) = img.dimensions();

        // Determine effective dimensions after applying EXIF rotation
        let (effective_width, effective_height) = match rotation {
            RotationDegrees::Deg90 | RotationDegrees::Deg270 => {
                // These rotations swap width and height
                (height, width)
            }
            RotationDegrees::Deg0 | RotationDegrees::Deg180 => {
                // These rotations keep width and height as-is
                (width, height)
            }
        };

        // Classify based on effective dimensions
        let orientation = if effective_width >= effective_height {
            ImageOrientation::Landscape
        } else {
            ImageOrientation::Portrait
        };

        Ok(ImageInfo {
            path: path.to_path_buf(),
            rotation,
            orientation,
        })
    }
}

fn emit_json_progress(current: usize, total: usize, path: &Path) {
    let message = format!("Validating {}", path.display());
    JsonMessage::progress(current, total, message);
}

fn read_exif_rotation(path: &Path) -> Option<RotationDegrees> {
    let file = File::open(path).ok()?;
    let mut bufreader = BufReader::new(file);

    let exif = exif::Reader::new()
        .read_from_container(&mut bufreader)
        .ok()?;
    let field = exif.get_field(Tag::Orientation, In::PRIMARY)?;
    let value = field.value.get_uint(0)?;

    match value {
        1 => Some(RotationDegrees::Deg0),
        3 => Some(RotationDegrees::Deg180),
        6 => Some(RotationDegrees::Deg90),
        8 => Some(RotationDegrees::Deg270),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::logging::TEST_LOGGER;
    use image::RgbImage;
    use image::codecs::jpeg::JpegEncoder;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn test_inspect_invalid_file() {
        let temp = TempDir::new().unwrap();
        let file = temp.path().join("not_image.txt");
        fs::write(&file, b"hello").unwrap();

        let inspector = ImageInspector::new(&TEST_LOGGER);

        let result = inspector.inspect(&vec![file.clone()], false);
        assert!(result.valid.is_empty());
        assert_eq!(result.invalid.len(), 1);
        assert_eq!(result.invalid[0], file);
    }

    #[test]
    fn test_inspect_valid_images_with_rotations() {
        let temp = TempDir::new().unwrap();
        let root = temp.path();

        let files = vec![
            (root.join("img_0.jpg"), 1u16, RotationDegrees::Deg0),
            (root.join("img_90.jpg"), 6u16, RotationDegrees::Deg90),
            (root.join("img_180.jpg"), 3u16, RotationDegrees::Deg180),
            (root.join("img_270.jpg"), 8u16, RotationDegrees::Deg270),
        ];

        for (path, orientation, _) in &files {
            write_jpeg_with_exif_orientation(path, *orientation);
        }

        let inspector = ImageInspector::new(&TEST_LOGGER);
        let input_paths = files.iter().map(|(p, _, _)| p.clone()).collect::<Vec<_>>();

        let result = inspector.inspect(&input_paths, false);
        assert_eq!(result.invalid.len(), 0);
        assert_eq!(result.valid.len(), files.len());

        for (path, _, expected_rotation) in files {
            let entry = result
                .valid
                .iter()
                .find(|info| info.path == path)
                .expect("missing inspected image");
            assert_eq!(entry.rotation, expected_rotation);
        }
    }

    fn write_jpeg_with_exif_orientation(path: &Path, orientation: u16) {
        let img = RgbImage::from_fn(1, 1, |_x, _y| image::Rgb([255, 0, 0]));
        let mut jpeg_data = Vec::new();
        let mut encoder = JpegEncoder::new(&mut jpeg_data);
        encoder.encode_image(&img).unwrap();

        let exif_segment = build_exif_orientation_segment(orientation);
        let mut with_exif = Vec::with_capacity(jpeg_data.len() + exif_segment.len());

        with_exif.extend_from_slice(&jpeg_data[..2]);
        with_exif.extend_from_slice(&exif_segment);
        with_exif.extend_from_slice(&jpeg_data[2..]);

        fs::write(path, with_exif).unwrap();
    }

    fn build_exif_orientation_segment(orientation: u16) -> Vec<u8> {
        let mut payload = Vec::new();
        payload.extend_from_slice(b"Exif\0\0");

        // TIFF header (big endian): "MM", 42, offset to IFD0 = 8
        payload.extend_from_slice(&[0x4D, 0x4D, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x08]);

        // IFD0 with one entry
        payload.extend_from_slice(&[0x00, 0x01]);

        // Tag 0x0112 (Orientation), type SHORT (3), count 1
        payload.extend_from_slice(&[0x01, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01]);

        // Value (2 bytes) + padding
        payload.extend_from_slice(&orientation.to_be_bytes());
        payload.extend_from_slice(&[0x00, 0x00]);

        // Next IFD offset = 0
        payload.extend_from_slice(&[0x00, 0x00, 0x00, 0x00]);

        let length = (payload.len() + 2) as u16;

        let mut segment = Vec::new();
        segment.extend_from_slice(&[0xFF, 0xE1]);
        segment.extend_from_slice(&length.to_be_bytes());
        segment.extend_from_slice(&payload);
        segment
    }
}
