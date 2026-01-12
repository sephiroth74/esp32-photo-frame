# ESP32 Photo Frame - Image Processing Documentation

## Overview

The image processing system converts regular photos (JPEG, PNG, HEIC, etc.) into optimized BMP and binary files suitable for e-paper displays.

## Image Processing Tools

### Rust Image Processor (Recommended)

The high-performance Rust implementation provides fast, efficient image processing with advanced features:

📋 **[Rust Photoframe Processor Documentation](../rust/photoframe-processor/README.md)**

Features:
- 5-10x faster processing with optimized algorithms
- Parallel batch processing
- Smart portrait pairing
- AI-powered person detection for intelligent cropping
- Multi-format output (BMP, Binary, JPG, PNG)
- Customizable divider for combined portraits
- EXIF orientation handling
- Floyd-Steinberg dithering

Quick example:
```bash
# Black & white processing
./rust/photoframe-processor/target/release/photoframe-processor \
  -i ~/Photos -o ~/processed -t bw -s 800x480 --auto

# 6-color with custom divider
./rust/photoframe-processor/target/release/photoframe-processor \
  -i ~/Photos -o ~/processed -t 6c -s 800x480 \
  --divider-width 5 --divider-color "#FF0000" --auto
```

### Android App

A user-friendly GUI application for processing images:

📱 **[Android PhotoFrameProcessor](../android/PhotoFrameProcessor/)**

Features:
- Graphical user interface
- AI-powered person detection and smart cropping
- Automatic EXIF date extraction and labeling
- Portrait image pairing for landscape display
- Real-time preview of processed results

## Image Format Requirements

### Input Formats
- JPEG (.jpg, .jpeg)
- PNG (.png)
- HEIC (.heic)
- WebP (.webp)
- TIFF (.tiff)

### Output Formats

#### Binary Format (.bin)
- 8-bit color format: RRRGGGBB
- Optimized for ESP32 memory constraints
- Direct loading to e-paper display buffer

#### Bitmap Format (.bmp)
- Standard BMP for preview and debugging
- Compatible with all image viewers
- Preserves processed colors and dithering

## Processing Types

### Black & White (`bw`)
- Converts to grayscale using luminance weights
- Floyd-Steinberg dithering for smooth gradients
- Optimized for monochrome e-paper displays

### 6-Color (`6c`)
- Palette: Black, White, Red, Green, Blue, Yellow
- Euclidean distance color matching
- Error diffusion for optimal color distribution

## Portrait Combination

Portrait images are automatically paired and combined side-by-side:

- **Detection**: Images with height > width
- **Pairing**: Sequential or AI-based pairing
- **Divider**: Customizable width and color
- **Output**: Combined landscape format

## Best Practices

1. **Resolution**: Match your e-paper display resolution exactly
2. **Orientation**: Use `--auto` flag for automatic handling
3. **Batch Processing**: Process entire directories at once
4. **Format**: Use binary (.bin) files for ESP32, BMP for preview
5. **Color Mode**: Choose based on your e-paper display capabilities

## Troubleshooting

### Common Issues

**"No images found"**
- Check file extensions match `--extensions` parameter
- Verify input directory path is correct

**"Binary size mismatch"**
- Ensure target resolution matches display exactly
- Check for corrupted output files

**"Font not found"**
- Use system-installed fonts or provide full path
- Try generic fonts like "Arial" or "DejaVu Sans"

## Performance Tips

- Use `--jobs` parameter to optimize CPU usage
- Process images in batch rather than individually
- Use `--force` to reprocess all files
- Enable `--verbose` for debugging