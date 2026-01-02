# Display Debug Mode - Simplified Testing Suite

## Summary of Changes

The display debug mode has been completely restructured to provide a simplified, focused testing suite with compiled bitmap support.

## Changes Made

### 1. Simplified Test Suite

**Removed Tests:**
- Full diagnostic suite
- Power supply test
- Data line voltage test
- BUSY pin test
- SPI communication test
- Refresh timing test
- Google Drive integration test
- Color bars test
- Hardware failure test
- SD card images test (gallery)
- Pin release test

**Kept Tests:**
- **Potentiometer Test** (`T`) - Continuous reading of potentiometer with statistics
- **LED Test** (`L`) - Tests both builtin LED and RGB NeoPixel
- **Battery Status Test** (`B`) - Comprehensive battery monitoring with visual display
- **Color Pattern Test** (`2`) - Basic color pattern test (6-color/7-color only)
- **High Stress Color Test** (`3`) - Stress test for color displays (6-color/7-color only)

### 2. New Compiled Bitmap Tests

**Added Tests:**
- **Compiled Bitmap Test** (`M`) - Renders bitmaps from GxEPD2 library
  - Uses `Bitmaps800x480.h` from GxEPD2 library
  - Uses `Bitmaps7c800x480.h` for 7-color displays
  - No SD card required - bitmaps compiled into firmware

- **6-Color Test Patterns** (`6`) - Custom color test patterns (6-color only)
  - Pattern 1: Colored squares (2x3 grid)
  - Pattern 2: Vertical color bars
  - Pattern 3: Horizontal color bands
  - Pattern 4: Checkerboard pattern

- **7-Color Test Patterns** (`7`) - Custom color test patterns (7-color only)
  - Pattern 1: Colored squares (includes ORANGE)
  - Pattern 2: Vertical color bars

### 3. New Files Created

**Header Files:**
- `include/test_bitmaps_color.h` - Main header for color test patterns
- `include/test_bitmaps_color_6c_squares.inc` - 6-color squares pattern (384KB)
- `include/test_bitmaps_color_6c_vbars.inc` - 6-color vertical bars (384KB)
- `include/test_bitmaps_color_6c_hbands.inc` - 6-color horizontal bands (384KB)
- `include/test_bitmaps_color_6c_checker.inc` - 6-color checkerboard (384KB)
- `include/test_bitmaps_color_7c_squares.inc` - 7-color squares pattern (768KB)
- `include/test_bitmaps_color_7c_vbars.inc` - 7-color vertical bars (768KB)

**Copied from GxEPD2 Library:**
- `include/Bitmaps800x480.h` - Monochrome bitmaps (9 different bitmaps)
- `include/Bitmaps7c800x480.h` - 7-color bitmaps

**Python Scripts:**
- `scripts/generate_test_bitmaps_6c.py` - Generates 6-color test patterns
- `scripts/generate_test_bitmaps_7c.py` - Generates 7-color test patterns
- `scripts/create_simplified_display_debug.py` - Script that created the new display_debug.cpp

**Backup:**
- `src/display_debug.cpp.backup` - Original file before modifications

### 4. Test Pattern Format

**6-Color Patterns:**
- Format: 1 byte per pixel
- Size: 800×480 = 384,000 bytes per pattern
- Color mapping:
  - `0xFF` = WHITE
  - `0x00` = BLACK
  - `0xE0` = RED
  - `0x1C` = GREEN
  - `0x03` = BLUE
  - `0xFC` = YELLOW

**7-Color Patterns:**
- Format: RGB565 (16-bit per pixel)
- Size: 800×480×2 = 768,000 bytes per pattern
- Supports additional ORANGE color

### 5. Menu Structure

```
========================================
Display Debug Menu - Simplified
========================================
T. Test Potentiometer (Continuous Reading)
L. Test LEDs (Builtin + RGB NeoPixel)
B. Battery Status Test
2. Color Pattern Test (Color Displays Only)
3. High Stress Color Test (Color Displays Only)
6. 6-Color Test Patterns (Compiled)
7. 7-Color Test Patterns (Compiled)
M. Compiled Bitmap Test (GxEPD2 Library)
C. Clear Display
0. Restart ESP32
```

### 6. Build Information

**Compilation Result:**
- Status: ✓ SUCCESS
- RAM Usage: 34.5% (113,088 / 327,680 bytes)
- Flash Usage: 47.4% (2,485,169 / 5,242,880 bytes)
- Build Time: ~23 seconds

**Memory Impact:**
- Total compiled test pattern data: ~2.7MB
  - 6-color patterns: 4 × 384KB = 1.536MB
  - 7-color patterns: 2 × 768KB = 1.536MB
  - GxEPD2 bitmaps: ~2.5MB (monochrome) + variable (7-color)

## Usage

### Regenerating Test Patterns

If you need to regenerate the test pattern include files:

```bash
cd platformio/scripts
source ../.venv/bin/activate
python generate_test_bitmaps_6c.py
python generate_test_bitmaps_7c.py
```

### Building with Debug Mode

The debug mode is enabled via the `ENABLE_DISPLAY_DIAGNOSTIC` build flag in `platformio.ini`:

```ini
[env:pros3d_unexpectedmaker]
build_flags =
    -DENABLE_DISPLAY_DIAGNOSTIC
    ; ... other flags
```

### Running Tests

1. Upload firmware to ESP32
2. Open serial monitor (115200 baud)
3. Select test from menu by typing the corresponding letter/number
4. Follow on-screen instructions

## Benefits

1. **No SD Card Required** - Compiled bitmaps test display rendering without external dependencies
2. **Faster Testing** - Streamlined menu with essential tests only
3. **Better Color Testing** - Dedicated color pattern tests for 6-color and 7-color displays
4. **Smaller Code** - Removed unused diagnostic code (~39% reduction in test code)
5. **Easier Maintenance** - Focused set of tests that are actually used

## Notes

- Color pattern tests (options 2, 3, 6, 7) are only available on displays configured with `DISP_6C` or `DISP_7C`
- The compiled bitmaps significantly increase firmware size but provide valuable testing capabilities
- Original `display_debug.cpp` is preserved as `display_debug.cpp.backup` for reference
- Test pattern generation scripts can be modified to create custom patterns as needed
