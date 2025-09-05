# ESP32 Photo Frame Project - Technical Specifications

This document describes the complete flow and architecture of the ESP32 Photo Frame project, detailing each step from image preparation to display on the e-paper screen.

## Project Overview

The ESP32 Photo Frame is a battery-powered digital photo frame that displays images on an e-paper display. It supports two main image sources:
- **Local SD Card**: Images processed and stored locally
- **Google Drive**: Cloud-based image storage with local caching

## Architecture Components

### 1. Hardware Components

#### ESP32 Microcontroller Options
- **Arduino Nano ESP32** (ESP32-S3)
- **Seeed Studio XIAO ESP32C6**
- **DFRobot FireBeetle 2 ESP32-C6** (default)
- **Waveshare ESP32-S3-Zero**

#### Display
- **7.5" E-Paper Display** (Waveshare or Good Display DESPI-C02)
- **Resolution**: 800x480 pixels
- **Colors**: Black & White (BW_V2), 6-color (6C), or 7-color (7C_F)
- **Interface**: SPI connection

#### Storage & Sensors
- **MicroSD Card**: Local image storage and configuration
- **Battery Monitoring**: Analog voltage divider or MAX1704X sensor
- **RTC Module**: DS3231 Real-Time Clock (optional)
- **Potentiometer**: Manual refresh rate control

### 2. Software Architecture

#### ESP32 Firmware (PlatformIO/Arduino)
- **Location**: `platformio/`
- **Main Entry**: `src/main.cpp`
- **Framework**: Arduino Framework
- **Key Libraries**: GxEPD2, ArduinoJson, RTClib, Adafruit MAX1704X

#### Android Companion App
- **Location**: `android/PhotoFrameProcessor/`
- **Language**: Kotlin
- **Purpose**: Image processing and conversion to e-paper format
- **Features**: Person detection, smart cropping, binary conversion

#### Image Processing Scripts
- **Location**: `scripts/`
- **Main Script**: `auto.sh`
- **Purpose**: Batch image processing for SD card workflow

## Complete Project Flow

### Phase 1: Hardware Setup & Configuration

#### 1.1 Hardware Assembly
1. **Connect ESP32 to E-Paper Display**
   - SPI pins configuration (CS, DC, RST, BUSY)
   - Power connections (3.3V, GND)
   - Pin mapping defined in `platformio/include/config/{board}.h`

2. **Connect Additional Components**
   - SD card module (SPI interface)
   - RTC module (I2C interface) - optional
   - Battery monitoring circuit
   - Potentiometer for refresh control

3. **Board-Specific Pin Configurations**
   - **DFRobot FireBeetle 2 ESP32-C6** (default):
     ```
     SD Card: CS=9, MISO=21, MOSI=22, SCK=23
     E-Paper: CS=1, DC=3, RST=4, BUSY=5
     RTC: SDA=6, SCL=7
     Battery: Pin=0, Potentiometer: Pin=2
     ```

#### 1.2 Firmware Configuration
1. **Select Target Board** in `platformio.ini`:
   ```ini
   default_envs = dfrobot_firebeetle2_esp32c6
   ```

2. **Configure Hardware Settings** in `platformio/include/config/{board}.h`:
   - Pin assignments
   - Display type (DISP_BW_V2, DISP_6C, DISP_7C_F)
   - Driver selection (USE_DESPI_DRIVER or USE_WAVESHARE_DRIVER)
   - Image format (EPD_USE_BINARY_FILE)
   - Battery monitoring settings
   - WiFi and time settings

### Phase 2: Image Source Configuration

#### Option A: Google Drive Integration (Cloud-Based)

#### 2A.1 Google Cloud Setup
1. **Create Google Service Account**:
   - Go to [Google Cloud Console](https://console.cloud.google.com/)
   - Create new project or select existing
   - Enable Google Drive API
   - Create Service Account credentials
   - Download JSON key file

2. **Share Google Drive Folder**:
   - Create folder in Google Drive
   - Share with service account email
   - Note the folder ID from URL

#### 2A.2 Device Configuration
1. **Enable Google Drive** in board config:
   ```cpp
   #define USE_GOOGLE_DRIVE
   #define GOOGLE_DRIVE_CONFIG_FILEPATH "/google_drive_config.json"
   ```

2. **Create Configuration File** on SD card (`/google_drive_config.json`):
   ```json
   {
     "authentication": {
       "service_account_email": "your-service-account@project.iam.gserviceaccount.com",
       "private_key_pem": "-----BEGIN PRIVATE KEY-----\nYOUR_PRIVATE_KEY\n-----END PRIVATE KEY-----\n",
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

#### Option B: Local SD Card Storage

#### 2B.1 Image Processing with Scripts
1. **Process Images** using `scripts/auto.sh`:
   ```bash
   # Black & white processing (800x480 display)
   ./scripts/auto.sh -i ~/Photos -o ~/processed_images -t bw -s 800x480 --extensions jpg,png --auto
   
   # 6-color processing
   ./scripts/auto.sh -i ~/Photos -o ~/processed_images -t 6c -s 800x480 --extensions jpg,png --auto
   ```

2. **Script Features**:
   - **Smart Orientation**: Handles portrait/landscape automatically
   - **Image Combining**: Merges two portraits into landscape
   - **Format Support**: JPG, PNG, HEIC, and other formats
   - **Annotation**: Adds filename overlays
   - **Binary Output**: Generates .bmp and .bin files
   - **Color Modes**: BW and 6-color e-paper support

#### 2B.2 Android App Processing
1. **Install Android App**: Build and install `android/PhotoFrameProcessor/`

2. **Process Images**:
   - Select photos from gallery or share from other apps
   - App analyzes images for orientation and person detection
   - **Portrait Images**: Automatically paired and combined side-by-side
   - **Landscape Images**: Smart-cropped to focus on detected persons
   - **Person Detection**: Uses MobileNet model for intelligent cropping

3. **Generated Files**:
   - **Binary Files** (`.bin`): 8-bit color format for ESP32
   - **Preview Files** (`.bmp`): Visual previews for verification
   - **File Naming**: Based on original filenames, sanitized for filesystem

4. **File Format Details**:
   - **Target Resolution**: 800x480 pixels
   - **Color Format**: 8-bit RRRGGGBB (3-3-2 bits)
   - **Processing**: Grayscale conversion + Floyd-Steinberg dithering
   - **Output**: Raw binary data matching ESP32 expectations

### Phase 3: Device Configuration

#### 3.1 SD Card Setup
1. **Required Files**:
   ```
   /wifi.txt              # WiFi credentials (SSID:PASSWORD)
   /google_drive_config.json  # Google Drive config (if using cloud)
   /certs/google_root_ca.pem  # SSL certificate (if using secure TLS)
   /bin-bw/               # Binary image files directory
   /gdrive/cache/         # Google Drive cache directory
   /gdrive/temp/          # Google Drive temporary files
   ```

2. **WiFi Configuration** (`/wifi.txt`):
   ```
   YourWiFiSSID:YourWiFiPassword
   ```

#### 3.2 Firmware Upload
1. **Build and Upload**:
   ```bash
   # Using PlatformIO CLI
   pio run -t upload
   
   # Or using VSCode with PlatformIO extension
   # Ctrl+Shift+P -> PlatformIO: Upload
   ```

### Phase 4: Runtime Operation Flow

#### 4.1 Device Startup Sequence (`main.cpp:setup()`)

1. **Hardware Initialization** (Lines 71-100):
   ```cpp
   Serial.begin(115200);
   Wire.begin();  // I2C for RTC and battery sensor
   pinMode(LED_BUILTIN, OUTPUT);  // Status LED
   ```

2. **Wakeup Reason Detection** (Lines 102-135):
   - `ESP_SLEEP_WAKEUP_UNDEFINED`: First boot or reset
   - `ESP_SLEEP_WAKEUP_TIMER`: Scheduled wakeup
   - `ESP_SLEEP_WAKEUP_EXT1`: External wakeup (button)

3. **Battery Level Check** (Lines 142-163):
   ```cpp
   battery_reader.init();
   battery_info_t battery_info = battery_reader.read();
   
   if (battery_info.is_empty()) {
     // Enter deep sleep to preserve battery
     enter_deep_sleep();
   }
   ```

4. **Time Synchronization** (Lines 167-210):
   - Initialize WiFi manager
   - Connect to configured network
   - Fetch current time from NTP servers
   - Update RTC if available
   - Handle timezone conversion

#### 4.2 Image Selection Process

##### Google Drive Workflow (Lines 234-381):
1. **Initialize Google Drive Client**:
   ```cpp
   error = drive.initialize_from_json(sdCard, GOOGLE_DRIVE_CONFIG_FILEPATH);
   ```

2. **Cleanup Management**:
   - Remove temporary files from previous sessions
   - Check cleanup interval (default: 24 hours)
   - Manage disk space on SD card

3. **Table of Contents (TOC) Management**:
   ```cpp
   drive.retrieve_toc(batteryConservationMode);
   total_files = drive.get_toc_file_count(&tocError);
   ```

4. **Random Image Selection**:
   ```cpp
   image_index = random(0, total_files);
   google_drive_file selectedFile = drive.get_toc_file_by_index(image_index, &tocError);
   ```

5. **Smart Caching**:
   - Check if file exists in local cache
   - Download if not cached or in battery conservation mode
   - Set image source flag (local cache vs cloud)

##### Local SD Card Workflow (Lines 383-444):
1. **TOC File Management**:
   ```cpp
   if (write_toc || !sdCard.file_exists(TOC_FILENAME)) {
     error = sdCard.write_images_toc(&total_files, TOC_FILENAME, SD_DIRNAME, LOCAL_FILE_EXTENSION);
   }
   ```

2. **Random Selection**:
   ```cpp
   image_index = random(0, total_files);
   String image_file_path = sdCard.get_toc_file_path(image_index);
   file = sdCard.open(image_file_path.c_str(), FILE_READ);
   ```

#### 4.3 Display Update Process (Lines 464-574)

1. **E-Paper Initialization**:
   ```cpp
   photo_frame::renderer::init_display();
   display.clearScreen();
   ```

2. **Error Handling**:
   - Display error messages for failed operations
   - Show battery critical warnings
   - Display network connectivity issues

3. **Image Rendering**:
   ```cpp
   // For non-partial update displays
   display.firstPage();
   do {
     draw_bitmap_from_file(file, filename, 0, 0, false);
     draw_last_update(now, refresh_delay.refresh_seconds);
     draw_image_info(image_index, total_files, image_source);
     draw_battery_status(battery_info);
   } while (display.nextPage());
   ```

4. **Status Bar Information**:
   - Last update timestamp
   - Image index and total count
   - Image source indicator (local/cloud)
   - Battery level and charging status

#### 4.4 Power Management (Lines 588-607)

1. **Refresh Interval Calculation**:
   ```cpp
   refresh_delay_t calculate_wakeup_delay(battery_info_t& battery_info, DateTime& now) {
     // Base interval from potentiometer reading
     long refresh_seconds = read_refresh_seconds(battery_info.is_low());
     
     // Adjust for day/night schedule
     if (nextRefresh > dayEnd) {
       // Schedule for next day start
       nextRefresh = DateTime(tomorrow.year(), tomorrow.month(), tomorrow.day(), DAY_START_HOUR, 0, 0);
     }
     
     // Convert to microseconds for ESP32 deep sleep
     refresh_microseconds = (uint64_t)refresh_seconds * MICROSECONDS_IN_SECOND;
   }
   ```

2. **Deep Sleep Entry**:
   ```cpp
   esp_sleep_enable_timer_wakeup(refresh_microseconds);
   esp_deep_sleep_start();
   ```

### Phase 5: Power Management & Scheduling

#### 5.1 Battery Management
- **Monitoring**: Continuous voltage reading or MAX1704X sensor
- **Thresholds**:
  - Empty: <5% (enter deep sleep)
  - Critical: <10% (extended refresh intervals)
  - Low: <25% (reduced features)
  - Charging: >4.3V (normal operation)

#### 5.2 Refresh Scheduling
- **Base Interval**: Potentiometer-controlled (10 minutes to 4 hours)
- **Low Battery Mode**: Extended to 8 hours
- **Day/Night Schedule**: 
  - Active: 6 AM to 11 PM
  - Inactive: 11 PM to 6 AM (sleep until morning)
- **Maximum Sleep**: 24 hours (ESP32 hardware limit)

#### 5.3 Network Management
- **Connection**: Only when needed (time sync, Google Drive)
- **Timeout**: 10 seconds for WiFi, 15-30 seconds for HTTP
- **Disconnect**: Immediate after use to save power
- **Error Handling**: Graceful fallback to cached content

### Phase 6: Error Handling & Recovery

#### 6.1 Common Error Scenarios
1. **SD Card Failures**: Display error, continue with cached content
2. **Network Issues**: Use cached images, extend refresh interval
3. **Battery Critical**: Display warning, enter power-saving mode
4. **Image Processing**: Skip corrupted files, select next image
5. **Google Drive API Limits**: Implement rate limiting and backoff

#### 6.2 Recovery Mechanisms
- **Watchdog Timer**: Automatic reset on system hang
- **Error Display**: Visual feedback on e-paper screen
- **LED Indicators**: Blink patterns for different error types
- **Graceful Degradation**: Continue operation with reduced features

### Phase 7: Configuration & Customization

#### 7.1 Hardware Variations
- **Board Selection**: Multiple ESP32 variants supported
- **Display Types**: BW, 6-color, 7-color e-paper displays
- **Drivers**: Waveshare or DESPI driver selection
- **Sensors**: Optional RTC, battery sensor configurations

#### 7.2 Software Customization
- **Localization**: Multi-language support (English, Italian)
- **Fonts**: Configurable display fonts
- **Icons**: Custom icon generation from SVG/PNG
- **Timing**: Adjustable refresh intervals and schedules
- **Image Processing**: Configurable quality and color settings

## File Structure Summary

```
esp32-photo-frame/
├── platformio/                    # ESP32/Arduino firmware
│   ├── src/main.cpp               # Main application logic
│   ├── src/                       # Core modules (battery, wifi, display, etc.)
│   ├── include/config/            # Board-specific configurations
│   └── platformio.ini             # Build configuration
├── android/PhotoFrameProcessor/   # Android companion app
│   └── app/src/main/             # Kotlin source code
├── scripts/                       # Image processing scripts
│   └── auto.sh                   # Main batch processing script
├── README.md                     # Project documentation
└── specs.md                      # This technical specification
```

## Power Consumption Estimates

- **Active Operation**: ~150-200mA (WiFi, processing, display update)
- **Deep Sleep**: ~10-50µA (RTC running, external wakeup enabled)
- **Display Update**: ~20-40mA for 2-10 seconds
- **Battery Life**: 2-6 months with 5000mAh battery (depending on refresh rate)

## Performance Characteristics

- **Boot Time**: 2-5 seconds from deep sleep
- **Image Processing**: 10-30 seconds per image (Android app)
- **Display Update**: 5-15 seconds (depending on display type)
- **Google Drive Sync**: 30-120 seconds (depending on image count)
- **Network Connection**: 5-10 seconds (WiFi + NTP)

This specification covers the complete end-to-end flow of the ESP32 Photo Frame project, from initial hardware setup through runtime operation and maintenance.