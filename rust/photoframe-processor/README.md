# ESP32 Photo Frame Processor (Rust)

High-performance Rust implementation of the ESP32 Photo Frame image processing pipeline with optimized performance and enhanced functionality.

**Available as both CLI and GUI!** 🎨

- **CLI**: Powerful command-line tool for batch processing and automation
- **GUI**: User-friendly graphical interface built with egui (see [GUI_README.md](GUI_README.md))

## 🚀 Performance Benefits

- **5-10x faster processing** with optimized algorithms
- **Parallel batch processing** with automatic CPU core detection
- **Memory efficient** with optimized algorithms
- **Cross-platform** support (Windows, macOS, Linux)
- **Self-contained** - no external dependencies required

## ✨ Features

- **Smart Orientation Detection**: Reads EXIF data and handles all 8 orientation modes
- **Portrait Pairing**: Automatically combines portrait images side-by-side
- **Multi-Format Output**: Support for BMP, Binary, JPG, and PNG with organized subdirectories
- **High-Quality Resizing**: Uses Lanczos3 filtering for optimal image quality
- **Floyd-Steinberg Dithering**: Professional-grade dithering for e-paper displays
- **Text Annotations**: Filename overlays with customizable fonts and backgrounds
- **AI People Detection**: Optional smart cropping based on person detection (with YOLO11)
- **Auto Color Correction**: Intelligent color/saturation/levels adjustment using ImageMagick
- **ESP32 Binary Format**: Generates optimized .bin files for direct ESP32 loading
- **Progress Tracking**: Real-time progress bars with ETA estimates
- **Debug Visualization**: Detection box overlay for AI debugging
- **Duplicate Handling**: Smart skip logic to avoid reprocessing existing images
- **Hash Lookup**: Find original filenames from output file hashes
- **Comprehensive Validation**: Input validation with helpful error messages

## 📦 Installation

### Prerequisites

- Rust 1.70+ installed ([rustup.rs](https://rustup.rs/))

### Build from Source

```bash
cd rust/photoframe-processor
cargo build --release
```

The compiled binary will be available at `target/release/photoframe-processor`.

### Optional Features

```bash
# Build with AI-powered people detection using embedded YOLO11
cargo build --release --features ai

# The AI feature embeds the YOLO11n model directly in the binary
# No external files required - the model is included at compile time
```

## 🔧 Usage

### Basic Usage

```bash
# Black & white processing
./photoframe-processor -i ~/Photos -o ~/processed -t bw -s 800x480 --auto

# 6-color processing with multiple output formats
./photoframe-processor -i ~/Photos -o ~/processed -t 6c -s 800x480 --output-format bmp,bin,jpg --auto

# Generate only binary files for ESP32
./photoframe-processor -i ~/Photos -o ~/processed -t bw -s 800x480 --output-format bin --auto
```

### Advanced Usage

```bash
# Multiple input directories with custom settings and formats
./photoframe-processor \
  -i ~/Photos/2023 -i ~/Photos/2024 \
  -o ~/processed_photos \
  -t bw -s 800x480 \
  --output-format bmp,bin,png \
  --font "Arial-Bold" \
  --pointsize 32 \
  --extensions jpg,png,heic,webp \
  --jobs 8 \
  --auto --verbose

# With AI people detection and auto color correction (requires --features ai)
./photoframe-processor \
  -i ~/Photos -o ~/processed \
  -t 6c -s 800x480 \
  --output-format bmp,bin,jpg \
  --detect-people \
  --auto-color --annotate --force

# Validate files without processing
./photoframe-processor -i ~/Photos -o ~/processed -s 800x480 --validate-only

# Debug mode with detection visualization (requires --features ai)
./photoframe-processor -i ~/Photos -o ~/processed -s 800x480 --debug \
  --detect-people --output-format png

# Decode combined portrait filenames to find original images
./photoframe-processor --find-original "combined_bw_aW1hZ2Ux_aW1hZ2Uy.bin"

# Find original filename from hash (8-character hex)
./photoframe-processor --find-hash "a1b2c3d4"

# Dry run mode to simulate processing without creating files
./photoframe-processor -i ~/Photos -o ~/processed -t 6c --dry-run --verbose
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-i, --input <DIR>` | Input directories (can specify multiple) | **Required** |
| `-o, --output <DIR>` | Output directory for processed images | **Required** |
| `-s, --size <WIDTHxHEIGHT>` | Target display resolution (e.g., 800x480) | **Required** |
| `-t, --type <TYPE>` | Processing type: `bw`, `6c`, or `7c` | `bw` |
| `--output-format <FORMATS>` | Output formats: comma-separated list of `bmp`, `bin`, `jpg`, `png` | `bmp` |
| `--extensions <LIST>` | File extensions to process (comma-separated) | `jpg,jpeg,png,heic,webp,tiff` |
| `--auto` | Enable automatic orientation handling | `false` |
| `--pointsize <SIZE>` | Font size for annotations | `22` |
| `--font <FONT>` | Font family for annotations | `Arial` |
| `--annotate_background <COLOR>` | Text background color (hex with alpha) | `#00000040` |
| `--annotate` | Enable filename annotations on images | `false` |
| `--divider-width <WIDTH>` | Width of divider line between combined portraits (pixels) | `3` |
| `--divider-color <COLOR>` | Color of divider line (hex RGB, e.g., #FFFFFF) | `#FFFFFF` |
| `--auto-color` | Enable automatic color correction | `false` |
| `--detect-people` | Enable AI people detection for smart cropping (requires --features ai) | `false` |
| `--debug` | Debug mode: visualize detection boxes | `false` |
| `--force` | Process all files, bypassing duplicate checks | `false` |
| `--find-hash <HASH>` | Find original filename for given hash | None |
| `--find-original <FILENAME>` | Decode combined filename to show original filenames | None |
| `-j, --jobs <N>` | Number of parallel processing jobs (0 = auto) | `0` |
| `-v, --verbose` | Enable detailed output | `false` |
| `--validate-only` | Only validate inputs, don't process | `false` |

## 🏷️ Filename Organization

The processor uses a **prefixed filename system** to organize output files by processing type:

### Filename Format
- **Single images**: `{prefix}_{base64_filename}.{ext}`
- **Combined portraits**: `combined_{prefix}_{base64_file1}_{base64_file2}.{ext}`

### Processing Type Prefixes
| Prefix | Processing Type | Description |
|--------|----------------|-------------|
| `bw` | Black & White | Monochrome dithered images |
| `6c` | 6-Color | Red, Green, Blue, Yellow, Black, White |
| `7c` | 7-Color | 6-Color + Orange |

### Examples
```bash
# Single image examples
bw_aW1hZ2U=.bin          # "image.jpg" processed as black & white
6c_dmFjYXRpb24=.bmp       # "vacation.png" processed as 6-color
7c_c3Vuc2V0.jpg           # "sunset.heic" processed as 7-color

# Combined portrait examples
combined_bw_cG9ydHJhaXQx_cG9ydHJhaXQy.bin    # Two portraits combined in black & white
combined_6c_aW1hZ2Ux_aW1hZ2Uy.bmp            # Two portraits combined in 6-color
```

### Reverse Lookup
Use the built-in tools to decode filenames:
```bash
# Decode combined filenames
./photoframe-processor --find-original "combined_bw_aW1hZ2Ux_aW1hZ2Uy.bin"

# Output:
# ✓ Successfully decoded combined filename:
#   Processing Type: Black & White (bw)
#   Original 1: image1
#   Original 2: image2
```

## 📁 Output Structure

### Multiple Output Formats (--output-format bmp,bin,jpg,png)

```
output_directory/
├── bmp/                    # Standard BMP files for preview/debugging
│   ├── bw_dGVzdF9pbWFnZQ==.bmp           # Black & white single image
│   ├── 6c_dGVzdF9pbWFnZQ==.bmp           # 6-color single image
│   ├── combined_bw_ABC123_DEF456.bmp     # Black & white combined portrait
│   ├── combined_6c_ABC123_DEF456.bmp     # 6-color combined portrait
│   └── ...
├── bin/                    # Binary files for ESP32
│   ├── bw_dGVzdF9pbWFnZQ==.bin           # Black & white single image
│   ├── 6c_dGVzdF9pbWFnZQ==.bin           # 6-color single image
│   ├── combined_bw_ABC123_DEF456.bin     # Black & white combined portrait
│   ├── combined_6c_ABC123_DEF456.bin     # 6-color combined portrait
│   └── ...
├── jpg/                    # Compressed JPEG files for sharing
│   ├── bw_dGVzdF9pbWFnZQ==.jpg           # Black & white single image
│   ├── 7c_dGVzdF9pbWFnZQ==.jpg           # 7-color single image
│   ├── combined_7c_ABC123_DEF456.jpg     # 7-color combined portrait
│   └── ...
└── png/                    # Lossless PNG files
    ├── bw_dGVzdF9pbWFnZQ==.png           # Black & white single image
    ├── 6c_dGVzdF9pbWFnZQ==.png           # 6-color single image
    ├── combined_6c_ABC123_DEF456.png     # 6-color combined portrait
    └── ...
```

### Single Output Format (--output-format bin)

```
output_directory/
└── bin/                    # Only binary files
    ├── bw_dGVzdF9pbWFnZQ==.bin           # Black & white single image
    ├── bw_aW1hZ2UyMDI0.bin               # Another black & white image
    ├── combined_bw_ABC123_DEF456.bin     # Black & white combined portrait
    └── ...
```

**Note**: Filenames use a prefixed format for organization by processing type:
- **Single images**: `{prefix}_{base64_filename}.{ext}` (e.g., `bw_dGVzdA==.bin`, `6c_aW1hZ2U=.bmp`)
- **Combined portraits**: `combined_{prefix}_{base64_file1}_{base64_file2}.{ext}` (e.g., `combined_bw_ABC123_DEF456.bin`)
- **Prefixes**: `bw` (Black & White), `6c` (6-Color), `7c` (7-Color)
- **Base64 encoding**: Original filenames are base64-encoded for consistent cross-platform compatibility

## 🔄 Processing Pipeline

1. **Image Discovery**: Recursively scans input directories for supported formats
2. **Orientation Analysis**: Reads EXIF data and classifies portrait vs landscape
3. **Portrait Shuffling**: Randomizes portrait order for varied combinations
4. **Smart Resizing**: Three-stage intelligent cropping with face preservation
   - **Stage 1**: Scaling check (only scale if image is smaller than target)
   - **Stage 2**: Regular crop attempt (center-based expansion from detection)
   - **Stage 3**: Bidirectional expansion with face preservation priority
5. **Text Annotation**: Adds filename overlays with semi-transparent backgrounds
6. **Color Processing**: Applies grayscale conversion or 6-color/7-color palette reduction
7. **Dithering**: Floyd-Steinberg error diffusion for optimal e-paper display
8. **Multi-Format Output**: Generates multiple output formats with organized directory structure:
   - **BMP**: Standard bitmap files for viewing and debugging
   - **Binary**: ESP32-compatible .bin files with 8-bit RRRGGGBB format
   - **JPG**: Compressed files for web sharing and storage efficiency
   - **PNG**: Lossless files with transparency support
9. **Portrait Combination**: Merges portrait pairs into landscape layouts with customizable divider (3px white by default)

## 🧠 Smart Cropping Algorithm (with AI People Detection)

When `--detect-people` is enabled, the processor uses a sophisticated three-stage cropping algorithm:

### Stage 1: Scaling Check
- Only scales down if the **image** is smaller than target crop dimensions
- Does NOT scale if detection box is larger than crop (instead centers crop on detection)
- Avoids unnecessary image quality loss from scaling

### Stage 2: Regular Crop Attempt
- Expands from detection box center to target dimensions
- If detection fits entirely within crop area → uses regular crop (fast path)

### Stage 3: Smart Bidirectional Expansion
When regular crop would cut the detection:

**Horizontal Expansion**:
- Detection width ≥ target: centers crop on detection center
- Detection width < target: expands equally left/right with boundary redistribution

**Vertical Expansion with Face Preservation**:
- Detection height ≥ target: centers crop on detection center
- Detection height < target: expands equally top/bottom with redistribution
- **Face Priority**: If crop_y > det_y_min (would cut face), aligns crop with detection top
- Rationale: Faces are at top of person boxes; cutting bottom is better than cutting faces

### Examples

**Portrait with Person**:
```
Image: 960×1280, Detection: (83, 64, 958, 1205), Target: 768×1280
Result: crop_x=136, crop_y=0
→ X centered on detection (520 - 384 = 136)
→ Y starts at 0 (includes detection top at 64)
→ Face preserved ✓
```

**Detection Near Bottom**:
```
Image: 800×1000, Detection: (300, 750, 500, 950), Target: 400×600
→ Algorithm prioritizes keeping detection top (faces) in frame
→ Cuts bottom instead of top when space is limited
```

## 🎨 Color Processing

### Black & White Mode (`-t bw`)
- Converts to grayscale using luminance weights (0.299*R + 0.587*G + 0.114*B)
- Applies Floyd-Steinberg dithering for smooth gradients
- Optimized for monochrome e-paper displays

### 6-Color Mode (`-t 6c`)
- Reduces palette to 6 standard e-paper colors:
  - Black, White, Red, Green, Blue, Yellow
- Uses Euclidean distance in RGB space for color matching
- Applies error diffusion for optimal color distribution

### 7-Color Mode (`-t 7c`)
- Enhanced palette with 7 e-paper colors:
  - Black, White, Red, Green, Blue, Yellow, Orange
- Supports displays with orange color capability
- Advanced dithering for improved color accuracy

## 🔧 Technical Details

### Portrait Combination

The processor combines two portrait images side-by-side into a landscape layout:

- **Divider Width**: Customizable via `--divider-width` (default: 3 pixels)
- **Divider Color**: Customizable via `--divider-color` (default: white #FFFFFF)
- **Alignment**: Images are placed exactly at half-width boundaries
- **Format**: Combined images maintain the same processing type (BW, 6C, or 7C)
- **No Divider**: Set `--divider-width 0` to disable the divider line completely

### ESP32 Binary Format

The processor generates binary files using the same 8-bit color format as the original `bmp2cpp` tool:

- **3 bits for red** (0-7): `(R / 32) << 5`
- **3 bits for green** (0-7): `(G / 32) << 2`  
- **2 bits for blue** (0-3): `(B / 64)`
- **Final format**: `RRRGGGBB` (8 bits per pixel)

### EXIF Orientation Support

Handles all 8 EXIF orientation values:
- **1**: Normal (0°)
- **2**: Horizontally flipped
- **3**: Rotated 180°
- **4**: Vertically flipped
- **5**: Rotated 90° CCW + flipped
- **6**: Rotated 90° CW (common portrait)
- **7**: Rotated 90° CW + flipped
- **8**: Rotated 90° CCW (alternate portrait)

### Auto-Process Mode

When `--auto` is enabled, the processor automatically swaps target dimensions when image orientation doesn't match target orientation:

- Portrait image (H > W) + Landscape target (800x480) → Use 480x800
- Landscape image (W > H) + Portrait target (480x800) → Use 800x480

## 🧪 Testing

```bash
# Run unit tests
cargo test

# Run tests with verbose output
cargo test -- --nocapture

# Run performance benchmarks
cargo bench
```

## 🐛 Troubleshooting

### Common Issues

**"Failed to read EXIF data"**
- Some image formats don't contain EXIF data
- The processor will continue with default orientation

**"Binary size mismatch"**
- Indicates corrupt output file
- Check available disk space and file permissions

**"Font rendering failed"** 
- Font name not found on system
- Try using a generic font like "Arial" or "DejaVu Sans"

### Performance Tips

```bash
# Use optimal number of jobs for your CPU
./photoframe-processor -j $(nproc) ...

# Process only specific extensions for faster discovery
./photoframe-processor --extensions jpg,png ...

# Use --validate-only to check inputs before processing
./photoframe-processor --validate-only ...
```

## 🔮 Future Enhancements

- [x] **AI Subject Detection**: ✅ YOLO11-based person detection for intelligent cropping
- [x] **Smart Cropping Algorithm**: ✅ Three-stage algorithm with face preservation
- [x] **Bidirectional Expansion**: ✅ Intelligent boundary redistribution
- [x] **Multi-Format Output**: ✅ Support for multiple output formats with organized directories
- [x] **Auto Color Correction**: ✅ Intelligent color/saturation/levels adjustment
- [x] **Avoid Duplicate Processing**: ✅ Skip already processed images based on image name
- [x] **Debug Visualization**: ✅ Detection box overlay for AI debugging
- [x] **Filename Organization**: ✅ Processing type prefixes for organized output files
- [ ] **GPU Acceleration**: CUDA/OpenCL support for massive performance gains
- [ ] **Custom Dithering**: Configurable dithering algorithms and patterns
- [ ] **Batch Resume**: Resume interrupted batch processing
- [ ] **Cloud Integration**: Direct Google Drive/Cloud storage processing
- [ ] **GUI Interface**: Cross-platform desktop application
- [ ] **Web Service**: REST API for remote processing
- [ ] **Animation Support**: Process and optimize GIF/video frames for slideshow displays

## 📄 License

MIT License - Same as the main ESP32 Photo Frame project.

## 🤝 Contributing

This Rust processor is part of the larger ESP32 Photo Frame project. Contributions welcome!

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality  
4. Submit a pull request

## 🔗 Related Projects

- **Main Project**: [ESP32 Photo Frame](../../README.md)
- **Binary Converter**: [bin2bmp](../bin2bmp/) - Convert .bin files back to images
- **Android Processor**: [PhotoFrameProcessor](../../android/PhotoFrameProcessor/) - Mobile image processing app

---

**Performance Optimized • Memory Efficient • Cross-Platform**