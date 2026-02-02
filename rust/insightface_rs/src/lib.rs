//! # InsightFace Rust Library
//!
//! A high-performance Rust library for face detection and recognition using ONNX models.
//! This library provides a Rust interface to InsightFace models, supporting face detection,
//! landmark detection, face recognition, and attribute analysis.
//!
//! ## Features
//!
//! - **Face Detection**: RetinaFace-based detection with high accuracy
//! - **EXIF Orientation Support**: Automatic handling of rotated images
//! - **Non-Maximum Suppression (NMS)**: Configurable IoU-based filtering
//! - **Multiple Input Formats**: Accept both `DynamicImage` objects and file paths
//! - **High Performance**: Optimized with ONNX Runtime
//!
//! ## Quick Start
//!
//! ### From Image Object
//!
//! ```no_run
//! use insightface_rs::FaceAnalysis;
//! use std::path::Path;
//!
//! // Initialize the face analysis with models directory
//! let mut app = FaceAnalysis::new(
//!     Path::new("~/.insightface/models/buffalo_l"),
//!     None,
//! )?;
//!
//! // Prepare model: ctx_id, detection_threshold, input_size
//! app.prepare(0, 0.5, (640, 640))?;
//!
//! // Detect faces from an image object
//! let img = image::open("photo.jpg")?;
//! let faces = app.get(&img)?;
//!
//! // Process detected faces
//! for face in faces {
//!     println!("Face at [{:.0}, {:.0}, {:.0}, {:.0}] with confidence {:.2}",
//!         face.bbox[0], face.bbox[1], face.bbox[2], face.bbox[3],
//!         face.det_score);
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ### From File Path (with automatic EXIF orientation)
//!
//! ```no_run
//! use insightface_rs::FaceAnalysis;
//! use std::path::Path;
//!
//! let mut app = FaceAnalysis::new(
//!     Path::new("~/.insightface/models/buffalo_l"),
//!     None,
//! )?;
//! app.prepare(0, 0.5, (640, 640))?;
//!
//! // Detect faces directly from file path
//! // Automatically handles EXIF orientation from smartphones
//! let faces = app.get_from_path("photo.jpg")?;
//! println!("Found {} faces", faces.len());
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## Configuration
//!
//! - **Detection Threshold** (`det_thresh`): Minimum confidence score (0.0-1.0). Default: 0.5
//! - **NMS Threshold** (`nms_thresh`): IoU threshold for non-maximum suppression. Default: 0.4
//! - **Input Size**: Model input dimensions (height, width). Standard: (640, 640)
//!
//! ## Models
//!
//! Download models from [InsightFace Model Zoo](https://github.com/deepinsight/insightface/releases/tag/v0.7).
//! The library uses the buffalo_l model pack by default:
//!
//! - `det_10g.onnx`: RetinaFace detection model
//! - `w600k_r50.onnx`: Recognition model (ArcFace)
//! - `genderage.onnx`: Gender and age estimation
//! - `2d106det.onnx`: 106-point facial landmarks
//! - `1k3d68.onnx`: 68-point 3D landmarks

pub mod app;
pub mod assets;
pub mod error;
pub mod face;
pub mod model_zoo;
pub mod utils;

pub use app::FaceAnalysis;
pub use assets::AssetManager;
pub use error::InsightFaceError;
pub use face::Face;
