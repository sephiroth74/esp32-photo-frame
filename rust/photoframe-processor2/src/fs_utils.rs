use anyhow::{Context, Result};
use std::fs;
use std::path::{Path, PathBuf};

use crate::types::OutputType;

/// Create output subdirectories for each format
///
/// For each output format (bmp, pfr1, jpg, png), creates a subdirectory
/// under the base output directory following the pattern:
/// - base_output_dir/bmp/
/// - base_output_dir/pfr1/
/// - base_output_dir/jpg/
/// - base_output_dir/png/
///
/// # Arguments
/// * `base_output_dir` - The root output directory
/// * `formats` - List of output formats to create directories for
///
/// # Errors
/// Returns an error if directory creation fails
pub fn create_format_directories(base_output_dir: &Path, formats: &[OutputType]) -> Result<()> {
    for format in formats {
        let format_dir = get_format_directory(base_output_dir, format);

        fs::create_dir_all(&format_dir).with_context(|| {
            format!(
                "Failed to create format directory: {}",
                format_dir.display()
            )
        })?;
    }

    Ok(())
}

/// Get the subdirectory path for a specific format
///
/// # Arguments
/// * `base_output_dir` - The root output directory
/// * `format` - The output format
///
/// # Returns
/// Path to the format-specific subdirectory
pub fn get_format_directory(base_output_dir: &Path, format: &OutputType) -> PathBuf {
    let subdir = format.as_str();
    base_output_dir.join(subdir)
}

/// Get the file extension for a specific format
///
/// # Arguments
/// * `format` - The output format
///
/// # Returns
/// File extension string without the leading dot

pub fn get_format_extension(format: &OutputType) -> &'static str {
    match format {
        OutputType::Bmp => "bmp",
        OutputType::Pfr1 => "pfr1",
        OutputType::Jpg => "jpg",
        OutputType::Png => "png",
    }
}

/// Get output path for a given input image and format
/// Optional suffix will be appended to the filename stem.
#[allow(dead_code)]
pub fn get_format_output_path(
    base_output_dir: &Path,
    input_path: &Path,
    format: &OutputType,
    suffix: Option<&str>,
) -> PathBuf {
    let format_dir = get_format_directory(base_output_dir, format);
    let stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image");
    let ext = get_format_extension(format);

    let filename = if let Some(suffix) = suffix {
        format!("{}_{}.{}", stem, suffix, ext)
    } else {
        format!("{}.{}", stem, ext)
    };

    format_dir.join(filename)
}

/// Get output path for a combined image (paired images)
#[allow(dead_code)]
pub fn get_combined_format_output_path(
    base_output_dir: &Path,
    left_path: &Path,
    right_path: &Path,
    format: &OutputType,
) -> PathBuf {
    let format_dir = get_format_directory(base_output_dir, format);
    let left = left_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("left");
    let right = right_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("right");
    let ext = get_format_extension(format);

    format_dir.join(format!("combined_{}_{}.{}", left, right, ext))
}

/// Check if a path exists and is a directory
#[allow(dead_code)]
pub fn is_directory(path: &Path) -> bool {
    path.is_dir()
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn test_get_format_directory() {
        let base = Path::new("/output");

        assert_eq!(
            get_format_directory(base, &OutputType::Bmp),
            PathBuf::from("/output/bmp")
        );
        assert_eq!(
            get_format_directory(base, &OutputType::Pfr1),
            PathBuf::from("/output/pfr1")
        );
        assert_eq!(
            get_format_directory(base, &OutputType::Jpg),
            PathBuf::from("/output/jpg")
        );
        assert_eq!(
            get_format_directory(base, &OutputType::Png),
            PathBuf::from("/output/png")
        );
    }

    #[test]
    fn test_get_format_extension() {
        assert_eq!(get_format_extension(&OutputType::Bmp), "bmp");
        assert_eq!(get_format_extension(&OutputType::Pfr1), "pfr1");
        assert_eq!(get_format_extension(&OutputType::Jpg), "jpg");
        assert_eq!(get_format_extension(&OutputType::Png), "png");
    }

    #[test]
    fn test_create_format_directories() {
        let temp_dir = TempDir::new().unwrap();
        let base = temp_dir.path();

        let formats = vec![OutputType::Bmp, OutputType::Pfr1, OutputType::Jpg];

        create_format_directories(base, &formats).unwrap();

        assert!(base.join("bmp").exists());
        assert!(base.join("pfr1").exists());
        assert!(base.join("jpg").exists());
        assert!(!base.join("png").exists());
    }
}
