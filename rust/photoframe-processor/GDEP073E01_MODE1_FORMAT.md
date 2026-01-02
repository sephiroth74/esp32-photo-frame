# GDEP073E01 Display - Mode 1 Format Specification

## Overview

The GDEP073E01 6-color e-paper display requires a specific byte format when using `drawDemoBitmap()` with **mode 1**.

## Mode 1 Color Mapping

Mode 1 expects these specific byte values (1 byte per pixel, 384000 bytes for 800x480):

| Color  | RGB Value       | Byte Value | Notes |
|--------|----------------|------------|-------|
| BLACK  | (0, 0, 0)      | 0x00       | Pure black |
| WHITE  | (255, 255, 255)| 0xFF       | Pure white |
| YELLOW | (252, 252, 0)  | 0xFC       | Yellow |
| RED    | (252, 0, 0)    | 0xE0       | Red |
| BLUE   | (0, 0, 255)    | 0x03       | Blue |
| GREEN  | (0, 252, 0)    | 0x1C       | Green |

**Default**: Any unrecognized color defaults to BLACK (0x00)

## Implementation Steps

### 1. Add conversion function to `binary.rs`

Add this function after the existing conversion functions:

```rust
/// Convert RGB pixel to drawDemoBitmap format (mode 1) for GDEP073E01
///
/// drawDemoBitmap() with mode=1 expects these specific byte values:
/// - 0x00 = BLACK
/// - 0xFF = WHITE
/// - 0xFC = YELLOW
/// - 0xE0 = RED
/// - 0x03 = BLUE
/// - 0x1C = GREEN
///
/// This function maps the 6-color palette RGB values to these byte values.
pub fn rgb_to_demo_bitmap_mode1(pixel: &Rgb<u8>) -> u8 {
    let r = pixel[0];
    let g = pixel[1];
    let b = pixel[2];

    // Match against the 6-color palette with tolerance
    // White: RGB(255, 255, 255)
    if r >= 250 && g >= 250 && b >= 250 {
        return 0xFF;
    }

    // Yellow: RGB(252, 252, 0) or similar
    if r >= 250 && g >= 250 && b < 10 {
        return 0xFC;
    }

    // Red: RGB(252, 0, 0) or similar
    if r >= 250 && g < 10 && b < 10 {
        return 0xE0;
    }

    // Green: RGB(0, 252, 0) or similar
    if r < 10 && g >= 250 && b < 10 {
        return 0x1C;
    }

    // Blue: RGB(0, 0, 255) or similar
    if r < 10 && g < 10 && b >= 250 {
        return 0x03;
    }

    // Black: RGB(0, 0, 0)
    if r < 10 && g < 10 && b < 10 {
        return 0x00;
    }

    // Default to black for any other color
    0x00
}

/// Convert image to drawDemoBitmap format (mode 1) - 1 byte per pixel
pub fn convert_to_demo_bitmap_mode1(img: &RgbImage) -> Result<Vec<u8>> {
    let (width, height) = img.dimensions();
    let expected_size = (width * height) as usize;
    let mut binary_data = Vec::with_capacity(expected_size);

    for pixel in img.pixels() {
        let color_byte = rgb_to_demo_bitmap_mode1(pixel);
        binary_data.push(color_byte);
    }

    if binary_data.len() != expected_size {
        return Err(anyhow::anyhow!(
            "Binary data size mismatch: expected {}, got {}",
            expected_size,
            binary_data.len()
        ));
    }

    Ok(binary_data)
}
```

### 2. Update `mod.rs` to use the new conversion

In `image_processing/mod.rs`, replace calls to `convert_to_esp32_binary()` with `convert_to_demo_bitmap_mode1()`:

**Line 1112** (and similar locations):
```rust
// OLD:
binary::convert_to_esp32_binary(&processed_img)?

// NEW:
binary::convert_to_demo_bitmap_mode1(&processed_img)?
```

Search for all occurrences of `convert_to_esp32_binary` and replace with `convert_to_demo_bitmap_mode1`.

### 3. Rebuild and test

```bash
cd rust/photoframe-processor
cargo build --release
```

### 4. Regenerate test images

```bash
# Regenerate the image with correct format
./target/release/photoframe-processor \
  -i ~/photos -o ~/Desktop/arduino/photos/outputs \
  --size 800x480 --output-format bin --force
```

## ESP32 Firmware Changes (Already Done)

The firmware has been updated to use `drawDemoBitmap()` with mode 1:

- `display_debug.cpp`: Uses mode 1 for both SD card and PROGMEM tests
- Format: 384000 bytes (1 byte per pixel)
- Method: `display.epd2.drawDemoBitmap(buffer, 0, 0, 0, 800, 480, 1, false, false)`

## Verification

1. Generate images using the new Rust conversion
2. Copy to SD card under `/6c/bin/`
3. Test with **Display Tests Menu → Option 3** (Image Test from SD)
4. Colors should now display correctly

## Notes

- The old `convert_to_esp32_binary()` produced RRRGGGBB compressed format
- Mode 1 format is simpler and specifically designed for drawDemoBitmap()
- Mode 0 uses different byte values (0xFF, 0xFC, 0xF1, 0xE5, 0x4B, 0x39, 0x00)
- Mode 1 is recommended as it's simpler and the values are more intuitive
