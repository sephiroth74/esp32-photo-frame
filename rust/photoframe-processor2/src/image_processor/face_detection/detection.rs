use crate::image_processor::face_detection::Face;
/// Face detection module using InsightFace
///
/// This module provides face detection using the insightface-rs library,
/// which uses RetinaFace model for accurate face detection.
use anyhow::{Context, Result};
use image::RgbImage;
use insightface_rs::FaceAnalysis;
use std::path::Path;
use std::sync::{Arc, Mutex, OnceLock};
use std::time::Instant;

/// Global InsightFace detector instance
static GLOBAL_DETECTOR: OnceLock<Arc<Mutex<FaceAnalysis>>> = OnceLock::new();

/// Initialize the global face detector - returns error on failure
fn try_get_detector() -> Result<Arc<Mutex<FaceAnalysis>>> {
    // Check if already initialized
    if let Some(detector) = GLOBAL_DETECTOR.get() {
        return Ok(detector.clone());
    }

    // Initialize new detector
    let models_dir = if let Some(home) = std::env::var_os("HOME") {
        let home_path = Path::new(&home).join(".insightface/models/buffalo_l");
        if home_path.exists() {
            home_path
        } else {
            anyhow::bail!("InsightFace models not found at ~/.insightface/models/buffalo_l")
        }
    } else {
        anyhow::bail!("HOME environment variable not set")
    };

    let mut app =
        FaceAnalysis::new(&models_dir, None).context("Failed to initialize InsightFace")?;

    // Prepare with standard settings: ctx_id=0, det_thresh=0.5, input_size=(640, 640)
    app.prepare(0, 0.5, (640, 640))
        .context("Failed to prepare InsightFace model")?;

    let detector = Arc::new(Mutex::new(app));

    // Try to store in global, but use our instance regardless
    let _ = GLOBAL_DETECTOR.set(detector.clone());

    Ok(detector)
}

/// Detect faces in an image
///
/// # Arguments
/// * `img` - RGB image to detect faces in
/// * `confidence_threshold` - Minimum confidence score for face detection (0.0-1.0)
///
/// # Returns
/// Vector of detected faces with bounding boxes and confidence scores
pub fn detect_faces(img: &RgbImage, confidence_threshold: f32) -> Result<Vec<Face>> {
    let detector = try_get_detector()?;
    let mut detector_lock = detector
        .lock()
        .map_err(|e| anyhow::anyhow!("Failed to lock detector: {}", e))?;

    // Update detection threshold if different
    detector_lock.set_det_thresh(confidence_threshold);

    // Convert RgbImage to DynamicImage
    let dynamic_img = image::DynamicImage::ImageRgb8(img.clone());

    let start = Instant::now();

    // Detect faces
    let faces = detector_lock
        .get(&dynamic_img)
        .context("Failed to detect faces")?;
    let duration = start.elapsed();
    eprintln!("Face detection took: {:?}", duration);

    // Convert to our Face format
    let result: Vec<Face> = faces
        .iter()
        .map(|f| {
            let x1 = f.bbox[0].max(0.0) as u32;
            let y1 = f.bbox[1].max(0.0) as u32;
            let x2 = f.bbox[2] as u32;
            let y2 = f.bbox[3] as u32;

            Face {
                x1,
                y1,
                x2,
                y2,
                confidence: f.det_score,
            }
        })
        .collect();

    Ok(result)
}

/// Count faces in an image
#[allow(dead_code)]
pub fn count_faces(img: &RgbImage, confidence_threshold: f32) -> Result<usize> {
    let faces = detect_faces(img, confidence_threshold)?;
    Ok(faces.len())
}

/// Get the primary (largest or most confident) face from detected faces
#[allow(dead_code)]
pub fn get_primary_face(faces: &[Face]) -> Option<&Face> {
    if faces.is_empty() {
        return None;
    }

    // Find face with highest confidence
    faces.iter().max_by(|a, b| {
        a.confidence
            .partial_cmp(&b.confidence)
            .unwrap_or(std::cmp::Ordering::Equal)
    })
}
