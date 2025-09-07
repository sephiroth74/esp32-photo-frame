# ESP32 Photo Frame - Image Processing Scripts Documentation

This document provides comprehensive technical documentation for the image processing scripts used to convert photos into e-paper display-compatible formats for the ESP32 Photo Frame project.

## Overview

The image processing system converts regular photos (JPEG, PNG, HEIC, etc.) into optimized BMP and binary files suitable for e-paper displays. The system handles both portrait and landscape orientations intelligently, combining portrait images side-by-side and applying smart annotations.

## Main Entry Point: `auto.sh`

### Location
`scripts/auto.sh`

### Purpose
The main orchestration script that processes entire directories of images, handling orientation detection, portrait pairing, format conversion, and binary generation in a single automated workflow.

### Usage

```bash
# Basic usage - Black & white processing
./scripts/auto.sh -i ~/Photos -o ~/processed_images -t bw -s 800x480 --extensions jpg,png --auto

# 6-color processing with verbose output
./scripts/auto.sh -i ~/Photos -o ~/processed_images -t 6c -s 800x480 --extensions jpg,png,heic --auto -v

# Multiple input directories
./scripts/auto.sh -i ~/Photos/2023 -i ~/Photos/2024 -o ~/processed_images -t bw -s 800x480 --auto

# Custom font and annotation settings
./scripts/auto.sh -i ~/Photos -o ~/processed_images -t bw -s 800x480 \
  --font "Arial-Bold" --pointsize 32 --annotate_background "#FF000080" --auto
```

### Command Line Options

| Option | Required | Description | Default |
|--------|----------|-------------|---------|
| `-i, --input <directory>` | ✅ | Input directory containing images (can be specified multiple times) | - |
| `-o, --output <directory>` | ✅ | Output directory for processed images | - |
| `-s, --size <WIDTHxHEIGHT>` | ✅ | Target display resolution (e.g., 800x480) | - |
| `-t, --type <bw\|6c>` | ❌ | Processing type: `bw` (black & white) or `6c` (6-color) | `bw` |
| `-p, --palette <file>` | ❌ | Custom palette file for color processing | - |
| `--extensions <ext1,ext2,...>` | ❌ | Comma-separated list of image extensions to process | `jpg,jpeg,png,heic` |
| `--pointsize <size>` | ❌ | Font size for filename annotations | `24` |
| `--font <font>` | ❌ | Font family for annotations | `UbuntuMono-Nerd-Font-Bold` |
| `--annotate_background <color>` | ❌ | Background color for text annotations (hex with alpha) | `#00000040` |
| `--auto` | ❌ | Enable automatic orientation handling and size swapping | `false` |
| `-v, --verbose` | ❌ | Enable detailed progress output | `false` |
| `-h, --help` | ❌ | Show help message | - |

### Processing Workflow

The `auto.sh` script follows a sophisticated 6-stage processing pipeline:

#### Stage 1: Image Discovery and Classification
1. **Multi-directory scanning**: Recursively scans all specified input directories
2. **Format detection**: Identifies images by extension and validates file integrity
3. **Orientation analysis**: Determines portrait vs landscape orientation using EXIF data
4. **Format conversion**: Converts non-standard formats (HEIC, WebP, etc.) to JPEG for processing

#### Stage 2: Smart Portrait Pairing
1. **Portrait shuffling**: Randomizes portrait image order to create varied combinations
2. **Automatic pairing**: Groups portrait images in pairs for side-by-side combination
3. **Orphan handling**: Handles odd numbers of portrait images by duplicating the last image

#### Stage 3: Individual Image Processing
1. **Landscape processing**: Resizes landscape images to full target dimensions
2. **Portrait processing**: Resizes portrait images to half-width for later combination
3. **Smart cropping**: Applies intelligent cropping based on content analysis
4. **Annotation overlay**: Adds filename annotations with customizable styling

#### Stage 4: Portrait Combination
1. **Side-by-side combination**: Merges paired portrait images into landscape format
2. **Divider line**: Adds visual separator between combined images
3. **Size normalization**: Ensures combined images match target dimensions

#### Stage 5: Color Conversion
1. **Format standardization**: Converts all processed images to target color format
2. **Dithering application**: Applies Floyd-Steinberg dithering for e-paper optimization
3. **Palette application**: Uses custom palettes or automatic color reduction

#### Stage 6: Binary Generation
1. **BMP output**: Creates standard BMP files for preview and debugging
2. **Binary conversion**: Generates optimized .bin files for ESP32 consumption
3. **Format validation**: Verifies output file integrity and dimensions

### Output Structure

```
output_directory/
├── image1.bmp              # Standard BMP files
├── image2.bmp
├── combined_portrait_1.bmp # Combined portrait images
├── combined_portrait_2.bmp
└── bin/                    # Binary files for ESP32
    ├── image1.bin
    ├── image2.bin
    ├── combined_portrait_1.bin
    └── combined_portrait_2.bin
```

## Helper Scripts Architecture

### `private/utils.sh`
**Purpose**: Core utility functions for image metadata extraction

**Key Functions**:
- `extract_image_info()`: Extracts orientation, dimensions, and portrait detection from images
- Handles EXIF orientation tags (1-8) including rotated images
- Provides normalized width/height values accounting for rotation

**EXIF Orientation Handling**:
```bash
# Orientation codes and their meanings
TopLeft (1)      - Normal
TopRight (2)     - Flipped horizontally  
BottomRight (3)  - Rotated 180°
BottomLeft (4)   - Flipped vertically
LeftTop (5)      - Rotated 90° CCW + flipped
RightTop (6)     - Rotated 90° CW (portrait)
RightBottom (7)  - Rotated 90° CW + flipped
LeftBottom (8)   - Rotated 90° CCW (portrait)
```

### `private/auto_resize_and_annotate.sh`
**Purpose**: Intelligent image resizing with automatic orientation handling

**Features**:
- **Smart Size Swapping**: Automatically swaps target dimensions when `--auto` mode detects orientation mismatch
- **Content-Aware Resizing**: Uses ImageMagick's geometry operators for optimal scaling
- **Annotation Integration**: Adds filename overlays with configurable fonts and backgrounds
- **Quality Preservation**: Maintains optimal image quality during resize operations

**Usage**:
```bash
./auto_resize_and_annotate.sh -i input.jpg -o output.jpg --size 800x480 --auto --pointsize 28
```

### `private/resize_and_annotate.sh`
**Purpose**: Core image processing engine with advanced annotation features

**Capabilities**:
- **Multi-stage resizing**: Crop-to-fill followed by exact dimension scaling
- **Text annotation**: Filename overlay with background, shadows, and positioning
- **Font management**: Support for system fonts and custom font files
- **Background effects**: Semi-transparent backgrounds with configurable opacity

### `private/combine_images.sh`
**Purpose**: Side-by-side combination of portrait images

**Technical Details**:
- **Horizontal composition**: Places two images side-by-side with precise alignment
- **Divider line**: Adds configurable separator line between images
- **Size normalization**: Ensures output matches exact target dimensions
- **Aspect ratio preservation**: Maintains correct proportions during combination

**Usage**:
```bash
./combine_images.sh --size 800x480 --stroke_color "#ffffff" --stroke_width 3 image1.jpg image2.jpg output.jpg
```

### `private/convert.sh`
**Purpose**: Color space conversion and e-paper optimization

**Processing Modes**:

#### Black & White Mode (`--type grey`)
- Converts to grayscale using perceptual color weighting
- Applies Floyd-Steinberg dithering for smooth gradients
- Optimizes for monochrome e-paper displays

#### 6-Color Mode (`--type color`)
- Reduces color palette to 6 colors (black, white, red, yellow, blue, green)
- Uses advanced color quantization algorithms
- Supports custom palette files for specific color schemes

**Usage**:
```bash
# Black and white conversion
./convert.sh --type grey input.jpg output.bmp

# 6-color conversion
./convert.sh --type color --colors 6 input.jpg output.bmp

# Custom palette
./convert.sh --type color --palette custom_palette.gif input.jpg output.bmp
```

### `private/bmp_to_lcd.sh`
**Purpose**: Binary format conversion for ESP32 consumption

**Features**:
- **Batch processing**: Converts entire directories of BMP files
- **Format options**: Supports C-array header files and raw binary output
- **ESP32 optimization**: Generates files compatible with ESP32 memory layout
- **Validation**: Verifies output file integrity and format compliance

**Binary Format Details**:
- **Color Encoding**: 8-bit RRRGGGBB format (3-3-2 bits)
- **Byte Order**: Little-endian for ESP32 compatibility
- **No Headers**: Raw pixel data for direct memory loading
- **Size Validation**: Ensures file size matches expected dimensions

### `private/bmp2cpp`
**Purpose**: Core binary conversion engine (compiled executable)

**Functionality**:
- Reads standard BMP files
- Converts to 8-bit color format matching ESP32 expectations
- Outputs raw binary data or C header arrays
- Handles various BMP bit depths and color spaces

### `private/find_subject.py`
**Purpose**: AI-powered subject detection for intelligent cropping

**Dependencies**:
- **OpenCV**: Computer vision library for image processing
- **YOLOv3**: Pre-trained object detection model
- **NumPy**: Numerical computing for image arrays

**Features**:
- **Person Detection**: Identifies people in photos for portrait optimization
- **Bounding Box Generation**: Provides coordinates for intelligent cropping
- **Confidence Scoring**: Ranks detections by confidence level
- **Multiple Subject Support**: Handles images with multiple people

**Model Files**:
- `assets/yolov3.weights`: Pre-trained YOLO weights (150MB)
- `assets/yolov3.cfg`: Model configuration
- `assets/coco.names`: Class labels for object detection

## Technical Requirements

### Software Dependencies
- **ImageMagick** 7.0+: Core image processing engine
- **Python** 3.7+: Required for AI subject detection
- **OpenCV Python**: `pip install opencv-python`
- **NumPy**: `pip install numpy`
- **zsh**: Shell interpreter (or bash with minor modifications)

### System Requirements
- **Memory**: 2GB+ RAM recommended for large image processing
- **Storage**: 500MB+ for temporary files during processing
- **CPU**: Multi-core processor recommended for batch operations

### Supported Input Formats
- **JPEG** (.jpg, .jpeg): Standard photo format
- **PNG** (.png): Lossless format with transparency
- **HEIC** (.heic): Apple's modern photo format
- **WebP** (.webp): Google's efficient format
- **TIFF** (.tiff, .tif): High-quality format
- **BMP** (.bmp): Windows bitmap format

### Output Formats
- **BMP**: Standard bitmap for preview and debugging
- **Binary** (.bin): Optimized raw format for ESP32
- **C Header** (.h): Embedded array format for compilation

## Performance Characteristics

### Processing Speed (typical 4MP JPEG)
- **Individual resize**: 0.5-2 seconds
- **Portrait combination**: 1-3 seconds  
- **Color conversion**: 1-4 seconds
- **Binary generation**: 0.1-0.5 seconds
- **AI subject detection**: 2-8 seconds (when enabled)

### Memory Usage
- **Peak RAM**: 200-500MB per image during processing
- **Temporary storage**: 2-5x input file size
- **Output size**: ~375KB per 800x480 binary file

### Batch Processing Performance
For 100 mixed portrait/landscape images:
- **Total time**: 5-15 minutes (depending on CPU and disk speed)
- **Portrait pairs**: ~25 combined images generated
- **Landscape**: ~75 individual images processed
- **Output files**: ~200 files (BMP + binary pairs)

## Advanced Configuration

### Custom Fonts
```bash
# List available fonts
magick -list font

# Use custom font file
./auto.sh -i ~/Photos -o ~/processed --font "/path/to/font.ttf" --pointsize 32
```

### Color Palette Creation
```bash
# Create custom 6-color palette
magick xc:black xc:white xc:red xc:yellow xc:blue xc:green +append palette.gif
./auto.sh -i ~/Photos -o ~/processed -t 6c --palette palette.gif
```

### Portrait Combination Customization
```bash
# Custom divider line
./combine_images.sh --stroke_color "#ff0000" --stroke_width 5 img1.jpg img2.jpg out.jpg
```

## Troubleshooting

### Common Issues

#### "ImageMagick not found"
```bash
# Install ImageMagick
# macOS
brew install imagemagick

# Ubuntu/Debian
sudo apt-get install imagemagick

# Verify installation
magick --version
```

#### "HEIC format not supported"
```bash
# Install HEIC support for ImageMagick
# macOS
brew install libheif imagemagick

# Ubuntu/Debian
sudo apt-get install libheif-dev
sudo apt-get install --reinstall imagemagick
```

#### "YOLOv3 weights not found"
```bash
# Download YOLOv3 weights (150MB)
cd scripts/private/assets
wget https://pjreddie.com/media/files/yolov3.weights
```

#### "Python/OpenCV errors"
```bash
# Install Python dependencies
pip3 install opencv-python numpy

# Verify installation
python3 -c "import cv2; print(cv2.__version__)"
```

### Performance Optimization

#### Speed Improvements
```bash
# Disable AI subject detection for faster processing
# (Remove or rename find_subject.py)

# Use faster resize algorithm
export MAGICK_RESIZE_FILTER=Point

# Reduce image quality for faster processing
export MAGICK_QUALITY=85
```

#### Memory Optimization
```bash
# Limit ImageMagick memory usage
export MAGICK_MEMORY_LIMIT=1GB
export MAGICK_DISK_LIMIT=4GB

# Process images in smaller batches
# Split large directories into smaller chunks
```

## Integration with ESP32 Project

### File Placement
1. **Copy binary files** from `output_directory/bin/` to SD card
2. **Directory structure** on SD card:
   ```
   /bin-bw/           # For black & white displays
   ├── image1.bin
   ├── image2.bin
   └── combined_portrait_1.bin
   
   /bin-6c/           # For 6-color displays  
   ├── image1.bin
   ├── image2.bin
   └── combined_portrait_1.bin
   ```

### Binary Format Compatibility
- **Dimensions**: Must match display resolution exactly
- **Color Format**: 8-bit RRRGGGBB encoding
- **File Size**: Width × Height bytes (e.g., 800×480 = 384,000 bytes)
- **Endianness**: Little-endian for ESP32 compatibility

### Validation
```bash
# Verify binary file size
ls -la output/bin/*.bin

# Expected size for 800x480: 384,000 bytes
# Expected size for 400x240: 96,000 bytes
```

## Example Workflows

### Complete Processing Pipeline
```bash
#!/bin/bash
# Complete photo processing workflow

INPUT_DIR="$HOME/Photos/Vacation2024"
OUTPUT_DIR="$HOME/processed_photos"
DISPLAY_SIZE="800x480"

# Step 1: Process all images
./scripts/auto.sh \
  -i "$INPUT_DIR" \
  -o "$OUTPUT_DIR" \
  -t bw \
  -s "$DISPLAY_SIZE" \
  --extensions jpg,png,heic \
  --font "Arial-Bold" \
  --pointsize 28 \
  --auto \
  --verbose

# Step 2: Copy to SD card
cp "$OUTPUT_DIR"/bin/*.bin /Volumes/SDCARD/bin-bw/

# Step 3: Verify file count
echo "Processed $(ls "$OUTPUT_DIR"/bin/*.bin | wc -l) images"
echo "SD card now contains $(ls /Volumes/SDCARD/bin-bw/*.bin | wc -l) files"
```

### Batch Processing Multiple Directories
```bash
#!/bin/bash
# Process photos from multiple years

YEARS=("2022" "2023" "2024")
BASE_INPUT="$HOME/Photos"
OUTPUT_DIR="$HOME/all_processed"

for year in "${YEARS[@]}"; do
    echo "Processing photos from $year..."
    ./scripts/auto.sh \
      -i "$BASE_INPUT/$year" \
      -o "$OUTPUT_DIR" \
      -t bw \
      -s 800x480 \
      --auto \
      --verbose
done

echo "All years processed. Total files: $(ls "$OUTPUT_DIR"/bin/*.bin | wc -l)"
```

This comprehensive image processing system provides a robust, automated solution for converting regular photos into e-paper display-ready formats while maintaining high quality and intelligent handling of different image orientations and compositions.