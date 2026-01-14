use anyhow::Result;
use console::style;
use indicatif::{ProgressBar, ProgressStyle};
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::Read;
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

    // Size is determined automatically from display type and orientation
    // No validation needed

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

    // Validate people detection is available
    {
        if args.detect_people() {
            #[cfg(not(feature = "ai"))]
            {
                return Err(anyhow::anyhow!(
                    "People detection is not available. \
                     Rebuild with --features ai to enable native ONNX people detection"
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

/// Sanitize filename for FAT32 filesystem compatibility
/// Replaces invalid characters and ensures the filename is safe for FAT32
pub fn sanitize_filename_for_fat32(filename: &str) -> String {
    // FAT32 invalid characters: < > : " / \ | ? *
    // Also control characters (0-31) and character 127
    let mut sanitized = String::with_capacity(filename.len());

    for ch in filename.chars() {
        let replacement = match ch {
            '<' | '>' | ':' | '"' | '/' | '\\' | '|' | '?' | '*' => '_',
            c if c.is_control() => '_',
            c if (c as u32) == 127 => '_',
            // Replace other problematic characters
            '\t' | '\n' | '\r' => '_',
            // Keep the character if it's valid
            c => c,
        };
        sanitized.push(replacement);
    }

    // Replace consecutive underscores with a single underscore
    let mut result = String::new();
    let mut prev_was_underscore = false;

    for ch in sanitized.chars() {
        if ch == '_' {
            if !prev_was_underscore {
                result.push(ch);
                prev_was_underscore = true;
            }
        } else {
            result.push(ch);
            prev_was_underscore = false;
        }
    }

    // Trim underscores from start and end
    let trimmed = result.trim_matches('_');

    // If the filename ends with an underscore followed by a file extension, remove the underscore
    let cleaned = if let Some(dot_pos) = trimmed.rfind('.') {
        if dot_pos > 0 && trimmed.chars().nth(dot_pos - 1) == Some('_') {
            let mut s = trimmed.to_string();
            s.remove(dot_pos - 1);
            s
        } else {
            trimmed.to_string()
        }
    } else {
        trimmed.to_string()
    };

    // Truncate to a reasonable length (100 chars) to leave room for prefix and hash
    const MAX_NAME_LENGTH: usize = 100;
    if cleaned.len() > MAX_NAME_LENGTH {
        // Try to cut at a word boundary if possible
        let truncated = &cleaned[..MAX_NAME_LENGTH];
        if let Some(last_underscore) = truncated.rfind('_') {
            if last_underscore >= 80 {
                // Keep at least 80 chars
                &truncated[..last_underscore]
            } else {
                truncated
            }
        } else {
            truncated
        }
        .to_string()
    } else {
        cleaned
    }
}

/// Generate an 8-character hash from file content for uniqueness
/// Uses SHA256 and takes the first 8 hex characters
pub fn generate_content_hash(file_path: &Path) -> Result<String> {
    // Read first 4KB of file for hash (faster for large files)
    let mut file = File::open(file_path)?;
    let mut buffer = vec![0; 4096];
    let bytes_read = file.read(&mut buffer)?;
    buffer.truncate(bytes_read);

    // Also include file size and name for better uniqueness
    let metadata = std::fs::metadata(file_path)?;
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
    let hex_hash = format!("{:x}", result);

    // Return first 8 characters of hash
    Ok(hex_hash[..8].to_string())
}

/// Create a readable filename with hash for uniqueness
/// Format: {prefix}_{sanitized_name}_{hash8}.{ext}
pub fn create_readable_filename(
    input_path: &Path,
    processing_type_prefix: &str,
    extension: &str,
) -> Result<String> {
    let stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image");

    let sanitized = sanitize_filename_for_fat32(stem);
    let hash = generate_content_hash(input_path)?;

    Ok(format!(
        "{}_{}_{}.{}",
        processing_type_prefix, sanitized, hash, extension
    ))
}

/// Create readable combined portrait filename
/// Format: combined_{prefix}_{name1}_{name2}_{hash8}.{ext}
pub fn create_readable_combined_filename(
    left_path: &Path,
    right_path: &Path,
    processing_type_prefix: &str,
    extension: &str,
) -> Result<String> {
    let left_stem = left_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image1");
    let right_stem = right_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("image2");

    let left_sanitized = sanitize_filename_for_fat32(left_stem);
    let right_sanitized = sanitize_filename_for_fat32(right_stem);

    // Truncate names if combined would be too long
    const MAX_COMBINED_LENGTH: usize = 40; // 40 chars each for two names
    let left_truncated = if left_sanitized.len() > MAX_COMBINED_LENGTH {
        &left_sanitized[..MAX_COMBINED_LENGTH]
    } else {
        &left_sanitized
    };

    let right_truncated = if right_sanitized.len() > MAX_COMBINED_LENGTH {
        &right_sanitized[..MAX_COMBINED_LENGTH]
    } else {
        &right_sanitized
    };

    // Generate combined hash from both files
    let left_hash = generate_content_hash(left_path)?;
    let right_hash = generate_content_hash(right_path)?;
    let mut hasher = Sha256::new();
    hasher.update(left_hash.as_bytes());
    hasher.update(right_hash.as_bytes());
    let combined_result = hasher.finalize();
    let combined_hash = format!("{:x}", combined_result)[..8].to_string();

    Ok(format!(
        "combined_{}_{}_{}_{}.{}",
        processing_type_prefix, left_truncated, right_truncated, combined_hash, extension
    ))
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
    fn test_generate_filename_hash() {
        let hash1 = generate_filename_hash("test.jpg");
        let hash2 = generate_filename_hash("test.jpg");
        let hash3 = generate_filename_hash("different.jpg");

        assert_eq!(hash1, hash2); // Same input should give same hash
        assert_ne!(hash1, hash3); // Different input should give different hash
        assert_eq!(hash1.len(), 8); // Should be 8 characters
    }

    #[test]
    fn test_sanitize_filename_for_fat32() {
        // Test normal filenames
        assert_eq!(sanitize_filename_for_fat32("normal_file"), "normal_file");
        assert_eq!(
            sanitize_filename_for_fat32("file-with-dashes"),
            "file-with-dashes"
        );

        // Test FAT32 invalid characters
        assert_eq!(sanitize_filename_for_fat32("file<>test"), "file_test");
        assert_eq!(
            sanitize_filename_for_fat32("file:with:colons"),
            "file_with_colons"
        );
        assert_eq!(
            sanitize_filename_for_fat32("file/with\\slashes"),
            "file_with_slashes"
        );
        assert_eq!(
            sanitize_filename_for_fat32("file|pipe?question*star"),
            "file_pipe_question_star"
        );
        assert_eq!(sanitize_filename_for_fat32("\"quoted\""), "quoted");

        // Test control characters
        assert_eq!(
            sanitize_filename_for_fat32("file\twith\ttabs"),
            "file_with_tabs"
        );
        assert_eq!(
            sanitize_filename_for_fat32("file\nwith\nnewlines"),
            "file_with_newlines"
        );
        assert_eq!(
            sanitize_filename_for_fat32("file\rwith\rreturns"),
            "file_with_returns"
        );

        // Test consecutive underscores
        assert_eq!(
            sanitize_filename_for_fat32("file___multiple___underscores"),
            "file_multiple_underscores"
        );
        assert_eq!(sanitize_filename_for_fat32("___leading"), "leading");
        assert_eq!(sanitize_filename_for_fat32("trailing___"), "trailing");

        // Test length truncation (create a 110 char filename)
        let long_name = "a".repeat(110);
        let sanitized = sanitize_filename_for_fat32(&long_name);
        assert!(sanitized.len() <= 100);
        assert_eq!(sanitized.len(), 100);

        // Test Unicode characters (should be preserved if valid)
        assert_eq!(sanitize_filename_for_fat32("日本語"), "日本語");
        assert_eq!(sanitize_filename_for_fat32("фото"), "фото");
        assert_eq!(sanitize_filename_for_fat32("café"), "café");

        // Test mixed problematic cases
        assert_eq!(
            sanitize_filename_for_fat32("My:Photo<2024>|vacation*.jpg"),
            "My_Photo_2024_vacation.jpg"
        );
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
