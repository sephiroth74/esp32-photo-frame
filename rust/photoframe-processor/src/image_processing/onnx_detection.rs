/// Complete ONNX-based people detection module with embedded YOLO11 model
/// This module is completely self-contained and replaces Python-based detection
use anyhow::Result;
use image::RgbImage;
use ort::session::{builder::GraphOptimizationLevel, Session};
use ort::value::Value;
use std::sync::{Arc, Mutex, OnceLock};

// Embed the YOLO11 model directly in this module
const YOLO11N_MODEL: &[u8] = include_bytes!("../assets/yolo11n.onnx");

/// Global session for reusing the loaded model
static GLOBAL_SESSION: OnceLock<Arc<Mutex<Session>>> = OnceLock::new();

// Re-export compatibility types for debug mode
pub mod python_yolo_integration {
    use serde::{Deserialize, Serialize};

    #[derive(Clone, Debug, Serialize, Deserialize)]
    pub struct ImageSize {
        pub width: u32,
        pub height: u32,
    }

    #[derive(Clone, Debug, Serialize, Deserialize)]
    pub struct Detection {
        #[serde(rename = "box")]
        pub bounding_box: [i32; 4],
        pub confidence: f32,
        pub class: String,
        pub class_id: i32,
    }

    #[derive(Clone, Debug, Serialize, Deserialize)]
    pub struct FindSubjectResult {
        pub image: String,
        pub imagesize: ImageSize,
        #[serde(rename = "box")]
        pub bounding_box: [i32; 4],
        pub center: [u32; 2],
        pub offset: [i32; 2],
        #[serde(default)]
        pub detections: Vec<Detection>,
        #[serde(default)]
        pub error: Option<String>,
    }
}

/// Detection result from YOLO people detection
#[derive(Debug, Clone)]
pub struct SubjectDetectionResult {
    /// Center point of the detected subject area for cropping/resizing
    pub center: (u32, u32),
    /// Offset from image center for smart cropping
    pub offset_from_center: (i32, i32),
    /// Combined bounding box of all detections [x_min, y_min, x_max, y_max]
    pub bounding_box: Option<(u32, u32, u32, u32)>,
    /// Highest confidence among detected people
    pub confidence: f32,
    /// Number of people detected
    pub person_count: usize,
}

/// Internal detection structure
#[derive(Debug, Clone)]
struct Detection {
    x: f32,
    y: f32,
    width: f32,
    height: f32,
    confidence: f32,
    class_id: usize,
}

/// ONNX-based subject detector using embedded YOLO11 model
pub struct SubjectDetector {
    session: Arc<Mutex<Session>>,
    verbose: bool,
}

impl SubjectDetector {
    /// Create a new subject detector using embedded YOLO11 model
    pub fn new(verbose: bool) -> Result<Self> {
        // Initialize ONNX Runtime if not already done
        let _ = ort::init();

        // Get or create the global session
        let session = GLOBAL_SESSION.get_or_init(|| {
            if verbose {
                println!(
                    "Loading embedded YOLO11 model ({:.2} MB)...",
                    YOLO11N_MODEL.len() as f32 / (1024.0 * 1024.0)
                );
            }

            // Load model from embedded bytes
            let session = Session::builder()
                .unwrap()
                .with_optimization_level(GraphOptimizationLevel::Level3)
                .unwrap()
                .with_intra_threads(4)
                .unwrap()
                .commit_from_memory(YOLO11N_MODEL)
                .expect("Failed to load embedded YOLO11 model");

            Arc::new(Mutex::new(session))
        });

        Ok(Self {
            session: Arc::clone(session),
            verbose,
        })
    }

    /// Detect people in an image and return detection result
    pub fn detect_people(
        &self,
        img: &RgbImage,
        confidence_threshold: f32,
    ) -> Result<SubjectDetectionResult> {
        // Prepare image for YOLO (resize to 640x640)
        let resized =
            image::imageops::resize(img, 640, 640, image::imageops::FilterType::CatmullRom);

        // Convert to tensor
        let tensor_data = prepare_tensor(&resized);

        // Create input tensor
        let input_shape = vec![1usize, 3, 640, 640];
        let input_value = Value::from_array((input_shape, tensor_data))?;

        // Run inference
        let mut session = self.session.lock().unwrap();
        let outputs = session.run(ort::inputs!["images" => input_value])?;

        // Process output
        let (output_shape, output_data) = outputs["output0"].try_extract_tensor::<f32>()?;
        let shape_vec: Vec<usize> = output_shape.iter().map(|&x| x as usize).collect();

        // Post-process detections
        let detections = process_yolo_output(
            output_data,
            shape_vec.as_slice(),
            confidence_threshold,
            0.4, // IOU threshold for NMS
        )?;

        // Filter for person class (class_id = 0 in COCO)
        let person_detections: Vec<_> =
            detections.into_iter().filter(|d| d.class_id == 0).collect();

        // Scale back to original image size
        let original_dimensions = img.dimensions();
        let scale_x = original_dimensions.0 as f32 / 640.0;
        let scale_y = original_dimensions.1 as f32 / 640.0;

        // Convert to result format
        if !person_detections.is_empty() {
            let mut min_x = f32::MAX;
            let mut min_y = f32::MAX;
            let mut max_x = f32::MIN;
            let mut max_y = f32::MIN;

            let mut individual_boxes = Vec::new();
            let mut highest_confidence = 0.0f32;

            for det in &person_detections {
                let x_min = ((det.x - det.width / 2.0) * scale_x).max(0.0) as u32;
                let y_min = ((det.y - det.height / 2.0) * scale_y).max(0.0) as u32;
                let x_max = ((det.x + det.width / 2.0) * scale_x) as u32;
                let y_max = ((det.y + det.height / 2.0) * scale_y) as u32;

                individual_boxes.push((x_min, y_min, x_max, y_max));

                min_x = min_x.min(x_min as f32);
                min_y = min_y.min(y_min as f32);
                max_x = max_x.max(x_max as f32);
                max_y = max_y.max(y_max as f32);

                highest_confidence = highest_confidence.max(det.confidence);
            }

            let center_x = ((min_x + max_x) / 2.0) as u32;
            let center_y = ((min_y + max_y) / 2.0) as u32;

            let offset_x = center_x as i32 - (original_dimensions.0 / 2) as i32;
            let offset_y = center_y as i32 - (original_dimensions.1 / 2) as i32;

            Ok(SubjectDetectionResult {
                center: (center_x, center_y),
                offset_from_center: (offset_x, offset_y),
                bounding_box: Some((min_x as u32, min_y as u32, max_x as u32, max_y as u32)),
                confidence: highest_confidence,
                person_count: person_detections.len(),
            })
        } else {
            // No people detected - return center of image
            let center_x = original_dimensions.0 / 2;
            let center_y = original_dimensions.1 / 2;

            Ok(SubjectDetectionResult {
                center: (center_x, center_y),
                offset_from_center: (0, 0),
                bounding_box: None,
                confidence: 0.0,
                person_count: 0,
            })
        }
    }
}

/// Create a default subject detector
pub fn create_default_detector(verbose: bool) -> Result<SubjectDetector> {
    SubjectDetector::new(verbose)
}

/// Prepare tensor data from image
fn prepare_tensor(img: &RgbImage) -> Vec<f32> {
    let mut tensor_data = Vec::with_capacity(1 * 3 * 640 * 640);

    // Fill tensor in NCHW format
    for c in 0..3 {
        for y in 0..640 {
            for x in 0..640 {
                let pixel = img.get_pixel(x, y);
                let value = pixel[c] as f32 / 255.0;
                tensor_data.push(value);
            }
        }
    }

    tensor_data
}

/// Process YOLO output tensor and extract detections
fn process_yolo_output(
    output_data: &[f32],
    shape: &[usize],
    confidence_threshold: f32,
    iou_threshold: f32,
) -> Result<Vec<Detection>> {
    // YOLO11 output shape: [1, 84, 8400]
    let batch_size = shape[0];
    let features = shape[1]; // 84
    let predictions = shape[2]; // 8400

    assert_eq!(batch_size, 1);
    assert_eq!(features, 84);
    assert_eq!(predictions, 8400);

    let mut detections = Vec::new();

    // Process each prediction
    for i in 0..predictions {
        // Access data in transposed order
        let mut prediction = vec![0.0f32; features];
        for j in 0..features {
            prediction[j] = output_data[j * predictions + i];
        }

        // First 4 values are bbox coordinates (cx, cy, w, h)
        let cx = prediction[0];
        let cy = prediction[1];
        let w = prediction[2];
        let h = prediction[3];

        // Next 80 values are class scores
        let class_scores = &prediction[4..84];

        // Find the class with maximum score
        let (class_id, &confidence) = class_scores
            .iter()
            .enumerate()
            .max_by(|(_, a), (_, b)| a.partial_cmp(b).unwrap())
            .unwrap();

        // Filter by confidence threshold
        if confidence > confidence_threshold {
            detections.push(Detection {
                x: cx,
                y: cy,
                width: w,
                height: h,
                confidence,
                class_id,
            });
        }
    }

    // Apply Non-Maximum Suppression (NMS)
    let nms_detections = non_maximum_suppression(detections, iou_threshold);

    Ok(nms_detections)
}

/// Non-Maximum Suppression to remove overlapping boxes
fn non_maximum_suppression(mut detections: Vec<Detection>, iou_threshold: f32) -> Vec<Detection> {
    // Sort by confidence (descending)
    detections.sort_by(|a, b| b.confidence.partial_cmp(&a.confidence).unwrap());

    let mut keep = Vec::new();

    while !detections.is_empty() {
        let current = detections.remove(0);
        keep.push(current.clone());

        // Remove all boxes with high IoU with the current box
        detections.retain(|det| {
            if det.class_id != current.class_id {
                true // Keep detections of different classes
            } else {
                calculate_iou(&current, det) < iou_threshold
            }
        });
    }

    keep
}

/// Calculate Intersection over Union (IoU) between two boxes
fn calculate_iou(box1: &Detection, box2: &Detection) -> f32 {
    // Convert from center format to corner format
    let box1_x1 = box1.x - box1.width / 2.0;
    let box1_y1 = box1.y - box1.height / 2.0;
    let box1_x2 = box1.x + box1.width / 2.0;
    let box1_y2 = box1.y + box1.height / 2.0;

    let box2_x1 = box2.x - box2.width / 2.0;
    let box2_y1 = box2.y - box2.height / 2.0;
    let box2_x2 = box2.x + box2.width / 2.0;
    let box2_y2 = box2.y + box2.height / 2.0;

    // Calculate intersection area
    let inter_x1 = box1_x1.max(box2_x1);
    let inter_y1 = box1_y1.max(box2_y1);
    let inter_x2 = box1_x2.min(box2_x2);
    let inter_y2 = box1_y2.min(box2_y2);

    if inter_x2 < inter_x1 || inter_y2 < inter_y1 {
        return 0.0; // No intersection
    }

    let inter_area = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);

    // Calculate union area
    let box1_area = box1.width * box1.height;
    let box2_area = box2.width * box2.height;
    let union_area = box1_area + box2_area - inter_area;

    // Calculate IoU
    inter_area / union_area
}
