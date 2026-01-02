# Display Rendering Architecture

## Overview

The ESP32 Photo Frame supports multiple display types with different rendering strategies optimized for each display's capabilities.

## Display Types

### 6-Color (DISP_6C) and 7-Color (DISP_7C) Displays
- **Examples**: GDEP073E01 (6-color), ACeP displays
- **Rendering Method**: Non-paged, full-frame rendering
- **Key Characteristics**:
  - No partial update support
  - Renders entire image at once
  - Uses `drawDemoBitmap()` or `drawNative()` methods
  - Overlays drawn separately after image

### Black & White (DISP_BW_V2) Displays
- **Examples**: Standard 2-color e-paper displays
- **Rendering Method**: Paged rendering
- **Key Characteristics**:
  - May or may not support partial updates
  - Renders image page-by-page for memory efficiency
  - Uses `draw_binary_from_file_paged()` method
  - Overlays drawn during each page iteration

## Rendering Pipeline

### 6C/7C Displays (Non-Paged)

```
1. Clear display (fillScreen WHITE)
2. Render image:
   - Native format (192KB): drawNative()
   - Mode 1 format (384KB): drawDemoBitmap()
   - BMP format: firstPage/nextPage loop
3. Draw overlays on top:
   - Status bar (top)
   - Battery status
   - Image info (index, total)
   - Weather info (bottom)
4. Display refresh
```

**Code Location**: [main.cpp:392-444](src/main.cpp#L392-L444)

**Key Points**:
- ✅ No paging for binary formats
- ✅ Image drawn first, overlays second
- ✅ All overlays visible
- ✅ Single display refresh

### B/W Displays (Paged)

```
For each page:
  1. Render page section of image
  2. Draw overlays for this page section
  3. Move to next page
```

**Code Location**: [main.cpp:445-491](src/main.cpp#L445-L491)

**Key Points**:
- ✅ Paged rendering for memory efficiency
- ✅ Overlays drawn during each page
- ✅ All content visible
- ✅ Multiple page refreshes

## File Format Detection

### Binary Formats

```cpp
size_t standardSize = DISP_WIDTH * DISP_HEIGHT;      // 384,000 bytes (800x480)
size_t nativeSize = (DISP_WIDTH * DISP_HEIGHT) / 2;  // 192,000 bytes (800x480)
size_t fileSize = file.size();

if (fileSize == nativeSize) {
    // Native format: 2 pixels per byte (RRRGGGBB compressed)
    // Only for 6C/7C displays
    draw_native_binary_from_file();
} else if (fileSize == standardSize) {
    // Mode 1 format: 1 byte per pixel (drawDemoBitmap values)
    // For 6C/7C: drawDemoBitmap()
    // For B/W: paged pixel-by-pixel rendering
    draw_demo_bitmap_mode1_from_file() OR draw_binary_from_file_paged();
}
```

### BMP Format

- Always uses `firstPage()`/`nextPage()` loop
- Works on all display types
- Less efficient than binary formats

## Rendering Methods

### `drawDemoBitmap()` (6C/7C only)
- **Function**: `draw_demo_bitmap_mode1_from_file()`
- **Format**: 384KB (1 byte per pixel)
- **Paging**: No
- **Usage**: Primary method for 6C/7C displays

### `drawNative()` (6C/7C only)
- **Function**: `draw_native_binary_from_file()`
- **Format**: 192KB (2 pixels per byte, RRRGGGBB)
- **Paging**: No
- **Usage**: More efficient, but less compatible

### Paged Pixel Rendering (B/W)
- **Function**: `draw_binary_from_file_paged()`
- **Format**: 384KB (1 byte per pixel, then converted)
- **Paging**: Yes
- **Usage**: Memory-efficient for B/W displays

### BMP Rendering (All displays)
- **Function**: `draw_bitmap_from_file()`
- **Format**: Standard BMP files
- **Paging**: Yes (via firstPage/nextPage)
- **Usage**: Debugging, compatibility

## Overlay Rendering

### Components
1. **Status Bar (Top)**
   - Last update timestamp
   - Next refresh time

2. **Battery Status (Top Right)**
   - Battery percentage
   - Charging indicator
   - Critical/Low warnings

3. **Image Info (Top Right)**
   - Current image index
   - Total images
   - Source (SD/Drive)

4. **Weather Info (Bottom)**
   - Current weather icon
   - Temperature
   - Conditions
   - Only shown if battery not critical

### Implementation Differences

**6C/7C Displays**:
```cpp
// Draw image first (no paging)
drawDemoBitmap() or drawNative()

// Then draw all overlays
if (!rendering_failed) {
    display.writeFillRect(0, 0, width, 16, WHITE);
    draw_last_update();
    draw_image_info();
    draw_battery_status();
    draw_weather_info();
}
```

**B/W Displays**:
```cpp
display.firstPage();
do {
    // Draw image page section
    draw_binary_from_file_paged(page_index);

    // Draw overlays for this page
    display.writeFillRect(0, 0, width, 16, WHITE);
    draw_last_update();
    draw_image_info();
    draw_battery_status();
    draw_weather_info();

    page_index++;
} while (display.nextPage());
```

## Why Different Approaches?

### 6C/7C: Non-Paged
**Reasons**:
1. `drawDemoBitmap()` writes directly to display controller memory
2. No page buffer concept in driver
3. Must render complete image before overlays
4. More efficient for color displays

**Advantages**:
- ✅ Faster rendering
- ✅ No page coordination needed
- ✅ Simpler code flow

**Disadvantages**:
- ⚠️ Requires more RAM during rendering
- ⚠️ Can't interrupt mid-render

### B/W: Paged
**Reasons**:
1. Limited ESP32 RAM (even with PSRAM)
2. Large frame buffers (800×480 = 384KB)
3. Page-based rendering reduces memory pressure
4. GxEPD2 library architecture

**Advantages**:
- ✅ Lower memory usage
- ✅ Works on boards without PSRAM
- ✅ Stable and well-tested

**Disadvantages**:
- ⚠️ Slower rendering (multiple page refreshes)
- ⚠️ More complex overlay coordination

## Display Debug vs Main App

### Display Debug (display_debug.cpp)
```cpp
// Simple, direct rendering (no overlays needed)
display.epd2.drawDemoBitmap(buffer, 0, 0, 0, 800, 480, 1, false, false);
// OR
display.firstPage();
do {
    draw_bitmap_from_file();
} while (display.nextPage());
```

**Characteristics**:
- No status bars
- No battery info
- No weather
- Just the image
- Simpler logic

### Main App (main.cpp)
```cpp
// Complex rendering with overlays
draw_image();  // As described above
draw_overlays();  // Status, battery, weather
```

**Characteristics**:
- Full UI with overlays
- Battery monitoring
- Weather display
- Image info
- More complex logic

## Common Issues & Solutions

### Issue 1: Overlays Not Showing on 6C/7C
**Cause**: Using `break` inside firstPage/nextPage loop
**Solution**: Render image outside loop, overlays after

### Issue 2: Memory Overflow on Large Images
**Cause**: Not using paged rendering
**Solution**: Use paged methods for B/W displays

### Issue 3: Slow Refresh on 6C/7C
**Cause**: Using paged rendering unnecessarily
**Solution**: Use direct drawDemoBitmap() without paging

### Issue 4: Washout on Color Displays
**Cause**: Insufficient power recovery time
**Solution**: Add 200ms delay after refresh (already implemented)

## Code Maintenance

When modifying rendering code:

1. **Always** separate 6C/7C from B/W logic with `#if defined(DISP_6C) || defined(DISP_7C)`
2. **Never** use paging for drawDemoBitmap() or drawNative()
3. **Always** draw overlays after image on 6C/7C
4. **Always** draw overlays during page loop on B/W
5. **Test** on actual hardware before committing

## References

- Main rendering code: [src/main.cpp](src/main.cpp)
- Renderer functions: [src/renderer.cpp](src/renderer.cpp)
- Display debug: [src/display_debug.cpp](src/display_debug.cpp)
- Format specification: [GDEP073E01_FORMAT_UPDATE.md](../GDEP073E01_FORMAT_UPDATE.md)
