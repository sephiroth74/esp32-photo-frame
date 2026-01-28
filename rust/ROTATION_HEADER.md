# Rotation in BIN Header

## Summary

The `.bin` file format now includes the display rotation (0-3) in the header. This allows overlay renderers to know the physical orientation of the display.

## Changes

### Rust photoframe-processor

1. **ProcessingConfig** (`src/image_processing/mod.rs`):
   - Changed `target_orientation` from `TargetOrientation` (enum) to `OrientationConfig` (struct with both orientation and ble_rotation)
   - No separate field needed - rotation is accessed via `target_orientation.ble_rotation`

2. **main.rs**:
   - Passes full `args.target_orientation` (OrientationConfig) to ProcessingConfig

3. **build_bin_file calls** (`src/image_processing/mod.rs`):
   - Updated all three calls to use `self.config.target_orientation.ble_rotation`
   - Locations: lines ~1031, ~1359, ~2065

4. **Match statements**:
   - Updated all pattern matches to use `self.config.target_orientation.orientation`

### Rust photoframe-lib (photoframe-dithering)

**photoframe_convert_with_processing** (`src/lib.rs`):
- Now wraps output with PFR1 header using `build_bin_file()`
- Automatically infers color mode (0 for BW, 1 for 6C)
- Sets rotation=0 by default (can be overridden by caller if needed)
- Returns complete .bin file with header instead of raw payload

### Flutter Desktop

The BinParser already reads the rotation field from the header:
- `lib/services/bin_parser.dart`: `BinHeader` includes `rotation` field at byte offset 11

## Rotation Values

The `--orientation` CLI parameter maps to rotation values:
- `0` or `landscape`: 0 (horizontal, no rotation)
- `1` or `portrait`: 1 (90° clockwise)
- `2` or `landscape-reverse`: 2 (180°)
- `3` or `portrait-reverse`: 3 (270° clockwise)

## Usage

### photoframe-processor CLI
```bash
photoframe-processor -i photos/ -o output/ -t bw --output-format bin --orientation 1
```

The resulting `.bin` files will have `rotation=1` in the header.

### FFI (photoframe_convert_with_processing)
```c
// Returns .bin file with PFR1 header
DitheringResult result = photoframe_convert_with_processing(
    image_data,  // Raw image bytes (JPEG, PNG, etc.)
    image_len,   // Length of image data
    0,           // processing_type: 0=BW, 1=6C
    2            // rotation: 0-3
);
// result.data_ptr now points to complete .bin with header
```

**Parameters:**
- `image_data`: Pointer to raw image bytes (JPEG, PNG, etc.)
- `image_len`: Length of image data
- `processing_type`: 0 = BlackAndWhite, 1 = SixColors
- `rotation`: Display rotation (0-3)

## Testing

All rotation values (0-3) have been tested:
```bash
# Test rotation encoding
for rot in 0 1 2 3; do
    photoframe-processor -i test.jpg -o output_$rot/ --orientation $rot
    # Verify byte 11 in header equals $rot
done
```

FFI function tested with Python ctypes - confirms PFR1 format output.

## Overlay Rendering

Applications that render overlays (e.g., Flutter desktop) can now read `parsed.header.rotation` to know the display orientation and position overlays correctly.
