# ESP32 E-Paper Photo Frame

## Introduction

This project implements a battery-powered digital photo frame using an ESP32 microcontroller and e-paper display technology. The system displays images from multiple sources (Google Drive cloud storage or local SD card) achieving months of battery life through intelligent power management and the inherent low-power characteristics of e-paper displays.

The photo frame features automatic image synchronization, configurable refresh intervals, and a comprehensive configuration system that allows runtime customization without firmware recompilation. Images are processed through dedicated tools (Rust processor, Android app, or Flutter app) to optimize them for e-paper display characteristics before being uploaded to storage.

<img src="assets/PXL_20251019_160143849.jpg" alt="ESP32 Photo Frame - Front View" width="600" />
<img src="assets/PXL_20251019_160130264.jpg" alt="ESP32 Photo Frame - Side View" width="600" />
<img src="assets/screenshot2.png" alt="ESP32 Photo Frame - 3D Model Front" width="600" />
<img src="assets/screenshot3.png" alt="ESP32 Photo Frame - 3D Model Back" width="600" />

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
- AI-powered person detection using YOLO11 built directly into the binary (no Python required!)
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
| E-Paper Display | Waveshare 7.5" (800×480 pixels, B/W or color) | [Waveshare](https://www.waveshare.com/7.5inch-e-paper-hat.htm) |
| Display Connector | Good Display DESPI-C02 | [Good Display](https://www.good-display.com/companyfile/DESPI-C02-Specification-29.html) |
| SD Card Module | Adafruit MicroSD Breakout Board | [Adafruit #4682](https://www.adafruit.com/product/4682) |
| Battery | 3.7V 5000mAh LiPo with JST connector | Various suppliers |
| Potentiometer | 50kΩ linear potentiometer | Various suppliers |
| Push Buttons | Momentary switch (normally open) | Various suppliers |
| MicroSD Card | 8GB or larger, FAT32 formatted | Various suppliers |

### Optional Components

| Component | Purpose | Notes |
|-----------|---------|-------|
| RTC Module | Accurate timekeeping without NTP | PCF8523 or DS3231 compatible |
| Battery Monitor | Precise battery level tracking | MAX1704X I2C fuel gauge |

### Wiring Components

| Component | Specification | Quantity | Purpose |
|-----------|--------------|----------|---------|
| Jumper Wires | Female-to-Female, 10-20cm | 20+ | Component connections |
| Breadboard | Half-size (400 tie points) | 1 | Prototyping (optional) |
| Pull-up Resistor | 10kΩ, 1/4W | 1-2 | BUSY pin and I2C (if needed) |
| Capacitor | 100-470µF, 16V electrolytic | 1 | Power supply filtering |
| Heat Shrink Tubing | Various sizes | Assorted | Wire insulation |

### Enclosure and Mounting

| Component | Description |
|-----------|-------------|
| 3D Printed Case | Custom design, PLA or PETG material |
| Mounting Screws | M2.5 × 6mm or M3 × 6mm |
| Standoffs | 5-10mm height, brass or nylon |
| Display Mounting | Double-sided tape or mounting brackets |

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
| Unexpected Maker FeatherS3 | Primary | Default configuration, full feature support |
| Unexpected Maker ProS3 | Full | Extended GPIO, MAX1704X battery monitor |
| Adafruit Huzzah32 Feather V2 | Good | 2MB PSRAM, reduced buffer sizes |
| DFRobot FireBeetle 2 ESP32-C6 | Limited | I2C/WiFi interference, requires isolation |
| Arduino Nano ESP32 | Limited | Requires pin remapping |
| Generic ESP32-S3 (8MB PSRAM) | Good | May require configuration adjustments |

### Display Support

| Display Type | Resolution | Color Support | Status |
|-------------|------------|---------------|--------|
| Waveshare 7.5" V2 | 800×480 | Black/White | Fully Supported |
| Waveshare 7.5" | 800×480 | Black/White/Red | Fully Supported |
| Good Display 7.5" | 800×480 | 6-Color ACeP | Fully Supported |
| Waveshare 7.5" HD | 880×528 | Black/White | Configuration required |
| Waveshare 5.65" | 600×448 | 7-Color | Configuration required |

### Limitations by Platform

#### ESP32-C6 Boards
- Hardware I2C/WiFi interference requires complete I2C shutdown during network operations
- RTC modules cannot be used reliably
- NTP-only time synchronization

#### Low PSRAM Boards (2MB)
- Reduced JSON buffer sizes
- May experience slower Google Drive synchronization
- Limited to ~200 files without streaming optimizations

#### Non-ESP32-S3 Boards
- No native USB support
- Reduced processing performance
- May require external programmer

## Wiring

The project uses separate communication buses to avoid conflicts between the SD card and e-paper display. The SD card utilizes the high-speed SDIO interface while the display uses a dedicated SPI bus.

For detailed pin connections and wiring diagrams, see [Wiring Diagram Documentation](docs/wiring-diagram.md).

Key connections:
- SD Card: SDIO interface (GPIO 11, 12, 13, 14, 16, 17)
- E-Paper: Dedicated SPI (GPIO 35, 36, 37, 38 plus control pins)
- Potentiometer: Analog input (GPIO 18)
- Wake Button: GPIO 1 with internal pull-up

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
   - Select your board environment in `platformio.ini` (default: feathers3_unexpectedmaker)

4. **Python Virtual Environment** (Important!)
   - PlatformIO requires Python 3.10-3.13
   - Always use the project's virtual environment:
   ```bash
   cd platformio
   source .venv/bin/activate  # On macOS/Linux
   # or
   .venv\Scripts\activate     # On Windows
   ```
   - This ensures compatibility with PlatformIO's build system

### Configuration System

The photo frame uses a unified configuration system with a single `/config.json` file on the SD card root.

#### Configuration with SD Card Source

```json
{
  "wifi": [
    {
      "ssid": "YourPrimaryNetwork",
      "password": "YourPassword"
    }
  ],
  "board_config": {
    "refresh": {
      "min_seconds": 600,
      "max_seconds": 14400,
      "step": 300,
      "default": 1800,
      "low_battery_multiplier": 3
    },
    "day_start_hour": 6,
    "day_end_hour": 23,
    "portrait_mode": false
  },
  "sd_card_config": {
    "enabled": true,
    "images_directory": "/images",
    "use_toc_cache": true,
    "toc_max_age_seconds": 86400
  }
}
```

#### Configuration with Google Drive Source

```json
{
  "wifi": [
    {
      "ssid": "YourHomeNetwork",
      "password": "YourHomePassword"
    },
    {
      "ssid": "YourOfficeNetwork",
      "password": "YourOfficePassword"
    },
    {
      "ssid": "YourMobileHotspot",
      "password": "YourHotspotPassword"
    }
  ],
  "board_config": {
    "refresh": {
      "min_seconds": 300,
      "max_seconds": 14400,
      "step": 300,
      "default": 1800,
      "low_battery_multiplier": 3
    },
    "day_start_hour": 6,
    "day_end_hour": 23,
    "portrait_mode": true
  },
  "google_drive_config": {
    "enabled": true,
    "authentication": {
      "service_account_email": "photoframe@myproject.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
      "client_id": "123456789"
    },
    "drive": {
      "folder_id": "1ABC...XYZ",
      "root_ca_path": "/certs/google_root_ca.pem",
      "list_page_size": 100,
      "use_insecure_tls": false
    },
    "caching": {
      "local_path": "/gdrive",
      "toc_max_age_seconds": 604800
    },
    "rate_limiting": {
      "max_requests_per_window": 100,
      "rate_limit_window_seconds": 100,
      "min_request_delay_ms": 500
    }
  },
  "sd_card_config": {
    "enabled": false
  }
}
```

### Image Source Configuration

The photo frame supports two image sources that can be configured via `config.json`:

#### SD Card Source
When `sd_card_config.enabled` is `true`, the photo frame reads images directly from the SD card:
- **images_directory**: Path to the folder containing processed images (e.g., "/images")
- **use_toc_cache**: Enable caching of file list for faster startup
- **toc_max_age_seconds**: How long to cache the file list before rebuilding

#### Google Drive Source
When `google_drive_config.enabled` is `true`, the photo frame downloads images from Google Drive:
- Requires service account authentication
- Automatically caches images locally for offline viewing
- Supports rate limiting to avoid API quota issues
- Streaming architecture handles 350+ files efficiently

**Note**: Only one source can be enabled at a time. If both are enabled, SD card takes precedence.

### WiFi Configuration

The photo frame supports multiple WiFi networks with automatic failover (added in v0.11.0).

#### Multiple Network Setup

Configure up to 3 WiFi networks in your `config.json` file. The photo frame will:
1. Try connecting to the first network (up to 3 attempts with exponential backoff)
2. If the first network fails, try the second network
3. If the second network fails, try the third network
4. Continue until a successful connection is established

#### WiFi Settings

```json
"wifi": [
  {
    "ssid": "YourPrimaryNetwork",
    "password": "YourPrimaryPassword"
  },
  {
    "ssid": "YourBackupNetwork",
    "password": "YourBackupPassword"
  }
]
```

**Configuration Options:**
- **ssid**: Network name (SSID) - required for each network
- **password**: Network password - required for each network

**Failover Behavior:**
- Each network is tried sequentially with up to 3 connection attempts
- Connection timeout: 10 seconds per attempt
- Exponential backoff with jitter between retries on the same network
- 1-second delay between switching to different networks
- Detailed logging shows which network is being attempted

**Use Cases:**
- Primary home network + backup mobile hotspot
- Home network + office network + travel hotspot
- Multiple networks at different locations

## Images Pipeline

### Google Drive Setup

1. **Create a Google Cloud Project**
   - Visit [Google Cloud Console](https://console.cloud.google.com/)
   - Create a new project or select existing
   - Enable the Google Drive API

2. **Create Service Account**
   - Navigate to IAM & Admin → Service Accounts
   - Create a new service account
   - Download the JSON key file
   - Extract credentials for config.json

3. **Prepare Google Drive Folder**
   - Create a folder in Google Drive for your photos
   - Share the folder with the service account email
   - Copy the folder ID from the URL

4. **Process Images**
   - Use either the Rust processor or Android app (see below)
   - Upload processed files to the Google Drive folder

### Using the Rust Processor

```bash
cd rust/photoframe-processor
cargo build --release

# Process images for black & white display
./target/release/photoframe-processor \
  -i ~/photos -o ~/processed \
  -t bw -s 800x480 --auto

# Process with person detection and 6-color output
./target/release/photoframe-processor \
  -i ~/photos -o ~/processed \
  -t 6c -s 800x480 \
  --detect-people --auto-color --force
```

### Using the Android App

1. Install the PhotoFrameProcessor app from `android/PhotoFrameProcessor/`
2. Select images from your gallery
3. Configure processing options (crop, color mode, annotations)
4. Process batch and upload directly to Google Drive

### Using the Flutter Desktop App

```bash
cd photoframe_flutter

# Install dependencies
flutter pub get

# Run the application
flutter run -d windows  # On Windows
flutter run -d macos    # On macOS
flutter run -d linux    # On Linux

# Build for distribution
flutter build windows   # Creates .exe installer
flutter build macos     # Creates .app bundle
flutter build linux     # Creates AppImage/deb package
```

The Flutter app provides:
- Drag-and-drop file selection
- Real-time processing preview
- Multiple output format support (BMP, binary, JPEG, PNG)
- Batch processing with progress tracking
- Cross-platform compatibility

## Image Processing

### Technical Overview

The e-paper display requires images in specific formats optimized for its unique characteristics. Unlike traditional LCD screens, e-paper displays have limited color palettes and refresh rates, necessitating specialized image processing.

#### Processing Pipeline

1. **Input Analysis**: Images are loaded and analyzed for composition, detecting faces and subjects using AI models
2. **Intelligent Cropping**: Based on detected subjects, images are cropped to the target aspect ratio
3. **Color Quantization**: Images are converted to the target color palette (B/W, 6-color, or 7-color)
4. **Dithering**: Floyd-Steinberg or ordered dithering is applied for better perceived quality
5. **Binary Encoding**: Final image is encoded in a compact binary format for efficient storage

#### Binary Format Structure

The custom binary format consists of:
- 4-byte header with version and flags
- Image dimensions and color mode
- Compressed pixel data using run-length encoding
- Optional metadata (date, annotations)

This format achieves 50-70% size reduction compared to BMP while maintaining fast decoding on the ESP32.

### Rust Processor Details

The Rust-based processor (`rust/photoframe-processor`) provides:
- Parallel processing utilizing all CPU cores
- YOLO11 neural network for person detection
- Automatic portrait pairing for combined displays
- EXIF metadata extraction for date annotations
- Multiple output formats with organized directory structure

For detailed documentation, see [Rust Processor Documentation](docs/rust-photoframe-processor.md).

### Android App Details

The Android application offers:
- Intuitive touch interface for image selection
- Real-time preview of processing effects
- Batch operations with progress tracking
- Direct Google Drive integration
- Settings persistence for repeated use

## Documentation Index

### Firmware Documentation
- [Technical Specifications](docs/tech_specs.md) - System architecture and API documentation
- [Wiring Diagram](docs/wiring-diagram.md) - Detailed hardware connections
- [Google Drive API](docs/google_drive_api.md) - Google Drive integration and setup
- [Display Rendering Architecture](platformio/DISPLAY_RENDERING_ARCHITECTURE.md) - Display rendering pipeline
- [Buffer Management](platformio/BUFFER_MANAGEMENT.md) - Memory allocation and buffer lifecycle

### Image Processing
- [Rust Processor Guide](docs/rust-photoframe-processor.md) - Advanced image processing with AI features
- [Image Processing Pipeline](docs/image_processing.md) - Image format and processing details

### Development
- [CLAUDE.md](CLAUDE.md) - AI assistant guidance for code contributions
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