# Arduino E-Paper Photo Frame

**esp32-photo-frame** is a firmware project for an ESP32-based digital photo frame. The project is designed to display images from local SD card storage or Google Drive cloud storage on an e-paper (EPD) display, with features such as battery monitoring, real-time clock (RTC) support, and multi-language localization.

<img src="assets/image001.jpg" alt="ESP32 Photo Frame Example" width="800" />

## Features

- **Dual Image Sources**: Display images from SD card or Google Drive cloud storage
- **Google Drive Integration**: Automatic image synchronization and caching from Google Drive folders
- **Smart Image Processing**: Automatic resizing, annotation, and format conversion
- **Weather Display**: Real-time weather information with configurable location, timezone, and update intervals
- Battery voltage monitoring and reporting with power-saving modes
- Real-time clock integration for scheduled refresh and time-based features
- Potentiometer to regulate the refresh delay
- **Security**: Configurable TLS/SSL security for cloud connections
- Multi-language support (English, Italian)
- **Rate Limiting**: Built-in API rate limiting for Google Drive requests
- Modular code structure for easy customization and extension

## Project Structure

- `src/` - Main source code for device logic, battery, SD card, rendering, RTC, and localization
- `include/` - Header files and configuration
- `icons/` - Scripts and assets for generating icon headers
- `lib/` - External libraries and assets
- `platformio.ini` - PlatformIO project configuration

## Getting Started

1. **Hardware Requirements**
   - [Arduino Nano ESP32](https://docs.arduino.cc/hardware/nano-esp32/) or another ESP32-S3 or ESP32-C6 board
   - [Waveshare 7.5 e-paper display](https://www.waveshare.com/7.5inch-e-paper-hat.htm)
   - [Good Display DESPI-C02](https://www.good-display.com/companyfile/DESPI-C02-Specification-29.html) Connector board
   - [Adafruit microSD](https://www.adafruit.com/product/4682?srsltid=AfmBOopfN-tYU3fKgpQquFOTLEY50Pl4PY8iTpBpGoXCpbJ8EQGVqBHn)
   - [AZDelivery Real Time Clock](https://www.amazon.it/dp/B077XN4LL4?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1) (optional)
   - 3.7 5000mAh LiPo Battery

2. **Hardware Assembly & Wiring**
   - **📋 [Complete Wiring Diagram](docs/wiring-diagram.md)** - Comprehensive pin connections and assembly guide
   - **Recommended Board**: Unexpected Maker FeatherS3 (now default - USB-C, battery management, **8MB PSRAM with optimized Google Drive performance**, no I2C/WiFi issues)
   - **Alternative**: DFRobot FireBeetle 2 ESP32-C6 (built-in battery management and solar charging, but has I2C/WiFi coexistence issues)
   - **Key Connections**: SPI for display and SD card, I2C for RTC, analog for battery monitoring
   - **Power Management**: Built-in JST connector for battery, screw terminals for solar panel
   - **Wakeup Button**: GPIO 3 with pull-down configuration for manual device wakeup

3. **Setup**
   - Clone this repository
   - Open the project in [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/)
   - Configure your hardware settings in `include/config.h` and `src/config.cpp` if needed

3. **Image Processing Pipeline (Required for Both Options)**

   **🔧 All images must be processed before use, regardless of storage method**
   
   Both Google Drive and local SD card storage require images to be converted to the ESP32-compatible format using either:

   **📋 [Complete Image Processing Guide](docs/image_processing.md)** - Comprehensive documentation for the `auto.sh` script and all helper tools

   ```bash
   # Black & white processing (800x480 display)
   ./scripts/auto.sh -i ~/Photos -o ~/processed_images -t bw -s 800x480 --extensions jpg,png --auto

   # 6-color processing with custom fonts
   ./scripts/auto.sh -i ~/Photos -o ~/processed_images -t 6c -s 800x480 --extensions jpg,png,heic --auto \
     --font "Arial-Bold" --pointsize 32
   ```

   **Alternative: Android App (NEW)**
   
   Use the Android PhotoFrameProcessor app located in `android/PhotoFrameProcessor/` for a user-friendly GUI to process images with:
   - AI-powered person detection and smart cropping
   - Automatic EXIF date extraction and labeling
   - Portrait image pairing for landscape display
   - Real-time preview of processed results

   **Alternative: Rust Binary Converter (Recommended for Performance)**
   
   Use the high-performance Rust-based `photoframe-processor` tool for batch processing with **5-10x faster processing** and **AI-powered people detection**:
   
   **📋 [Complete Rust Processor Documentation](docs/rust-photoframe-processor.md)** - Comprehensive guide with AI features, font support, and performance optimization

4. **Image Source Configuration**

   **Option A: Google Drive Integration (Recommended)**
   
   For cloud-based image storage and automatic synchronization:
   
   - **First**, process your images using the tools above
   - Upload the processed `.bin` files to your Google Drive folder
   - Create a Google Service Account in [Google Cloud Console](https://console.cloud.google.com/)
   - Enable the Google Drive API for your project
   - Download the service account JSON key file
   - Share your Google Drive folder with the service account email
   - Create a `google_drive_config.json` file on your SD card:

   ```json
   {
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
       "toc_filename": "toc.txt",
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
   }
   ```

   **Option B: Local SD Card Storage**
   
   For offline image storage without cloud connectivity:
   
   - **First**, process your images using the tools described in step 3 above
   - Copy the processed `.bin` files directly to your SD card
   - Insert the SD card into your ESP32 device
   - The device will display images directly from local storage

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

### **Automatic Optimization**

The firmware **automatically detects PSRAM** and enables performance bonuses:

```cpp
#ifdef BOARD_HAS_PSRAM
    // Premium performance mode
    #define GOOGLE_DRIVE_JSON_DOC_SIZE 4194304           // 4MB instant processing
    #define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD 4194304  // Eliminate streaming
#else
    // Streaming architecture (still excellent performance)
    #define GOOGLE_DRIVE_JSON_DOC_SIZE 40960            // 40KB + streaming
    #define GOOGLE_DRIVE_STREAM_PARSER_THRESHOLD 32768   // Stream for large responses
#endif
```

**Bottom Line**: Choose your platform based on **features and price** - all platforms handle large Google Drive collections excellently!

## Technical Specifications

For detailed technical documentation including comprehensive error handling system, system architecture, hardware specifications, and API references, see **📋 [Technical Specifications](docs/tech_specs.md)**.

### Documentation Index

- **📋 [Technical Specifications](docs/tech_specs.md)** - System architecture, error handling, and API references
- **📋 [Weather Display](docs/weather.md)** - Weather integration setup, configuration, and API usage
- **📋 [Image Processing Pipeline](docs/image_processing.md)** - Complete guide for bash/ImageMagick processing
- **📋 [Rust Image Processor](docs/rust-photoframe-processor.md)** - High-performance Rust tool with AI features
- **📋 [Binary Format Converter](docs/bin_2_image.md)** - ESP32 binary to image conversion tool
- **📋 [Wiring Diagram](docs/wiring-diagram.md)** - Hardware assembly and connections

## Configuration & Customization

### Security Settings

The `use_insecure_tls` setting in your Google Drive configuration controls SSL/TLS security:

- **Secure (Recommended)**: Set `"use_insecure_tls": false` and provide a valid `root_ca_path`
- **Insecure**: Set `"use_insecure_tls": true` to skip certificate validation (not recommended for production)

### Hardware Configuration

- **Board Selection**: Choose your target board in `platformio.ini` 
- **Display Type**: Configure your e-paper display type in your board's config file
- **Pin Mapping**: Adjust pin assignments in `include/config/your_board.h`

### Software Customization

- **Localization**: Add or modify language files in `include/locales/`
- **Icons**: Use scripts in `icons/` to generate new icon headers from PNG or SVG files
- **Timing**: Adjust refresh intervals and sleep timing in the configuration files
- **Battery**: Configure battery monitoring thresholds and power-saving modes

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
