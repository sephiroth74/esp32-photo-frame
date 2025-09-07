use anyhow::Result;
use image::RgbImage;
use std::path::Path;

#[cfg(feature = "ai")]
use yolo_rs::{Yolo, YoloResult};

/// Detection result from YOLO people detection
/// Based on find_subject.py output format
#[derive(Debug, Clone)]
pub struct SubjectDetectionResult {
    /// Combined bounding box (x, y, width, height) of all detected people
    pub bounding_box: Option<(u32, u32, u32, u32)>,
    /// Center point of the combined detection area
    pub center: (u32, u32),
    /// Offset from image center for smart cropping
    pub offset_from_center: (i32, i32),
    /// Highest confidence among detected people
    pub confidence: f32,
    /// Number of people detected
    pub person_count: usize,
}

/// YOLO-based subject detector for people detection
/// Replicates the functionality of find_subject.py using YOLOv3
#[cfg(feature = "ai")]
pub struct SubjectDetector {
    yolo: Yolo,
    confidence_threshold: f32,
    nms_threshold: f32,
}

#[cfg(feature = "ai")]
impl SubjectDetector {
    /// Create a new subject detector with YOLOv3 models
    /// 
    /// This loads the YOLOv3 configuration, weights, and class names
    /// from the assets directory, matching find_subject.py behavior
    pub fn new(
        config_path: &Path,
        weights_path: &Path,
        confidence_threshold: f32,
        nms_threshold: f32,
    ) -> Result<Self> {
        let yolo = Yolo::new(
            config_path.to_str().context("Invalid config path")?,
            weights_path.to_str().context("Invalid weights path")?,
            confidence_threshold,
        ).context("Failed to initialize YOLO model")?;

        Ok(Self {
            yolo,
            confidence_threshold,
            nms_threshold,
        })
    }

    /// Detect people in an image and return detection result
    /// 
    /// This replicates the detect_people_yolo function from find_subject.py:
    /// 1. Runs YOLO inference on the image
    /// 2. Filters for 'person' class detections
    /// 3. Applies non-maximum suppression
    /// 4. Calculates combined bounding box and center point
    /// 5. Returns offset from image center for smart cropping
    pub fn detect_people(&self, img: &RgbImage) -> Result<SubjectDetectionResult> {
        let (width, height) = img.dimensions();
        
        // Convert RgbImage to format expected by YOLO
        let img_data = self.prepare_image_for_yolo(img)?;
        
        // Run YOLO detection
        let detections = self.yolo.inference(&img_data)
            .context("YOLO inference failed")?;

        // Filter for person class (class_id = 0 in COCO dataset)
        let person_detections: Vec<&YoloResult> = detections
            .iter()
            .filter(|detection| {
                detection.class_id == 0 && // Person class in COCO
                detection.confidence > self.confidence_threshold
            })
            .collect();

        if person_detections.is_empty() {
            // No people detected - return image center as fallback
            return Ok(SubjectDetectionResult {
                bounding_box: None,
                center: (width / 2, height / 2),
                offset_from_center: (0, 0),
                confidence: 0.0,
                person_count: 0,
            });
        }

        // Apply Non-Maximum Suppression to remove overlapping detections
        let filtered_detections = self.apply_nms(&person_detections)?;

        // Calculate combined bounding box (like find_subject.py lines 133-149)
        let mut x_min = f32::INFINITY;
        let mut y_min = f32::INFINITY;
        let mut x_max = 0.0f32;
        let mut y_max = 0.0f32;
        let mut max_confidence = 0.0f32;

        for detection in &filtered_detections {
            let x = detection.x * width as f32;
            let y = detection.y * height as f32;
            let w = detection.width * width as f32;
            let h = detection.height * height as f32;

            // Convert center coordinates to bounding box
            let left = x - w / 2.0;
            let top = y - h / 2.0;
            let right = x + w / 2.0;
            let bottom = y + h / 2.0;

            x_min = x_min.min(left);
            y_min = y_min.min(top);
            x_max = x_max.max(right);
            y_max = y_max.max(bottom);
            max_confidence = max_confidence.max(detection.confidence);
        }

        // Convert to integer coordinates
        let bbox_x = x_min.max(0.0) as u32;
        let bbox_y = y_min.max(0.0) as u32;
        let bbox_width = (x_max - x_min).max(0.0) as u32;
        let bbox_height = (y_max - y_min).max(0.0) as u32;

        // Calculate center point (like find_subject.py lines 175-177)
        let center_x = (bbox_x + bbox_width / 2).min(width);
        let center_y = (bbox_y + bbox_height / 2).min(height);

        // Calculate offset from image center (like find_subject.py lines 180-185)
        let image_center_x = width / 2;
        let image_center_y = height / 2;
        let offset_x = center_x as i32 - image_center_x as i32;
        let offset_y = center_y as i32 - image_center_y as i32;

        Ok(SubjectDetectionResult {
            bounding_box: Some((bbox_x, bbox_y, bbox_width, bbox_height)),
            center: (center_x, center_y),
            offset_from_center: (offset_x, offset_y),
            confidence: max_confidence,
            person_count: filtered_detections.len(),
        })
    }

    /// Prepare image data for YOLO input
    fn prepare_image_for_yolo(&self, img: &RgbImage) -> Result<Vec<u8>> {
        // Convert RgbImage to Vec<u8> in BGR format (OpenCV format)
        let mut img_data = Vec::new();
        for pixel in img.pixels() {
            img_data.push(pixel[2]); // B
            img_data.push(pixel[1]); // G  
            img_data.push(pixel[0]); // R
        }
        Ok(img_data)
    }

    /// Apply Non-Maximum Suppression to filter overlapping detections
    fn apply_nms(&self, detections: &[&YoloResult]) -> Result<Vec<YoloResult>> {
        // Simple NMS implementation
        // In production, you'd want a more sophisticated algorithm
        let mut filtered = Vec::new();
        let mut processed = vec![false; detections.len()];

        for (i, detection) in detections.iter().enumerate() {
            if processed[i] {
                continue;
            }

            filtered.push((*detection).clone());
            processed[i] = true;

            // Mark overlapping detections as processed
            for (j, other) in detections.iter().enumerate().skip(i + 1) {
                if processed[j] {
                    continue;
                }

                let iou = self.calculate_iou(detection, other);
                if iou > self.nms_threshold {
                    processed[j] = true;
                }
            }
        }

        Ok(filtered)
    }

    /// Calculate Intersection over Union (IoU) between two detections
    fn calculate_iou(&self, det1: &YoloResult, det2: &YoloResult) -> f32 {
        // Convert center coordinates to corner coordinates
        let x1_min = det1.x - det1.width / 2.0;
        let y1_min = det1.y - det1.height / 2.0;
        let x1_max = det1.x + det1.width / 2.0;
        let y1_max = det1.y + det1.height / 2.0;

        let x2_min = det2.x - det2.width / 2.0;
        let y2_min = det2.y - det2.height / 2.0;
        let x2_max = det2.x + det2.width / 2.0;
        let y2_max = det2.y + det2.height / 2.0;

        // Calculate intersection
        let inter_x_min = x1_min.max(x2_min);
        let inter_y_min = y1_min.max(y2_min);
        let inter_x_max = x1_max.min(x2_max);
        let inter_y_max = y1_max.min(y2_max);

        let inter_width = (inter_x_max - inter_x_min).max(0.0);
        let inter_height = (inter_y_max - inter_y_min).max(0.0);
        let intersection = inter_width * inter_height;

        // Calculate union
        let area1 = det1.width * det1.height;
        let area2 = det2.width * det2.height;
        let union = area1 + area2 - intersection;

        if union > 0.0 {
            intersection / union
        } else {
            0.0
        }
    }
}

// Placeholder implementations when AI feature is not enabled
#[cfg(not(feature = "ai"))]
pub struct SubjectDetector;

#[cfg(not(feature = "ai"))]
impl SubjectDetector {
    pub fn new(
        _config_path: &Path,
        _weights_path: &Path,
        _confidence_threshold: f32,
        _nms_threshold: f32,
    ) -> Result<Self> {
        Ok(Self)
    }

    pub fn detect_people(&self, img: &RgbImage) -> Result<SubjectDetectionResult> {
        let (width, height) = img.dimensions();
        Ok(SubjectDetectionResult {
            bounding_box: None,
            center: (width / 2, height / 2),
            offset_from_center: (0, 0),
            confidence: 0.0,
            person_count: 0,
        })
    }
}

/// Create a subject detector using the YOLOv3 models from the assets directory
pub fn create_default_detector(
    assets_path: &Path,
    confidence_threshold: f32,
    nms_threshold: f32,
) -> Result<SubjectDetector> {
    let config_path = assets_path.join("yolov3.cfg");
    let weights_path = assets_path.join("yolov3.weights");

    if !config_path.exists() {
        return Err(anyhow::anyhow!(
            "YOLOv3 config file not found: {}",
            config_path.display()
        ));
    }

    if !weights_path.exists() {
        return Err(anyhow::anyhow!(
            "YOLOv3 weights file not found: {}. Download from: https://pjreddie.com/media/files/yolov3.weights",
            weights_path.display()
        ));
    }

    SubjectDetector::new(&config_path, &weights_path, confidence_threshold, nms_threshold)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;

    fn create_test_image(width: u32, height: u32) -> RgbImage {
        ImageBuffer::from_fn(width, height, |x, y| {
            image::Rgb([(x % 256) as u8, (y % 256) as u8, 128])
        })
    }

    #[test]
    fn test_subject_detection_result() {
        let result = SubjectDetectionResult {
            bounding_box: Some((100, 100, 200, 200)),
            center: (200, 200),
            offset_from_center: (50, 50),
            confidence: 0.85,
            person_count: 1,
        };

        assert_eq!(result.person_count, 1);
        assert_eq!(result.center, (200, 200));
        assert_eq!(result.offset_from_center, (50, 50));
    }

    #[test]
    fn test_no_ai_feature_fallback() {
        // This test works regardless of whether AI feature is enabled
        let img = create_test_image(400, 300);
        
        #[cfg(not(feature = "ai"))]
        {
            let detector = SubjectDetector;
            let result = detector.detect_people(&img).unwrap();
            assert_eq!(result.center, (200, 150)); // Image center
            assert_eq!(result.person_count, 0);
        }
    }
}