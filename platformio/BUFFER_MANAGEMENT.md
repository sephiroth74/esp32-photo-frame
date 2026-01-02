# Buffer Management Architecture

## Overview

As of v0.10.0, the ESP32 Photo Frame uses a static PSRAM buffer for image rendering with proper memory lifecycle management. This document describes the buffer architecture, allocation strategy, and cleanup procedures.

## Buffer Ownership

### Main Application (main.cpp)

The main application owns and manages its own image buffer:

```cpp
// Static image buffer for Mode 1 format (allocated once in PSRAM)
static uint8_t* g_image_buffer = nullptr;
static const size_t IMAGE_BUFFER_SIZE = DISP_WIDTH * DISP_HEIGHT;  // 384,000 bytes for 800x480
```

**Lifecycle**:
1. **Allocation**: `init_image_buffer()` called early in `setup()` before any image operations
2. **Usage**: Buffer passed to renderer functions for loading and rendering images
3. **Cleanup**: `cleanup_image_buffer()` called in `finalize_and_enter_sleep()` before deep sleep

### Display Debug (display_debug.cpp)

Display debug mode has its own independent buffer:

```cpp
namespace display_debug {
    static uint8_t* g_image_buffer = nullptr;
    static const size_t IMAGE_BUFFER_SIZE = DISP_WIDTH * DISP_HEIGHT;
}
```

**Lifecycle**:
1. **Allocation**: Automatic via `ensureDisplayInitialized()` when display tests are run
2. **Usage**: Persists throughout the debug session
3. **Cleanup**: `cleanup_image_buffer()` available but not automatically called (debug is continuous loop)

## Allocation Strategy

### PSRAM First, Heap Fallback

```cpp
#if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
    // Try to allocate in PSRAM first
    g_image_buffer = (uint8_t*)heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);

    if (g_image_buffer != nullptr) {
        // Success - using PSRAM
    } else {
        // Fallback to regular heap
        g_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
    }
#else
    // No PSRAM available, use regular heap
    g_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
#endif
```

### Memory Requirements

- **Buffer Size**: 384,000 bytes (384 KB) for 800×480 display
- **PSRAM Usage**: ~4.7% of 8MB PSRAM on ESP32-S3 boards
- **Allocation Type**: Single allocation at startup, reused for all images
- **Deallocation**: Before entering deep sleep to release memory

## Renderer Functions

The renderer now accepts the buffer as a parameter instead of managing it internally:

### API Signatures

```cpp
// Load image from file to buffer
uint16_t load_image_to_buffer(uint8_t* buffer, File& file, const char* filename, int width, int height);

// Render image from buffer to display
uint16_t render_image_from_buffer(uint8_t* buffer, int width, int height);

// Legacy: Load and render in one call
uint16_t draw_demo_bitmap_mode1_from_file(uint8_t* buffer, File& file, const char* filename, int width, int height);
```

### Usage Example

```cpp
// Initialize buffer
init_image_buffer();

// Load image
uint16_t loadError = photo_frame::renderer::load_image_to_buffer(
    g_image_buffer, file, filename, DISP_WIDTH, DISP_HEIGHT);

if (loadError == 0) {
    // Render image
    uint16_t renderError = photo_frame::renderer::render_image_from_buffer(
        g_image_buffer, DISP_WIDTH, DISP_HEIGHT);
}

// Before sleep
cleanup_image_buffer();
```

## Benefits of This Architecture

### 1. Separation of Concerns
- **Renderer**: Pure utility that operates on provided data
- **Application**: Owns memory and controls lifecycle
- **Clear Responsibility**: No ambiguity about who manages what

### 2. Testability
- Renderer can be tested with different buffers
- Mock buffers can be provided for unit tests
- No hidden state in renderer

### 3. Flexibility
- Main app and debug mode manage buffers independently
- Different allocation strategies possible per context
- Easy to extend for multi-buffer scenarios

### 4. Memory Efficiency
- Single allocation per application context
- No malloc/free overhead per image
- Reduced memory fragmentation
- Proper cleanup before deep sleep

## Memory Lifecycle

### Normal Operation (main.cpp)

```
Startup
  ↓
init_image_buffer()
  ↓ (buffer allocated in PSRAM)
Load image from SD → Buffer
  ↓
Close SD card
  ↓
Render from Buffer → Display
  ↓
Display refresh
  ↓
cleanup_image_buffer()
  ↓ (buffer freed)
Enter deep sleep
```

### Debug Mode (display_debug.cpp)

```
Debug startup
  ↓
ensureDisplayInitialized()
  → init_image_buffer()
     ↓ (buffer allocated in PSRAM)
Run tests (load/render images)
  ↓
Loop continues (buffer persists)
  ↓
(cleanup optional - debug is continuous)
```

## Error Handling

### Allocation Failure

If buffer allocation fails:

```cpp
if (g_image_buffer == nullptr) {
    log_e("[main] CRITICAL: Failed to allocate image buffer!");
    log_e("[main] Free heap: %u bytes", ESP.getFreeHeap());
    log_e("[main] Cannot continue without image buffer");
}
```

The system logs the error but continues execution. Subsequent image operations will fail gracefully with appropriate error codes.

### Cleanup Safety

The cleanup function is idempotent and safe to call multiple times:

```cpp
void cleanup_image_buffer()
{
    if (g_image_buffer != nullptr) {
        log_i("[main] Freeing image buffer at address: %p", g_image_buffer);
        free(g_image_buffer);
        g_image_buffer = nullptr;  // Prevent double-free
        log_i("[main] Image buffer freed");
    }
}
```

## Migration Notes

### From v0.9.x to v0.10.0

**Old Code** (renderer managed buffer):
```cpp
// Renderer owned the buffer internally
photo_frame::renderer::init_image_buffer();
photo_frame::renderer::load_image_to_buffer(file, filename, width, height);
photo_frame::renderer::render_image_from_buffer(width, height);
```

**New Code** (application manages buffer):
```cpp
// Application owns buffer
init_image_buffer();  // Local function, not in renderer namespace
photo_frame::renderer::load_image_to_buffer(g_image_buffer, file, filename, width, height);
photo_frame::renderer::render_image_from_buffer(g_image_buffer, width, height);
cleanup_image_buffer();  // Clean up before sleep
```

**Key Changes**:
1. Buffer allocation moved from renderer to application
2. All renderer functions now require buffer parameter
3. Explicit cleanup before deep sleep
4. `init_image_buffer()` and `cleanup_image_buffer()` are now application-level functions

## Best Practices

### 1. Initialize Early
Call `init_image_buffer()` as early as possible in `setup()`, before any operations that might need the buffer.

### 2. Check for Null
Always verify the buffer was successfully allocated before using it:
```cpp
if (g_image_buffer == nullptr) {
    // Handle allocation failure
    return;
}
```

### 3. Clean Up Before Sleep
Always call `cleanup_image_buffer()` before entering deep sleep to properly release memory.

### 4. Don't Share Buffers
Each context (main vs debug) should manage its own buffer. Don't try to share buffers between contexts.

### 5. Monitor PSRAM Usage
Log PSRAM free space after allocation to ensure sufficient memory remains:
```cpp
log_i("[main] PSRAM free after allocation: %u bytes", ESP.getFreePsram());
```

## Troubleshooting

### Out of Memory Errors

If buffer allocation fails:
1. Check available PSRAM: `ESP.getFreePsram()`
2. Verify PSRAM is properly initialized: `psramFound()`
3. Check for memory leaks in other parts of the application
4. Consider reducing buffer size (only if display resolution changes)

### Memory Leaks

Signs of memory leak:
- Available heap/PSRAM decreasing over time
- Allocation failures after multiple wake cycles
- System crashes with memory-related errors

Prevention:
- Always call `cleanup_image_buffer()` before sleep
- Monitor free memory with `ESP.getFreeHeap()` and `ESP.getFreePsram()`
- Use ASAN (Address Sanitizer) if available for leak detection

### Double-Free Errors

The cleanup function prevents double-free by setting the pointer to nullptr after freeing:
```cpp
free(g_image_buffer);
g_image_buffer = nullptr;  // Prevents double-free
```

## Technical References

- **ESP32-S3 PSRAM**: 8MB Quad SPI (qio_qspi) configuration
- **Buffer Size**: 800 × 480 = 384,000 bytes (Mode 1 format)
- **Allocation API**: `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
- **Deallocation API**: `free(ptr)`

## See Also

- [Display Rendering Architecture](DISPLAY_RENDERING_ARCHITECTURE.md)
- [Native Format Integration](../NATIVE_FORMAT_INTEGRATION_COMPLETE.md)
- [CHANGELOG.md](../CHANGELOG.md) - v0.10.0 release notes
