# Image Processing Guide

## Overview

The ESP32 Photo Frame requires images to be processed into a specific binary format (`.bin`) optimized for e-paper displays. This guide covers the image processing pipeline and the available tools.

## Why Image Processing is Required

E-paper displays have unique characteristics that require specialized image processing:

- **Limited Color Palette**: Black & white or 6-color (Black, White, Red, Green, Blue, Yellow)
- **Fixed Resolution**: Images must match the exact display resolution (e.g., 800×480)
- **Binary Format**: Optimized format for efficient storage and fast rendering on ESP32
- **Dithering**: Required to simulate gradients with limited colors

## Available Processing Tools

### 🦀 Rust Processor (Command Line)

High-performance command-line tool with advanced features including AI-powered person detection.

**Key Features:**
- Batch processing with parallel execution
- YOLO11 AI model for intelligent cropping
- Multiple dithering algorithms
- Automatic portrait pairing
- EXIF metadata extraction
- Multi-format output support

**Quick Start:**
```bash
cd rust/photoframe-processor
cargo build --release

# Basic processing
./target/release/photoframe-processor -i ~/photos -o ~/output -t bw -s 800x480 --auto
```

📖 **[Detailed Rust Processor Documentation →](rust-photoframe-processor.md)**

### 🎨 Flutter Desktop App (GUI)

Cross-platform desktop application with an intuitive graphical interface.

**Key Features:**
- Drag-and-drop file selection
- Real-time preview
- Batch processing with progress tracking
- Multiple output formats
- Works on Windows, macOS, and Linux

**Quick Start:**
```bash
cd photoframe_flutter
flutter pub get
flutter run -d [windows/macos/linux]
```

📖 **[Detailed Flutter App Documentation →](flutter_app.md)**

## Image Format Specifications

### Input Formats
Both tools support common image formats:
- JPEG (.jpg, .jpeg)
- PNG (.png)
- HEIC (.heic) - iPhone photos
- WebP (.webp)
- BMP (.bmp)

### Output Format
The ESP32 firmware requires binary format (`.bin`) files:
- **File Extension**: `.bin`
- **Resolution**: Must match your display exactly (e.g., 800×480)
- **Color Depth**: 8-bit per pixel (RRRGGGBB format)
- **Size**: Approximately 375KB for 800×480 resolution

## Color Modes

### Black & White Mode (`bw`)
- Converts images to grayscale
- Applies Floyd-Steinberg dithering for smooth gradients
- Best for: Text, portraits, high-contrast images
- File prefix: `bw_`

### 6-Color Mode (`6c`)
- Maps to 6-color palette: Black, White, Red, Green, Blue, Yellow
- Uses color distance algorithm for best match
- Applies error diffusion for better color distribution
- Best for: Colorful images, landscapes, artwork
- File prefix: `6c_`

## Processing Pipeline

1. **Load Image** - Read source image file
2. **Auto-Orient** - Apply EXIF rotation if needed
3. **Smart Crop** - AI detection or center crop to target aspect ratio
4. **Resize** - Scale to exact display resolution
5. **Color Quantization** - Convert to target color palette
6. **Dithering** - Apply Floyd-Steinberg or ordered dithering
7. **Binary Encoding** - Convert to ESP32 binary format
8. **Save Output** - Write `.bin` file with appropriate naming

## Portrait Pairing

Portrait-oriented images (height > width) are automatically paired for landscape displays:

- Two portrait images are combined side-by-side
- A customizable divider separates the images
- Output filename: `combined_[mode]_[hash1]_[hash2].bin`
- Unpaired portraits are processed individually

## File Naming Convention

Processed files use a systematic naming scheme:

```
Single images:     [mode]_[base64_filename].bin
Combined images:   combined_[mode]_[hash1]_[hash2].bin

Examples:
- bw_cGhvdG8x.bin         (black & white single image)
- 6c_aW1hZ2Uy.bin         (6-color single image)
- combined_bw_YWJj_ZGVm.bin (combined black & white portraits)
```

## Best Practices

### Image Selection
- Choose high-resolution source images (larger than display resolution)
- Ensure good contrast for black & white mode
- Avoid very dark images as e-paper has limited dynamic range

### Processing Options
- Use `--auto` flag for automatic orientation handling
- Enable `--detect-people` for better portrait cropping
- Process in batches for efficiency
- Use `--force` to reprocess existing files

### Folder Organization
```
/processed/
  ├── bw/          # Black & white images
  │   └── bin/     # Binary files for ESP32
  └── 6c/          # 6-color images
      └── bin/     # Binary files for ESP32
```

### Display Matching
Ensure your processing parameters match your display:

| Display Model | Resolution | Recommended Size Parameter |
|--------------|------------|---------------------------|
| 7.5" Waveshare | 800×480 | `-s 800x480` |
| 7.5" Waveshare (Portrait) | 480×800 | `-s 480x800` |
| Other displays | Check specs | `-s WIDTHxHEIGHT` |

## Troubleshooting

### Common Issues

**"No suitable images found"**
- Check that input directory contains supported image formats
- Verify file extensions match (case-sensitive on Linux)

**"Binary size mismatch" on ESP32**
- Ensure processed images match exact display resolution
- Check that you're using the correct orientation (portrait vs landscape)

**Poor image quality**
- Try different dithering algorithms
- Ensure source images have good contrast
- Consider using 6-color mode for colorful images

**Processing is slow**
- Use `--jobs` parameter to utilize more CPU cores
- Process images in batches rather than individually
- Consider using release build for Rust processor

## Performance Tips

- **Batch Processing**: Process entire directories at once
- **Parallel Execution**: Rust processor uses all CPU cores automatically
- **Skip Existing**: Omit `--force` to skip already processed images
- **Resolution**: Don't process at higher resolution than your display

## Next Steps

1. Choose your preferred processing tool (Rust CLI or Flutter GUI)
2. Process your image collection
3. Copy `.bin` files to SD card or upload to Google Drive
4. Configure your ESP32 photo frame to use the appropriate image source

For tool-specific details and advanced features, see:
- 📖 [Rust Processor Documentation](rust-photoframe-processor.md)
- 📖 [Flutter App Documentation](flutter_app.md)