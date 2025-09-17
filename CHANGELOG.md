# Changelog

All notable changes to the ESP32 Photo Frame project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[v0.3.0]: https://github.com/sephiroth74/arduino/compare/v0.2.0...v0.3.0
[v0.2.0]: https://github.com/sephiroth74/arduino/compare/v0.1.0...v0.2.0
[v0.1.0]: https://github.com/sephiroth74/arduino/releases/tag/v0.1.0