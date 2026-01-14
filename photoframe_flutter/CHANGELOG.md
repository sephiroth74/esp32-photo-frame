# Changelog - Photoframe Flutter

All notable changes to the Photoframe Flutter application will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-01-14

### ⚠️ BREAKING CHANGES
- Removed support for `--force` flag to match Rust processor v3.0.0
- Updated to work with new readable filename format from processor

### Changed
- **Configuration Model**
  - Removed `force` field from `ProcessingConfig` model
  - Removed Force re-process checkbox from UI
  - Simplified command-line argument generation

### Fixed
- **Compatibility**
  - Updated to work with Rust photoframe-processor v3.0.0
  - Regenerated serialization code for updated models

### Technical Details
- Updated generated files with `build_runner`
- Removed all references to duplicate detection logic
- Simplified processing flow without skip detection

## [1.0.0] - 2026-01-06

### Added
- **Initial Release**
  - macOS native GUI using AppKit UI elements
  - Configuration profiles with save/load functionality
  - Recent files tracking
  - Real-time processing progress
  - Support for all photoframe-processor features:
    - Black & White and 6-Color display modes
    - Multiple dithering methods
    - People detection with AI
    - Image annotation
    - Multiple output formats (BMP, BIN, JPG, PNG)
  - JSON-based progress tracking from Rust processor
  - Automatic processor binary discovery
  - Custom processor path configuration