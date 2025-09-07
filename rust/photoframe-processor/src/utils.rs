use anyhow::Result;
use console::style;
use indicatif::{ProgressBar, ProgressStyle};
use std::path::Path;
use std::time::Duration;

use crate::cli::Args;

/// Create a styled progress bar
pub fn create_progress_bar(total: u64) -> ProgressBar {
    let pb = ProgressBar::new(total);
    pb.set_style(
        ProgressStyle::with_template(
            "{spinner:.blue} [{elapsed_precise}] [{wide_bar:.cyan/blue}] {pos}/{len} {msg} ({eta})"
        )
        .unwrap()
        .progress_chars("#>-")
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
    // Validate input directories
    for input_dir in &args.input_dirs {
        if !input_dir.exists() {
            return Err(anyhow::anyhow!(
                "Input directory does not exist: {}",
                input_dir.display()
            ));
        }
        if !input_dir.is_dir() {
            return Err(anyhow::anyhow!(
                "Input path is not a directory: {}",
                input_dir.display()
            ));
        }
    }

    // Validate size format
    args.parse_size()
        .map_err(|e| anyhow::anyhow!("Size validation failed: {}", e))?;

    // Validate palette file if provided
    if let Some(palette_path) = &args.palette {
        if !palette_path.exists() {
            return Err(anyhow::anyhow!(
                "Palette file does not exist: {}",
                palette_path.display()
            ));
        }
        if !palette_path.is_file() {
            return Err(anyhow::anyhow!(
                "Palette path is not a file: {}",
                palette_path.display()
            ));
        }
    }

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

/// Create output filename based on input filename and processing type
pub fn create_output_filename(input_path: &Path, suffix: Option<&str>, extension: &str) -> String {
    let stem = input_path.file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image");
    
    let sanitized_stem = sanitize_filename(stem);
    
    match suffix {
        Some(suffix) => format!("{}_{}.{}", sanitized_stem, suffix, extension),
        None => format!("{}.{}", sanitized_stem, extension),
    }
}

/// Print verbose information if verbose mode is enabled
pub fn verbose_println(verbose: bool, message: &str) {
    if verbose {
        println!("{} {}", style("[VERBOSE]").dim(), message);
    }
}

/// Print warning message
pub fn warn_println(message: &str) {
    println!("{} {}", style("[WARNING]").yellow().bold(), message);
}

/// Print error message
pub fn error_println(message: &str) {
    eprintln!("{} {}", style("[ERROR]").red().bold(), message);
}

/// Calculate processing statistics
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

    pub fn success_rate(&self) -> f64 {
        if self.total_files == 0 {
            0.0
        } else {
            (self.successful as f64 / self.total_files as f64) * 100.0
        }
    }

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
        assert_eq!(sanitize_filename("file/with\\bad:chars"), "file_with_bad_chars");
        assert_eq!(sanitize_filename("file*with?quotes\""), "file_with_quotes_");
    }

    #[test]
    fn test_create_output_filename() {
        let path = Path::new("/path/to/image.jpg");
        assert_eq!(create_output_filename(path, None, "bmp"), "image.bmp");
        assert_eq!(create_output_filename(path, Some("processed"), "bin"), "image_processed.bin");
    }
}