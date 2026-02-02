//! ONNX model management and loading

use crate::assets::AssetManager;
use crate::error::Result;
use std::path::{Path, PathBuf};

/// Configuration for model inference
#[derive(Debug, Clone)]
pub struct ModelConfig {
    /// Input image size (height, width)
    pub input_size: (i64, i64),
    /// Model input node name
    pub input_name: String,
    /// Model output node names
    pub output_names: Vec<String>,
    /// Whether to normalize pixel values [0, 255] to [-1, 1]
    pub normalize: bool,
}

/// Detection model configuration
#[derive(Debug, Clone)]
pub struct DetectionConfig {
    pub model_config: ModelConfig,
    /// Detection threshold for score filtering
    pub det_thresh: f32,
}

impl DetectionConfig {
    pub fn new(model_config: ModelConfig) -> Self {
        DetectionConfig {
            model_config,
            det_thresh: 0.5,
        }
    }

    pub fn with_threshold(mut self, thresh: f32) -> Self {
        self.det_thresh = thresh;
        self
    }
}

/// Available model types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ModelType {
    RetinaFace,
    SCRFD,
    ArcFace,
}

/// Trait for models that can be loaded from ONNX
pub trait OnnxModel: Send + Sync {
    /// Get model input size
    fn input_size(&self) -> (i64, i64);

    /// Get input node name
    fn input_name(&self) -> &str;

    /// Get output node names
    fn output_names(&self) -> Vec<&str>;

    /// Get model path relative to model store
    fn model_path(&self) -> &str;
}

/// RetinaFace detection model
pub struct RetinaFaceModel;

impl OnnxModel for RetinaFaceModel {
    fn input_size(&self) -> (i64, i64) {
        (640, 640)
    }

    fn input_name(&self) -> &str {
        "input.1"
    }

    fn output_names(&self) -> Vec<&str> {
        vec!["scores", "boxes", "kps"]
    }

    fn model_path(&self) -> &str {
        "det_10g.onnx"
    }
}

/// SCRFD detection model
pub struct SCRFDModel;

impl OnnxModel for SCRFDModel {
    fn input_size(&self) -> (i64, i64) {
        (640, 640)
    }

    fn input_name(&self) -> &str {
        "input.1"
    }

    fn output_names(&self) -> Vec<&str> {
        vec!["scores", "boxes", "kps"]
    }

    fn model_path(&self) -> &str {
        "detection_scrfd_500m.onnx"
    }
}

/// ArcFace recognition model
pub struct ArcFaceModel;

impl OnnxModel for ArcFaceModel {
    fn input_size(&self) -> (i64, i64) {
        (112, 112)
    }

    fn input_name(&self) -> &str {
        "data"
    }

    fn output_names(&self) -> Vec<&str> {
        vec!["fc1"]
    }

    fn model_path(&self) -> &str {
        "arcface_w600k_r50.onnx"
    }
}

/// Model store for managing ONNX models
#[derive(Debug)]
pub struct ModelStore {
    asset_manager: AssetManager,
}

impl ModelStore {
    /// Create a new model store with a specific models directory
    pub fn new<P: AsRef<Path>>(models_dir: P) -> Result<Self> {
        let asset_manager = AssetManager::new(models_dir)?;
        Ok(ModelStore { asset_manager })
    }

    /// Get path to a model file
    pub fn get_model_path(&self, model_name: &str) -> Result<PathBuf> {
        self.asset_manager.get_model(model_name)
    }

    /// Get RetinaFace model path
    pub fn retinaface(&self) -> Result<PathBuf> {
        self.get_model_path(RetinaFaceModel.model_path())
    }

    /// Get SCRFD model path
    pub fn scrfd(&self) -> Result<PathBuf> {
        self.get_model_path(SCRFDModel.model_path())
    }

    /// Get ArcFace model path
    pub fn arcface(&self) -> Result<PathBuf> {
        self.get_model_path(ArcFaceModel.model_path())
    }

    /// Get the models directory path
    pub fn models_dir(&self) -> &Path {
        self.asset_manager.models_dir()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_model_paths() {
        assert_eq!(RetinaFaceModel.model_path(), "det_10g.onnx");
        assert_eq!(SCRFDModel.model_path(), "detection_scrfd_500m.onnx");
        assert_eq!(ArcFaceModel.model_path(), "arcface_w600k_r50.onnx");
    }

    #[test]
    fn test_input_sizes() {
        assert_eq!(RetinaFaceModel.input_size(), (640, 640));
        assert_eq!(SCRFDModel.input_size(), (640, 640));
        assert_eq!(ArcFaceModel.input_size(), (112, 112));
    }
}
