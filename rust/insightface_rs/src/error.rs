//! Error types for InsightFace

use thiserror::Error;

#[derive(Error, Debug)]
pub enum InsightFaceError {
    #[error("ONNX Runtime error: {0}")]
    OnnxError(String),

    #[error("Image error: {0}")]
    ImageError(#[from] image::ImageError),

    #[error("Model not found: {0}")]
    ModelNotFound(String),

    #[error("Model preparation failed: {0}")]
    ModelPrepareError(String),

    #[error("Detection failed: {0}")]
    DetectionError(String),

    #[error("Invalid input: {0}")]
    InvalidInput(String),

    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),

    #[error("Model initialization error")]
    InitializationError,

    #[error("Unexpected model output")]
    UnexpectedModelOutput,
}

pub type Result<T> = std::result::Result<T, InsightFaceError>;
