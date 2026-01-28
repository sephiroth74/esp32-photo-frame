# Bluetooth Functionality Separation

## Overview

The Bluetooth functionality has been separated from the main image processor into a dedicated binary called `bt_client`. This provides several benefits:

## Benefits

1. **Focused Functionality**: Each binary has a clear, single purpose
   - `photoframe-processor`: Image processing and conversion
   - `bt_client`: Bluetooth communication with devices

2. **Cleaner Dependencies**: The main processor doesn't require Bluetooth libraries unless needed
   - Reduces binary size when Bluetooth isn't required
   - Faster compilation times for the main processor
   - Easier to maintain and debug

3. **Better User Experience**:
   - Clear separation between processing and uploading steps
   - More intuitive command-line interface
   - Easier to script and automate workflows

4. **Modular Design**: Each tool can be updated independently without affecting the other

## Migration Guide

### Old Workflow (Combined Binary)
```bash
# Process and upload in one step
photoframe-processor -i ~/Photos -o ~/processed --auto --upload --device PhotoFrame-ABC
```

### New Workflow (Separated Binaries)
```bash
# Step 1: Process images
photoframe-processor -i ~/Photos -o ~/processed --auto

# Step 2: Upload to device
bt_client upload -f ~/processed/bin/output.pfr1 -d PhotoFrame-ABC
```

## Build Instructions

```bash
# Build main processor only (no Bluetooth dependencies)
cargo build --release

# Build with Bluetooth support (includes bt_client)
cargo build --release --features bluetooth

# Build with all features
cargo build --release --features "ai,bluetooth"
```

## Binary Locations

After building:
- `target/release/photoframe-processor` - Main image processor (always available)
- `target/release/bt_client` - Bluetooth tool (only when built with `--features bluetooth`)

## Code Organization

- **Main processor**: `src/main.rs` - Image processing entry point
- **Bluetooth tool**: `src/bin/bt_client.rs` - Bluetooth operations entry point
- **Shared library**: `src/lib.rs` - Common functionality used by both binaries
- **Bluetooth module**: `src/bluetooth.rs` - BLE communication implementation (feature-gated)

## API Changes

The bluetooth module now exports:
- `scan_devices()` - Scan for PhotoFrame devices
- `upload_binary()` - Upload with automatic dimension inference
- `upload_image_with_dimensions()` - Upload with explicit dimensions

Previous `upload_image()` has been renamed to `upload_image_with_dimensions()` for clarity.
