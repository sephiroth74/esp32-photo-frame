# ESP32 Photo Frame - Rust Image Processor

A high-performance Rust implementation of the image processing pipeline for ESP32-based e-paper photo frames. This tool provides significant performance improvements over the original bash/ImageMagick pipeline while adding advanced features like YOLO11-based people detection for smart cropping.

## 🚀 Key Features

### Performance & Efficiency
- **5-10x faster processing** than bash scripts
- **Parallel batch processing** with progress tracking
- **Memory-efficient** operations with streaming support
- **Cross-platform** support (macOS, Linux, Windows)

### Image Processing
- **Smart orientation detection** with EXIF support
- **Portrait image pairing** and combination
- **Floyd-Steinberg dithering** for e-paper displays
- **ESP32-optimized binary format** generation
- **Flexible output formats** (BMP, binary, or both)

### Text Annotations
- **True font rendering** using system fonts
- **Cross-platform font support** with automatic fallbacks
- **EXIF date extraction** with intelligent filename parsing
- **Customizable font size and background colors**

### AI-Powered Features (Optional)
- **YOLO11-based people detection** for smart cropping (upgraded from YOLOv3 for better accuracy)
- **Intelligent crop positioning** to keep people in frame
- **Detection statistics** with verbose logging
- **Configurable confidence thresholds**

## 📋 Requirements

### System Requirements
- **Rust 1.70+** with Cargo
- **Operating System**: macOS, Linux, or Windows
- **Memory**: Minimum 4GB RAM (8GB+ recommended for large batches)

### Dependencies
The project uses several high-performance Rust crates:
- `image` - Core image processing
- `imageproc` - Advanced image operations
- `fast_image_resize` - High-quality resizing
- `ab_glyph` - Font rendering
- `rayon` - Parallel processing
- `clap` - Command-line interface

### AI Features (Optional)
To enable YOLO people detection:
- **Feature flag**: `ai` (enabled by default)
- **YOLO models**: YOLO11 model file (`yolo11n.onnx` - nano model for optimal performance)
- **Additional dependencies**: `onnxruntime`, `ndarray`

## 🛠 Installation

### Building from Source

1. **Clone the repository**:
   ```bash
   git clone https://github.com/sephiroth74/arduino/esp32-photo-frame.git
   cd esp32-photo-frame/rust/photoframe-processor
   ```

2. **Build the project**:
   ```bash
   # Standard build (AI features enabled by default)
   cargo build --release
   
   # Build without AI features
   cargo build --release --no-default-features
   ```

3. **Install locally** (optional):
   ```bash
   cargo install --path .
   ```

### YOLO11 Model Setup (for AI features)

1. **Download YOLO11 model**:
   The YOLO11 nano model (`yolo11n.onnx`) is included in the project assets. If you need to update or use a different model:
   ```bash
   # The model file should be placed in scripts/private/assets/
   # Default location: scripts/private/assets/yolo11n.onnx
   ```

2. **Verify installation**:
   ```bash
   ./target/release/photoframe-processor --help
   ```

## 🎯 Usage

### Basic Usage

```bash
# Process all images in a directory (default: 800x480, both BMP and binary)
photoframe-processor -i ~/Photos -o ~/processed --auto

# Process a single image file
photoframe-processor -i ~/Photos/image.jpg -o ~/processed

# Custom size and output format
photoframe-processor -i ~/Photos -o ~/processed -s 1024x768 --output-format bin
```

### Advanced Options

```bash
# 6-color processing with custom font
photoframe-processor -i ~/Photos -o ~/processed \
  -t 6c \
  --font "Arial-Bold" \
  --pointsize 32 \
  --auto --verbose

# Multiple input sources
photoframe-processor \
  -i ~/Photos/2023 \
  -i ~/Photos/2024 \
  -i ~/single-image.jpg \
  -o ~/processed \
  --jobs 8
```

### AI-Powered People Detection (YOLO11)

```bash
# Enable YOLO11 people detection with custom thresholds
photoframe-processor -i ~/Photos -o ~/processed \
  --detect-people \
  --python-script ./scripts/private/find_subject.py \
  --auto --verbose

# People detection with detailed statistics and multiple formats
photoframe-processor -i ~/family-photos -o ~/processed \
  --detect-people \
  --python-script ./scripts/private/find_subject.py \
  --output-format bmp,bin,png \
  --verbose  # Shows detection statistics
```

### Utility Commands

```bash
# Find original filename from hash (for debugging processed images)
photoframe-processor --find-hash a1b2c3d4

# Decode combined portrait filename to show original filenames
photoframe-processor --find-original "combined_bw_aW1hZ2Ux_aW1hZ2Uy.bin"

# Dry run: simulate processing without creating files
photoframe-processor -i ~/Photos -o ~/processed --auto --dry-run --verbose

# Validate images without processing
photoframe-processor -i ~/Photos --validate-only --verbose

# Debug mode: visualize detection boxes and crop areas
photoframe-processor -i ~/Photos -o ~/processed --detect-people \
  --python-script ./scripts/private/find_subject.py --debug --verbose
```

## ⚙️ Configuration Options

### Processing Types
- `bw` - Black & white processing (default)
- `6c` - 6-color processing for color e-paper displays
- `7c` - 7-color processing for advanced color e-paper displays

### Output Formats
- `bmp` - Generate only BMP files
- `bin` - Generate only ESP32 binary files
- `jpg` - Generate only JPEG files
- `png` - Generate only PNG files
- Multiple formats can be specified with comma separation (e.g., `bmp,bin,jpg`)

### Font Options
- `--font` - Font family name (e.g., "Arial", "Helvetica-Bold")
- `--pointsize` - Font size in pixels (default: 24)
- `--annotate_background` - Hex color with alpha (e.g., "#00000040")

### AI Detection Options
- `--detect-people` - Enable YOLO11 people detection
- `--python-script` - Path to find_subject.py script for people detection
- `--python-path` - Path to Python interpreter (if not in system PATH)

### Utility Options
- `--find-hash` - Find original filename from an 8-character hash
- `--find-original` - Decode combined portrait filename to show original filenames
- `--dry-run` - Simulate processing without creating files
- `--validate-only` - Skip processing and only validate input files
- `--debug` - Enable debug mode with visualization of detection boxes
- `--annotate` - Enable filename annotations on processed images
- `--auto-color` - Enable automatic color correction before processing

### Performance Options
- `--jobs` - Number of parallel jobs (0 = auto-detect cores)
- `--auto` - Enable automatic orientation handling
- `--verbose` - Show detailed processing information

## 📊 Performance Comparison

| Feature | Bash/ImageMagick | Rust Processor | Improvement |
|---------|------------------|----------------|-------------|
| **Processing Speed** | ~2.5s per image | ~0.3s per image | **8.3x faster** |
| **Memory Usage** | ~150MB peak | ~45MB peak | **3.3x less** |
| **Parallel Processing** | Limited | Full CPU utilization | **N-core scaling** |
| **Font Rendering** | Basic | True font support | **Quality improvement** |
| **People Detection** | None | YOLO-based | **New feature** |

## 🧠 AI Features Deep Dive

### People Detection Workflow

1. **Image Analysis**: YOLO model analyzes each image for people
2. **Bounding Box Calculation**: Determines combined area containing all detected people  
3. **Smart Cropping**: Adjusts crop position to keep people optimally framed
4. **Confidence Filtering**: Only uses detections above specified threshold
5. **Non-Maximum Suppression**: Removes overlapping detections

### Detection Statistics (Verbose Mode)

When `--verbose` is enabled, the processor outputs detailed statistics:

```
🔍 Running people detection on: family-photo.jpg
📊 People Detection Results:
   • People found: 3
   • Highest confidence: 87.23%
   • Combined bounding box: 450×380 at (125, 90)
   • Detection center: (350, 280)
   • Offset from image center: (+25, -40)
   • Offset percentage: (+3.1%, -5.2%)
   • ✓ People well-centered, standard cropping suitable
```

### Smart Cropping Algorithm

The processor implements a sophisticated three-stage smart cropping algorithm with face preservation:

#### Stage 1: Scaling Check
- **Image-based scaling**: Only scales down when the image is too small for the target crop dimensions
- **No unnecessary scaling**: Does not scale if detection box is larger than crop (instead centers crop on detection)
- **Padding calculation**: Uses 20% padding factor when scaling is necessary

#### Stage 2: Regular Crop Attempt
- **Center-based expansion**: Expands from detection box center to target dimensions
- **Containment check**: Verifies if detection fits entirely within crop area
- **Fast path**: Uses regular crop if detection is fully contained (optimal performance)

#### Stage 3: Smart Bidirectional Expansion
When regular crop would cut the detection, applies intelligent expansion:

**Horizontal (X-axis) Expansion**:
- If detection width ≥ target width: centers crop on detection
- If detection width < target width: expands equally left/right
- **Boundary redistribution**: When hitting image edge, redistributes expansion to opposite side

**Vertical (Y-axis) Expansion with Face Preservation**:
- If detection height ≥ target height: centers crop on detection
- If detection height < target height: expands equally top/bottom with redistribution
- **Face preservation priority**: If final crop_y > det_y_min (would cut face), aligns crop with detection top
- **Rationale**: Faces are typically at the top of person bounding boxes; cutting the bottom is preferable to cutting faces

#### Example Scenarios

**Scenario 1: Portrait with Person**
```
Image: 960×1280 (portrait)
Detection: (83, 64, 958, 1205) - person box
Target crop: 768×1280 (portrait orientation, auto mode)

Result: crop_x=136, crop_y=0
- X: Centered on detection center (520) → 520 - 384 = 136 ✓
- Y: Starts at 0 (detection top at 64 is included) ✓
- Face preserved: Top of detection included in crop ✓
```

**Scenario 2: Detection Near Bottom**
```
Image: 800×1000
Detection: (300, 750, 500, 950) - person near bottom
Target crop: 400×600

Without face preservation: crop_y=400 (would start below detection top)
With face preservation: crop_y=400 (maximum to fit, preserves as much as possible)
- Algorithm prioritizes keeping detection top (faces) in frame
```

**Scenario 3: Wide Detection Box**
```
Image: 1000×800
Detection: (100, 100, 900, 700) - 800×600 box
Target crop: 600×400

Result: Centers crop on detection, lets edges be cut
- No scaling needed (image is large enough for crop)
- X: Centered on detection (500 - 300 = 200)
- Y: Aligned with detection top for face preservation (100)
```

### Detection Statistics (Verbose Mode)

When `--verbose` is enabled, the processor outputs detailed statistics:

```
🔍 Running people detection on: family-photo.jpg
📊 People Detection Results:
   • People found: 3
   • Highest confidence: 87.23%
   • Combined bounding box: 450×380 at (125, 90)
   • Detection center: (350, 280)
   • Smart crop applied: bidirectional expansion with face preservation
   • Crop area: (75, 90, 475, 490)
```

## 🔧 Integration with ESP32 Pipeline

### Binary Format Output

The processor generates ESP32-optimized binary files:

- **Bit-packed format** for efficient storage
- **Direct memory mapping** for fast ESP32 loading
- **Optimized for e-paper displays** (1-bit or 3-bit color)
- **Filename preservation** in generated binaries

### Compatibility

- **Drop-in replacement** for bash processing scripts
- **Same output format** as original pipeline
- **Compatible file naming** conventions
- **Preserves EXIF orientation** handling

## 🚨 Troubleshooting

### Common Issues

**Font not found errors**:
```bash
# Check available system fonts
fc-list  # Linux
ls /System/Library/Fonts/  # macOS

# Use a fallback font
photoframe-processor --font "Arial" ...  # Try common fonts
```

**YOLO11 model errors**:
```bash
# Verify model file exists
ls -la scripts/private/assets/
# Should show: yolo11n.onnx and coco.names

# Check file permissions
chmod 644 scripts/private/assets/*

# Test with Python script directly
python scripts/private/find_subject.py --image test.jpg --model scripts/private/assets/yolo11n.onnx
```

**Memory issues with large batches**:
```bash
# Reduce parallel jobs
photoframe-processor --jobs 2 ...

# Process in smaller batches
find ~/Photos -name "*.jpg" | head -50 | xargs -I {} photoframe-processor -i {} ...
```

### Performance Tuning

**Optimize for your system**:
```bash
# Check CPU cores
nproc  # Linux
sysctl -n hw.ncpu  # macOS

# Set optimal job count (usually cores - 1)
photoframe-processor --jobs 7 ...  # For 8-core system
```

**Memory optimization**:
- Process images in smaller batches for very large collections
- Use `--output-format bin` if you only need binary files
- Consider increasing system swap if processing massive images

## 🤝 Contributing

### Development Setup

1. **Install Rust toolchain**:
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

2. **Clone and setup**:
   ```bash
   git clone https://repo-url
   cd esp32-photo-frame/rust/photoframe-processor
   cargo build
   ```

3. **Run tests**:
   ```bash
   cargo test
   cargo test --features ai  # Test AI features
   ```

### Code Structure

```
src/
├── main.rs                 # CLI entry point
├── cli.rs                  # Command-line argument parsing
├── utils.rs                # Utility functions
└── image_processing/
    ├── mod.rs              # Processing engine
    ├── annotate.rs         # Text annotation with font support
    ├── binary.rs           # ESP32 binary format generation
    ├── combine.rs          # Portrait image combination
    ├── convert.rs          # Color conversion and dithering
    ├── orientation.rs      # EXIF orientation handling
    ├── resize.rs           # Smart resizing with people detection
    ├── subject_detection.rs # YOLO people detection
    └── batch.rs            # Batch processing coordination
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

## 🔗 Related Documentation

- [Main Project README](../README.md)
- [Technical Specifications](./tech_specs.md)
- [Image Processing Pipeline](./image_processing.md)
- [Binary Format Documentation](./bin_2_image.md)

## 📈 Roadmap

### Recent Achievements
- [x] YOLO11 model integration for people detection (upgraded from YOLOv3)
- [x] Smart cropping with face preservation (three-stage algorithm)
- [x] Bidirectional expansion with boundary redistribution
- [x] Portrait image pairing and combination
- [x] Multi-format output support (BMP, binary, JPG, PNG)
- [x] Utility commands for filename management and debugging

### Near-term Goals
- [ ] Custom color palette support
- [ ] Batch processing resume functionality
- [ ] Enhanced error recovery and reporting
- [ ] Performance optimization for large image batches

### Future Enhancements
- [ ] GPU acceleration support
- [ ] Additional AI models (face detection, object recognition)
- [ ] Cloud processing integration
- [ ] REST API for remote processing
- [ ] Docker containerization

---

**Built with ❤️ for the ESP32 Photo Frame project**