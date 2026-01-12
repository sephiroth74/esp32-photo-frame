use anyhow::Result;
use exif::{Exif, Reader, Tag};
use image::{imageops::FilterType, DynamicImage, GenericImageView, Pixel};
use ort::session::{builder::GraphOptimizationLevel, Session};
use ort::value::Value;
use std::fs::File;
use std::io::BufReader;

#[derive(Debug, Clone)]
struct Detection {
    x: f32,
    y: f32,
    width: f32,
    height: f32,
    confidence: f32,
    class_id: usize,
}

fn main() -> Result<()> {
    // Get image path from command line arguments
    let args: Vec<String> = std::env::args().collect();
    let img_path = if args.len() > 1 {
        &args[1]
    } else {
        "/Users/alessandro/Desktop/arduino/photos/photos/20221227_145520.jpg"
    };

    println!("🚀 YOLO11 Rust Inference Test (with EXIF rotation)");
    println!("===================================================\n");

    // 1. Initialize ONNX Runtime
    println!("📦 Initializing ONNX Runtime...");
    let _ = ort::init();

    // 2. Load the YOLO11 model
    let model_path = "/Users/alessandro/Documents/git/sephiroth74/arduino/esp32-photo-frame/scripts/private/assets/yolo11n.onnx";
    println!("🧠 Loading YOLO11 model from: {}", model_path);

    let mut session = Session::builder()?
        .with_optimization_level(GraphOptimizationLevel::Level3)?
        .with_intra_threads(4)?
        .commit_from_file(model_path)?;

    println!("✅ Model loaded successfully!\n");

    // 3. Load and prepare the test image
    println!("📷 Loading test image: {}", img_path);

    // Load image and apply EXIF rotation
    let original_img = load_and_rotate_image(img_path)?;
    let original_dimensions = original_img.dimensions();
    println!(
        "   Original size (after EXIF rotation): {}x{}",
        original_dimensions.0, original_dimensions.1
    );

    // Resize to 640x640 for YOLO
    let (width, height) = (640, 640);
    let resized = original_img.resize_exact(width, height, FilterType::Triangle);
    println!("   Resized to: {}x{}\n", width, height);

    // 4. Convert image to tensor [Batch, Channels, Height, Width]
    println!("🔄 Converting image to tensor...");

    // Create a flat vector for the tensor data in NCHW format
    let mut tensor_data = Vec::with_capacity(1 * 3 * 640 * 640);

    // Fill the tensor in NCHW format
    for c in 0..3 {
        for y in 0..640 {
            for x in 0..640 {
                let pixel = resized.get_pixel(x, y);
                let rgb = pixel.to_rgb();
                let value = rgb[c] as f32 / 255.0;
                tensor_data.push(value);
            }
        }
    }

    // Create the input tensor using ort's Value
    let input_shape = vec![1usize, 3, 640, 640];
    let input_value = Value::from_array((input_shape, tensor_data))?;

    println!("   Tensor shape: [1, 3, 640, 640]\n");

    // 5. Run inference
    println!("⚡ Running inference...");
    let outputs = session.run(ort::inputs!["images" => input_value])?;

    // 6. Get output tensor
    let (output_shape, output_data) = outputs["output0"].try_extract_tensor::<f32>()?;
    println!("✅ Inference completed!");
    println!("   Output shape: {:?}\n", output_shape);

    // 7. Post-processing
    println!("🔍 Post-processing detections...");
    let shape_vec: Vec<usize> = output_shape.iter().map(|&x| x as usize).collect();
    let detections = process_yolo_output(output_data, shape_vec.as_slice(), 0.5, 0.4)?;

    println!("\n📊 Detection Results:");
    println!("   Total detections: {}", detections.len());

    // Filter for person class (class_id = 0 in COCO)
    let person_detections: Vec<_> = detections.iter().filter(|d| d.class_id == 0).collect();

    println!("   👤 People detected: {}", person_detections.len());

    if !person_detections.is_empty() {
        println!("\n   Person detections:");
        for (i, det) in person_detections.iter().enumerate() {
            // Scale coordinates back to original image size
            let scale_x = original_dimensions.0 as f32 / 640.0;
            let scale_y = original_dimensions.1 as f32 / 640.0;

            let x_min = (det.x - det.width / 2.0) * scale_x;
            let y_min = (det.y - det.height / 2.0) * scale_y;
            let x_max = (det.x + det.width / 2.0) * scale_x;
            let y_max = (det.y + det.height / 2.0) * scale_y;

            println!("   [{}] Confidence: {:.2}%", i + 1, det.confidence * 100.0);
            println!(
                "       Box: ({:.0}, {:.0}) -> ({:.0}, {:.0})",
                x_min.max(0.0),
                y_min.max(0.0),
                x_max,
                y_max
            );
        }

        // Calculate combined bounding box (like Python script does)
        if !person_detections.is_empty() {
            let scale_x = original_dimensions.0 as f32 / 640.0;
            let scale_y = original_dimensions.1 as f32 / 640.0;

            let mut min_x = f32::MAX;
            let mut min_y = f32::MAX;
            let mut max_x = f32::MIN;
            let mut max_y = f32::MIN;

            for det in &person_detections {
                let x_min = ((det.x - det.width / 2.0) * scale_x).max(0.0);
                let y_min = ((det.y - det.height / 2.0) * scale_y).max(0.0);
                let x_max = (det.x + det.width / 2.0) * scale_x;
                let y_max = (det.y + det.height / 2.0) * scale_y;

                min_x = min_x.min(x_min);
                min_y = min_y.min(y_min);
                max_x = max_x.max(x_max);
                max_y = max_y.max(y_max);
            }

            let center_x = (min_x + max_x) / 2.0;
            let center_y = (min_y + max_y) / 2.0;
            let offset_x = center_x - (original_dimensions.0 as f32 / 2.0);
            let offset_y = center_y - (original_dimensions.1 as f32 / 2.0);

            println!("\n   📍 Combined bounding box:");
            println!(
                "       Box: ({:.0}, {:.0}) -> ({:.0}, {:.0})",
                min_x, min_y, max_x, max_y
            );
            println!("       Center: ({:.0}, {:.0})", center_x, center_y);
            println!(
                "       Offset from image center: ({:.0}, {:.0})",
                offset_x, offset_y
            );
        }
    } else {
        println!("   No people detected in the image.");
    }

    println!("\n✨ Test completed successfully!");

    Ok(())
}

/// Load image and apply EXIF rotation
fn load_and_rotate_image(path: &str) -> Result<DynamicImage> {
    let file = File::open(path)?;
    let mut bufreader = BufReader::new(file);

    // Try to read EXIF data
    let exif_reader = Reader::new();
    let orientation = if let Ok(exif) = exif_reader.read_from_container(&mut bufreader) {
        if let Some(field) = exif.get_field(Tag::Orientation, exif::In::PRIMARY) {
            field.value.get_uint(0).unwrap_or(1) as u32
        } else {
            1
        }
    } else {
        1
    };

    println!("   EXIF Orientation: {}", orientation);

    // Load the image
    let mut img = image::open(path)?;

    // Apply rotation based on EXIF orientation
    img = match orientation {
        1 => img,                     // Normal
        2 => img.fliph(),             // Flip horizontal
        3 => img.rotate180(),         // Rotate 180
        4 => img.flipv(),             // Flip vertical
        5 => img.rotate90().fliph(),  // Rotate 90 CW + flip horizontal
        6 => img.rotate90(),          // Rotate 90 CW
        7 => img.rotate270().fliph(), // Rotate 270 CW + flip horizontal
        8 => img.rotate270(),         // Rotate 270 CW
        _ => img,
    };

    Ok(img)
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
            // Original layout: [batch, features, predictions]
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
