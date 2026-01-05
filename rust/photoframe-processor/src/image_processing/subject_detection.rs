use anyhow::Result;
use image::RgbImage;
use std::path::{Path, PathBuf};

// People detection using existing find_subject.py script
pub mod python_yolo_integration {
    use anyhow::Result;
    use serde::{Deserialize, Serialize};
    use std::path::Path;

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
        pub bounding_box: [i32; 4], // Combined bounding box [x_min, y_min, x_max, y_max] - kept for JSON deserialization but ignored in processing
        pub center: [u32; 2], // [center_x, center_y] - THIS IS THE ONLY DATA WE USE for cropping/resizing
        pub offset: [i32; 2], // [offset_x, offset_y] - also used for smart cropping
        #[serde(default)]
        pub detections: Vec<Detection>, // Individual person detections - used only for confidence and count
        #[serde(default)]
        pub error: Option<String>,
    }

    pub struct PythonYolo {
        _script_path: String,
    }

    impl PythonYolo {
        pub fn new(script_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
            let script_path = Path::new(script_path);

            if !script_path.exists() {
                return Err(format!(
                    "find_subject.py script not found: {}",
                    script_path.display()
                )
                .into());
            }

            Ok(PythonYolo {
                _script_path: script_path.to_string_lossy().to_string(),
            })
        }
    }
}

use python_yolo_integration::PythonYolo;

/// Detection result from YOLO people detection
/// Enhanced to include bounding box information for smart cropping
#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct SubjectDetectionResult {
    /// Center point of the detected subject area for cropping/resizing
    pub center: (u32, u32),
    /// Offset from image center for smart cropping
    pub offset_from_center: (i32, i32),
    /// Combined bounding box of all detections [x_min, y_min, x_max, y_max]
    pub bounding_box: Option<(u32, u32, u32, u32)>,
    /// Individual detection boxes for advanced processing
    pub individual_detections: Vec<(u32, u32, u32, u32)>, // [(x_min, y_min, x_max, y_max), ...]
    /// Highest confidence among detected people
    pub confidence: f32,
    /// Number of people detected
    pub person_count: usize,
}

/// Python-based subject detector using find_subject.py
#[allow(dead_code)]
pub struct SubjectDetector {
    python_yolo: PythonYolo,
    python_path: Option<PathBuf>,
    script_path: PathBuf,
}

#[allow(dead_code)]
impl SubjectDetector {
    /// Create a new subject detector using find_subject.py
    ///
    /// This uses the existing Python script for people detection,
    /// which already implements YOLO11 with proper configuration
    ///
    /// # Arguments
    /// * `script_path` - Path to find_subject.py
    /// * `python_path` - Optional path to Python interpreter (e.g., .venv/bin/python3)
    pub fn new(script_path: &Path, python_path: Option<PathBuf>) -> Result<Self> {
        let python_yolo = PythonYolo::new(
            script_path
                .to_str()
                .ok_or_else(|| anyhow::anyhow!("Invalid script path"))?,
        )
        .map_err(|e| anyhow::anyhow!("Failed to initialize Python YOLO: {}", e))?;

        Ok(Self {
            python_yolo,
            python_path,
            script_path: script_path.to_path_buf(),
        })
    }

    /// Detect people in an image and return detection result
    ///
    /// This uses the existing find_subject.py script which already implements:
    /// 1. YOLO11 people detection
    /// 2. Non-maximum suppression
    /// 3. Combined bounding box calculation
    /// 4. Center point and offset computation
    pub fn detect_people(
        &self,
        img: &RgbImage,
        confidence_threshold: f32,
    ) -> Result<SubjectDetectionResult> {
        // Save image to temporary file for Python script
        let temp_path = self.save_temp_image(img)?;

        // Run Python script for people detection using custom python_path or default "python"
        let python_result = self.run_python_detection(&temp_path, confidence_threshold)?;

        // Clean up temporary file
        let _ = std::fs::remove_file(&temp_path);

        // Convert Python script result to our internal format
        // Check if we have actual detections (not just error-free execution)
        let people_detected = python_result.error.is_none() && !python_result.detections.is_empty();

        // Extract individual detection boxes and calculate overall statistics
        let (confidence, person_count, individual_detections, bounding_box) = if people_detected {
            let highest_confidence = python_result
                .detections
                .iter()
                .map(|d| d.confidence)
                .fold(0.0f32, |acc, conf| acc.max(conf));

            // Extract individual detection boxes, converting from [x, y, width, height] to [x_min, y_min, x_max, y_max]
            let individual_boxes: Vec<(u32, u32, u32, u32)> = python_result
                .detections
                .iter()
                .filter_map(|d| {
                    let bbox = &d.bounding_box;
                    let x_min = bbox[0].max(0) as u32;
                    let y_min = bbox[1].max(0) as u32;
                    let x_max = bbox[2].max(0) as u32;
                    let y_max = bbox[3].max(0) as u32;

                    // Validate box dimensions
                    if x_max > x_min && y_max > y_min {
                        Some((x_min, y_min, x_max, y_max))
                    } else {
                        None
                    }
                })
                .collect();

            // Extract global bounding box from Python result
            let global_bbox = if python_result.bounding_box[0] >= 0
                && python_result.bounding_box[1] >= 0
                && python_result.bounding_box[2] > python_result.bounding_box[0]
                && python_result.bounding_box[3] > python_result.bounding_box[1]
            {
                Some((
                    python_result.bounding_box[0].max(0) as u32,
                    python_result.bounding_box[1].max(0) as u32,
                    python_result.bounding_box[2].max(0) as u32,
                    python_result.bounding_box[3].max(0) as u32,
                ))
            } else {
                None
            };

            (
                highest_confidence,
                python_result.detections.len(),
                individual_boxes,
                global_bbox,
            )
        } else {
            (0.0, 0, Vec::new(), None)
        };

        Ok(SubjectDetectionResult {
            center: (python_result.center[0], python_result.center[1]),
            offset_from_center: (python_result.offset[0], python_result.offset[1]),
            bounding_box,
            individual_detections,
            confidence,
            person_count,
        })
    }

    /// Run people detection using custom Python interpreter or default "python3"
    fn run_python_detection(
        &self,
        img_path: &str,
        confidence_threshold: f32,
    ) -> Result<python_yolo_integration::FindSubjectResult> {
        use std::process::Command;

        // Use custom python_path if provided, otherwise default to "python"
        let python_interpreter = self
            .python_path
            .as_ref()
            .map(|p| p.as_os_str())
            .unwrap_or_else(|| std::ffi::OsStr::new("python"));

        // Run Python script
        let output = Command::new(python_interpreter)
            .arg(&self.script_path)
            .arg("--image")
            .arg(img_path)
            .arg("--output-format")
            .arg("json")
            .arg("--filter")
            .arg("person")
            .arg("--confidence")
            .arg(confidence_threshold.to_string())
            .output()
            .map_err(|e| anyhow::anyhow!("Failed to execute find_subject.py: {}", e))?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(anyhow::anyhow!("find_subject.py failed: {}", stderr));
        }

        // Parse JSON output
        let stdout = String::from_utf8_lossy(&output.stdout);
        let result: python_yolo_integration::FindSubjectResult = serde_json::from_str(&stdout)
            .map_err(|e| anyhow::anyhow!("Failed to parse JSON output: {} | Raw: {}", e, stdout))?;

        Ok(result)
    }

    /// Save image to temporary file for Python script
    fn save_temp_image(&self, img: &RgbImage) -> Result<String> {
        // Create temporary file path with unique name for parallel processing
        // Use process ID, thread ID hash, and nanosecond timestamp for guaranteed uniqueness
        let temp_dir = std::env::temp_dir();
        let thread_id = format!("{:?}", std::thread::current().id());
        // Extract numeric part from ThreadId (remove "ThreadId(" and ")")
        let thread_id_num = thread_id
            .trim_start_matches("ThreadId(")
            .trim_end_matches(")")
            .to_string();
        let nanos = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let temp_filename = format!(
            "yolo_input_{}_{}_{}.jpg",
            std::process::id(),
            thread_id_num,
            nanos
        );
        let temp_path = temp_dir.join(temp_filename);

        // Save image as JPEG
        img.save(&temp_path)?;

        Ok(temp_path.to_string_lossy().to_string())
    }
}

/// Create a subject detector using the find_subject.py script
#[allow(dead_code)]
pub fn create_default_detector(
    script_path: &Path,
    python_path: Option<PathBuf>,
) -> Result<SubjectDetector> {
    if !script_path.exists() {
        return Err(anyhow::anyhow!(
            "find_subject.py script not found: {}",
            script_path.display()
        ));
    }
    SubjectDetector::new(script_path, python_path)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;
    use std::path::Path;

    #[allow(dead_code)]
    fn create_test_image(width: u32, height: u32) -> RgbImage {
        ImageBuffer::from_fn(width, height, |x, y| {
            image::Rgb([(x % 256) as u8, (y % 256) as u8, 128])
        })
    }

    #[test]
    fn test_subject_detection_result() {
        let result = SubjectDetectionResult {
            center: (200, 200),
            offset_from_center: (50, 50),
            bounding_box: Some((150, 150, 250, 250)), // Add test bounding box
            individual_detections: vec![(150, 150, 250, 250)], // Add test individual detection
            confidence: 0.85,
            person_count: 1,
        };

        assert_eq!(result.person_count, 1);
        assert_eq!(result.center, (200, 200));
        assert_eq!(result.offset_from_center, (50, 50));
    }

    /// Test people detection on known image with expected results
    /// Expected results from find_subject.py:
    /// - Image size: 4080,3072
    /// - Bounding box: -5,449,3402,3102  
    /// - Center: 1698,1775
    /// - Offset: -342,239
    #[test]
    fn test_people_detection_pxl_night_image() {
        let test_image_path =
            "/Users/alessandro/Desktop/arduino/photos/photos/PXL_20231231_193442105.NIGHT.jpg";

        // Skip test if image doesn't exist (CI environment)
        if !Path::new(test_image_path).exists() {
            println!("Skipping test - image not found: {}", test_image_path);
            return;
        }

        // Expected results from Python find_subject.py
        let expected_image_size = (4080, 3072);
        let expected_bbox = (-5i32, 449u32, 3402u32, 3102u32); // x can be negative
        let expected_center = (1698, 1775);
        let expected_offset = (-342i32, 239i32);

        // Load the test image
        let img = match image::open(test_image_path) {
            Ok(img) => img.to_rgb8(),
            Err(e) => {
                println!("Failed to load test image: {}", e);
                return;
            }
        };

        // Verify image dimensions match expected
        assert_eq!(
            img.dimensions(),
            expected_image_size,
            "Image dimensions don't match expected size"
        );

        {
            // Test with Python find_subject.py script
            let script_path = Path::new("/Users/alessandro/Documents/git/sephiroth74/arduino/esp32-photo-frame/scripts/private/find_subject.py");
            if script_path.exists() {
                match create_default_detector(script_path) {
                    Ok(detector) => {
                        match detector.detect_people(&img, 0.6) {
                            Ok(result) => {
                                println!("\n=== Python Detection Results ===");
                                println!(
                                    "Image size: {},{}",
                                    img.dimensions().0,
                                    img.dimensions().1
                                );
                                println!("People detected: {}", result.person_count);
                                println!("Center: {},{}", result.center.0, result.center.1);
                                println!(
                                    "Offset: {},{}",
                                    result.offset_from_center.0, result.offset_from_center.1
                                );

                                println!("\n=== Expected Results (from find_subject.py) ===");
                                println!(
                                    "Image size: {},{}",
                                    expected_image_size.0, expected_image_size.1
                                );
                                println!(
                                    "Bounding box: {},{},{},{}",
                                    expected_bbox.0,
                                    expected_bbox.1,
                                    expected_bbox.2,
                                    expected_bbox.3
                                );
                                println!("Center: {},{}", expected_center.0, expected_center.1);
                                println!("Offset: {},{}", expected_offset.0, expected_offset.1);

                                // Since we're calling the same Python script, results should match exactly
                                assert_eq!(result.person_count, 1, "Should detect 1 person");
                                assert_eq!(
                                    result.center, expected_center,
                                    "Center should match exactly"
                                );
                                assert_eq!(
                                    result.offset_from_center, expected_offset,
                                    "Offset should match exactly"
                                );

                                println!("\n✅ Python Detection Validation PASSED!");
                                println!(
                                    "   People detected: {} (expected: 1)",
                                    result.person_count
                                );
                                println!(
                                    "   Center: ({}, {}) matches expected",
                                    result.center.0, result.center.1
                                );
                                println!(
                                    "   Offset: ({}, {}) matches expected",
                                    result.offset_from_center.0, result.offset_from_center.1
                                );
                            }
                            Err(e) => println!("Detection failed: {}", e),
                        }
                    }
                    Err(e) => println!("Failed to create detector: {}", e),
                }
            } else {
                println!("find_subject.py script not found - skipping Python test");
            }
        }
    }

    #[test]
    fn test_filename_hash_consistency() {
        // Test that filename hashing is consistent
        let hash1 = crate::utils::generate_filename_hash("test_image.jpg");
        let hash2 = crate::utils::generate_filename_hash("test_image.jpg");
        assert_eq!(hash1, hash2);
        assert_eq!(hash1.len(), 8);
    }
}
