# Changelog - Photoframe Processor (Rust)

All notable changes to the Photoframe Processor Rust tools will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - 2026-01-06

### Removed
- **Rust GUI Application** (`photoframe-processor-gui`)
  - Removed egui-based GUI in favor of new Flutter macOS application
  - Removed GUI-related dependencies: `eframe`, `egui`, `rfd`, `dirs`, `font-kit`
  - Removed `--features gui` build option
  - See `../../photoframe_flutter/` for the new native macOS GUI

### Notes
- The CLI tool (`photoframe-processor`) remains unchanged
- For GUI functionality, use the new Flutter app: `cd ../../photoframe_flutter && flutter run -d macos`
- All GUI features have been reimplemented in Flutter with native macOS UI

## [v1.1.0] - 2026-01-05

### Added
- **GUI Application** (`photoframe-processor-gui`)
  - Native cross-platform GUI built with egui/eframe
  - Real-time progress monitoring with visual progress bar next to process button
  - System font enumeration for annotation settings
  - Configuration persistence to `~/.config/photoframe-processor/gui_config.json`
  - Compact UI layout with optimized spacing:
    - Display Type and Target Orientation on same line
    - Strength and Contrast sliders on same line (250px each)
    - Dry run and Debug mode checkboxes on same line
    - Divider width and color on same line
    - Font and size selectors on same line
  - Progress section shows processing count (X/Y) and current file being processed
  - Error handling with clear error messages displayed in red
  - Build with: `cargo build --release --features gui`
  - Binary: `photoframe-processor-gui`

- **Enhanced Progress Tracking System**
  - JSON progress output mode for GUI integration (`--json-progress`)
  - Dynamic progress bar length updates after orientation analysis
  - Accurate progress counts for portrait/landscape pairing operations
  - Atomic completion flag (`Arc<AtomicBool>`) for precise progress monitoring
  - Progress monitoring thread tracks dynamic total count without timeouts
  - Correct increment logic: 1 per output file (not per input file pair)

- **Library Exports** (`src/lib.rs`)
  - Core types now exported for integration with GUI and other applications
  - Exported modules: `cli`, `image_processing`, `utils`, `json_output`
  - Re-exported types:
    - `ProcessingConfig`, `ProcessingEngine`, `ProcessingResult`
    - `ProcessingType`, `ImageType`, `SkipReason`
    - `ColorType`, `DitherMethod`, `OutputType`, `TargetOrientation`
    - `JsonMessage`

### Changed
- **Version**: Updated from v1.0.0 to v1.1.0
- **Progress Calculation Logic**:
  - Output count = `landscape_files.len() + (portrait_files.len() / 2)` for landscape target
  - Output count = `portrait_files.len() + (landscape_files.len() / 2)` for portrait target
  - Progress bar length (`set_length()`) updated dynamically after pairing analysis
  - Progress increments by 1 per output file instead of 2 per input file pair
- **Thread Pool Initialization**:
  - Rayon global thread pool initialization now handles re-initialization gracefully
  - `build_global()` errors are silently ignored (pool already initialized)
  - GUI can process images multiple times without "Failed to initialize thread pool" error
  - First call initializes global pool, subsequent calls use existing pool

### Fixed
- **GUI Process Button**: Fixed issue where button wouldn't work on second click
  - Thread pool initialization failure when processing multiple times in same session
  - Changed from `.context("Failed to initialize thread pool")?` to `let _ = ...`
  - Now handles global thread pool being already initialized
- **Progress Bar Accuracy**: Fixed progress count exceeding total (e.g., 531/528)
  - Corrected to count output files not input files in pairing scenarios
  - Portrait pairs now increment progress by 1 (one combined output from two inputs)
  - Landscape pairs now increment progress by 1 (one combined output from two inputs)
- **JSON Progress Monitoring**: Fixed hanging at end when total doesn't match expected
  - Replaced timeout-based exit condition with atomic completion flag
  - Thread now reads dynamic progress bar length for accurate totals
  - Progress monitoring exits cleanly when `processing_complete` flag is set

### Removed
- **Validate Only from GUI**: Removed from GUI interface (still available in CLI with `--validate-only`)

### Technical Details
- **GUI Dependencies** (optional, enabled with `gui` feature):
  - `eframe = "0.33.3"` - Application framework for native apps
  - `egui = "0.33.3"` - Immediate mode GUI library
  - `rfd = "0.16.0"` - Native file dialogs (cross-platform)
  - `dirs = "6.0.0"` - Platform-specific config directories
  - `font-kit = "0.14.2"` - System font enumeration

- **Progress Monitoring Architecture**:
  - `Arc<AtomicBool>` for completion signaling between threads
  - `std::sync::mpsc::channel` for progress updates from background thread to GUI
  - Progress bar shows percentage and animates during processing
  - 25 FPS update rate (40ms sleep interval) for smooth progress display
  - Final progress message sent before thread exits

- **Configuration Persistence**:
  - Config saved to: `~/.config/photoframe-processor/gui_config.json` (Linux/macOS)
  - Config saved to: `%APPDATA%/photoframe-processor/gui_config.json` (Windows)
  - Automatically saves settings when "Process Images" is clicked
  - Automatically loads settings on application startup
  - JSON format with all processing parameters preserved

- **GUI Layout Optimizations**:
  - Horizontal layouts reduce vertical space usage
  - Sliders sized at 250px each for precise control
  - Progress bar (300px × 40px) appears next to process button when active
  - Font combo box with 200px width shows all system fonts sorted alphabetically
  - Processing/Progress section only visible when processing or showing results

### Migration Notes
- **For CLI Users**: No breaking changes, all existing commands work as before
- **For GUI Users**:
  - Build: `cd rust/photoframe-processor && cargo build --release --features gui`
  - Binary location: `target/release/photoframe-processor-gui`
  - First run will use default settings, which are saved after first processing
- **For Developers**:
  - Thread pool initialization is now idempotent and safe for repeated calls
  - Core types available via `use photoframe_processor::{ProcessingEngine, ProcessingConfig};`
  - Progress monitoring can be implemented using `--json-progress` CLI flag

## [v1.0.0] - 2025-12-30

### Added
- Initial stable release of photoframe-processor Rust tools
- High-performance image processing for ESP32 e-paper displays
- Support for Black & White, 6-Color, and 7-Color displays
- Multiple dithering algorithms: Floyd-Steinberg, Atkinson, Stucki, Jarvis-Judice-Ninke, Ordered
- Auto-optimization mode for per-image parameter tuning
- Smart portrait pairing for landscape displays
- YOLO11-based person detection for intelligent cropping
- Multiple output formats: BMP, BIN, JPG, PNG
- EXIF data extraction for automatic date annotation
- Parallel processing with configurable job count
- Command-line interface with comprehensive options
- Utility commands:
  - `--find-hash` - Find original filename from hash
  - `--find-original` - Decode combined portrait filename
  - `--dry-run` - Simulate processing without creating files
- Base64-encoded filenames with processing type prefixes (bw, 6c, 7c)
- Optimization report generation with detailed metrics
- Portrait pre-rotation support for physical display orientation

### Features
- **Processing Types**:
  - Black & White (800×480 or 480×800)
  - 6-Color e-paper (800×480 with 90° CW pre-rotation for portrait)
  - 7-Color e-paper (800×480 with 90° CW pre-rotation for portrait)

- **Dithering Methods**:
  - Floyd-Steinberg: Classic error diffusion
  - Atkinson: Lighter error diffusion variant
  - Stucki: Enhanced error diffusion with wider kernel
  - Jarvis-Judice-Ninke: High-quality error diffusion (default)
  - Ordered/Bayer: Pattern-based dithering

- **Image Processing**:
  - Intelligent cropping with subject detection (optional)
  - Auto color correction for 6C/7C displays
  - Adjustable dither strength (0.5-1.5)
  - Contrast adjustment (-0.5 to +0.5)
  - Smart orientation detection and handling
  - Portrait pair detection and combination

- **Output Options**:
  - Multiple formats in single run with organized directory structure
  - BMP: Standard bitmap for viewing/debugging
  - BIN: Optimized binary for ESP32 displays
  - JPG: Compressed for web/sharing
  - PNG: Lossless with transparency support

- **Performance**:
  - Parallel processing with Rayon
  - Auto-detect CPU cores or specify job count
  - Progress bars with indicatif
  - Efficient memory usage with fast_image_resize

### Technical Details
- **Dependencies**:
  - `image = "0.25.9"` - Core image processing
  - `imageproc = "0.25"` - Image operations
  - `fast_image_resize = "5.4.0"` - High-performance resizing
  - `clap = "4.5.53"` - Command-line parsing
  - `rayon = "1.11.0"` - Parallel processing
  - `indicatif = "0.18.3"` - Progress bars
  - `anyhow = "1.0.100"` - Error handling
  - `serde_json = "1.0.148"` - JSON serialization

- **Build Profiles**:
  - Release: LTO enabled, single codegen unit, stripped, panic=abort
  - Development: Opt-level 1 for faster compile times

- **Optional Features**:
  - `gui` - Enable GUI application (adds egui, eframe, rfd, dirs, font-kit)
  - `test_dithering` - Enable dithering test binary

[v1.1.0]: https://github.com/sephiroth74/arduino/compare/rust-v1.0.0...rust-v1.1.0
[v1.0.0]: https://github.com/sephiroth74/arduino/releases/tag/rust-v1.0.0
