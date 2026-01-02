# Release Notes - v0.10.0

**Release Date**: December 30, 2025

## Overview

Version 0.10.0 introduces a major refactoring of the image buffer management system, improving code architecture, memory safety, and testability. This release also removes legacy format support and eliminates intermediate file storage for better performance.

## Breaking Changes

### Buffer Management API

The renderer functions now require a buffer parameter instead of managing an internal static buffer:

**Before (v0.9.x)**:
```cpp
photo_frame::renderer::init_image_buffer();
photo_frame::renderer::load_image_to_buffer(file, filename, width, height);
photo_frame::renderer::render_image_from_buffer(width, height);
```

**After (v0.10.0)**:
```cpp
init_image_buffer();  // Application-level function
photo_frame::renderer::load_image_to_buffer(g_image_buffer, file, filename, width, height);
photo_frame::renderer::render_image_from_buffer(g_image_buffer, width, height);
cleanup_image_buffer();  // New: explicit cleanup
```

### Removed Features

1. **Native Binary Format** (192KB)
   - Only Mode 1 format (384KB) is now supported
   - All images must be 800×480 pixels = 384,000 bytes
   - Removed `draw_native_binary_from_file()` function

2. **LittleFS Intermediate Storage**
   - Binary images now load directly from SD card to PSRAM
   - Eliminated 500-800ms file copy overhead
   - Simplified code flow and error handling

## New Features

### Buffer Memory Management

- **Explicit Cleanup**: New `cleanup_image_buffer()` function ensures proper memory deallocation
- **Application Ownership**: Main app and display_debug manage their own buffers independently
- **Better Lifecycle**: Clear allocation (startup) → usage → cleanup (before sleep) pattern

### Memory Safety Improvements

- Buffer is properly freed before entering deep sleep
- Prevents memory leaks across wake/sleep cycles
- Idempotent cleanup function prevents double-free errors
- Null pointer safety checks throughout

## Improvements

### Code Architecture

1. **Separation of Concerns**
   - Renderer is now a pure utility (no state management)
   - Applications control memory lifecycle
   - Clear responsibility boundaries

2. **Testability**
   - Renderer can be tested with mock buffers
   - No hidden internal state
   - Easy to verify memory behavior

3. **Flexibility**
   - Different allocation strategies per context
   - Independent buffer management in main vs debug modes
   - Extensible for future multi-buffer scenarios

### Performance

- Single PSRAM allocation at startup (no malloc/free per image)
- Reduced memory fragmentation
- Direct SD card → PSRAM transfer (no intermediate storage)
- Memory properly released before sleep

## Bug Fixes

### Display Initialization Sequence

- **Fixed**: "Busy Timeout!" error when displaying errors
- **Root Cause**: `display.clearScreen()` caused premature refresh
- **Solution**: Replaced with `setFullWindow()` + `fillScreen()` + `delay()` pattern
- **Impact**: Consistent initialization across main app and display_debug

### SD Card Resource Management

- **Fixed**: SPI bus conflicts when errors occurred
- **Root Cause**: SD card not closed in all error paths
- **Solution**: Added `sdCard.end()` calls in all error scenarios
- **Impact**: Prevents display initialization failures after SD errors

## Migration Guide

### For Application Developers

1. **Update buffer initialization**:
   ```cpp
   // OLD: photo_frame::renderer::init_image_buffer();
   // NEW: init_image_buffer();  // Local function
   ```

2. **Add buffer parameter to renderer calls**:
   ```cpp
   // OLD: load_image_to_buffer(file, filename, w, h);
   // NEW: load_image_to_buffer(g_image_buffer, file, filename, w, h);
   ```

3. **Add cleanup before sleep**:
   ```cpp
   cleanup_image_buffer();  // Call before deep sleep
   ```

### For Image Processing

- **No changes required**: Image format remains the same (Mode 1, 384KB)
- Rust processor continues to work without modifications
- Existing image libraries remain compatible

## Technical Details

### Buffer Specifications

- **Size**: 384,000 bytes (800 × 480 pixels)
- **Location**: PSRAM preferred, heap fallback
- **Allocation**: `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
- **Deallocation**: `free()` with null pointer reset
- **PSRAM Usage**: ~4.7% of 8MB on ESP32-S3

### Memory Lifecycle

```
Startup → init_image_buffer()
       ↓ (allocate 384KB in PSRAM)
Load image from SD → buffer
       ↓
Close SD card
       ↓
Render buffer → display
       ↓
Display refresh
       ↓
cleanup_image_buffer()
       ↓ (free buffer)
Enter deep sleep
```

### API Changes Summary

| Function | Old Signature | New Signature |
|----------|--------------|---------------|
| `load_image_to_buffer` | `(File& file, ...)` | `(uint8_t* buffer, File& file, ...)` |
| `render_image_from_buffer` | `(int width, int height)` | `(uint8_t* buffer, int width, int height)` |
| `draw_demo_bitmap_mode1_from_file` | `(File& file, ...)` | `(uint8_t* buffer, File& file, ...)` |
| `init_image_buffer` | In `renderer` namespace | Application-level function |
| `cleanup_image_buffer` | Not present | **New**: Application-level function |

## Documentation

New documentation added:
- [Buffer Management Architecture](BUFFER_MANAGEMENT.md) - Complete buffer lifecycle guide
- [Display Rendering Architecture](DISPLAY_RENDERING_ARCHITECTURE.md) - Updated with buffer changes

Updated documentation:
- [CHANGELOG.md](../CHANGELOG.md) - Full v0.10.0 changelog entry
- [README.md](../README.md) - Updated documentation index

## Testing Recommendations

1. **Verify buffer allocation logs** at startup
2. **Check PSRAM usage** with `ESP.getFreePsram()`
3. **Monitor memory across wake/sleep cycles** for leaks
4. **Test error scenarios** to ensure proper cleanup
5. **Validate display initialization** after errors

## Known Issues

None at this time.

## Upgrade Path

1. Update firmware to v0.10.0
2. Verify PSRAM is available on your board (ESP32-S3 required)
3. Monitor first few wake/sleep cycles for stability
4. Check logs for any allocation failures

## Support

For issues or questions:
- GitHub Issues: https://github.com/sephiroth74/esp32-photo-frame/issues
- Documentation: See links above
- CHANGELOG: [../CHANGELOG.md](../CHANGELOG.md)

## Contributors

- Alessandro Crugnola (@sephiroth74) - Main development

## Next Steps

Future improvements planned:
- Consider buffer pooling for multi-image scenarios
- Explore compression to reduce buffer size requirements
- Investigate DMA transfers for faster loading
- Add buffer usage metrics and diagnostics
