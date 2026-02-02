use anyhow::{Context, Result};
use std::collections::HashSet;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

use crate::logging::Logger;

/// Discovery class that scans input paths and returns all matching files.
pub struct Discovery<'a> {
    extensions: HashSet<String>,
    logger: &'a Logger,
}

impl<'a> Discovery<'a> {
    /// Create a new Discovery instance with a list of allowed extensions.
    ///
    /// Extensions are normalized to lowercase and stripped of any leading dots.
    pub fn new(extensions: &str, logger: &'a Logger) -> Self {
        let mut normalized = HashSet::new();

        for ext in extensions.split(',') {
            let normalized_ext = ext.trim().trim_start_matches('.').to_lowercase();
            if !normalized_ext.is_empty() {
                normalized.insert(normalized_ext);
            }
        }

        Self {
            extensions: normalized,
            logger,
        }
    }

    /// Scan all input paths (files or directories) and return a full list of
    /// matching files found recursively.
    pub fn discover(&self, inputs: &[PathBuf]) -> Result<Vec<PathBuf>> {
        let mut files: HashSet<PathBuf> = HashSet::new();

        if inputs.is_empty() {
            self.logger.warning("No input paths provided for discovery");
            return Ok(Vec::new());
        }

        for input in inputs {
            if input.is_file() {
                self.process_file(input, &mut files);
            } else if input.is_dir() {
                self.process_dir(input, &mut files)
                    .with_context(|| format!("Failed to scan directory: {}", input.display()))?;
            } else {
                self.logger.warning(&format!(
                    "Input path is neither file nor directory: {}",
                    input.display()
                ));
            }
        }

        let mut results: Vec<PathBuf> = files.into_iter().collect();
        results.sort();

        self.logger
            .info(&format!("Found {} matching file(s)", results.len()));

        Ok(results)
    }

    fn process_dir(&self, dir: &Path, files: &mut HashSet<PathBuf>) -> Result<()> {
        self.logger
            .verbose(&format!("Scanning directory: {}", dir.display()));

        for entry in WalkDir::new(dir).follow_links(false) {
            let entry = entry.with_context(|| "Failed to read directory entry")?;
            if entry.file_type().is_file() {
                self.process_file(entry.path(), files);
            }
        }

        Ok(())
    }

    fn process_file(&self, path: &Path, files: &mut HashSet<PathBuf>) {
        if self.is_valid_extension(path) {
            if files.insert(path.to_path_buf()) {
                self.logger
                    .verbose(&format!("Discovered file: {}", path.display()));
            }
        } else {
            self.logger.verbose(&format!(
                "Skipping (extension mismatch): {}",
                path.display()
            ));
        }
    }

    fn is_valid_extension(&self, path: &Path) -> bool {
        let ext = path
            .extension()
            .and_then(|s| s.to_str())
            .unwrap_or_default()
            .trim()
            .to_lowercase();

        if ext.is_empty() {
            return false;
        }

        self.extensions.contains(&ext)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::logging::TEST_LOGGER;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn test_discover_files_recursive() -> Result<()> {
        let temp = TempDir::new()?;
        let root = temp.path();
        let sub = root.join("sub");
        fs::create_dir_all(&sub)?;

        let jpg = root.join("photo.jpg");
        let png = root.join("image.PNG");
        let txt = root.join("notes.txt");
        let jpeg = sub.join("nested.jpeg");

        fs::write(&jpg, b"a")?;
        fs::write(&png, b"b")?;
        fs::write(&txt, b"c")?;
        fs::write(&jpeg, b"d")?;

        let discovery = Discovery::new("jpg,jpeg,png", &TEST_LOGGER);

        let results = discovery.discover(&vec![root.to_path_buf()])?;
        let paths: HashSet<PathBuf> = results.into_iter().collect();

        assert!(paths.contains(&jpg));
        assert!(paths.contains(&png));
        assert!(paths.contains(&jpeg));
        assert!(!paths.contains(&txt));

        Ok(())
    }

    #[test]
    fn test_discover_single_file() -> Result<()> {
        let temp = TempDir::new()?;
        let root = temp.path();
        let file = root.join("only.webp");
        fs::write(&file, b"x")?;

        let discovery = Discovery::new("webp", &TEST_LOGGER);

        let results = discovery.discover(&vec![file.clone()])?;
        assert_eq!(results.len(), 1);
        assert_eq!(results[0], file);

        Ok(())
    }

    #[test]
    fn test_discover_ignores_non_matching_extensions() -> Result<()> {
        let temp = TempDir::new()?;
        let root = temp.path();
        let file = root.join("only.bmp");
        fs::write(&file, b"x")?;

        let discovery = Discovery::new("jpg", &TEST_LOGGER);

        let results = discovery.discover(&vec![file])?;
        assert!(results.is_empty());

        Ok(())
    }
}
