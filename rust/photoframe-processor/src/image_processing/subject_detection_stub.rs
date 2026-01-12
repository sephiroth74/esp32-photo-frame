/// Stub module for when AI features are disabled
/// Provides empty implementations to maintain API compatibility
use anyhow::Result;
use image::RgbImage;

/// Stub detection result when AI is disabled
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

/// Stub subject detector when AI is disabled
pub struct SubjectDetector {
    #[allow(dead_code)]
    verbose: bool,
}

impl SubjectDetector {
    /// Create a new stub detector
    #[allow(dead_code)]
    pub fn new(verbose: bool) -> Result<Self> {
        Ok(Self { verbose })
    }

    /// Stub detection - always returns no people detected
    pub fn detect_people(
        &self,
        img: &RgbImage,
        _confidence_threshold: f32,
    ) -> Result<SubjectDetectionResult> {
        let dimensions = img.dimensions();
        let center_x = dimensions.0 / 2;
        let center_y = dimensions.1 / 2;

        Ok(SubjectDetectionResult {
            center: (center_x, center_y),
            offset_from_center: (0, 0),
            bounding_box: None,
            confidence: 0.0,
            person_count: 0,
        })
    }
}

/// Create a stub detector
#[allow(dead_code)]
pub fn create_default_detector(verbose: bool) -> Result<SubjectDetector> {
    SubjectDetector::new(verbose)
}

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
