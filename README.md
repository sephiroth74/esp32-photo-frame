# Arduino E-Paper Photo Frame

**esp32-photo-frame** is a firmware project for an ESP32-based digital photo frame. The project is designed to display images from local SD card storage or Google Drive cloud storage on an e-paper (EPD) display, with features such as battery monitoring, real-time clock (RTC) support, and multi-language localization.

<img src="assets/image001.jpg" alt="ESP32 Photo Frame Example" width="800" />

## Features

- **Dual Image Sources**: Display images from SD card or Google Drive cloud storage
- **Google Drive Integration**: Automatic image synchronization and caching from Google Drive folders
- **Smart Image Processing**: Automatic resizing, annotation, and format conversion
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
   - **Recommended Board**: DFRobot FireBeetle 2 ESP32-C6 (built-in battery management and solar charging)
   - **Key Connections**: SPI for display and SD card, I2C for RTC, analog for battery monitoring
   - **Power Management**: Built-in JST connector for battery, screw terminals for solar panel
   - **Wakeup Button**: GPIO 3 with pull-down configuration for manual device wakeup

3. **Setup**
   - Clone this repository
   - Open the project in [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/)
   - Configure your hardware settings in `include/config.h` and `src/config.cpp` if needed

3. **Image Source Configuration**

   **Option A: Google Drive Integration (Recommended)**
   
   For cloud-based image storage and automatic synchronization:
   
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
       "list_page_size": 150,
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
   
   For offline image storage, use the `scripts/auto.sh` script to process images:

   ```bash
   # Black & white processing (800x480 display)
   ./scripts/auto.sh -i ~/Photos -o ~/processed_images -t bw -s 800x480 --extensions jpg,png --auto

   # 6-color processing
   ./scripts/auto.sh -i ~/Photos -o ~/processed_images -t 6c -s 800x480 --extensions jpg,png --auto
   ```

   **Auto.sh Script Features:**
   - **Smart Orientation**: Automatically handles portrait and landscape images
   - **Image Combining**: Merges two portrait images into landscape layout
   - **Format Support**: Processes jpg, png, heic, and other image formats
   - **Annotation**: Adds filename overlays with customizable fonts
   - **Batch Processing**: Handles entire directories efficiently
   - **Binary Output**: Generates both .bmp and optimized .bin files
   - **Color Modes**: Supports black & white and 6-color e-paper displays

4. **Binary to Image Conversion**

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

5. **Build and Upload**
   - Connect your ESP32 board
   - Use PlatformIO to build and upload the firmware

## ESP32-C6 Specific Considerations

### Known Issues and Workarounds

When using **ESP32-C6 boards** (such as DFRobot FireBeetle 2 ESP32-C6), you may encounter I2C/WiFi coexistence issues that can cause JSON parsing errors in Google Drive API responses. The symptoms include:

- JSON parsing errors with truncated responses at exactly 32KB
- HTTP response corruption with character sequences like `�����`
- `ArduinoJson` errors: `IncompleteInput` or `InvalidInput`

### **Root Cause**
ESP32-C6 has hardware-level interference between I2C operations (used for RTC communication) and WiFi operations (used for Google Drive API calls). This interference corrupts incoming WiFi data.

### **Implemented Solution**
The firmware includes comprehensive workarounds that completely isolate I2C and WiFi operations:

1. **Complete I2C Shutdown**: I2C bus is completely shut down before any WiFi operations begin
2. **WiFi Operation Isolation**: All network operations (time sync + Google Drive) happen while I2C is disabled  
3. **Delayed I2C Restart**: I2C is only restarted after WiFi is completely disconnected
4. **RTC Update Management**: Time fetched during WiFi operations is stored and applied to RTC after I2C restart

### **Alternative Solution**
For new projects, consider using **ESP32-S3 boards** (like DFRobot FireBeetle 2 ESP32-S3) which don't suffer from these coexistence issues and offer additional benefits:
- Better I2C/WiFi isolation
- More memory (PSRAM support)
- Dual-core architecture
- More stable operation

### **Code Implementation**
The workarounds are implemented in:
- `platformio/src/main.cpp`: I2C shutdown before all WiFi operations
- `platformio/src/rtc_util.cpp`: Deferred RTC updates after I2C restart
- `platformio/include/rtc_util.h`: New RTC update function declarations

## Technical Specifications

For detailed technical documentation including comprehensive error handling system, system architecture, hardware specifications, and API references, see **📋 [Technical Specifications](docs/tech_specs.md)**.

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
