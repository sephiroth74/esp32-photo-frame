# InsightFace Rust Library

Rust library for face detection and recognition using ONNX models

## Features

- **Face Detection** - RetinaFace-based detection with high accuracy
- **EXIF Orientation Support** - Automatic handling of rotated images from smartphones
- **Bounding Box Visualization** - Draw detection results on images

## Quick Start

### Basic Usage - From Image Object

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize with models directory
    let mut app = FaceAnalysis::new(
        Path::new("~/.insightface/models/buffalo_l"),
        None,
    )?;

    // Prepare: ctx_id, detection_threshold, input_size
    app.prepare(0, 0.5, (640, 640))?;

    // Detect faces from image object
    let img = image::open("photo.jpg")?;
    let faces = app.get(&img)?;

    println!("Found {} faces", faces.len());
    for face in &faces {
        println!("  - Confidence: {:.2}", face.det_score);
        println!("    Box: [{:.0}, {:.0}, {:.0}, {:.0}]",
            face.bbox[0], face.bbox[1], face.bbox[2], face.bbox[3]);
    }

    Ok(())
}
```

### From File Path (with automatic EXIF orientation)

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut app = FaceAnalysis::new(
        Path::new("~/.insightface/models/buffalo_l"),
        None,
    )?;
    app.prepare(0, 0.5, (640, 640))?;

    // Detect faces from file path - handles EXIF rotation automatically
    let faces = app.get_from_path("IMG_1234.jpg")?;
    
    // Draw and save results
    let img = image::open("IMG_1234.jpg")?;
    let result = app.draw_on(&img, &faces);
    result.save("output.jpg")?;

    Ok(())
}
```

### Using as a Library Dependency

Add to your `Cargo.toml`:

```toml
[dependencies]
insightface-rs = { path = "/path/to/insightface_rs" }
# or when published:
# insightface-rs = "0.1.0"
```

## Requirements

- Rust 1.70+
- Pre-trained models (buffalo_l)

## Model Setup

The models must be placed in an accessible directory. You have several options:

### Option 1: Download from GitHub Releases (Recommended)

Download the buffalo_l models directly:

```bash
# Download and extract the models
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip
unzip buffalo_l.zip -d ~/.insightface/models/

# Or use curl
curl -L https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip -o buffalo_l.zip
unzip buffalo_l.zip -d ~/.insightface/models/
```

### Option 2: Using Python InsightFace

```bash
# Install Python InsightFace
pip install insightface

# Download models (saved to ~/.insightface/models/)
python -c "from insightface.app import FaceAnalysis; app = FaceAnalysis('buffalo_l'); app.prepare(ctx_id=0)"
```

### Using the Models

#### From Default Directory (Recommended)

The library automatically searches in `~/.insightface/models/buffalo_l/`:

```rust
let models_dir = Path::new(&std::env::var("HOME")?).join(".insightface/models/buffalo_l");
let mut app = FaceAnalysis::new(&models_dir, None)?;
```

#### From Custom Directory

```bash
# Copy models to your preferred location
mkdir -p ./models/buffalo_l
cp ~/.insightface/models/buffalo_l/*.onnx ./models/buffalo_l/
```

```rust
let mut app = FaceAnalysis::new(Path::new("./models/buffalo_l"), None)?;
```

### Required Model Files

The buffalo_l package includes:
- `det_10g.onnx` - Face detection (SCRFD-10G)
- `w600k_r50.onnx` - Face recognition (ArcFace)
- `genderage.onnx` - Gender and age estimation (optional)
- Other auxiliary models

## Installation

Add the dependency to your `Cargo.toml`:

```toml
[dependencies]
insightface-rs = { path = "../path/to/insightface_rs" }
image = "0.25"
```

## Usage Examples

### Basic Example

```rust
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Specify the models directory
    let models_dir = Path::new(&std::env::var("HOME")?)
        .join(".insightface/models/buffalo_l");
    
    // Initialize FaceAnalysis
    let mut app = FaceAnalysis::new(&models_dir, None)?;
    
    // Prepare the model: ctx_id, detection_threshold, input_size
    app.prepare(0, 0.5, (640, 640))?;
    
    // Load and process an image
    let img = image::open("face.jpg")?;
    let faces = app.get(&img)?;
    
    // Process results
    for (i, face) in faces.iter().enumerate() {
        println!("Face {}: bbox={:?}, score={:.3}", i, face.bbox, face.det_score);
        
        // Access keypoints if available
        if let Some(kps) = &face.kps {
            for (j, kp) in kps.iter().enumerate() {
                println!("  Landmark {}: x={:.1}, y={:.1}", j, kp[0], kp[1]);
            }
        }
    }
    
    Ok(())
}
```

### Configuration Options

```rust
// Change detection threshold
app.set_det_thresh(0.6);

// Change NMS threshold
app.set_nms_thresh(0.3);

// Change input size
app.prepare(0, 0.5, (512, 512))?;
```

## Library Structure

- `app.rs` - Main API (`FaceAnalysis`)
- `face.rs` - `Face` structure and utilities
- `error.rs` - Error types
- `model_zoo.rs` - ONNX model management
- `utils.rs` - Utility functions (NMS, embedding distance, etc.)

## Result Structure

Each `Face` contains:

```rust
pub struct Face {
    pub bbox: [f32; 4],              // [x1, y1, x2, y2]
    pub det_score: f32,              // Confidence score (0-1)
    pub kps: Option<Vec<[f32; 2]>>, // 5 optional keypoints
    pub embedding: Option<Vec<f32>>, // Face embedding (future)
    pub gender: Option<i32>,         // 0=Female, 1=Male (future)
    pub age: Option<i32>,            // Estimated age (future)
    pub attributes: Option<HashMap<String, serde_json::Value>>,
}
```

## Face Methods

```rust
// Bounding box dimensions
face.width()              // Width
face.height()             // Height
face.area()               // Area

// Embedding (if available)
face.embedding_norm()     // L2 norm of embedding
face.normed_embedding()   // Normalized embedding (unit vector)

// Gender
face.sex()                // "M" or "F"
```

## 🛠Utility Functions

```rust
use insightface_rs::utils::*;

// NMS - Non-Maximum Suppression
let dets = [[x1, y1, x2, y2, score], ...];
let keep = nms(&dets, 0.4);  // IoU threshold

// Embedding distance (L2)
let distance = embedding_distance(&emb1, &emb2);

// Cosine similarity
let similarity = cosine_similarity(&emb1, &emb2);
```

## Process Directory Example

```rust
use std::fs;
use insightface_rs::FaceAnalysis;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let model_dir = Path::new(&std::env::var("HOME")?)
        .join(".insightface/models/buffalo_l");
    let mut app = FaceAnalysis::new(&model_dir, None)?;
    app.prepare(0, 0.5, (640, 640))?;
    
    // Process all images in a directory
    for entry in fs::read_dir("./images")? {
        let entry = entry?;
        let path = entry.path();
        
        if path.extension().map(|e| e == "jpg" || e == "png").unwrap_or(false) {
            let img = image::open(&path)?;
            let faces = app.get(&img)?;
            
            println!("{}: {} faces detected", path.display(), faces.len());
        }
    }
    
    Ok(())
}
```

## License

This project follows the license of the original InsightFace project.

## 🔗 References

- [InsightFace GitHub](https://github.com/deepinsight/insightface)
- [InsightFace Models (v0.7)](https://github.com/deepinsight/insightface/releases/tag/v0.7)
- [ONNX Runtime Rust](https://github.com/nbz0/onnxruntime-rs)
- [RetinaFace Documentation](https://github.com/biubug6/Pytorch_Retinaface)
