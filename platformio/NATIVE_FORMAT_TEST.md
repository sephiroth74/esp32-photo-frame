# Native Format Performance Test

## Overview

This document describes the native format optimization for 6-color e-paper displays. This optimization uses the display's native pixel format (2 pixels per byte) with the `writeNative()` method for **significantly faster rendering**.

## Background

### Previous Approach (Slow)
The old rendering method:
1. Stored 1 byte per pixel in memory (384,000 bytes for 800×480)
2. Converted each pixel individually during rendering:
   ```cpp
   for (each pixel) {
       color_byte = buffer[pixel_index];
       if (color_byte == 0xE0) color = GxEPD_RED;
       // ... more if/else checks
       display.drawPixel(x, y, color);  // Function call overhead
   }
   ```
3. Made 384,000 individual function calls to `drawPixel()`
4. **Result**: ~19,000 ms rendering time

### New Approach (Fast)
The native format method:
1. Stores 2 pixels per byte (192,000 bytes for 800×480) - **50% memory savings**
2. Uses native display format (nibbles):
   - High nibble = pixel 1 (4 bits)
   - Low nibble = pixel 2 (4 bits)
3. Bulk transfer with single function call:
   ```cpp
   display.writeNative(buffer, nullptr, 0, 0, WIDTH, HEIGHT, false, false, false);
   display.refresh();
   ```
4. **Expected result**: 3-5x faster rendering

## Native Format Specification

### GDEP073E01 (6-Color Display) Native Format

**File Format**:
- **Size**: 192,000 bytes (800×480÷2)
- **Encoding**: 2 pixels per byte, 4 bits per pixel
- **Packing**: High nibble = left pixel, Low nibble = right pixel

**Color Mapping**:
```
Input Format (1 byte/pixel)    →    Native Format (4 bits/pixel)
─────────────────────────────────────────────────────────────
0xFF (WHITE)                    →    0x1
0x00 (BLACK)                    →    0x0
0xE0 (RED)                      →    0x3
0x1C (GREEN)                    →    0x6
0x03 (BLUE)                     →    0x5
0xFC (YELLOW)                   →    0x2
```

**Example**: Two adjacent pixels (WHITE, BLACK) = `0x10`
```
Pixel 1: WHITE  = 0x1  (high nibble)
Pixel 2: BLACK  = 0x0  (low nibble)
Result:  0x10          (one byte)
```

## Files Created

### 1. Conversion Script
**Location**: `scripts/convert_6c_to_native.py`

**Purpose**: Converts existing 6-color .bin files to native format

**Usage**:
```bash
cd platformio
.venv/bin/python scripts/convert_6c_to_native.py
```

**What it does**:
- Reads `data/test_photo_6c.bin` (384,000 bytes)
- Converts to native format (192,000 bytes)
- Writes `data/test_photo_6c_native.bin`
- Shows statistics and hex dump

### 2. Test Implementation
**Location**: `src/display_debug.cpp::testNativeFormat()`

**Menu Option**: `8` - Native Format Performance Test (FASTEST!)

**What it does**:
1. Reads native format file from SD card into PSRAM
2. Uses `display.writeNative()` for bulk transfer
3. Calls `display.refresh()` once
4. Measures and reports performance

### 3. Binary Files
- **Input**: `data/test_photo_6c.bin` (384,000 bytes)
- **Output**: `data/test_photo_6c_native.bin` (192,000 bytes)

## Testing Instructions

### Step 1: Copy File to SD Card
```bash
# Copy native format file to SD card root
cp platformio/data/test_photo_6c_native.bin /Volumes/YOUR_SD_CARD/

# Verify
ls -lh /Volumes/YOUR_SD_CARD/test_photo_6c_native.bin
```

Expected size: **192,000 bytes**

### Step 2: Upload Firmware
```bash
cd platformio
.venv/bin/pio run -e pros3d_unexpectedmaker -t upload
```

### Step 3: Open Serial Monitor
```bash
.venv/bin/pio device monitor -b 115200
```

### Step 4: Run Tests

**Test Sequence**:
1. Run option `9` (PROGMEM vs SD Card) - baseline comparison
   - Expected: ~19,000 ms rendering time
2. Run option `8` (Native Format) - optimized version
   - Expected: ~3,000-5,000 ms rendering time
   - **Should be 3-5x faster!**

## Expected Output

### Option 9 (Old Method)
```
[TEST] 6C Photo Comparison - PROGMEM vs SD Card
========================================
[TEST 1/2] Rendering from PROGMEM...
[PROGMEM] Total rendering time: 19098 ms

[TEST 2/2] Rendering from SD Card (PSRAM buffered)...
[SD] Read 384000 bytes in 926 ms
[SD] Rendering time: 19332 ms
[SD] Total time (read + render): 20258 ms
========================================
```

### Option 8 (Native Format - Expected)
```
[TEST] Native Format Performance Test
========================================
[NATIVE] SD card initialized
[NATIVE] File size: 192000 bytes
[NATIVE] PSRAM buffer allocated: 192000 bytes
[NATIVE] Read 192000 bytes in ~500 ms
[NATIVE] Rendering time: ~3000-5000 ms  ← MUCH FASTER!
[NATIVE] Total time (read + render): ~3500-5500 ms

========================================
NATIVE FORMAT RESULTS
========================================
Read time:                   500 ms
Render time:                3000 ms  ← 3-5x faster than old method
Total time:                 3500 ms
========================================
```

## Performance Benefits

**Comparison**:
- **Old Method** (pixel-by-pixel): ~19,000 ms
- **Native Format** (bulk transfer): ~3,000-5,000 ms
- **Improvement**: 3-5x faster (75-85% reduction)

**Why It's Faster**:
1. ✅ No per-pixel color conversion (384,000 if/else eliminated)
2. ✅ No per-pixel function calls (384,000 calls eliminated)
3. ✅ Single bulk SPI transfer
4. ✅ Native format understood directly by display controller
5. ✅ 50% smaller file size (faster SD card read, less PSRAM usage)

## Next Steps: Rust Integration

Once performance is confirmed, we'll modify the Rust photoframe-processor to output native format:

### Rust Changes Needed

**File**: `rust/photoframe-processor/src/convert.rs`

**New Function**:
```rust
pub fn write_6c_native_binary(
    image: &RgbImage,
    output_path: &Path,
) -> Result<(), Box<dyn Error>> {
    let mut native_data = Vec::with_capacity((image.width() * image.height() / 2) as usize);

    for y in 0..image.height() {
        for x in (0..image.width()).step_by(2) {
            let pixel1 = image.get_pixel(x, y);
            let pixel2 = image.get_pixel(x + 1, y);

            let nibble1 = color_to_6c_native(pixel1);
            let nibble2 = color_to_6c_native(pixel2);

            let packed = (nibble1 << 4) | nibble2;
            native_data.push(packed);
        }
    }

    std::fs::write(output_path, &native_data)?;
    Ok(())
}

fn color_to_6c_native(rgb: &Rgb<u8>) -> u8 {
    // Map RGB to 6-color native nibble
    match color_to_6c(rgb) {
        0xFF => 0x1,  // WHITE
        0x00 => 0x0,  // BLACK
        0xE0 => 0x3,  // RED
        0x1C => 0x6,  // GREEN
        0x03 => 0x5,  // BLUE
        0xFC => 0x2,  // YELLOW
        _ => 0x1,     // Default WHITE
    }
}
```

**Command Line**:
```bash
# New native format output
./photoframe-processor -i ~/photos -o ~/outputs \
  --size 800x480 --output-format native --auto

# Or combined with other formats
./photoframe-processor -i ~/photos -o ~/outputs \
  --size 800x480 --output-format native,bmp,jpg --auto
```

## Technical Details

### Memory Usage
- **Old Format**: 384,000 bytes PSRAM
- **Native Format**: 192,000 bytes PSRAM
- **Savings**: 192,000 bytes (50% reduction)

### SPI Transfer
- **Old Method**: 384,000 individual `drawPixel()` calls → SPI overhead
- **Native Method**: Single bulk transfer → minimal overhead

### Display Controller
The GDEP073E01 controller expects data in nibble format:
- Each byte contains 2 pixels (4 bits each)
- `writeNative()` sends data directly to controller
- Controller interprets nibbles as color indices
- No conversion needed by display

## Troubleshooting

### File Not Found
```
[NATIVE] FAIL: Could not open /test_photo_6c_native.bin
```
**Solution**: Copy file to SD card root with exact name `test_photo_6c_native.bin`

### Wrong File Size
```
[NATIVE] WARNING: Expected 192000 bytes, got XXXXX
```
**Solution**: Regenerate file using conversion script

### Display Shows Wrong Colors
**Solution**: Verify color mapping in conversion script matches display controller

### Still Slow Performance
**Solution**:
1. Verify using `writeNative()` not `drawPixel()`
2. Check PSRAM is enabled and working
3. Verify file is in native format (192KB not 384KB)

## References

- Display Controller: GDEP073E01
- GxEPD2 Library: `_convert_to_native()` function ([GxEPD2_730c_GDEP073E01.cpp:785-806](file:///Users/alessandro/Documents/Arduino/libraries/GxEPD2/src/epd7c/GxEPD2_730c_GDEP073E01.cpp#L785-L806))
- Native Format: 4 bits per pixel, 2 pixels per byte
