# Binary to Image Converter

Convert ESP32 photo frame `.bin` files back to viewable images (BMP, JPEG, PNG).

## Overview

The ESP32 photo frame project stores images in a custom binary format where each byte represents a pixel value. This tool reverses that process to recreate viewable images.

## Binary Format

- **File size**: `width × height` bytes
- **Pixel format**: 1 byte per pixel
- **Color mapping**:
  - **B&W display** (`bw`): `> 127` = white, `≤ 127` = black
  - **6-color display** (`6c`): `255`=white, `0`=black, `224`=red, `28`=green, `252`=yellow, `3`=blue

## Available Tools

### 1. Rust Binary (`bin2bmp`) - **Recommended**

**Requirements**: None (self-contained binary)

**Features**:
- 🚀 **Fast**: Native Rust performance, ~20x faster than shell scripts
- 📊 **Visual Progress**: Animated progress bars with ETA (in verbose mode)
- 🎯 **Smart Validation**: Helpful error messages with dimension suggestions
- 🎨 **Multiple Formats**: BMP, JPEG, PNG output support
- 📁 **Auto Naming**: Automatically generates output filename from input name

**Usage**:
```bash
# Output to current directory with default size (auto-generates: input.bmp)
./bin2bmp -i input.bin -t bw

# Output to specific directory with custom size (auto-generates: /path/to/output/input.jpg) 
./bin2bmp -i input.bin -o /path/to/output/ -s 1200x825 -t 6c -f jpeg

# Validation only with custom size
./bin2bmp -i input.bin --validate-only -s 640x384
```

## Arguments

| Argument | Description | Required |
|----------|-------------|----------|
| `-i, --input` | Input .bin file | ✓ |
| `-o, --output` | Output directory (auto-generates filename) | (default: current directory) |
| `-s, --size` | Image size in WIDTHxHEIGHT format | (default: `800x480`) |
| `-t, --type` | Display type: `bw` or `6c` | (default: `bw`) |
| `-f, --format` | Output format: `bmp`, `jpeg`, `png` | (default: `bmp`) |
| `-v, --verbose` | Enable verbose output with progress bars | |
| `--validate-only` | Only validate, don't convert | |

## Examples

### Basic Conversion
```bash
# Convert to current directory with default size (creates: photo.bmp)
./bin2bmp -i photo.bin -t bw

# Convert 6-color binary to JPEG in specific directory (creates: output/photo.jpg)
./bin2bmp -i photo.bin -o output/ -s 1200x825 -t 6c -f jpeg

# Convert with PNG output and custom size (creates: images/photo.png)
./bin2bmp -i photo.bin -o images/ -s 640x384 -f png
```

### Validation Only
```bash
# Check file validity with default size (no output file created)
./bin2bmp -i photo.bin --validate-only

# Check with custom size
./bin2bmp -i photo.bin --validate-only -s 480x800
```

### Verbose Mode with Progress Bars
```bash
# Get detailed processing with animated progress bars
./bin2bmp -i photo.bin -o results/ -s 800x480 -v
```

**Progress Bar Features** (Rust binary only):
- 📊 Real-time progress indication with percentage
- ⏱️ Elapsed time and ETA estimation  
- 🎯 Pixel processing rate display
- 🎨 Color-coded progress bars and spinners

## Common Display Dimensions

| Display | Dimensions | File Size |
|---------|------------|-----------|
| 1.54" B&W | 200×200 | 40,000 bytes |
| 4.2" B&W | 400×300 | 120,000 bytes |
| 7.5" B&W | 800×480 | 384,000 bytes |
| 9.7" 6-color | 1200×825 | 990,000 bytes |

## Troubleshooting

### File Size Mismatch
If you get a file size error, the script will suggest possible dimensions:
```
Error: File size does not match specified dimensions!
  Specified: 800x480 (expected 384000 bytes)
  Actual file size: 245760 bytes

Possible correct dimensions for this file (245760 bytes):
  640x384
  384x640
```

### Invalid Colors
For 6-color displays, unexpected pixel values might indicate:
- Dithering was used in the original conversion
- Wrong display type specified
- Corrupted binary file

### Dependencies
- **ImageMagick**: Install via `brew install imagemagick` (macOS) or `apt install imagemagick` (Linux)
- **Rust** (for .rs version): Install via [rustup.rs](https://rustup.rs/)

## Integration

These tools complement the existing image processing pipeline:

```
Original Image → scripts/auto.sh → .bmp → bmp_to_lcd.sh → .bin → bin2bmp → Recovered Image
```

Perfect for debugging the image processing pipeline or recovering images from the ESP32's binary format.