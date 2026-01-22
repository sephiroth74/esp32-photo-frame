# ESP32 E-Paper Photo Frame

## Introduction

This project implements a battery-powered digital photo frame using an ESP32 microcontroller and e-paper display technology. The system displays images from multiple sources (Google Drive cloud storage or local SD card) achieving months of battery life through intelligent power management and the inherent low-power characteristics of e-paper displays.

The photo frame features automatic image synchronization, configurable refresh intervals, and a comprehensive configuration system that allows runtime customization without firmware recompilation. Images are processed through dedicated tools (Rust processor, Android app, or Flutter app) to optimize them for e-paper display characteristics before being uploaded to storage.

<img src="assets/screenshot-001.jpg" alt="ESP32 Photo Frame - Front View" width="746" />
<img src="assets/screenshot-002.jpg" alt="ESP32 Photo Frame - Back View" width="746" />

## Features

### Core Firmware Features
- Dual image source support: Google Drive cloud storage or local SD card directory (v0.13.0)
- Native display driver integration replacing GxEPD2 library for better color display support (v0.13.0)
- Multiple WiFi network support (v0.11.0) with automatic failover between up to 3 configured networks
- Exponential potentiometer control (v0.9.3) using cubic curve mapping for precise refresh interval adjustment
- Battery monitoring with power-saving modes and adaptive refresh scheduling
- Deep sleep operation between updates for extended battery life (2-3 months on 5000mAh)
- Binary image format (.bin) support optimized for e-paper displays
- Streaming architecture supporting 350+ files on any ESP32 variant
- Unified configuration system via single JSON file
- Multi-language support (English and Italian localization)
- Day/night scheduling to prevent overnight updates
- Comprehensive debug mode for hardware troubleshooting
- Portrait mode support for vertical display orientation (v0.13.0)

### Image Processing Tools

#### Rust Processor (rust/photoframe-processor)
- High-performance batch processing with 5-10x speed improvement over shell scripts
- AI-powered person detection with YOLO11 built directly into the binary
- Multiple dithering algorithms: Floyd-Steinberg, Ordered (Bayer), Sierra, Atkinson
- Automatic portrait pairing for landscape displays
- Multi-format output support (BMP, binary, JPEG, PNG)
- EXIF metadata extraction for date annotation
- Font customization for image overlays
- Filename encoding with Base64 for special character support

#### Android Application (android/PhotoFrameProcessor)
- Graphical user interface for image selection and processing
- Real-time preview of processed images
- Touch-based crop selection
- Batch processing with progress tracking
- Direct Google Drive upload integration
- Person detection and smart cropping

#### Flutter Application (photoframe_flutter)
- Cross-platform desktop application for Windows, macOS, and Linux
- Modern Material Design interface
- Batch image processing with preview
- Multiple output format support
- EXIF metadata handling
- Drag-and-drop file selection

## Project Structure

```
esp32-photo-frame/
├── platformio/                 # ESP32 firmware (PlatformIO project)
│   ├── src/                   # Source code files
│   ├── include/               # Header files and board configurations
│   ├── lib/                   # External libraries
│   ├── data/                  # SD card files (config.json.template)
│   └── platformio.ini         # PlatformIO configuration
├── rust/                      # Rust-based tools
│   ├── photoframe-processor/  # Main image processing tool
│   ├── bin2bmp/              # Binary to image converter
│   └── bmp2cpp/              # Bitmap to C++ header converter
├── android/                   # Android companion app
│   └── PhotoFrameProcessor/   # Kotlin-based image processor
├── photoframe_flutter/        # Flutter desktop application
│   ├── lib/                  # Dart source code
│   ├── windows/              # Windows platform files
│   ├── macos/                # macOS platform files
│   └── linux/                # Linux platform files
├── docs/                      # Technical documentation
├── assets/                    # Images and resources
│   └── 3d model/             # 3D printable enclosure files
├── scripts/                   # Shell scripts (deprecated)
└── icons/                     # Icon generation scripts
```

## Recent Changes

For a complete list of changes and version history, see [CHANGELOG.md](CHANGELOG.md).

## Required Hardware

### Essential Components

| Component | Specification | Purchase Link |
|-----------|--------------|---------------|
| Microcontroller | Unexpected Maker FeatherS3(D) (ESP32-S3, 8MB PSRAM, 16MB Flash) | [Unexpected Maker](https://unexpectedmaker.com/shop/feathers3) |
| E-Paper Display | Good Display 7.3" (800×480 pixels, 6 color) | [Good Display](https://www.good-display.com/blank7.html?productId=533) |
| Display Connector | Good Display DESPI-C73 (DESPI-C73) | [Good Display](https://www.good-display.com/product/522.html) |
| SD Card Module | Adafruit MicroSD Breakout Board | [Adafruit #4682](https://www.adafruit.com/product/4682) |
| Battery | 3.7V 5000mAh LiPo with JST connector | Various suppliers |
| Push Buttons | 6x6x13mm Momentary switch | Various suppliers |
| MicroSD Card | 8GB or larger, FAT32 formatted | Various suppliers |
| Capacitor | 100uF 16V | Various suppliers |
| Heat breass threaded inserts | 4x5 | Various suppliers |
| Screws | M2.5x5 M3x5 M3x6 | Various suppliers |


### Required Tools

- Soldering iron with fine tip (15-25W)
- Solder wire (0.6-0.8mm rosin core)
- Wire strippers
- Multimeter for continuity testing
- USB-C cable for programming
- MicroSD card reader
- 3D printer or access to printing service
- Small screwdriver
- Flush cutters for wire trimming

## Supported Hardware

### Microcontroller Boards

| Board | Support Level | Notes |
|-------|--------------|-------|
| Unexpected Maker FeatherS3 | Full | Default configuration, full feature support |
| Unexpected Maker ProS3 | Full | Extended GPIO, MAX1704X battery monitor |

### Display Support

| Display Type | Resolution | Color Support | Status |
|-------------|------------|---------------|--------|
| Good Display 7.5" GDEY075T7 | 800×480 | Black/White | Fully Supported |
| Good Display 7.3" GDEP073E01 | 800×480 | 6-Color ACeP | Fully Supported |


## Wiring

The project uses separate communication buses to avoid conflicts between the SD card and e-paper display. The SD card utilizes the high-speed SDIO interface while the display uses a dedicated SPI bus.

For detailed pin connections and wiring diagrams, see [Wiring Diagram Schematics](docs/pros3d_schematics.pdf).


## Setup

### Development Environment

1. **Install Visual Studio Code**
   - Download from [code.visualstudio.com](https://code.visualstudio.com/)
   - Install the PlatformIO IDE extension from the marketplace

2. **Clone the Repository**
   ```bash
   git clone https://github.com/sephiroth74/esp32-photo-frame.git
   cd esp32-photo-frame/platformio
   ```

3. **Configure PlatformIO**
   - Open the project in VS Code
   - PlatformIO will automatically install required packages
   - Select your board environment in `platformio.ini` (default: pros3_unexpectedmaker)

### Configuration

The photo frame uses a JSON configuration file (`/config.json`) on the SD card root. The configuration supports:

- **Multiple WiFi networks** - Up to 3 networks with automatic failover
- **Dual image sources** - Google Drive cloud storage or local SD card directory
- **Display settings** - Portrait/landscape mode, refresh intervals, day/night scheduling
- **Power management** - Battery-aware refresh adjustments

#### Quick Configuration Examples

**Using SD Card as image source:**
```json
{
  "wifi": [{"ssid": "YourNetwork", "password": "YourPassword"}],
  "sd_card_config": {
    "enabled": true,
    "images_directory": "/images"
  }
}
```

**Using Google Drive as image source:**
```json
{
  "wifi": [{"ssid": "YourNetwork", "password": "YourPassword"}],
  "google_drive_config": {
    "enabled": true,
    "authentication": {
      "service_account_email": "photoframe@project.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
      "client_id": "123456789"
    },
    "drive": {
      "folder_id": "YOUR_FOLDER_ID",
      "use_insecure_tls": true
    }
  }
}
```

📖 **[Complete Configuration Reference →](docs/config_reference.md)**

Details on all configuration options including WiFi failover, portrait mode, refresh intervals, and advanced Google Drive settings.

## Image Processing

Images must be processed into a specific binary format optimized for e-paper displays. The firmware supports only `.bin` files that match your display's resolution and color mode.

### Processing Tools

Two main tools are available for image processing:

#### 🦀 Rust Processor (Command Line)
High-performance batch processing with AI features:
```bash
cd rust/photoframe-processor
cargo build --release

# Process for black & white display
./target/release/photoframe-processor -i ~/photos -o ~/processed -t bw -s 800x480 --auto

# Process with AI person detection for 6-color display
./target/release/photoframe-processor -i ~/photos -o ~/processed -t 6c -s 800x480 --detect-people --auto
```

#### 🎨 Flutter App (Desktop GUI)
Cross-platform desktop application with graphical interface:
```bash
cd photoframe_flutter
flutter pub get
flutter run -d [windows/macos/linux]
```

📖 **[Complete Image Processing Guide →](docs/image_processing.md)**

Details on color modes, dithering algorithms, portrait pairing, and batch processing.

## Setup Guide

### Google Drive Setup (Optional)

If using Google Drive as your image source:

1. **Create Service Account** - Set up authentication in [Google Cloud Console](https://console.cloud.google.com/)
2. **Share Folder** - Grant access to the service account email
3. **Process Images** - Use the tools above to prepare your photos
4. **Configure** - Add credentials to `config.json`

📖 **[Google Drive Setup Guide →](docs/google_drive_api.md)**

## Documentation Index

### Firmware Documentation
- [Technical Specifications](docs/tech_specs.md) - System architecture and API documentation
- [Schematics](docs/pros3d_schematics.pdf) - Detailed hardware connections
- [Google Drive API](docs/google_drive_api.md) - Google Drive integration and setup

### Image Processing
- [Rust Processor Guide](docs/rust-photoframe-processor.md) - Advanced image processing with AI features
- [Image Processing Pipeline](docs/image_processing.md) - Image format and processing details

### Development
- [CHANGELOG.md](CHANGELOG.md) - Version history and release notes

## 3D Printable Enclosure

A complete 3D printable enclosure design is available at [`/assets/3d model/ESP32-Photo-Frame.3mf`](assets/3d%20model/ESP32-Photo-Frame.3mf).

The enclosure features:
- Integrated mounting points for all components
- Cable management channels
- Ventilation for battery safety
- Easy access to SD card and USB port
- Stand for desktop display

Recommended print settings:
- Layer height: 0.2mm
- Infill: 20%
- Support: Required for some parts
- Material: PLA or PETG

## License

MIT License

Copyright (c) 2025 Alessandro Crugnola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.