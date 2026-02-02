//! Main Face Analysis application module
//!
//! This module provides the primary API for face detection and analysis

use crate::error::{InsightFaceError, Result};
use crate::face::Face;
use crate::model_zoo::{ModelStore, OnnxModel, RetinaFaceModel};
use image::DynamicImage;
use image::ImageBuffer;
use ndarray::Array3;
use ort::session::Session;
use ort::session::builder::GraphOptimizationLevel;
use std::fs::File;
use std::io::BufReader;
use std::path::Path;

/// Detection result from ONNX model
#[derive(Debug, Clone)]
struct DetectionResult {
    scores: Vec<f32>,
    bboxes: Vec<[f32; 4]>,
    keypoints: Option<Vec<[[f32; 2]; 5]>>,
}

/// Main Face Analysis application
pub struct FaceAnalysis {
    model_store: ModelStore,
    detection_session: Option<Session>,
    det_thresh: f32,
    input_size: (i64, i64),
    nms_thresh: f32,
}

impl FaceAnalysis {
    /// Create a new FaceAnalysis instance with specified models directory
    ///
    /// # Arguments
    /// * `model_dir` - Path to directory containing ONNX model files
    /// * `ctx_id` - GPU context ID (None for CPU)
    ///
    /// # Example
    /// ```no_run
    /// use insightface_rs::FaceAnalysis;
    /// use std::path::Path;
    ///
    /// let app = FaceAnalysis::new(Path::new("/path/to/models"), None)?;
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn new<P: AsRef<Path>>(model_dir: P, _ctx_id: Option<i32>) -> Result<Self> {
        let model_store = ModelStore::new(model_dir)?;
        let input_size = RetinaFaceModel.input_size();

        Ok(FaceAnalysis {
            model_store,
            detection_session: None,
            det_thresh: 0.5,
            input_size,
            nms_thresh: 0.4,
        })
    }

    /// Prepare (load) models for inference
    ///
    /// # Arguments
    /// * `ctx_id` - GPU context ID (0 for GPU, -1 for CPU, ignored in current implementation)
    /// * `det_thresh` - Detection threshold (0.0 to 1.0)
    /// * `input_size` - Input size (height, width) for detection model
    pub fn prepare(&mut self, _ctx_id: i32, det_thresh: f32, input_size: (i64, i64)) -> Result<()> {
        self.det_thresh = det_thresh.max(0.0).min(1.0);
        self.input_size = input_size;

        // Load detection model
        self.load_detection_model()?;

        Ok(())
    }

    /// Load the detection model
    fn load_detection_model(&mut self) -> Result<()> {
        let model_path = self.model_store.retinaface()?;

        // Create session with ort
        let session = Session::builder()
            .map_err(|e| {
                InsightFaceError::OnnxError(format!("Failed to create session builder: {}", e))
            })?
            .with_optimization_level(GraphOptimizationLevel::Level3)
            .map_err(|e| {
                InsightFaceError::OnnxError(format!("Failed to set optimization level: {}", e))
            })?
            .commit_from_file(model_path)
            .map_err(|e| InsightFaceError::OnnxError(format!("Failed to load model: {}", e)))?;

        self.detection_session = Some(session);

        Ok(())
    }

    /// Detect faces in an image from a DynamicImage object
    ///
    /// Use this method when you already have a loaded image in memory.
    /// For automatic EXIF orientation handling, use [`get_from_path`](Self::get_from_path) instead.
    ///
    /// # Arguments
    /// * `img` - Reference to a DynamicImage from the image crate
    ///
    /// # Returns
    /// Vector of detected Face objects, sorted by detection confidence (descending)
    ///
    /// # Example
    /// ```no_run
    /// # use insightface_rs::FaceAnalysis;
    /// # use std::path::Path;
    /// # let mut app = FaceAnalysis::new(Path::new("/path/to/models"), None)?;
    /// # app.prepare(0, 0.5, (640, 640))?;
    /// let img = image::open("face.jpg")?;
    /// let faces = app.get(&img)?;
    ///
    /// for face in &faces {
    ///     println!("Detected face with score: {:.2}", face.det_score);
    ///     if let Some(kps) = &face.kps {
    ///         println!("  Landmarks: {} points", kps.len());
    ///     }
    /// }
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn get(&mut self, img: &DynamicImage) -> Result<Vec<Face>> {
        if self.detection_session.is_none() {
            return Err(InsightFaceError::ModelPrepareError(
                "Detection model not loaded. Call prepare() first.".to_string(),
            ));
        }

        // Convert image to RGB if needed
        let rgb_img = img.to_rgb8();
        let (img_h, img_w) = (rgb_img.height() as usize, rgb_img.width() as usize);

        // Create ndarray from image
        let mut img_array = Array3::<u8>::zeros((img_h, img_w, 3));
        for (x, y, pixel) in rgb_img.enumerate_pixels() {
            img_array[[y as usize, x as usize, 0]] = pixel[0];
            img_array[[y as usize, x as usize, 1]] = pixel[1];
            img_array[[y as usize, x as usize, 2]] = pixel[2];
        }

        // Prepare input tensor with normalization
        let input_tensor = self.prepare_detection_input(img_array, self.input_size)?;
        let det_thresh = self.det_thresh;

        // Run inference and extract raw data
        let (scores_vec, bboxes_vec, keypoints_vec) = {
            let session =
                self.detection_session
                    .as_mut()
                    .ok_or(InsightFaceError::ModelPrepareError(
                        "Detection session not available".to_string(),
                    ))?;

            // Run with ort - use Value::from_array with ndarray
            let input_ndarray = input_tensor.clone();

            let outputs = session
                .run(vec![(
                    "input.1",
                    ort::value::Value::from_array(input_ndarray).map_err(|e| {
                        InsightFaceError::OnnxError(format!("Failed to create tensor: {}", e))
                    })?,
                )])
                .map_err(|e| {
                    InsightFaceError::OnnxError(format!("Failed to run detection: {}", e))
                })?;

            // Extract raw data from outputs - inline to avoid calling self method
            if outputs.len() < 2 {
                return Err(InsightFaceError::UnexpectedModelOutput);
            }

            let mut scores_vec = Vec::new();
            let mut bboxes_vec = Vec::new();
            let mut keypoints_vec = Vec::new();

            // RetinaFace model parameters
            let fmc = 3; // Feature map count
            let feat_stride_fpn = [8, 16, 32]; // Strides for each scale
            let num_anchors = 2;

            let (input_height, input_width) = self.input_size;

            for (idx, &stride) in feat_stride_fpn.iter().enumerate() {
                // Get scores for this scale
                let scores = if let Ok(scores_data) = outputs[idx].try_extract_tensor::<f32>() {
                    scores_data.1
                } else {
                    continue;
                };

                // Get bbox predictions for this scale
                let bbox_preds =
                    if let Ok(bbox_data) = outputs[idx + fmc].try_extract_tensor::<f32>() {
                        bbox_data.1
                    } else {
                        continue;
                    };

                // Get keypoint predictions if available
                let kps_preds = if outputs.len() > idx + fmc * 2 {
                    if let Ok(kps_data) = outputs[idx + fmc * 2].try_extract_tensor::<f32>() {
                        Some(kps_data.1)
                    } else {
                        None
                    }
                } else {
                    None
                };

                // Calculate feature map dimensions
                let height = (input_height / stride) as usize;
                let width = (input_width / stride) as usize;

                // Generate anchor centers for this scale
                let mut anchor_centers = Vec::new();
                for y in 0..height {
                    for x in 0..width {
                        for _ in 0..num_anchors {
                            anchor_centers.push([
                                (x as f32 + 0.5) * stride as f32,
                                (y as f32 + 0.5) * stride as f32,
                            ]);
                        }
                    }
                }

                // Process detections for this scale
                for i in 0..scores.len() {
                    let score = scores[i];

                    if score >= det_thresh && i < anchor_centers.len() {
                        let center = anchor_centers[i];

                        // Decode bbox using distance2bbox
                        let bbox_idx = i * 4;
                        if bbox_idx + 3 < bbox_preds.len() {
                            let x1 = center[0] - bbox_preds[bbox_idx] * stride as f32;
                            let y1 = center[1] - bbox_preds[bbox_idx + 1] * stride as f32;
                            let x2 = center[0] + bbox_preds[bbox_idx + 2] * stride as f32;
                            let y2 = center[1] + bbox_preds[bbox_idx + 3] * stride as f32;

                            scores_vec.push(score);
                            bboxes_vec.push([x1, y1, x2, y2]);

                            // Decode keypoints if available
                            if let Some(kps) = kps_preds {
                                let kps_idx = i * 10;
                                if kps_idx + 9 < kps.len() {
                                    let mut landmarks = [[0.0; 2]; 5];
                                    for j in 0..5 {
                                        landmarks[j][0] =
                                            center[0] + kps[kps_idx + j * 2] * stride as f32;
                                        landmarks[j][1] =
                                            center[1] + kps[kps_idx + j * 2 + 1] * stride as f32;
                                    }
                                    keypoints_vec.push(landmarks);
                                }
                            }
                        }
                    }
                }
            }

            (scores_vec, bboxes_vec, keypoints_vec)
        };

        // Create detection result from extracted data
        let det_result = DetectionResult {
            scores: scores_vec,
            bboxes: bboxes_vec,
            keypoints: if keypoints_vec.is_empty() {
                None
            } else {
                Some(keypoints_vec)
            },
        };

        // Now self is available again for further operations
        // Scale back to original image size
        let (model_h, model_w) = self.input_size;
        let scale_h = img_h as f32 / model_h as f32;
        let scale_w = img_w as f32 / model_w as f32;

        // Create Face objects
        let mut faces = Vec::new();

        // Apply NMS to filter overlapping detections
        let keep_indices = self.nms(&det_result.bboxes, &det_result.scores);

        for &idx in &keep_indices {
            if idx >= det_result.bboxes.len() {
                break;
            }

            let bbox = &det_result.bboxes[idx];
            let scaled_bbox = [
                bbox[0] * scale_w,
                bbox[1] * scale_h,
                bbox[2] * scale_w,
                bbox[3] * scale_h,
            ];

            let mut face = Face::new(scaled_bbox, det_result.scores[idx]);

            // Add keypoints if available
            if let Some(ref kps) = det_result.keypoints {
                if idx < kps.len() {
                    face.kps = Some(
                        kps[idx]
                            .iter()
                            .map(|kp| [kp[0] * scale_w, kp[1] * scale_h])
                            .collect(),
                    );
                }
            }

            faces.push(face);
        }

        Ok(faces)
    }

    /// Detect faces in an image from a file path with automatic EXIF orientation handling
    ///
    /// This method loads an image from the specified path and automatically applies
    /// EXIF orientation metadata, which is crucial for photos from smartphones and
    /// digital cameras that may be rotated.
    ///
    /// # Arguments
    /// * `path` - Path to the image file (supports JPEG, PNG, WebP, TIFF)
    ///
    /// # Returns
    /// Vector of detected Face objects, sorted by detection confidence (descending)
    ///
    /// # Errors
    /// Returns an error if:
    /// - The file cannot be opened or read
    /// - The image format is not supported
    /// - The detection model is not loaded
    ///
    /// # Example
    /// ```no_run
    /// # use insightface_rs::FaceAnalysis;
    /// # use std::path::Path;
    /// # let mut app = FaceAnalysis::new(Path::new("/path/to/models"), None)?;
    /// # app.prepare(0, 0.5, (640, 640))?;
    /// // Process a smartphone photo with EXIF rotation
    /// let faces = app.get_from_path("IMG_1234.jpg")?;
    /// println!("Found {} faces", faces.len());
    ///
    /// // Draw bounding boxes on the image
    /// let img = image::open("IMG_1234.jpg")?;
    /// let result = app.draw_on(&img, &faces);
    /// result.save("output.jpg")?;
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn get_from_path<P: AsRef<Path>>(&mut self, path: P) -> Result<Vec<Face>> {
        let path = path.as_ref();

        // Load image
        let img = image::open(path)?;

        // Apply EXIF orientation
        let img = self.apply_exif_orientation(img, path)?;

        // Detect faces
        self.get(&img)
    }

    /// Apply EXIF orientation transformation to an image
    fn apply_exif_orientation(&self, img: DynamicImage, path: &Path) -> Result<DynamicImage> {
        let file = File::open(path)?;

        let mut bufreader = BufReader::new(file);
        let exifreader = exif::Reader::new();

        let exif_data = match exifreader.read_from_container(&mut bufreader) {
            Ok(data) => data,
            Err(_) => return Ok(img), // No EXIF data, return original
        };

        let orientation = match exif_data.get_field(exif::Tag::Orientation, exif::In::PRIMARY) {
            Some(field) => match field.value.get_uint(0) {
                Some(v) => v,
                None => return Ok(img),
            },
            None => return Ok(img),
        };

        // Apply transformation based on EXIF orientation tag
        // See: https://www.impulseadventure.com/photo/exif-orientation.html
        let transformed = match orientation {
            1 => img,                     // Normal
            2 => img.fliph(),             // Flip horizontal
            3 => img.rotate180(),         // Rotate 180°
            4 => img.flipv(),             // Flip vertical
            5 => img.rotate90().fliph(),  // Rotate 90° CW + flip horizontal
            6 => img.rotate90(),          // Rotate 90° CW
            7 => img.rotate270().fliph(), // Rotate 270° CW + flip horizontal
            8 => img.rotate270(),         // Rotate 270° CW
            _ => img,                     // Unknown orientation
        };

        Ok(transformed)
    }

    /// Non-Maximum Suppression to filter overlapping detections
    fn nms(&self, bboxes: &[[f32; 4]], scores: &[f32]) -> Vec<usize> {
        if bboxes.is_empty() {
            return Vec::new();
        }

        let thresh = self.nms_thresh;
        let n = bboxes.len();

        // Calculate areas
        let mut areas = Vec::with_capacity(n);
        for bbox in bboxes {
            let area = (bbox[2] - bbox[0] + 1.0) * (bbox[3] - bbox[1] + 1.0);
            areas.push(area);
        }

        // Sort by score descending
        let mut order: Vec<usize> = (0..n).collect();
        order.sort_by(|&a, &b| {
            scores[b]
                .partial_cmp(&scores[a])
                .unwrap_or(std::cmp::Ordering::Equal)
        });

        let mut keep = Vec::new();
        let mut suppressed = vec![false; n];

        for &i in &order {
            if suppressed[i] {
                continue;
            }
            keep.push(i);

            let bbox_i = &bboxes[i];

            for &j in &order {
                if i == j || suppressed[j] {
                    continue;
                }

                let bbox_j = &bboxes[j];

                // Calculate intersection
                let xx1 = bbox_i[0].max(bbox_j[0]);
                let yy1 = bbox_i[1].max(bbox_j[1]);
                let xx2 = bbox_i[2].min(bbox_j[2]);
                let yy2 = bbox_i[3].min(bbox_j[3]);

                let w = (xx2 - xx1 + 1.0).max(0.0);
                let h = (yy2 - yy1 + 1.0).max(0.0);
                let inter = w * h;

                // Calculate IoU (Intersection over Union)
                let ovr = inter / (areas[i] + areas[j] - inter);

                if ovr > thresh {
                    suppressed[j] = true;
                }
            }
        }

        keep
    }

    /// Prepare input tensor with proper normalization
    fn prepare_detection_input(
        &self,
        img: Array3<u8>,
        target_size: (i64, i64),
    ) -> Result<ndarray::Array4<f32>> {
        let (target_h, target_w) = (target_size.0 as u32, target_size.1 as u32);

        // Convert ndarray to RgbImage for fast resizing
        let (h, w, _) = img.dim();
        let mut rgb_img = image::RgbImage::new(w as u32, h as u32);
        for y in 0..h {
            for x in 0..w {
                rgb_img.put_pixel(
                    x as u32,
                    y as u32,
                    image::Rgb([img[[y, x, 0]], img[[y, x, 1]], img[[y, x, 2]]]),
                );
            }
        }

        // Fast resize using image crate (much faster than manual nearest-neighbor)
        let resized = image::imageops::resize(
            &rgb_img,
            target_w,
            target_h,
            image::imageops::FilterType::Triangle, // Bilinear interpolation
        );

        // Convert to tensor with normalization: (pixel - 127.5) / 128.0
        let mut tensor = ndarray::Array4::zeros((1, 3, target_h as usize, target_w as usize));

        for y in 0..target_h as usize {
            for x in 0..target_w as usize {
                let pixel = resized.get_pixel(x as u32, y as u32);
                tensor[[0, 0, y, x]] = (pixel[0] as f32 - 127.5) / 128.0;
                tensor[[0, 1, y, x]] = (pixel[1] as f32 - 127.5) / 128.0;
                tensor[[0, 2, y, x]] = (pixel[2] as f32 - 127.5) / 128.0;
            }
        }

        Ok(tensor)
    }

    /// Get current detection threshold
    pub fn get_det_thresh(&self) -> f32 {
        self.det_thresh
    }

    /// Draw bounding boxes on image
    ///
    /// # Arguments
    /// * `img` - Input image as DynamicImage
    /// * `faces` - Vector of detected faces
    ///
    /// # Returns
    /// Image with drawn bounding boxes
    pub fn draw_on(&self, img: &DynamicImage, faces: &[Face]) -> DynamicImage {
        let mut rgb_img = img.to_rgb8();

        for face in faces {
            let x1 = face.bbox[0].max(0.0) as i32;
            let y1 = face.bbox[1].max(0.0) as i32;
            let x2 = face.bbox[2].min(rgb_img.width() as f32) as i32;
            let y2 = face.bbox[3].min(rgb_img.height() as f32) as i32;

            // Draw rectangle border (green color: [0, 255, 0])
            let color = image::Rgb([0u8, 255u8, 0u8]);
            draw_rectangle(&mut rgb_img, x1, y1, x2, y2, color);

            // Draw confidence score text (simplified - just draw a point)
            let score_text = format!("{:.2}", face.det_score);
            let _ = score_text; // Suppress unused warning - full text rendering would require additional deps
        }

        DynamicImage::ImageRgb8(rgb_img)
    }

    /// Get the current NMS (Non-Maximum Suppression) threshold
    ///
    /// # Returns
    /// The IoU threshold (0.0-1.0) used for filtering overlapping detections
    pub fn get_nms_thresh(&self) -> f32 {
        self.nms_thresh
    }

    /// Set the detection threshold
    ///
    /// # Arguments
    /// * `thresh` - New detection threshold (will be clamped to 0.0-1.0)
    pub fn set_det_thresh(&mut self, thresh: f32) {
        self.det_thresh = thresh.max(0.0).min(1.0);
    }

    /// Set the NMS threshold
    ///
    /// # Arguments
    /// * `thresh` - New NMS threshold (will be clamped to 0.0-1.0)
    pub fn set_nms_thresh(&mut self, thresh: f32) {
        self.nms_thresh = thresh.max(0.0).min(1.0);
    }
}

/// Helper function to draw a rectangle on an image
fn draw_rectangle(
    img: &mut ImageBuffer<image::Rgb<u8>, Vec<u8>>,
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
    color: image::Rgb<u8>,
) {
    let width = img.width() as i32;
    let height = img.height() as i32;

    // Draw top and bottom edges
    for x in x1..=x2.min(width - 1) {
        if x >= 0 && y1 >= 0 && y1 < height {
            img.put_pixel(x as u32, y1 as u32, color);
            img.put_pixel(x as u32, (y1 + 1) as u32, color);
        }
        if x >= 0 && y2 >= 0 && y2 < height {
            img.put_pixel(x as u32, y2 as u32, color);
            img.put_pixel(x as u32, (y2 - 1) as u32, color);
        }
    }

    // Draw left and right edges
    for y in y1..=y2.min(height - 1) {
        if y >= 0 && x1 >= 0 && x1 < width {
            img.put_pixel(x1 as u32, y as u32, color);
            img.put_pixel((x1 + 1) as u32, y as u32, color);
        }
        if y >= 0 && x2 >= 0 && x2 < width {
            img.put_pixel(x2 as u32, y as u32, color);
            img.put_pixel((x2 - 1) as u32, y as u32, color);
        }
    }
}

#[cfg(test)]
mod tests {
    #[allow(unused_imports)]
    use super::*;

    #[test]
    fn test_face_analysis_creation() {
        // This would require actual model directory
        // Skip in tests without proper setup
    }
}
