use anyhow::Result;
use base64::{engine::general_purpose, Engine as _};
use console::style;
use indicatif::{ProgressBar, ProgressStyle};
use std::path::Path;
use std::time::Duration;

#[cfg(test)]
use std::collections::hash_map::DefaultHasher;
#[cfg(test)]
use std::hash::{Hash, Hasher};

use crate::cli::Args;

/// Create a styled progress bar
pub fn create_progress_bar(total: u64) -> ProgressBar {
    let pb = ProgressBar::new(total);
    pb.set_style(
        ProgressStyle::with_template(
            "{spinner:.blue} [{elapsed_precise}] [{wide_bar:.cyan/blue}] {pos}/{len} {msg} ({eta})",
        )
        .unwrap()
        .progress_chars("#>-"),
    );
    pb
}

/// Format duration in a human-readable way
pub fn format_duration(duration: Duration) -> String {
    let total_secs = duration.as_secs();
    let millis = duration.subsec_millis();

    if total_secs >= 60 {
        let mins = total_secs / 60;
        let secs = total_secs % 60;
        format!("{}m {}s", mins, secs)
    } else if total_secs > 0 {
        format!("{}.{:03}s", total_secs, millis)
    } else {
        format!("{}ms", duration.as_millis())
    }
}

/// Validate command line arguments
pub fn validate_inputs(args: &Args) -> Result<()> {
    // Validate input paths (directories or files)
    for input_path in &args.input_paths {
        if !input_path.exists() {
            return Err(anyhow::anyhow!(
                "Input path does not exist: {}",
                input_path.display()
            ));
        }
        if !input_path.is_dir() && !input_path.is_file() {
            return Err(anyhow::anyhow!(
                "Input path is neither a file nor a directory: {}",
                input_path.display()
            ));
        }
    }

    // Validate size format
    args.parse_size()
        .map_err(|e| anyhow::anyhow!("Size validation failed: {}", e))?;

    // Validate extensions
    let extensions = args.parse_extensions();
    if extensions.is_empty() {
        return Err(anyhow::anyhow!("No valid extensions specified"));
    }

    // Validate font size
    if args.pointsize == 0 || args.pointsize > 200 {
        return Err(anyhow::anyhow!(
            "Font size must be between 1 and 200 pixels, got: {}",
            args.pointsize
        ));
    }

    // Validate job count
    if args.jobs > 32 {
        return Err(anyhow::anyhow!(
            "Job count too high (max 32), got: {}",
            args.jobs
        ));
    }

    // Validate annotation background color format
    if !is_valid_hex_color(&args.annotate_background) {
        return Err(anyhow::anyhow!(
            "Invalid annotation background color format: '{}'. Expected hex format like #RRGGBBAA",
            args.annotate_background
        ));
    }

    // Validate Python script configuration
    {
        if args.detect_people {
            // Validate Python script path
            if let Some(script_path) = &args.python_script_path {
                if !script_path.exists() {
                    return Err(anyhow::anyhow!(
                        "Python script does not exist: {}",
                        script_path.display()
                    ));
                }
                if !script_path.is_file() {
                    return Err(anyhow::anyhow!(
                        "Python script path is not a file: {}",
                        script_path.display()
                    ));
                }
            } else {
                return Err(anyhow::anyhow!(
                    "Python script path is required when people detection is enabled. \
                     Use --python-script <FILE> to specify the path to find_subject.py"
                ));
            }
        }
    }

    Ok(())
}

/// Check if a string is a valid hex color (with optional alpha)
fn is_valid_hex_color(color: &str) -> bool {
    if !color.starts_with('#') {
        return false;
    }

    let hex_part = &color[1..];

    // Accept #RGB, #RRGGBB, #RRGGBBAA formats
    match hex_part.len() {
        3 | 6 | 8 => hex_part.chars().all(|c| c.is_ascii_hexdigit()),
        _ => false,
    }
}

/// Get file extension in lowercase
pub fn get_file_extension(path: &Path) -> Option<String> {
    path.extension()
        .and_then(|ext| ext.to_str())
        .map(|ext| ext.to_lowercase())
}

/// Check if a file has one of the specified extensions
pub fn has_valid_extension(path: &Path, extensions: &[String]) -> bool {
    if let Some(ext) = get_file_extension(path) {
        extensions.contains(&ext)
    } else {
        false
    }
}

/// Generate a safe filename by removing/replacing invalid characters
#[allow(dead_code)]
pub fn sanitize_filename(filename: &str) -> String {
    filename
        .chars()
        .map(|c| match c {
            // Replace problematic characters with underscores
            '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|' => '_',
            // Keep other characters as-is
            c if c.is_ascii_graphic() || c.is_ascii_whitespace() => c,
            // Replace non-ASCII with underscore
            _ => '_',
        })
        .collect::<String>()
        .trim()
        .to_string()
}


/// Encode filename to base64 and replace '/' with '-' to match auto.sh convention
/// This matches: echo -n "$basename" | base64 -w0 | tr '/' '-'
pub fn encode_filename_base64(filename: &str) -> String {
    let encoded = general_purpose::STANDARD.encode(filename.as_bytes());
    encoded.replace('/', "-")
}

/// Create output filename based on input filename base64 encoding with processing type prefix
/// Format: prefix_base64(basename).extension or prefix_base64(basename)_suffix.extension
pub fn create_output_filename(
    input_path: &Path,
    suffix: Option<&str>,
    extension: &str,
    processing_type_prefix: &str
) -> String {
    let stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image");

    let base64_filename = encode_filename_base64(stem);

    match suffix {
        Some(suffix) => format!("{}_{}__{}.{}", processing_type_prefix, base64_filename, suffix, extension),
        None => format!("{}_{}.{}", processing_type_prefix, base64_filename, extension),
    }
}

/// Create combined portrait filename using base64 encoding to match auto.sh convention
/// Format: combined_prefix_base64(basename1)_base64(basename2).extension
pub fn create_combined_portrait_filename(
    left_path: &Path,
    right_path: &Path,
    extension: &str,
    processing_type_prefix: &str,
) -> String {
    let left_stem = left_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image1");
    let right_stem = right_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image2");

    let base64_file1 = encode_filename_base64(left_stem);
    let base64_file2 = encode_filename_base64(right_stem);

    format!("combined_{}_{}_{}.{}", processing_type_prefix, base64_file1, base64_file2, extension)
}

/// Print verbose information if verbose mode is enabled
pub fn verbose_println(verbose: bool, message: &str) {
    if verbose {
        println!("{} {}", style("[VERBOSE]").dim(), message);
    }
}

/// Print warning message
#[allow(dead_code)]
pub fn warn_println(message: &str) {
    println!("{} {}", style("[WARNING]").yellow().bold(), message);
}

/// Print error message
#[allow(dead_code)]
pub fn error_println(message: &str) {
    eprintln!("{} {}", style("[ERROR]").red().bold(), message);
}

/// Generate an 8-character hash from a filename for duplicate detection
/// This creates a deterministic hash that's used to identify potential duplicates
#[cfg(test)]
pub fn generate_filename_hash(filename: &str) -> String {
    let mut hasher = DefaultHasher::new();
    filename.hash(&mut hasher);
    let hash = hasher.finish();

    // Convert to hex and take first 8 characters
    format!("{:016x}", hash)[..8].to_string()
}


/// Calculate processing statistics
#[allow(dead_code)]
#[derive(Debug)]
pub struct ProcessingStats {
    pub total_files: usize,
    pub successful: usize,
    pub failed: usize,
    pub portraits_found: usize,
    pub landscapes_found: usize,
    pub portraits_combined: usize,
    pub total_duration: Duration,
}

impl ProcessingStats {
    #[allow(dead_code)]
    pub fn new(total_files: usize) -> Self {
        Self {
            total_files,
            successful: 0,
            failed: 0,
            portraits_found: 0,
            landscapes_found: 0,
            portraits_combined: 0,
            total_duration: Duration::new(0, 0),
        }
    }

    #[allow(dead_code)]
    pub fn success_rate(&self) -> f64 {
        if self.total_files == 0 {
            0.0
        } else {
            (self.successful as f64 / self.total_files as f64) * 100.0
        }
    }

    #[allow(dead_code)]
    pub fn average_duration(&self) -> Duration {
        if self.successful == 0 {
            Duration::new(0, 0)
        } else {
            self.total_duration / self.successful as u32
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format_duration() {
        assert_eq!(format_duration(Duration::from_millis(500)), "500ms");
        assert_eq!(format_duration(Duration::from_secs(1)), "1.000s");
        assert_eq!(format_duration(Duration::from_secs(65)), "1m 5s");
    }

    #[test]
    fn test_is_valid_hex_color() {
        assert!(is_valid_hex_color("#000"));
        assert!(is_valid_hex_color("#000000"));
        assert!(is_valid_hex_color("#00000000"));
        assert!(is_valid_hex_color("#FF00FF80"));

        assert!(!is_valid_hex_color("000000"));
        assert!(!is_valid_hex_color("#GG0000"));
        assert!(!is_valid_hex_color("#00"));
        assert!(!is_valid_hex_color("#0000000000"));
    }

    #[test]
    fn test_sanitize_filename() {
        assert_eq!(sanitize_filename("normal.jpg"), "normal.jpg");
        assert_eq!(
            sanitize_filename("file/with\\bad:chars"),
            "file_with_bad_chars"
        );
        assert_eq!(sanitize_filename("file*with?quotes\""), "file_with_quotes_");
    }

    #[test]
    fn test_create_output_filename() {
        let path = Path::new("/path/to/image.jpg");
        let result = create_output_filename(path, None, "bmp", "bw");
        assert!(result.ends_with(".bmp"));
        // Base64 encoding of "image" should be "aW1hZ2U="
        assert_eq!(result, "bw_aW1hZ2U=.bmp");

        let result_with_suffix = create_output_filename(path, Some("portrait"), "bin", "6c");
        assert!(result_with_suffix.ends_with(".bin"));
        assert!(result_with_suffix.contains("_portrait"));
        assert_eq!(result_with_suffix, "6c_aW1hZ2U=__portrait.bin");
    }

    #[test]
    fn test_generate_filename_hash() {
        let hash1 = generate_filename_hash("test.jpg");
        let hash2 = generate_filename_hash("test.jpg");
        let hash3 = generate_filename_hash("different.jpg");

        assert_eq!(hash1, hash2); // Same input should give same hash
        assert_ne!(hash1, hash3); // Different input should give different hash
        assert_eq!(hash1.len(), 8); // Should be 8 characters
    }

    #[test]
    fn test_create_combined_portrait_filename() {
        let left_path = Path::new("/path/to/image1.jpg");
        let right_path = Path::new("/path/to/image2.jpg");
        let result = create_combined_portrait_filename(left_path, right_path, "bmp", "bw");

        assert!(result.starts_with("combined_"));
        assert!(result.ends_with(".bmp"));
        assert!(result.matches('_').count() == 3); // Should have exactly 3 underscores now (prefix adds one)

        // Base64 encoding of "image1" = "aW1hZ2Ux", "image2" = "aW1hZ2Uy"
        assert_eq!(result, "combined_bw_aW1hZ2Ux_aW1hZ2Uy.bmp");
    }

    #[test]
    fn test_encode_filename_base64() {
        // Test basic encoding
        assert_eq!(encode_filename_base64("image"), "aW1hZ2U=");
        assert_eq!(encode_filename_base64("test"), "dGVzdA==");

        // Test '/' replacement with '-' using a string that produces '/' in base64
        // "sure." encodes to "c3VyZS4=" which contains no '/', but "???>" produces "Pz8/Pg==" with '/'
        let test_string = "???>"; // This should produce base64 with '/' characters
        let encoded = encode_filename_base64(test_string);
        let raw_base64 = general_purpose::STANDARD.encode(test_string.as_bytes());

        // Verify our function replaces '/' with '-'
        let expected = raw_base64.replace('/', "-");
        assert_eq!(encoded, expected);

        // Make sure no '/' characters remain
        assert!(!encoded.contains('/'));

        // Test that the replacement actually happened if there were '/' chars in raw base64
        if raw_base64.contains('/') {
            assert!(encoded.contains('-'));
        }
    }
}
