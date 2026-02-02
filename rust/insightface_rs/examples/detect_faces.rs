use ab_glyph::{FontRef, PxScale};
use image::{DynamicImage, Rgb};
use imageproc::drawing::{draw_hollow_rect_mut, draw_text_mut};
use imageproc::rect::Rect;
use insightface_rs::{Face, FaceAnalysis};
use std::fs;
use std::io::Write;
use std::path::Path;
use std::time::Instant;

/// Draw faces with bounding boxes and scores on an image
fn draw_faces_with_scores(img: &DynamicImage, faces: &[Face]) -> DynamicImage {
    let mut rgb_img = img.to_rgb8();
    let (img_width, img_height) = rgb_img.dimensions();

    // Load a basic font (using DejaVu Sans Mono embedded)
    let font_data = include_bytes!("../assets/DejaVuSansMNerdFont-Regular.ttf");
    let font = FontRef::try_from_slice(font_data).expect("Failed to load font");

    let green = Rgb([0u8, 255u8, 0u8]);
    let scale = PxScale::from(20.0);

    for face in faces {
        let bbox = face.bbox;
        let x1 = bbox[0].max(0.0) as i32;
        let y1 = bbox[1].max(0.0) as i32;
        let x2 = bbox[2].min(img_width as f32) as i32;
        let y2 = bbox[3].min(img_height as f32) as i32;

        // Draw bounding box (2 pixel thick)
        if x2 > x1 && y2 > y1 {
            let rect = Rect::at(x1, y1).of_size((x2 - x1) as u32, (y2 - y1) as u32);
            draw_hollow_rect_mut(&mut rgb_img, rect, green);
            // Draw again for thickness
            if x1 + 1 < x2 && y1 + 1 < y2 {
                let rect_inner =
                    Rect::at(x1 + 1, y1 + 1).of_size((x2 - x1 - 2) as u32, (y2 - y1 - 2) as u32);
                draw_hollow_rect_mut(&mut rgb_img, rect_inner, green);
            }
        }

        // Draw score text
        let score_text = format!("{:.2}", face.det_score);

        // Determine text position based on available space
        let text_height = 25;
        let text_width = score_text.len() as i32 * 12; // Approximate

        let (text_x, text_y) = if y1 >= text_height {
            // Above the box
            (x1, y1 - text_height)
        } else if (y2 + text_height) < img_height as i32 {
            // Below the box
            (x1, y2 + 5)
        } else if (x2 + text_width) < img_width as i32 {
            // Right of the box
            (x2 + 5, y1)
        } else if x1 >= text_width {
            // Left of the box
            (x1 - text_width, y1)
        } else {
            // Inside the box at top
            (x1 + 5, y1 + 5)
        };

        draw_text_mut(
            &mut rgb_img,
            green,
            text_x.max(0),
            text_y.max(0),
            scale,
            &font,
            &score_text,
        );
    }

    DynamicImage::ImageRgb8(rgb_img)
}

/// Apply EXIF orientation to an image
fn apply_exif_orientation(img: image::DynamicImage, path: &Path) -> image::DynamicImage {
    use std::fs::File;
    use std::io::BufReader;

    let file = match File::open(path) {
        Ok(f) => f,
        Err(_) => return img,
    };

    let mut bufreader = BufReader::new(file);
    let exifreader = exif::Reader::new();
    let exif_data = match exifreader.read_from_container(&mut bufreader) {
        Ok(data) => data,
        Err(_) => return img,
    };

    let orientation = match exif_data.get_field(exif::Tag::Orientation, exif::In::PRIMARY) {
        Some(field) => match field.value.get_uint(0) {
            Some(v) => v,
            None => return img,
        },
        None => return img,
    };

    // Apply transformation based on EXIF orientation
    // https://www.impulseadventure.com/photo/exif-orientation.html
    match orientation {
        1 => img,                     // Normal
        2 => img.fliph(),             // Flip horizontal
        3 => img.rotate180(),         // Rotate 180
        4 => img.flipv(),             // Flip vertical
        5 => img.rotate90().fliph(),  // Rotate 90 CW and flip horizontal
        6 => img.rotate90(),          // Rotate 90 CW
        7 => img.rotate270().fliph(), // Rotate 270 CW and flip horizontal
        8 => img.rotate270(),         // Rotate 270 CW
        _ => img,
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Specify the models directory - use the buffalo_l model pack
    let models_dir = if let Some(home) = std::env::var_os("HOME") {
        let home_path = Path::new(&home).join(".insightface/models/buffalo_l");
        if home_path.exists() {
            home_path
        } else {
            Path::new("./models/buffalo_l").to_path_buf()
        }
    } else {
        Path::new("./models/buffalo_l").to_path_buf()
    };

    println!("Using models directory: {}", models_dir.display());
    let _ = std::io::stdout().flush();

    // Initialize the FaceAnalysis with the models directory
    let mut app = FaceAnalysis::new(&models_dir, None)?;

    // Prepare the detection model
    // Arguments: ctx_id, det_thresh, input_size
    app.prepare(0, 0.5, (640, 640))?;

    println!("Model loaded successfully!");
    println!("Detection model: det_10g.onnx (RetinaFace)");
    println!("Detection threshold: {}", app.get_det_thresh());
    println!("NMS threshold: {}", app.get_nms_thresh());
    println!("Creating output directory...");
    let _ = std::io::stdout().flush();

    // Create output directory
    fs::create_dir_all("output")?;

    // Process test images from the folder
    let test_folder = "/Users/alessandro/Desktop/arduino/photos/test";
    let image_extensions = [".jpg", ".jpeg", ".png", ".bmp"];

    if let Ok(entries) = fs::read_dir(test_folder) {
        let mut image_files: Vec<_> = entries
            .filter_map(|entry| entry.ok())
            .filter(|entry| {
                entry.path().is_file() && {
                    let ext = entry
                        .path()
                        .extension()
                        .and_then(|e| e.to_str())
                        .unwrap_or("")
                        .to_lowercase();
                    image_extensions
                        .iter()
                        .any(|&e| format!(".{}", ext) == e || e == ext)
                }
            })
            .collect();

        image_files.sort_by_key(|entry| entry.path());

        if image_files.is_empty() {
            println!("No images found in {}", test_folder);
        } else {
            println!("Found {} images to process", image_files.len());

            for entry in image_files {
                let image_path = entry.path();
                let image_filename = image_path
                    .file_name()
                    .and_then(|n| n.to_str())
                    .unwrap_or("unknown");

                print!("Processing: {}...", image_filename);
                let _ = std::io::stdout().flush();

                // Use get_from_path for automatic EXIF orientation handling
                let start = Instant::now();
                match app.get_from_path(&image_path) {
                    Ok(faces) => {
                        let elapsed = start.elapsed();
                        println!(
                            "✓ {} - Found {} faces in {:.2}ms",
                            image_filename,
                            faces.len(),
                            elapsed.as_secs_f64() * 1000.0
                        );

                        // Load image for drawing (EXIF will be applied)
                        let img = match image::open(&image_path) {
                            Ok(img) => apply_exif_orientation(img, &image_path),
                            Err(e) => {
                                eprintln!("  ✗ Failed to reload image for drawing: {}", e);
                                continue;
                            }
                        };

                        // Draw faces on image with scores
                        println!("  Drawing boxes and scores...");
                        let result_img = draw_faces_with_scores(&img, &faces);

                        // Save output
                        println!("  Saving output...");
                        let output_path = Path::new("output").join(image_filename);
                        result_img.save(&output_path)?;
                        println!("  → Saved to: {}", output_path.display());
                    }
                    Err(e) => {
                        eprintln!("✗ {} - Detection error: {}", image_filename, e);
                    }
                }
            }
        }
    } else {
        eprintln!("Error reading directory: {}", test_folder);
    }

    println!("\nProcessing complete! Results saved to output/ directory.");
    Ok(())
}
