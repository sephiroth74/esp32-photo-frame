//! Asset management for models
//!
//! This module handles loading ONNX models from a specified directory.

use crate::error::{InsightFaceError, Result};
use std::path::{Path, PathBuf};

/// Asset manager for models
#[derive(Debug)]
pub struct AssetManager {
    models_dir: PathBuf,
}

impl AssetManager {
    /// Create a new asset manager with an explicit models directory
    pub fn new<P: AsRef<Path>>(models_dir: P) -> Result<Self> {
        let models_dir = models_dir.as_ref().to_path_buf();

        if !models_dir.exists() {
            return Err(InsightFaceError::ModelNotFound(format!(
                "Models directory does not exist: {}",
                models_dir.display()
            )));
        }

        Ok(AssetManager { models_dir })
    }

    /// Get a model file from the models directory
    pub fn get_model(&self, model_name: &str) -> Result<PathBuf> {
        let model_path = self.models_dir.join(model_name);

        if model_path.exists() {
            Ok(model_path)
        } else {
            Err(InsightFaceError::ModelNotFound(format!(
                "Model '{}' not found in directory: {}",
                model_name,
                self.models_dir.display()
            )))
        }
    }

    /// Get the models directory path
    pub fn models_dir(&self) -> &Path {
        &self.models_dir
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn test_asset_manager_creation() {
        // Create a temporary directory for testing
        let temp_dir = std::env::temp_dir().join("test_models");
        fs::create_dir_all(&temp_dir).unwrap();

        let manager = AssetManager::new(&temp_dir);
        assert!(manager.is_ok());

        // Cleanup
        let _ = fs::remove_dir_all(&temp_dir);
    }
}
