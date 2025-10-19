# Arduino E-Paper Photo Frame

**esp32-photo-frame** is a firmware project for an ESP32-based digital photo frame. The project is designed to display images from Google Drive cloud storage on an e-paper (EPD) display, with features such as battery monitoring, real-time clock (RTC) support, and multi-language localization. The SD card is used for initial operations including local caching, configuration storage, and temporary files, then shut down before display operations to avoid SPI conflicts.

<img src="assets/PXL_20251019_160143849.jpg" alt="ESP32 Photo Frame" width="600" />
<img src="assets/PXL_20251019_160130264.jpg" alt="ESP32 Photo Frame" width="600" />
<img src="assets/screenshot2.png" alt="ESP32 Photo Frame 3D model" width="600" />
<img src="assets/screenshot3.png" alt="ESP32 Photo Frame 3D model" width="600" />

## Features

- **🎨 RGB Status System** (v0.5.0): Visual feedback using built-in NeoPixel LED with 14 predefined status states
  - Real-time system status indication (WiFi, SD operations, Google Drive, rendering, etc.)
  - Battery-aware power management with automatic brightness scaling
  - FreeRTOS task-based smooth animations (50Hz update rate)
  - Ultra power-efficient design (2-5mA additional current, auto-disabled during sleep)
- **Google Drive Integration**: Primary image source with automatic synchronization and caching from Google Drive folders
- **SD Card Caching**: Local caching system for improved performance and offline capabilities
- **Smart Image Processing**: Automatic resizing, annotation, and format conversion
- **Weather Display**: Runtime-configurable weather information with location, timezone, and update intervals (controlled via JSON config)
- **Enhanced FeatherS3 Support**: Optimized pin configuration and deep sleep wakeup for Unexpected Maker FeatherS3
- Battery voltage monitoring and reporting with power-saving modes
- **PCF8523 RTC Integration**: Real-time clock support with automatic NTP fallback for accurate timekeeping
- **Exponential Potentiometer Control** (v0.9.3): Cubic curve mapping for ultra-fine control at short intervals
  - First 25% of rotation covers 5-9 minutes for precise short-interval adjustments
  - First 50% covers 5-33 minutes (most common usage range)
  - Remaining 50% covers 33-240 minutes for occasional/overnight use
- **Security**: Configurable TLS/SSL security for cloud connections
- Multi-language support (English, Italian)
- **Rate Limiting**: Built-in API rate limiting for Google Drive requests
- Modular code structure for easy customization and extension

### Recent Improvements (v0.8.0)
- **🆕 ProS3(d) Board Support**: Full integration for Unexpected Maker ProS3(d) ESP32-S3 development board
  - **Enhanced GPIO Layout**: Extensive pin availability with optimized pin assignments
  - **I2C Battery Fuel Gauge**: MAX1704X precision battery monitoring on shared I2C bus
  - **Advanced Power Management**: 5V detection, LDO2 control, enhanced deep sleep capabilities
  - **High-Performance Configuration**: 16MB Flash, 8MB PSRAM, USB-C connectivity
- **🕒 PCF8523 RTC Integration**: Real-time clock support eliminates NTP dependency
  - **Adafruit PCF8523 Support**: Accurate timekeeping with backup battery capability
  - **Shared I2C Bus**: Efficient resource utilization with battery fuel gauge
  - **Smart Time Management**: Automatic fallback to NTP with intelligent validation
  - **Deep Sleep Persistence**: Continuous time tracking during power-saving modes
- **🎯 PSRAM Mandatory**: Simplified architecture with guaranteed PSRAM availability
  - **Code Simplification**: Removed conditional PSRAM compilation blocks
  - **Enhanced Performance**: Optimized memory allocation strategies
  - **Dynamic Buffer Sizing**: Intelligent scaling based on available flash memory (1MB-4MB buffers)

### Previous Improvements (v0.7.1)
- **⚙️ Unified Configuration System**: Single `/config.json` file replaces 3 separate configuration files
  - **Runtime Weather Control**: Weather functionality now controlled by JSON config
  - **Enhanced Validation**: Automatic value capping and fallback to preprocessor constants

### Previous Improvements (v0.7.0)
- **🗂️ Optimized Storage Layout**: Eliminated factory partition and redistributed 4MB additional space
- **⚡ PSRAM Streaming**: Enhanced HTTP client to leverage full 8MB PSRAM with chunked reading
- **🌅 Fixed Weather Times**: Corrected sunrise/sunset display using Unix timestamps
- **🛡️ Memory Safety**: Fixed critical memory corruption crashes with improved HTTPClient usage

## Project Structure

- `src/` - Main source code for device logic, battery, SD card, rendering, RTC, and localization
- `include/` - Header files and configuration
- `icons/` - Scripts and assets for generating icon headers
- `lib/` - External libraries and assets
- `platformio.ini` - PlatformIO project configuration

## Getting Started

1. **Hardware Requirements**
   - ESP32-S3 board with **PSRAM** (mandatory) - see supported boards below
   - [Waveshare 7.5 e-paper display](https://www.waveshare.com/7.5inch-e-paper-hat.htm)
   - [Good Display DESPI-C02](https://www.good-display.com/companyfile/DESPI-C02-Specification-29.html) Connector board
   - [Adafruit microSD](https://www.adafruit.com/product/4682?srsltid=AfmBOopfN-tYU3fKgpQquFOTLEY50Pl4PY8iTpBpGoXCpbJ8EQGVqBHn)
   - [Adafruit PCF8523 RTC Breakout](https://www.adafruit.com/product/3295) (optional, eliminates NTP dependency)
   - 3.7V 5000mAh LiPo Battery

2. **Hardware Assembly & Wiring**
   - **📋 [Complete Wiring Diagram](docs/wiring-diagram.md)** - Comprehensive pin connections and assembly guide
   - **Supported Boards (v0.8.0)**:
     - **🆕 Unexpected Maker ProS3(d)** - Professional development board with advanced features
       - ESP32-S3 dual core, **8MB PSRAM**, 16MB Flash, extensive GPIO layout
       - **I2C Battery Fuel Gauge**: MAX1704X precision battery monitoring (IO8/IO9)
       - **Advanced Features**: 5V detection (IO33), LDO2 power control (IO17), USB-C
       - **PCF8523 RTC Support**: Shares I2C bus with battery fuel gauge for optimal pin usage
       - **Enhanced Pin Layout**: More available GPIOs for custom expansions
     - **🥇 Unexpected Maker FeatherS3** - Fully optimized with RGB status system
       - ESP32-S3 dual core, **8MB PSRAM**, 16MB Flash, built-in NeoPixel RGB LED
       - USB-C, excellent battery management, no I2C/WiFi interference issues
       - **RGB Status Feedback**: Built-in NeoPixel on GPIO40 for visual system status
       - **Deep Sleep Wakeup**: Optimized RTC GPIO configuration (GPIO1 with proper pull-up)
   - **Alternative Boards**:
     - **Adafruit Huzzah32 Feather v2** (ESP32-S3, 2MB PSRAM, Feather ecosystem - requires external RGB LED)
   - **Key Connections**:
     - **SD Card**: SD_MMC interface (SDIO) for high-speed access
     - **E-Paper Display**: Dedicated SPI pins to avoid SD card conflicts
     - **RGB Status LED**: Built-in NeoPixel (FeatherS3) or external WS2812B LED
     - **I2C**: PCF8523 RTC, MAX1704X battery fuel gauge (ProS3(d)), optional sensors
     - **Analog**: Battery monitoring via built-in voltage divider or I2C fuel gauge
   - **Wakeup Button**: Connected between GPIO1 and GND with internal pull-up (FeatherS3 optimized)

3. **Setup**
   - Clone this repository
   - Open the project in [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/)
   - Configure your hardware settings in `include/config.h` and `src/config.cpp` if needed

## 🎨 RGB Status System (v0.5.0)

The FeatherS3 version includes a comprehensive RGB status system using the built-in NeoPixel LED for real-time visual feedback:

### Status Indicators
- **🔷 Starting**: White pulse during system startup (3s)
- **🔵 WiFi Connecting**: Blue pulse while connecting to WiFi
- **🔴 WiFi Failed**: Red slow blink when connection fails
- **🟢 Weather Fetching**: Green pulse while fetching weather data
- **🟠 SD Operations**: Orange pulse during SD card operations
- **🟡 SD Writing**: Yellow pulse during SD card writes
- **🔵 Google Drive**: Cyan pulse during Google Drive operations
- **🟣 Downloading**: Purple pulse during file downloads
- **🟡 Rendering**: Pink solid during display rendering
- **🔴 Battery Low**: Red slow blink for critical battery warning
- **🔴 Error**: Red fast blink for system errors
- **⚪ Sleep Prep**: Dim white fade-out before deep sleep

### Power Management
- **Battery-Aware**: Automatic brightness reduction when battery is low
- **Ultra Power Efficient**: 2-5mA additional consumption (ultra-low brightness levels)
- **Sleep Optimization**: Complete shutdown during deep sleep (0mA impact)
- **Critical Battery Protection**: RGB system auto-disables to preserve power

### Technical Features
- **FreeRTOS Task**: Smooth 50Hz animations in dedicated thread
- **14 Predefined States**: Comprehensive system status coverage
- **Custom Colors**: Support for user-defined colors and effects
- **Hardware Control**: Power management via GPIO39 (LDO2)

4. **Image Processing Pipeline (Required)**

   **🔧 All images must be processed before upload to Google Drive**

   Images must be converted to the ESP32-compatible format and uploaded to Google Drive using:

   **📋 [Rust Image Processor](rust/photoframe-processor/)** - High-performance image processing tool

   ```bash
   # Black & white processing (800x480 display)
   ./rust/photoframe-processor/target/release/photoframe-processor \
     -i ~/Photos -o ~/processed_images -t bw -s 800x480 --extensions jpg,png --auto

   # 6-color processing with custom fonts
   ./rust/photoframe-processor/target/release/photoframe-processor \
     -i ~/Photos -o ~/processed_images -t 6c -s 800x480 --extensions jpg,png,heic \
     --auto --font "Arial-Bold" --pointsize 32
   ```

   **Alternative: Android App**
   
   Use the Android PhotoFrameProcessor app located in `android/PhotoFrameProcessor/` for a user-friendly GUI to process images with:
   - AI-powered person detection and smart cropping
   - Automatic EXIF date extraction and labeling
   - Portrait image pairing for landscape display
   - Real-time preview of processed results

   **Alternative: Rust Binary Converter (Recommended for Performance)**
   
   Use the high-performance Rust-based `photoframe-processor` tool for batch processing with **5-10x faster processing** and **AI-powered people detection**:
   
   **📋 [Complete Rust Processor Documentation](docs/rust-photoframe-processor.md)** - Comprehensive guide with AI features, font support, and performance optimization

4. **Unified Configuration System (v0.7.1)**

   **🔧 Single Configuration File**: All settings (WiFi, Google Drive, Weather, Board) in one unified `/config.json` file on the SD card

   Create a single `/config.json` file on your SD card root with all system configuration:

   ```json
   {
     "wifi": {
       "ssid": "YourWiFiNetwork",
       "password": "YourWiFiPassword"
     },
     "google_drive": {
       "authentication": {
         "service_account_email": "your-service-account@project.iam.gserviceaccount.com",
         "private_key_pem": "-----BEGIN PRIVATE KEY-----\nYOUR_PRIVATE_KEY_CONTENT\n-----END PRIVATE KEY-----\n",
         "client_id": "your-client-id"
       },
       "drive": {
         "folder_id": "your-google-drive-folder-id",
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
         "min_request_delay_ms": 500,
         "max_retry_attempts": 3,
         "backoff_base_delay_ms": 5000,
         "max_wait_time_ms": 30000
       }
     },
     "weather": {
       "enabled": true,
       "latitude": 40.7128,
       "longitude": -74.0060,
       "update_interval_minutes": 120,
       "celsius": true,
       "battery_threshold": 15,
       "max_age_hours": 3,
       "timezone": "auto",
       "temperature_unit": "celsius",
       "wind_speed_unit": "kmh",
       "precipitation_unit": "mm"
     },
     "board": {
       "day_start_hour": 6,
       "day_end_hour": 23,
       "refresh": {
         "min_seconds": 600,
         "max_seconds": 14400,
         "step": 300,
         "low_battery_multiplier": 3
       }
     }
   }
   ```

5. **Google Drive Setup (Required)**

   **📤 Complete Workflow**: Process images → Upload to Google Drive → Share with service account → Configure JSON file

   Google Drive is the **only supported image source** for the photo frame:

   - **Step 1**: Process your images using the tools above to create `.bin` files
   - **Step 2**: Create a Google Service Account in [Google Cloud Console](https://console.cloud.google.com/)
   - **Step 3**: Enable the Google Drive API for your project
   - **Step 4**: Download the service account JSON key file
   - **Step 5**: **Create a Google Drive folder** and upload your processed `.bin` files to it
   - **Step 6**: **Critical**: Share your Google Drive folder with the service account email (found in the JSON key file)
   - **Step 7**: Add your Google Drive configuration to the unified `/config.json` file (see configuration section above)

   **SD Card Usage**

   The SD card serves as **configuration and caching storage only** (images are stored in Google Drive):

   - **Unified Configuration**: Single `/config.json` file with all system settings (WiFi, Google Drive, Weather, Board)
   - **Local Image Caching**: Downloaded Google Drive images are cached locally for improved performance and offline display
   - **Weather Data**: Cached weather information for offline display
   - **Temporary Files**: Download and processing temporary storage
   - **TOC System**: Two-file Table of Contents system (`toc_data.txt` and `toc_meta.txt`) for enhanced data integrity

   **Architecture Note**: Images are sourced exclusively from Google Drive and cached on SD card. The SD card is shut down after download operations complete to avoid SPI conflicts with the e-paper display. On shared SPI boards (e.g., FeatherS3), images are copied to LittleFS before display operations begin.

   **Enhanced TOC System (2025)**: The firmware now uses a robust 2-file TOC system with `toc_data.txt` containing file entries and `toc_meta.txt` containing metadata and integrity information, providing better error recovery and data validation.

5. **Binary to Image Conversion**

   The project includes a powerful Rust-based tool for converting ESP32 binary image files back to viewable formats:

   **📋 [Binary to Image Converter Documentation](docs/bin_2_image.md)** - Complete guide for the `bin2bmp` tool

   ```bash
   # Convert binary to BMP with default 800x480 size
   ./rust/bin2bmp/target/release/bin2bmp -i photo.bin -t bw
   
   # Convert 6-color binary to JPEG with custom size
   ./rust/bin2bmp/target/release/bin2bmp -i photo.bin -o output/ -s 1200x825 -t 6c -f jpeg
   
   # Validate binary file only
   ./rust/bin2bmp/target/release/bin2bmp --validate-only -i photo.bin
   ```

   **Key Features:**
   - 🚀 **Fast**: Native Rust performance with animated progress bars
   - 🎯 **Smart Validation**: Helpful error messages with dimension suggestions  
   - 🎨 **Multiple Formats**: BMP, JPEG, PNG output support
   - 📁 **Auto Naming**: Automatically generates output filenames

6. **Build and Upload**
   - Connect your ESP32 board
   - Use PlatformIO to build and upload the firmware

## ESP32-C6 Success Story - Memory Breakthrough 🚀

**ESP32-C6 is now fully recommended** with breakthrough memory optimizations that completely eliminate previous limitations!

### **Revolutionary Performance Achievement**
Recent testing on **DFRobot FireBeetle 2 ESP32-C6** achieved remarkable results:

- ✅ **352 Google Drive files** processed successfully (exceeded previous estimates by 200%!)
- ✅ **38.2-38.4% memory usage** throughout entire operation (vs 96.5% crashes before)
- ✅ **Streaming TOC processing** eliminates memory spikes entirely
- ✅ **Complete stability** with room for even larger collections

### **Breakthrough Memory Optimizations (2025)**

The firmware now implements **revolutionary streaming architecture** that works on ALL ESP32 variants:

| Optimization Technology | Memory Saved | Performance Impact |
|------------------------|--------------|-------------------|
| **Streaming TOC Processing** | ~100-150KB | Direct disk writing eliminates file accumulation |
| **Eliminated google_drive_files_list** | ~50-100KB | No more 347-file vector storage |
| **Simplified Metadata** | ~7-14KB | Removed unused mimeType/modifiedTime fields |
| **Optimized JSON Parsing** | ~20-50KB | Static allocation prevents fragmentation |
| **Total Memory Savings** | **200-350KB+** | **Any ESP32 → 350+ files capable** |

### **I2C/WiFi Coexistence Solution**
While ESP32-C6 has I2C/WiFi interference, the firmware includes **complete isolation architecture**:

1. **Complete I2C Shutdown**: I2C bus shut down before any WiFi operations
2. **Sequential Operation**: All network tasks happen while I2C is disabled  
3. **NTP-Only Time Sync**: Eliminates RTC complexity entirely
4. **Deferred Updates**: Time stored during WiFi, applied after I2C restart

**Result**: ESP32-C6 now operates **flawlessly** with large Google Drive collections.

### **Platform Comparison (Updated 2025)**

| Platform | Max Files Tested | Memory Usage | Recommendation |
|----------|------------------|--------------|----------------|
| **ESP32-C6** | **352 files** ✅ | **38% stable** | **✅ Fully Recommended** |
| **ESP32-S3** | **350+ files** ✅ | **30-35%** | **✅ Excellent Choice** |
| **Standard ESP32** | **300+ files** ✅ | **45-50%** | **✅ Good Performance** |

**All platforms now support large collections** - choose based on **features**, not memory limitations!

## PSRAM Enhancement - Performance Bonus (Optional)

With the **breakthrough streaming architecture**, PSRAM is **no longer required** for large Google Drive collections. However, **ESP32-S3 boards with PSRAM** still receive **performance bonuses** for ultra-fast processing.

### **All Platforms Now Handle Large Collections**

**✅ New Reality (2025)**: Every ESP32 variant can handle 300+ files with streaming optimizations:

| Platform | Files Supported | Memory Usage | Speed |
|----------|----------------|--------------|--------|
| **Standard ESP32** | **300+ files** ✅ | 45-50% | Streaming architecture |
| **ESP32-C6** | **352+ files** ✅ | 38% | Streaming + isolation fixes |
| **ESP32-S3** | **350+ files** ✅ | 30-35% | Streaming baseline |
| **ESP32-S3 + PSRAM** | **500+ files** ✅ | 25-30% | **Performance bonus** |

### **PSRAM Performance Bonus Features**

While no longer necessary, PSRAM still provides **premium performance** for power users:

**Enhanced Capabilities with PSRAM:**
- **Larger Buffer Processing**: 4MB JSON responses processed instantly (vs streaming)
- **Faster Sync Times**: 3-5x faster for very large collections (500+ files)
- **Zero Stream Parsing**: Eliminate processing overhead entirely
- **Future-Proofing**: Ready for massive collections (1000+ files)

### **Image Rendering Performance Optimization (v0.2.0)**

The firmware includes improved page-aware rendering that reduces image display time:

**Performance Comparison:**
| Rendering Stage | Before | After | Improvement |
|----------------|--------|--------|-------------|
| **Full Image Rendering** | 81+ seconds | ~27 seconds | 3x faster |
| **Per-Page Processing** | 27s each × 3 pages | ~9s each × 3 pages | 67% reduction per page |
| **Pixel Processing** | 384,000 pixels × 3 | ~128,000 pixels/page | Optimized processing |

**Technical Improvements:**
- **Page-Aware Processing**: Only processes pixels relevant to current display page
- **Coordinate System**: Uses absolute coordinates with GxEPD2's automatic clipping
- **Memory Efficiency**: Maintains low memory usage while improving rendering speed
- **Multi-Page Support**: Optimized for displays requiring multiple page updates (163px height sections)

**Result**: Image display time reduced from over a minute to under 30 seconds.

### **Automatic Optimization**

**Bottom Line**: Choose your platform based on **features and price** - all platforms handle large Google Drive collections excellently!

## Technical Specifications

For detailed technical documentation including comprehensive error handling system, system architecture, hardware specifications, and API references, see **📋 [Technical Specifications](docs/tech_specs.md)**.

### Documentation Index

- **📋 [Technical Specifications](docs/tech_specs.md)** - System architecture, error handling, and API references
- **📋 [Changelog](CHANGELOG.md)** - Version history and release notes
- **📋 [Weather Display](docs/weather.md)** - Weather integration setup, configuration, and API usage
- **📋 [Image Processing Pipeline](docs/image_processing.md)** - Complete guide for bash/ImageMagick processing
- **📋 [Rust Image Processor](docs/rust-photoframe-processor.md)** - High-performance Rust tool with AI features
- **📋 [Binary Format Converter](docs/bin_2_image.md)** - ESP32 binary to image conversion tool
- **📋 [Wiring Diagram](docs/wiring-diagram.md)** - Hardware assembly and connections

## ⚙️ Unified Configuration System (v0.7.1)

### **Single Configuration File**

The photo frame now uses a **unified configuration system** with a single `/config.json` file on the SD card, replacing the previous 3 separate configuration files:

**Previous System (v0.7.0 and earlier)**:
- ❌ `wifi.txt` - WiFi credentials
- ❌ `google_drive_config.json` - Google Drive settings
- ❌ `weather_config.json` - Weather configuration

**New Unified System (v0.7.1+)**:
- ✅ `/config.json` - **All configuration in one place**

### **Key Benefits**

- **🔧 Simplified Setup**: All settings in one JSON file
- **⚡ Faster Startup**: Single SD card read instead of 3 separate reads
- **🛡️ Enhanced Validation**: Automatic value capping and fallback handling
- **🔄 Runtime Control**: Weather functionality controlled at runtime (no recompilation needed)
- **💾 Better Error Handling**: Extended sleep on SD card failure with graceful recovery
- **🧹 Code Simplification**: Eliminated conditional compilation blocks

### **Fallback System**

If the SD card fails or `/config.json` is missing, the system:
1. **Loads fallback values** from `config.h` preprocessor constants
2. **Enters extended sleep** (midpoint between min/max refresh intervals)
3. **Retries on next wakeup** for automatic recovery

### **Configuration Validation**

The system automatically validates and caps configuration values:
- **Refresh intervals**: Capped to valid ranges (5 min - 4 hours)
- **Weather settings**: Battery threshold (5-50%), max age (1-24 hours)
- **Board timing**: Day start/end hours validated
- **Google Drive**: Rate limiting and timeout values enforced

### **Runtime Weather Control**

Weather functionality is now **runtime-configurable**:
- **Enable/Disable**: Set `"weather.enabled": true/false` in config.json
- **No Recompilation**: Same firmware works with or without weather
- **Battery Aware**: Automatically disabled when battery is below threshold
- **Configuration Driven**: All weather settings in unified config

## Configuration & Customization

### Security Settings

The `use_insecure_tls` setting in your unified `/config.json` file controls SSL/TLS security:

- **Secure (Recommended)**: Set `"google_drive.drive.use_insecure_tls": false` and provide a valid `root_ca_path`
- **Insecure**: Set `"google_drive.drive.use_insecure_tls": true` to skip certificate validation (not recommended for production)

### Weather Configuration

Weather is now fully runtime-configurable via the unified `/config.json`:

- **Enable/Disable**: Control weather display without firmware changes
- **Location Settings**: Latitude/longitude for accurate weather data
- **Update Control**: Configurable intervals and battery thresholds
- **Units**: Temperature (Celsius/Fahrenheit), wind speed, precipitation
- **Timezone**: Auto-detection or manual specification

### Hardware Configuration

- **Board Selection**: Choose your target board in `platformio.ini` 
- **Display Type**: Configure your e-paper display type in your board's config file
- **Pin Mapping**: Adjust pin assignments in `include/config/your_board.h`

### Software Customization

- **Localization**: Add or modify language files in `include/locales/`
- **Icons**: Use scripts in `icons/` to generate new icon headers from PNG or SVG files
- **Timing**: Adjust refresh intervals and sleep timing in the configuration files
- **Battery**: Configure battery monitoring thresholds and power-saving modes


### 3D Model

A 3D model ready to print is included here <a href="assets/3d model/ESP32-Photo-Frame.3mf">/assets/3d model/ESP32-Photo-Frame.3mf</a>

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
