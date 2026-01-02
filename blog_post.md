# Building a Smart E-Paper Photo Frame: From Simple Display to Cloud-Connected Art

*How I transformed a basic ESP32 and e-paper display into a cloud-connected digital photo frame with some unexpected engineering challenges along the way.*

![ESP32 Photo Frame](assets/PXL_20251019_160143849.jpg)

## The Vision

It started with a simple idea: create a battery-powered digital photo frame using an e-paper display that could last for months on a single charge. E-paper's ultra-low power consumption and paper-like appearance made it perfect for displaying family photos. But what began as a weekend project evolved into a sophisticated system featuring Google Drive integration, weather displays, and an RGB status system.

## The Hardware Foundation

At the heart of the project is an **Unexpected Maker FeatherS3D** - an ESP32-S3 board with 8MB of PSRAM and a built-in NeoPixel LED. The display is a 7.5" e-paper panel capable of showing either black & white or 6 colors at 800×480 resolution.

The complete build includes:
- **MicroSD card** for configuration and image caching
- **50kΩ potentiometer** for refresh rate control
- **5000mAh LiPo battery** for months of operation
- **Custom 3D-printed enclosure** (STL files included in the project)


## Image Processing Pipeline

The project includes multiple ways to prepare images:

### High-Performance Rust Processor
```bash
./photoframe-processor -i ~/photos -o ~/processed \
  --size 800x480 --output-format bmp,bin,jpg \
  --detect-people --auto-color --force
```

Features AI-powered person detection using YOLO11 for smart cropping and automatic portrait pairing for landscape displays.

### Android Companion App
A full-featured Android app provides:
- Real-time preview of processed images
- Touch-based crop selection
- Batch processing with progress tracking
- Direct Google Drive upload

### Binary File Format Detection
The firmware now automatically detects file formats at runtime:

```cpp
// Smart format detection - no compile-time flags needed
if (filename.endsWith(".bin")) {
    renderBinaryImage(file);
} else if (filename.endsWith(".bmp")) {
    renderBitmapImage(file);
}
```

## The Comprehensive Debug System

One of the most valuable additions has been the display debug mode, featuring:

- **Battery status test** with voltage/percentage display
- **Google Drive integration test** with API verification
- **Potentiometer test** showing real-time values and statistics
- **Gallery test** cycling through all cached images
- **Stress test** with Floyd-Steinberg dithering
- **Hardware diagnostics** for troubleshooting

## Power Optimization

The system achieves months of battery life through intelligent power management:

- **Deep sleep** between image updates (consuming ~10µA)
- **Configurable day/night schedule** (no updates overnight)
- **Battery-aware features** (reduced functionality below 25%)
- **WiFi on-demand** (connects only when needed)
- **SD card shutdown** after operations complete

## Performance Metrics

- **Image rendering**: Reduced from 81+ seconds to ~27 seconds (3× improvement)
- **Memory usage**: 38% stable (previously 96% crashes)
- **File capacity**: 350+ files on any ESP32 variant
- **Battery life**: 2-3 months on 5000mAh battery
- **Startup time**: 8-12 seconds from deep sleep to display

## Configuration Made Simple

Version 0.7.1 introduced a unified configuration system with a single `/config.json` file:

```json
{
  "wifi": {
    "ssid": "YourNetwork",
    "password": "YourPassword"
  },
  "google_drive": {
    "folder_id": "your-folder-id",
    "service_account_email": "your-service@project.iam"
  },
  "weather": {
    "enabled": true,
    "latitude": 40.7128,
    "longitude": -74.0060,
    "celsius": true
  }
}
```

## What's Next?

Future enhancements I'm considering:
- **Multi-folder support** for themed galleries
- **Solar charging** integration
- **HomeAssistant integration** for smart home control

## Open Source & Community

**Repository**: [github.com/sephiroth74/esp32-photo-frame](https://github.com/sephiroth74/esp32-photo-frame)

### Key Specifications
- **Supported Boards**: ESP32-S3 (FeatherS3, ProS3), ESP32-C6, ESP32
- **Display Sizes**: 7.5" (800×480), adaptable to other sizes
- **Image Formats**: Binary (.bin), Bitmap (.bmp), auto-detected
- **Color Modes**: Black & white, 6-color, 7-color
- **Processing Tools**: Rust CLI, Android app, Python scripts
- **Power Consumption**: ~10µA sleep, 50-150mA active

## Conclusion

What started as a simple photo frame has evolved into a sophisticated device that seamlessly blends cloud connectivity, power efficiency, and user experience.

Whether you're interested in e-paper displays, ESP32 development, or just want a unique way to display your photos, I hope this project inspires you to build something amazing.

Feel free to fork the project, suggest improvements, or share your own builds.

---

*Follow the project on [GitHub](https://github.com/sephiroth74/esp32-photo-frame) | Built with ❤️ and lots of coffee*