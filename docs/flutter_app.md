# Flutter Desktop Application Documentation

## Overview

The PhotoFrame Flutter application is a cross-platform desktop tool for processing images for e-paper displays. It provides a modern graphical interface for batch processing images with support for multiple output formats and color modes.

## Features

- **Cross-Platform**: Runs on Windows, macOS, and Linux
- **Batch Processing**: Process multiple images at once
- **Multiple Formats**: Output to BMP, Binary, JPEG, and PNG
- **Color Modes**: Support for Black/White, 6-color, and 7-color displays
- **Drag and Drop**: Easy file selection with drag-and-drop support
- **Real-time Preview**: See processing results before saving
- **Progress Tracking**: Visual progress bars for batch operations
- **EXIF Support**: Preserve image metadata and dates

## Installation

### Prerequisites

- Flutter SDK 3.0 or higher
- Dart SDK 2.17 or higher
- Platform-specific requirements:
  - **Windows**: Visual Studio 2019 or later with Desktop development with C++
  - **macOS**: Xcode 12 or later
  - **Linux**: GTK development libraries

### Building from Source

```bash
# Clone the repository
git clone https://github.com/sephiroth74/esp32-photo-frame.git
cd esp32-photo-frame/photoframe_flutter

# Install dependencies
flutter pub get

# Run in development mode
flutter run -d windows  # On Windows
flutter run -d macos    # On macOS
flutter run -d linux    # On Linux

# Build for production
flutter build windows   # Creates .exe in build/windows/runner/Release
flutter build macos     # Creates .app in build/macos/Build/Products/Release
flutter build linux     # Creates executable in build/linux/x64/release
```

## Usage

### Basic Workflow

1. **Launch the Application**
   - Double-click the executable or run from terminal

2. **Select Images**
   - Click "Select Files" button, or
   - Drag and drop images onto the window

3. **Configure Processing Options**
   - **Display Size**: Set your e-paper display resolution (e.g., 800x480)
   - **Color Mode**: Choose Black/White, 6-color, or 7-color
   - **Output Format**: Select one or more formats (BMP, Binary, JPEG, PNG)
   - **Crop Mode**: Auto, Center, or Manual selection

4. **Process Images**
   - Click "Process" to start batch processing
   - Monitor progress in the status bar

5. **Save Results**
   - Choose output directory
   - Processed files are organized by format in subdirectories

### Advanced Features

#### Portrait Mode Processing

For vertical displays, enable portrait mode to automatically rotate and scale images:

```
Settings → Display → Portrait Mode
```

#### Filename Encoding

The app uses Base64 encoding for filenames to handle special characters:
- Original: `photo (1).jpg`
- Encoded: `bw_cGhvdG8gKDEp.bin`

#### Batch Processing Optimizations

- **Parallel Processing**: Utilizes multiple CPU cores
- **Memory Management**: Processes images in chunks to avoid memory issues
- **Skip Existing**: Option to skip already processed files

## Configuration

### Settings File

Settings are stored in platform-specific locations:
- **Windows**: `%APPDATA%\PhotoFrameProcessor\settings.json`
- **macOS**: `~/Library/Application Support/PhotoFrameProcessor/settings.json`
- **Linux**: `~/.config/PhotoFrameProcessor/settings.json`

### Default Settings

```json
{
  "display": {
    "width": 800,
    "height": 480,
    "color_mode": "bw",
    "portrait": false
  },
  "processing": {
    "output_formats": ["bin"],
    "crop_mode": "auto",
    "dithering": "floyd_steinberg",
    "quality": 85
  },
  "paths": {
    "last_input": "",
    "last_output": ""
  }
}
```

## Output Formats

### Binary Format (.bin)

Optimized format for ESP32 firmware:
- Compact file size (50-70% smaller than BMP)
- Fast loading on microcontroller
- Direct memory mapping support

### BMP Format (.bmp)

Standard bitmap for debugging:
- Uncompressed RGB data
- Compatible with all image viewers
- Useful for verifying processing results

### JPEG Format (.jpg)

Compressed format for archival:
- Adjustable quality (1-100)
- Small file size
- Good for web sharing

### PNG Format (.png)

Lossless compression:
- Preserves all image data
- Supports transparency
- Larger than JPEG but higher quality

## Color Modes

### Black & White (1-bit)

- 2 colors: Black and White
- Smallest file size
- Best for text and line art
- Uses Floyd-Steinberg dithering

### 6-Color ACeP

- Colors: Black, White, Red, Yellow, Blue, Green
- Good Display 7.5" compatibility
- Natural color reproduction
- Advanced color quantization

### 7-Color

- Colors: Black, White, Red, Yellow, Blue, Green, Orange
- Waveshare 5.65" compatibility
- Extended color gamut
- Optimized palette mapping

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open files |
| Ctrl+S | Save processed images |
| Ctrl+P | Process batch |
| Ctrl+Q | Quit application |
| Space | Preview selected image |
| Delete | Remove from queue |
| Ctrl+A | Select all |
| F1 | Show help |

## Troubleshooting

### Application Won't Start

**Windows**: Install Visual C++ Redistributables
```
https://aka.ms/vs/17/release/vc_redist.x64.exe
```

**Linux**: Install GTK libraries
```bash
sudo apt-get install libgtk-3-0 libblkid1 liblzma5
```

**macOS**: Allow app in Security settings
```
System Preferences → Security & Privacy → Allow
```

### Processing Errors

- **Out of Memory**: Reduce batch size or close other applications
- **Invalid Format**: Ensure input images are JPEG, PNG, or HEIC
- **Permission Denied**: Check write permissions on output directory

### Performance Issues

- Enable GPU acceleration in Settings
- Process smaller batches
- Use lower quality settings for JPEG output
- Close unnecessary background applications

## Command Line Interface

The Flutter app also supports command-line operation:

```bash
# Process single image
photoframe_flutter --input photo.jpg --output processed.bin --mode bw --size 800x480

# Batch process directory
photoframe_flutter --input-dir ~/photos --output-dir ~/processed --mode 6c --format bin,bmp

# With options
photoframe_flutter --input photo.jpg --output photo.bin \
  --mode bw --size 800x480 --crop center --dither floyd
```

### CLI Options

| Option | Description | Default |
|--------|-------------|---------|
| `--input` | Input image file | Required |
| `--input-dir` | Input directory for batch | - |
| `--output` | Output file path | Required |
| `--output-dir` | Output directory | - |
| `--mode` | Color mode (bw, 6c, 7c) | bw |
| `--size` | Display size (WIDTHxHEIGHT) | 800x480 |
| `--format` | Output formats (comma-separated) | bin |
| `--crop` | Crop mode (auto, center, none) | auto |
| `--dither` | Dithering algorithm | floyd |
| `--quality` | JPEG quality (1-100) | 85 |
| `--portrait` | Enable portrait mode | false |
| `--skip-existing` | Skip already processed files | false |
| `--verbose` | Verbose output | false |

## Development

### Project Structure

```
photoframe_flutter/
├── lib/
│   ├── main.dart              # Application entry point
│   ├── models/                # Data models
│   ├── services/              # Image processing services
│   ├── widgets/               # UI components
│   └── utils/                 # Utility functions
├── windows/                   # Windows-specific code
├── macos/                     # macOS-specific code
├── linux/                     # Linux-specific code
├── test/                      # Unit and widget tests
└── pubspec.yaml              # Package dependencies
```

### Running Tests

```bash
# Run all tests
flutter test

# Run with coverage
flutter test --coverage

# Run specific test file
flutter test test/services/image_processor_test.dart
```

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## License

MIT License - See LICENSE file for details