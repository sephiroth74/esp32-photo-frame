# ESP32 Photo Frame - Rust Image Processor

A high-performance Rust implementation of the image processing pipeline for ESP32-based e-paper photo frames. This tool provides optimized processing with advanced features like YOLO11-based people detection for smart cropping.

## 🚀 Key Features

### Performance & Efficiency
- **5-10x faster processing** with optimized algorithms
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
   # Standard build (without AI features)
   cargo build --release
   
   # Build with AI people detection features (YOLO11)
   cargo build --release --features ai
   
   # Build with all features
   cargo build --release --all-features
   ```

3. **Install locally** (optional):
   ```bash
   cargo install --path .
   cargo install --path . --features ai  # With AI support
   ```

### YOLO11 Model Setup (for AI features)

The YOLO11 nano model (`yolo11n.onnx`) is embedded directly into the Rust binary when you build with `--features ai`. No additional setup is required!

**Verify installation**:
```bash
./target/release/photoframe-processor --help | grep detect-people
```

## 🎯 Usage

### Basic Usage

```bash
# Process all images in a directory (default: 800x480 landscape, BW, PFR1 format)
photoframe-processor -i ~/Photos -o ~/processed -t bw --output-format pfr1

# Process a single image file
photoframe-processor -i ~/Photos/image.jpg -o ~/processed

# Black & white processing with binary output
photoframe-processor -i ~/Photos -o ~/processed -t bw --output-format pfr1

# 6-color processing (hardware 800x480)
photoframe-processor -i ~/Photos -o ~/processed -t 6c --output-format pfr1

# Multiple input sources
photoframe-processor \
  -i ~/Photos/2023 \
  -i ~/Photos/2024 \
  -i ~/single-image.jpg \
  -o ~/processed
```

### Advanced Options

```bash
# 6-color processing with custom font and annotations
photoframe-processor -i ~/Photos -o ~/processed \
  -t 6c \
  --font "Arial" \
  --pointsize 28 \
  --annotate \
  --verbose

# Multiple output formats (creates subdirectories: bmp/, pfr1/, jpg/)
photoframe-processor -i ~/Photos -o ~/processed \
  --output-format bmp,pfr1,jpg \
  -j 8

# Brightness and contrast adjustment
photoframe-processor -i ~/Photos -o ~/processed \
  --brightness 20 \
  --contrast 30 \
  --saturation-boost 1.2

# Dithering customization
photoframe-processor -i ~/Photos -o ~/processed \
  --dithering floyd-steinberg \
  --dither-strength 1.5

# Parallel processing with custom job count
photoframe-processor -i ~/Photos -o ~/processed -j 4 --verbose

# Dry run: simulate processing without creating files
photoframe-processor -i ~/Photos -o ~/processed --dry-run --verbose
```

### AI-Powered People Detection (YOLO11 - Optional)

```bash
# Enable YOLO11 people detection (requires --features ai at build time)
photoframe-processor -i ~/Photos -o ~/processed \
  --detect-people \
  --verbose

# People detection with custom confidence threshold
photoframe-processor -i ~/family-photos -o ~/processed \
  --detect-people \
  --confidence 0.7 \
  --verbose

# Debug mode: visualize detection boxes and crop areas
photoframe-processor -i ~/Photos -o ~/processed \
  --detect-people \
  --debug \
  --verbose

# Multiple output formats with people detection
photoframe-processor -i ~/Photos -o ~/processed \
  --detect-people \
  --output-format bmp,pfr1,jpg \
  --verbose
```

### Utility Commands

```bash
# Validate a .pfr1 binary file
photoframe-processor --validate ~/output/image.pfr1

# Generate processing report table
photoframe-processor -i ~/Photos -o ~/processed --report --verbose

# Report in different formats
photoframe-processor -i ~/Photos -o ~/processed --report --report-output-format json
photoframe-processor -i ~/Photos -o ~/processed --report --report-output-format plain
photoframe-processor -i ~/Photos -o ~/processed --report --report-output-format rich

# Load configuration from JSON file
photoframe-processor -i ~/Photos -o ~/processed --config-file config.pfconfig

# JSON progress output (for GUI integration)
photoframe-processor -i ~/Photos -o ~/processed --json-progress

# Auto color correction before processing
photoframe-processor -i ~/Photos -o ~/processed --auto-color --verbose

# Display dimensions and orientation
photoframe-processor -i ~/Photos -o ~/processed \
  --orientation landscape \
  --verbose
```

## ⚙️ Configuration Options

### Processing & Display Options
- `-i, --input <DIR|FILE>` - Input directory or image file (can be specified multiple times)
- `-o, --output <DIR>` - Output directory for processed images (default: ".")
- `-t, --type <TYPE>` - Display type: `bw` (black & white) or `6c` (6-color) (default: "bw")
- `--orientation <ORIENTATION>` - Display mounting orientation:
  - `0` or `landscape` - Display mounted horizontally (800×480 visual)
  - `1` or `portrait` - Display mounted vertically (480×800 visual)
  - `2` or `landscape-reverse` - Upside-down horizontal
  - `3` or `portrait-reverse` - Upside-down vertical
  - (default: "landscape")

### Output Format Options
- `--output-format <FORMAT>` - Output formats: comma-separated list of `bmp`, `pfr1`, `jpg`, `png`
  - Examples: `--output-format pfr1`, `--output-format bmp,pfr1,jpg`
  - (default: "pfr1")

### File Processing Options
- `--extensions <EXTENSIONS>` - File extensions to process (comma-separated, default: "jpg,jpeg,png,heic,webp,tiff")
- `--validate <FILE>` - Validate a .pfr1 binary file and exit
- `--config-file <FILE>` - Load configuration from JSON file (compatible with Flutter app .pfconfig files)
- `--dry-run` - Simulate processing without creating files

### Image Adjustment Options
- `--brightness <ADJUSTMENT>` - Brightness adjustment (-100 to 100, default: 0)
  - Positive = lighter, negative = darker
- `--contrast <ADJUSTMENT>` - Contrast adjustment (-100 to 100, default: 0)
  - Positive = increase, negative = decrease
- `--saturation-boost <MULTIPLIER>` - Saturation boost (0.5-2.0, default: 1.1)
  - >1.0 = more vibrant, <1.0 = less vibrant
- `--auto-color` - Enable automatic color correction before processing (uses ImageMagick if available)

### Dithering Options
- `--dithering <METHOD>` - Dithering algorithm (default: "floyd-steinberg")
  - `floyd-steinberg` - Best gradients
  - `atkinson` - Bright, high contrast
  - `stucki` - Diffused dithering
  - `jarvis` - Good for photos
  - `ordered` - Good for text
- `--dither-strength <STRENGTH>` - Dithering strength multiplier (0.0-2.0, default: 1.0)
  - 1.0 = normal, <1.0 = subtle, >1.0 = pronounced
- `--auto-optimize` - Automatically select optimal dithering, strength, and contrast for each image

### Text Annotation Options
- `--annotate` - Enable filename annotations on processed images (default: false)
- `--font <FONT>` - Font specification for annotations (default: "Arial")
  - Can be font name ("Arial"), filename ("Arial.ttf"), or full path
- `--pointsize <SIZE>` - Font size for annotations (default: 22)
- `--annotate_background <COLOR>` - Background color for annotations (hex with alpha, default: "#00000040")

### Portrait Image Combination Options
- `--divider-width <WIDTH>` - Width of divider line between combined portrait images (default: 3)
- `--divider-color <COLOR>` - Color of divider line (hex RGB, default: "#FFFFFF" for white)

### AI Detection Options (requires `--features ai` at build time)
- `--detect-people` - Enable YOLO11 people detection for smart cropping
- `--confidence <THRESHOLD>` - Confidence threshold for people detection (0.0-1.0, default: 0.6)
- `--debug` - Enable debug mode: visualize detection boxes and crop areas

### Output & Reporting Options
- `--report` - Display formatted table with processing parameters
- `--report-output-format <FORMAT>` - Report output format (default: "rich")
  - `plain` - Simple text
  - `rich` - Formatted table
  - `json` - Structured data
- `--json-progress` - Output progress as JSON lines (for GUI integration, suppresses all other output)

### Performance Options
- `-j, --jobs <N>` - Number of parallel processing jobs (0 = auto-detect CPU cores, default: 0)

### Verbosity Options
- `-v, --verbose` - Enable verbose output with detailed progress information
- `--debug` - Enable debug mode with visualization (for AI detection)

## 📊 Performance Characteristics

| Feature                 | Specification                            |
| ----------------------- | ---------------------------------------- |
| **Processing Speed**    | ~0.3s per image                          |
| **Memory Usage**        | ~45MB peak                               |
| **Parallel Processing** | Full CPU utilization with N-core scaling |
| **Font Rendering**      | True font support with system fonts      |
| **People Detection**    | YOLO11-based smart cropping              |

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

- **ESP32 compatible output** formats
- **Standard file naming** conventions
- **Preserves EXIF orientation** handling
- **Cross-platform** support

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

**AI detection not working**:
```bash
# Verify the binary was compiled with AI features
photoframe-processor --help | grep detect-people

# Test with verbose output to see detection results
photoframe-processor -i test.jpg -o output --detect-people --verbose

# Adjust confidence threshold if needed
photoframe-processor -i test.jpg -o output --detect-people --confidence 0.5
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
- [Binary Format Documentation](./BINARY_FILE_FORMAT.md)

## 📈 Roadmap

### Recent Achievements
- [x] YOLO11 model integration for people detection (upgraded from YOLOv3)
- [x] Smart cropping with face preservation (three-stage algorithm)
- [x] Bidirectional expansion with boundary redistribution
- [x] Portrait image pairing and combination
- [x] Multi-format output support (BMP, binary, JPG, PNG)
- [x] Utility commands for filename management and debugging

### Future Enhancements
- [ ] GPU acceleration support
- [ ] Additional AI models (face detection, object recognition)
- [ ] Cloud processing integration
- [ ] REST API for remote processing
- [ ] Docker containerization

---

**Built with ❤️ for the ESP32 Photo Frame project**