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
    let subdir = match format {
        OutputType::Bmp => "bmp",
        OutputType::Pfr1 => "pfr1",
        OutputType::Jpg => "jpg",
        OutputType::Png => "png",
    };

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

/// Generate output file path for a specific format
///
/// Creates the full output path including format subdirectory and filename
/// with appropriate extension. Optionally adds a suffix to the filename.
///
/// # Arguments
/// * `base_output_dir` - The root output directory
/// * `input_path` - The original input file path (used for filename)
/// * `format` - The output format
/// * `suffix` - Optional suffix to add to filename (e.g., "portrait", "landscape")
///
/// # Returns
/// Full path to the output file
///
/// # Example
/// ```ignore
/// let output = get_format_output_path(
///     Path::new("/output"),
///     Path::new("/input/photo.jpg"),
///     &OutputFormat::Pfr1,
///     None
/// );
/// // Returns: /output/pfr1/photo.pfr1
/// ```
pub fn get_format_output_path(
    base_output_dir: &Path,
    input_path: &Path,
    format: &OutputType,
    suffix: Option<&str>,
) -> PathBuf {
    let format_dir = get_format_directory(base_output_dir, format);
    let extension = get_format_extension(format);

    // Get input filename without extension
    let stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("output");

    // Build filename with optional suffix
    let filename = if let Some(suf) = suffix {
        format!("{}_{}.{}", stem, suf, extension)
    } else {
        format!("{}.{}", stem, extension)
    };

    format_dir.join(filename)
}

/// Generate output file path for a combined image pair
///
/// Creates the output path for combined images (e.g., two portraits side-by-side)
/// using a readable filename that includes both original filenames.
///
/// # Arguments
/// * `base_output_dir` - The root output directory
/// * `left_path` - Path to the left/top image
/// * `right_path` - Path to the right/bottom image
/// * `format` - The output format
///
/// # Returns
/// Full path to the combined output file
///
/// # Example
/// ```ignore
/// let output = get_combined_format_output_path(
///     Path::new("/output"),
///     Path::new("/input/photo1.jpg"),
///     Path::new("/input/photo2.jpg"),
///     &OutputFormat::Pfr1
/// );
/// // Returns: /output/pfr1/photo1_photo2.pfr1
/// ```
pub fn get_combined_format_output_path(
    base_output_dir: &Path,
    left_path: &Path,
    right_path: &Path,
    format: &OutputType,
) -> PathBuf {
    let format_dir = get_format_directory(base_output_dir, format);
    let extension = get_format_extension(format);

    // Get both filenames without extensions
    let left_stem = left_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("left");

    let right_stem = right_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("right");

    // Create combined filename
    let filename = format!("{}_{}.{}", left_stem, right_stem, extension);

    format_dir.join(filename)
}

/// Check if a path exists and is a directory
///
/// # Arguments
/// * `path` - The path to check
///
/// # Returns
/// `true` if path exists and is a directory, `false` otherwise
pub fn is_directory(path: &Path) -> bool {
    path.exists() && path.is_dir()
}

/// Check if a path exists and is a file
///
/// # Arguments
/// * `path` - The path to check
///
/// # Returns
/// `true` if path exists and is a file, `false` otherwise
pub fn is_file(path: &Path) -> bool {
    path.exists() && path.is_file()
}

/// Get file size in bytes
///
/// # Arguments
/// * `path` - The file path
///
/// # Returns
/// File size in bytes, or error if file cannot be accessed
pub fn get_file_size(path: &Path) -> Result<u64> {
    let metadata = fs::metadata(path)
        .with_context(|| format!("Failed to get file metadata: {}", path.display()))?;
    Ok(metadata.len())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
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
    fn test_get_format_output_path() {
        let base = Path::new("/output");
        let input = Path::new("/input/photo.jpg");

        let path = get_format_output_path(base, input, &OutputType::Pfr1, None);
        assert_eq!(path, PathBuf::from("/output/pfr1/photo.pfr1"));

        let path_with_suffix =
            get_format_output_path(base, input, &OutputType::Bmp, Some("portrait"));
        assert_eq!(
            path_with_suffix,
            PathBuf::from("/output/bmp/photo_portrait.bmp")
        );
    }

    #[test]
    fn test_get_combined_format_output_path() {
        let base = Path::new("/output");
        let left = Path::new("/input/photo1.jpg");
        let right = Path::new("/input/photo2.jpg");

        let path = get_combined_format_output_path(base, left, right, &OutputType::Pfr1);
        assert_eq!(path, PathBuf::from("/output/pfr1/photo1_photo2.pfr1"));
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

    #[test]
    fn test_is_directory() {
        let temp_dir = TempDir::new().unwrap();
        let dir_path = temp_dir.path();
        let file_path = dir_path.join("test.txt");

        fs::write(&file_path, b"test").unwrap();

        assert!(is_directory(dir_path));
        assert!(!is_directory(&file_path));
        assert!(!is_directory(Path::new("/nonexistent")));
    }

    #[test]
    fn test_is_file() {
        let temp_dir = TempDir::new().unwrap();
        let dir_path = temp_dir.path();
        let file_path = dir_path.join("test.txt");

        fs::write(&file_path, b"test").unwrap();

        assert!(is_file(&file_path));
        assert!(!is_file(dir_path));
        assert!(!is_file(Path::new("/nonexistent")));
    }
}
