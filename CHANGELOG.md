# Changelog

All notable changes to the ESP32 Photo Frame project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v0.9.2] - 2025-01-14

### Fixed
- **Binary File Rendering on B/W Displays**: Fixed white screen issue when rendering .bin files
  - Changed from `writeImage()` to `display.drawPixel()` for proper pixel rendering
  - Added fallback color mapping for all B/W display types in `color2Epd()` function
  - Binary files now render correctly on black and white e-paper displays

- **Display Debug Stack Overflow**: Fixed crash when testing multiple images
  - Limited gallery test to 10 images maximum to prevent memory exhaustion
  - Replaced `std::vector<String>` with heap-allocated array
  - Removed memory-heavy `std::mt19937` in favor of ESP32's hardware RNG
  - Added proper memory cleanup with `delete[]` at all exit points

- **Weather Overlay Display**: Fixed weather box showing as white instead of black
  - Manually draws black background with white text for testing
  - Fixed timestamp staleness check preventing weather data display
  - Weather information now properly renders with correct contrast

### Added
- **Enhanced Display Debug Features**:
  - **Floyd-Steinberg Dithering**: Added grayscale patterns for B/W displays using error diffusion
  - **B/W Stress Test Pattern**: Comprehensive stress test including stripes, checkerboard, circles
  - **Balanced File Testing**: Ensures both .bmp and .bin files are tested (5 of each type)
  - **Overlay Testing**: Optional testing of last update, image info, battery status, and weather overlays with fake data

- **Gallery Test Result Summary**: Visual summary displayed on e-paper after test completion
  - Shows total images, success/fail counts, success rate
  - Displays total test time and average time per image
  - Color-coded results on color displays (Green=PASS, Red=FAIL, Yellow/Orange=PARTIAL)
  - Improved layout with proper spacing to avoid text/line overlap

### Changed
- **Gallery Test Improvements**:
  - File selection now balances between BMP and BIN formats
  - Added randomization using lightweight Fisher-Yates shuffle
  - Enhanced reporting shows file type breakdown (BMP: X, BIN: Y)
  - Result status line moved to Y=255 for better visual spacing

### Technical Details
- **Memory Optimization**: Reduced stack usage from ~10KB to under 2KB in gallery test
- **Rendering Performance**: Binary file rendering now uses pixel-by-pixel approach for B/W displays
- **Debug Logging**: Added comprehensive logging for pixel distribution and rendering progress

## [v0.9.0] - 2025-01-09

### Added
- **PSRAM Framework Detection**: Improved PSRAM initialization with automatic framework detection
  - Uses `psramFound()` to check if PSRAM is already initialized by the framework
  - Automatic restart on PSRAM failure for mandatory boards
  - Reports both total and free PSRAM for better memory monitoring

- **Enhanced Google Drive Debugging**: Comprehensive error reporting for OAuth authentication
  - Detailed HTTP 400 error analysis with specific OAuth error detection
  - JWT timestamp validation to detect system clock issues
  - System time validation (2020-2100 range) with warning messages
  - DEBUG_GOOGLE_DRIVE flag enabled for detailed troubleshooting

### Fixed
- **WiFi Disconnect Crash**: Fixed race condition causing "Guru Meditation Error" during WiFi disconnect
  - Added proper connection state check before disconnect
  - Added delays to ensure pending WiFi operations complete
  - Prevents LoadProhibited exception in WiFi event handler

- **PSRAM Heap Integration**: Fixed "Failed to add PSRAM to heap: ESP_FAIL" error
  - Removed deprecated `esp_spiram_add_to_heapalloc()` call
  - PSRAM now automatically added to heap via CONFIG_SPIRAM_USE_MALLOC
  - Added CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY for better utilization

- **Error Reporting Preservation**: Fixed access token errors being masked by local TOC fallback
  - Added `last_error` tracking in google_drive class
  - Original OAuth errors now properly preserved and reported
  - Prevents misleading "JSON Parse Failed" error when actual issue is authentication

### Changed
- **ProS3(d) Configuration**: Enhanced board configuration with proper PSRAM flags
  - Added missing PSRAM configuration flags for reliable memory management
  - Configured for 16MB flash and 8MB PSRAM
  - Enabled proper memory type settings for ESP32-S3

### Technical Details
- **Memory Management**: Improved PSRAM handling for ESP32-S3 boards
  - Automatic PSRAM detection and configuration
  - Proper memory allocation with MALLOC_CAP_SPIRAM
  - Stack allocation for WiFiClientSecure to prevent memory corruption

- **Error Diagnostics**: Enhanced troubleshooting capabilities
  - OAuth error differentiation (invalid_grant, invalid_client, invalid_request)
  - System clock validation for JWT generation
  - Comprehensive debug output for Google Drive API operations

## [v0.8.1] - 2025-09-29

### Changed
- **RGB Status System Simplified**: Replaced state-based color changes with continuous rainbow pulsing effect
  - Unified visual experience with smooth rainbow color transitions
  - Gentle pulsing/breathing animation throughout all operations
  - More aesthetically pleasing and less distracting than color state changes
  - Power-efficient with brightness range from 10% to 25%

### Fixed
- **Critical Include Order Issue**: Fixed board-specific configuration not being loaded before validation
  - Moved board config include to top of config.h before any validation checks
  - Resolved GPIO errors and crashes on boards with RGB LED support
  - Eliminated "GPIO number error" and RMT configuration failures

### Added
- **RTC Module Class Selection**: Added configuration options for different RTC chip types
  - Support for PCF8523 and DS3231 RTC modules
  - Configurable via RTC_CLASS_PCF8523 or RTC_CLASS_DS3231 defines
  - Maintains automatic NTP fallback for time synchronization

### Technical Details
- **Power Optimization**: RTC module provides ~55% daily power savings compared to NTP-only operation
- **RGB Animation**: Smooth 120-step pulse cycle with 1024-step rainbow transition
- **Build System**: Improved configuration validation order for robust compilation

## [v0.8.0] - 2025-09-29

### Added
- **ProS3(d) Board Support**: Added configuration for Unexpected Maker ProS3(d) ESP32-S3 development board
  - New `pros3d_unexpectedmaker` environment in platformio.ini
  - Dedicated board configuration with optimal pin assignments based on ProS3(d) pinout
  - I2C battery fuel gauge support (MAX1704X on IO8/IO9)
  - Enhanced GPIO availability with ProS3(d)'s extensive pin layout
  - 5V detection and LDO2 power control support

- **PCF8523 RTC Integration**: Real-time clock support to eliminate NTP dependency
  - Adafruit PCF8523 RTC board support for accurate timekeeping
  - Shared I2C bus configuration with battery fuel gauge (IO8/IO9)
  - Automatic fallback to NTP when RTC is unavailable or invalid
  - Time validation with automatic RTC synchronization from NTP
  - Persistent time tracking during deep sleep cycles

### Changed
- **PSRAM Mandatory Requirement**: PSRAM is now required for all supported board configurations
  - Removed all `#ifdef BOARD_HAS_PSRAM` conditionals throughout codebase
  - Added compile-time validation requiring BOARD_HAS_PSRAM definition
  - Simplified memory allocation strategies assuming PSRAM availability
  - Enhanced stability and performance with guaranteed PSRAM access

- **Dynamic Buffer Sizing**: Intelligent memory management based on available flash memory
  - Automatic flash size detection (4MB/8MB/16MB+) for optimal buffer allocation
  - Scaled Google Drive buffers: 1MB/2MB/4MB JSON docs, 3MB/6MB/12MB safety limits
  - Enhanced performance for boards with larger flash memory
  - Improved memory utilization efficiency across different hardware configurations

### Fixed
- **RTC Flow Simplification**: Eliminated complex deferred update mechanism
  - Removed unused `update_rtc_after_restart()` dead code and related global variables
  - Simplified RTC time synchronization to immediate updates after NTP fetch
  - Enhanced RTC time validation with reasonable date range checks (2020-2100)
  - Improved error handling and logging for RTC initialization and time sync

### Technical Details
- **Code Architecture**: Major cleanup removing conditional compilation complexity
- **Memory Management**: Unified PSRAM utilization across all supported platforms
- **Build System**: Streamlined configuration with mandatory PSRAM requirement
- **Hardware Abstraction**: Enhanced board-specific configuration system for ProS3(d)

## [v0.7.1] - 2025-01-27

### Changed
- **Runtime Weather Configuration**: Removed `USE_WEATHER` compile-time conditional in favor of unified runtime configuration
  - Weather functionality now controlled by `systemConfig.weather.enabled` in `/config.json`
  - Eliminated complex conditional compilation blocks throughout codebase (`main.cpp`, `renderer.cpp`, `config.cpp`)
  - Single firmware binary now supports weather on/off without recompilation
  - Simplified build system by removing `-D USE_WEATHER` build flag

### Technical Details
- **Architectural Improvement**: Transitioned from compile-time to runtime weather control
- **Code Simplification**: Removed all `#ifdef USE_WEATHER` conditionals across the entire codebase
- **Build System**: Unified build process eliminates need for separate weather/no-weather builds
- **Configuration System**: Weather system now fully integrated with unified JSON configuration architecture

## [v0.7.0] - 2025-09-24

### Added
- **Optimized Partition Layout**: Eliminated factory partition and redistributed space for better utilization
  - Increased OTA partitions from 4MB to 5MB each (+25% capacity for firmware growth)
  - Expanded SPIFFS from 3.8MB to 5.84MB (+54% capacity for assets and cache)
  - Improved upload address from 0x420000 to 0x20000 for direct ota_0 boot
- **PSRAM Streaming Optimization**: Enhanced HTTP client to leverage 8MB PSRAM efficiently
  - Replaced `getString()` with `getStreamPtr()` for chunked reading into PSRAM buffers
  - Uses `heap_caps_malloc(MALLOC_CAP_SPIRAM)` for direct PSRAM allocation
  - Automatic fallback to regular heap if PSRAM unavailable

### Fixed
- **Weather Sunrise/Sunset Times**: Fixed incorrect timezone handling causing 1-hour offset
  - Added `&timeformat=unixtime` parameter to Open-Meteo API requests
  - Eliminated complex ISO8601 parsing in favor of direct Unix timestamp reading
  - Sunrise/sunset times now display correct local times (e.g., 07:17 instead of 08:17)
- **Memory Management**: Fixed critical memory corruption crashes in HTTPClient usage
  - Replaced heap allocation with stack allocation for WiFiClientSecure objects
  - Eliminated use-after-free bugs that caused "Guru Meditation Error: InstrFetchProhibited"
- **FeatherS3 PSRAM Support**: Fixed PSRAM initialization failures on ESP32-S3 boards
  - Corrected memory type from `qio_opi` (Octal SPI) to `qio_qspi` (Quad SPI)
  - Properly configured for FeatherS3's 8MB Quad SPI PSRAM hardware
  - Eliminated "opi psram: PSRAM ID read error" initialization failures

### Changed
- **Simplified Cleanup Logic**: Completely refactored `cleanup_temporary_files` function
  - Eliminated complex O(n×m) orphaned file detection algorithm
  - Implemented clear decision tree: force cleanup, low space cleanup, or TOC maintenance
  - Reduced function complexity from 280+ lines to ~170 lines (39% reduction)
  - Improved performance and predictability with straightforward cleanup strategies
- **Memory Buffer Optimization**: Balanced performance and memory usage
  - Reduced JSON buffers from 2MB to 512KB - sufficient for large file lists
  - Optimized body reserve from 2MB to 256KB - adequate for API responses
  - Adjusted safety limits to 1MB - prevents memory fragmentation while maintaining functionality

### Technical Details
- **HTTP Client Migration**: Enhanced from manual HTTP parsing to HTTPClient with PSRAM streaming
- **Memory Management**: Optimized PSRAM utilization while preventing memory fragmentation
- **Storage Optimization**: New partition layout provides 4MB additional usable space
- **Error Recovery**: Improved robustness with better memory management and cleanup logic
- **Weather API**: Streamlined timezone handling eliminates parsing complexity and accuracy issues

## [v0.6.1] - 2025-09-23

### Fixed
- **Google Drive HTTP Parsing**: Fixed critical chunked HTTP response parsing corruption that caused JSON data corruption
  - Replaced manual chunked transfer encoding parsing with HTTPClient automatic handling
  - Eliminated filename truncation and data merging issues in Google Drive API responses
  - Improved reliability of OAuth token requests and file list operations
- **DateTime Formatting**: Fixed Italian locale datetime display in last update status
  - Changed from printf-style formatting to proper strftime format specifiers
  - Format now correctly displays "Lun, 1 Gen 2023 12:00" using `%a, %e %b %Y %H:%M`
  - Fixed format string inconsistencies between DateTime and tm struct formatting
- **FeatherS3 PSRAM Configuration**: Fixed PSRAM initialization failures on FeatherS3 boards
  - Changed memory type from `qio_opi` (Octal SPI) to `qio_qspi` (Quad SPI) for proper PSRAM support
  - Corrected PSRAM type configuration to match FeatherS3's 8MB Quad SPI PSRAM hardware
  - Eliminated "opi psram: PSRAM ID read error" initialization failures

### Changed
- **Google Drive Buffer Optimization**: Optimized memory buffer sizes for better performance
  - JSON document buffer: 512KB (was 2MB) - sufficient for large file lists
  - Body reserve buffer: 256KB (was 2MB) - adequate for API responses
  - Safety limit: 1MB (was 2MB) - reasonable protection without memory pressure
  - Stream parser threshold: 512KB (was 2MB) - balanced performance and memory usage

### Technical Details
- **HTTP Client Migration**: Transitioned from manual HTTP parsing to HTTPClient class for automatic chunked encoding support
- **PSRAM Streaming**: Replaced HTTPClient.getString() with getStreamPtr() and chunked reading into PSRAM-allocated buffers
  - Uses `heap_caps_malloc(MALLOC_CAP_SPIRAM)` to allocate response buffers directly in PSRAM
  - Reads HTTP responses in 1KB-4KB chunks to prevent memory pressure
  - Automatic fallback to regular heap if PSRAM allocation fails
- **Memory Management**: Optimized PSRAM utilization while preventing memory fragmentation from excessive buffer allocation
- **Locale Support**: Enhanced strftime formatting ensures proper Italian weekday and month name display
- **Hardware Compatibility**: Corrected ESP32-S3 PSRAM interface configuration for reliable memory expansion

## [v0.5.0] - 2025-01-21

### Added
- **RGB Status System**: Complete visual feedback system using FeatherS3 built-in NeoPixel LED
  - 🔷 **Starting**: White pulse during system startup
  - 🔵 **WiFi Connecting**: Blue pulse while connecting to WiFi
  - 🔴 **WiFi Failed**: Red slow blink when connection fails
  - 🟢 **Weather Fetching**: Green pulse while fetching weather data
  - 🟠 **SD Operations**: Orange pulse during SD card operations
  - 🟡 **SD Writing**: Yellow pulse during SD card writes
  - 🔵 **Google Drive**: Cyan pulse during Google Drive operations
  - 🟣 **Downloading**: Purple pulse during file downloads
  - 🟡 **Rendering**: Pink solid during display rendering
  - 🔴 **Battery Low**: Red slow blink for critical battery
  - 🔴 **Error**: Red fast blink for system errors
  - ⚪ **Sleep Prep**: Dim white fade-out before deep sleep
- **FreeRTOS Task Support**: RGB system runs in dedicated thread for smooth, non-blocking animations
- **Battery-Aware Power Management**: Automatic brightness reduction and RGB disable for power conservation
- **Power-Efficient Design**: Optimized brightness levels (max 96) and complete shutdown during sleep

### Fixed
- **Deep Sleep Wakeup**: Fixed critical EXT0 wakeup issues on ESP32-S3 by using proper RTC GPIO pins
  - Moved WAKEUP_PIN from GPIO8 (non-RTC) to GPIO1 (RTC-capable)
  - Added proper RTC GPIO initialization with pull-up/pull-down configuration
  - Fixed pin state validation and error reporting
- **FeatherS3 Pin Configuration**: Corrected all pin assignments based on actual hardware pinout
  - **SD_MMC Pins**: CLK=GPIO14, CMD=GPIO17, D0=GPIO7, D1=GPIO3, D2=GPIO12, D3=GPIO11
  - **E-Paper Display**: Optimized SPI pin assignments to avoid conflicts
  - **Battery Monitoring**: Confirmed GPIO2 for built-in voltage divider
  - **RGB NeoPixel**: GPIO40 with power control on GPIO39 (LDO2)
- **HTTP Download Reliability**: Added explicit buffer flushing to prevent file corruption during Google Drive downloads
- **Board Definition**: Updated to use proper FeatherS3 board definition (`um_feathers3`) instead of generic ESP32-S3

### Changed
- **Default Board**: Changed default environment to `feathers3_unexpectedmaker` for better ESP32-S3 support
- **Partition Table**: Optimized 16MB partition layout for FeatherS3 flash memory
- **Power Management**: Enhanced battery conservation with RGB system integration
- **Pin Assignments**: Systematic reorganization of FeatherS3 pins to eliminate conflicts

### Technical Details
- **RGB System Architecture**: NeoPixel control with 14 predefined states and custom color support
- **Memory Efficiency**: RGB task uses only 2KB stack with 50Hz update rate
- **Hardware Integration**: Full FeatherS3 support with built-in peripherals (NeoPixel, battery monitoring)
- **Deep Sleep Optimization**: Proper GPIO hold disable and RTC configuration for reliable wakeup
- **Error Handling**: Comprehensive pin validation and RTC capability testing

## [v0.4.1] - 2025-01-18

### Fixed
- **File Extension Detection Bug**: Fixed critical bug where runtime file format detection failed when using LittleFS temporary files
- **Enhanced Filename Handling**: System now preserves original filename for format detection even when files are copied to LittleFS as "temp_image.tmp"
- **Improved Error Reporting**: Error messages now display meaningful original filenames instead of generic temporary file names
- **Robust Format Selection**: Rendering engine selection now works correctly for both .bin and .bmp files regardless of temporary file storage

### Technical Details
- **Function Signature Updates**: Added `original_filename` parameter to `handle_google_drive_operations()`, `process_image_file()`, and `render_image()` functions
- **Format Detection Logic**: Modified format detection to use original filename extension instead of temporary file extension
- **Memory Efficiency**: Maintains existing LittleFS temporary file approach while fixing format detection accuracy
- **Error Context**: Improved error messages by using original filenames in renderer error reporting

## [v0.4.0] - 2025-01-17

### Added
- **Runtime File Format Detection**: ESP32 firmware now automatically detects file format based on extension (.bin or .bmp)
- **Mixed File Support**: Both binary and bitmap files can coexist in the same directory/Google Drive folder
- **Smart Rendering Engine Selection**: Automatically selects appropriate rendering function based on detected file format
- **New File Extension Constants**: Added `ALLOWED_FILE_EXTENSIONS[]` array supporting both `.bin` and `.bmp` formats

### Changed
- **Eliminated Compile-Time Configuration**: Removed `EPD_USE_BINARY_FILE` and `LOCAL_FILE_EXTENSION` compile-time dependencies
- **Updated Google Drive Filtering**: File filtering now accepts both `.bin` and `.bmp` extensions at runtime
- **Modernized SD Card Operations**: `list_files()` and `count_files()` methods now work with allowed extensions array
- **Enhanced File Validation**: Updated validation logic to accept multiple file formats dynamically

### Removed
- **EPD_USE_BINARY_FILE**: Removed compile-time flag from all board configurations
- **LOCAL_FILE_EXTENSION**: Replaced single extension constant with runtime extension array
- **Board-Specific Format Constraints**: No longer need to configure file format per board

### Technical Details
- **New Function**: `photo_frame::io_utils::is_binary_format()` for runtime format detection
- **Updated APIs**: SD card and Google Drive file operations now use extension arrays
- **Improved Flexibility**: Same firmware binary can handle both binary and bitmap files without recompilation
- **Backwards Compatibility**: Existing configurations continue to work without changes

## [v0.3.0] - 2025-09-15

### Added
- Granular error codes for different image rendering functions (codes 100-109)
- New `draw_error_with_details()` function that displays filename and specific error codes
- Function-specific error reporting for `draw_binary_from_file`, `draw_bitmap_from_file`, and buffered variants
- Enhanced error identification system with specific codes for pixel read errors, seek failures, and header corruption
- Detailed error logging with timestamps and context information

### Changed
- **Error Display**: Removed error icons and repositioned error text higher on screen for better visibility
- **Error Messages**: Added small detail line showing filename and error code below main error text
- **Status Bar**: Status elements (time, battery, image info) now always render regardless of image rendering success
- **Error Handling Flow**: Rendering errors now break out of main loop and display in separate dedicated loop
- **Performance**: Status elements moved out of main rendering loop - now drawn only once instead of per-page

### Fixed
- Error messages no longer cut off on last page due to `display.nextPage()` loop boundaries
- Status information always visible even when image rendering fails
- More specific error identification prevents generic "ImageFormatNotSupported" for different failure types
- Weather information only displays after successful image rendering (partial update mode)

### Technical Details
- Error codes 100-109 reserved for rendering-specific errors (vs general image processing errors 80-99)
- Status bar uses partial window updates for efficiency after main image rendering
- Separate error display loop ensures complete error visibility across all display pages
- Error code propagation system allows detailed error reporting up the call stack

## [v0.2.0] - 2025-09-13

### Added
- Version constants in `config.h` for proper release tracking
- Page-aware image rendering system for multi-page e-paper displays
- Enhanced Google Drive TOC system with separate data and metadata files
- Comprehensive error handling system with detailed logging
- Debug mode support with conditional compilation flags

### Changed
- **Performance**: Image rendering optimized from 81+ seconds to ~27 seconds (3x improvement)
- **Memory Management**: Improved pixel processing efficiency - only processes relevant pixels per page
- **Google Drive API**: All methods now consistently accept `sd_card&` parameter for better testability
- **File Operations**: Enhanced streaming architecture eliminates intermediate storage requirements
- **Caching System**: Smart cache management with orphaned file cleanup and space monitoring

### Fixed
- Multi-page display rendering now correctly displays complete images instead of repeated segments
- Google Drive TOC integrity verification prevents corruption-related failures
- Coordinate system uses absolute positioning with automatic GxEPD2 page clipping

### Technical Details
- Rendering now processes ~128,000 pixels per page instead of 384,000 pixels repeatedly
- TOC system uses `toc_data.txt` (file entries) and `toc_meta.txt` (metadata) for data integrity
- Page boundaries calculated dynamically based on display specifications (e.g., 163px height sections)

## [v0.1.0] - 2024-12-01

### Added
- Initial release of ESP32 Photo Frame firmware
- Google Drive integration for cloud image storage
- Multi-board support (ESP32, ESP32-C6, ESP32-S3)
- E-paper display support (BW, 6-color, 7-color)
- Weather display integration
- Battery management and monitoring
- Deep sleep power management
- SD card caching system
- WiFi connectivity with NTP time synchronization
- Image processing pipeline support

### Features
- Automatic image selection from Google Drive folders
- Local SD card caching for offline operation
- Status overlay with time, battery, and weather information
- Configurable refresh intervals with potentiometer control
- Multi-platform image processing tools (Shell scripts, Rust, Android)

[v0.4.1]: https://github.com/sephiroth74/arduino/compare/v0.4.0...v0.4.1
[v0.4.0]: https://github.com/sephiroth74/arduino/compare/v0.3.0...v0.4.0
[v0.3.0]: https://github.com/sephiroth74/arduino/compare/v0.2.0...v0.3.0
[v0.2.0]: https://github.com/sephiroth74/arduino/compare/v0.1.0...v0.2.0
[v0.1.0]: https://github.com/sephiroth74/arduino/releases/tag/v0.1.0