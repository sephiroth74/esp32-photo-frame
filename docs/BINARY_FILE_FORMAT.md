# PFR1 Binary File Format Specification

## Overview

**PFR1** (Photo Frame Rust 1) is a custom binary format designed for efficient image storage and transfer on the ESP32 Photo Frame. It encapsulates a complete image with metadata, error detection, and display configuration in a single file.

---

## File Structure

### Complete Layout

```
┌─────────────────────────────────────────────────────┐
│ PFR1 Header (21 bytes)                              │
├──────────────┬──────────────┬──────────────────────┤
│ Field        │ Size         │ Description          │
├──────────────┼──────────────┼──────────────────────┤
│ Magic        │ 4 bytes      │ 0x50465231 ('PFR1')  │
│ Version      │ 1 byte       │ Format version (1)   │
│ Header Len   │ 2 bytes      │ Header size in bytes │
│ Width        │ 2 bytes      │ Image width (pixels) │
│ Height       │ 2 bytes      │ Image height (pixels)│
│ Rotation     │ 1 byte       │ 0-3 (0°, 90°, 180°)  │
│ Color Mode   │ 1 byte       │ 0=BW, 1=6-color      │
│ Payload Len  │ 4 bytes      │ Image data size      │
│ Header CRC32 │ 4 bytes      │ CRC32 of header      │
├──────────────┴──────────────┴──────────────────────┤
│ Payload (variable size)                             │
│ - Raw pixel data (width × height bytes)             │
├─────────────────────────────────────────────────────┤
│ Payload CRC32 (4 bytes)                             │
└─────────────────────────────────────────────────────┘
```

### Total File Size Calculation

```
Total Size = Header (21) + Payload (W × H) + CRC32 (4)
           = 25 + (W × H) bytes
```

#### Example Sizes

| Width | Height | Payload   | Total Size |
| ----- | ------ | --------- | ---------- |
| 800   | 480    | 384,000 B | 384,025 B  |
| 1024  | 768    | 786,432 B | 786,457 B  |
| 600   | 400    | 240,000 B | 240,025 B  |

---

## Header Fields Detailed

### Magic Number (Offset: 0, Size: 4 bytes)

**Value**: `0x50465231` (little-endian)
**Representation**: ASCII 'PFR1'
**Purpose**: File format identification
**Validation**: Must match exactly to be recognized as valid PFR1 file

```c
uint32_t magic = 0x50465231;  // 'PFR1' in little-endian
```

### Version (Offset: 4, Size: 1 byte)

**Current Value**: 1
**Range**: 0-255
**Purpose**: Format version for future compatibility
**Behavior**: 
- Future versions with different structures should increment this
- Older readers should reject unknown versions
- Current implementation only supports version 1

```c
uint8_t version = 1;
```

### Header Length (Offset: 5, Size: 2 bytes)

**Value**: 21 (for current version)
**Range**: 16-65535
**Purpose**: Allows header extension in future versions
**Validation**: Current implementation expects 21 bytes

```c
uint16_t header_len = 21;  // little-endian
```

### Width (Offset: 7, Size: 2 bytes)

**Range**: 1-65535 pixels
**Typical Values**: 
- 800 (landscape standard)
- 480 (portrait)
- 1024 (high-res landscape)
**Purpose**: Image width in pixels
**Validation**: Must match display configuration

```c
uint16_t width = 800;  // little-endian
```

### Height (Offset: 9, Size: 2 bytes)

**Range**: 1-65535 pixels
**Typical Values**:
- 480 (landscape standard)
- 800 (portrait)
- 768 (high-res)
**Purpose**: Image height in pixels
**Validation**: Must match display configuration

```c
uint16_t height = 480;  // little-endian
```

### Rotation (Offset: 11, Size: 1 byte)

**Valid Values**: 0, 1, 2, 3
**Meaning**:
- `0` = 0° (no rotation, landscape)
- `1` = 90° clockwise (portrait)
- `2` = 180° (upside-down landscape)
- `3` = 270° clockwise (portrait reverse)

**Purpose**: Display orientation
**Application**: Applied after image loading, before display rendering

```c
uint8_t rotation = 0;  // 0-3 only
```

### Color Mode (Offset: 12, Size: 1 byte)

**Valid Values**: 0, 1
**Meaning**:
- `0` = Black & White (1 byte per pixel)
- `1` = 6-Color mode (1 byte per pixel with extended palette)

**Purpose**: Image color format indicator
**Validation**: Must match device hardware capabilities

```c
uint8_t color_mode = 0;  // 0=BW, 1=6C
```

### Payload Length (Offset: 13, Size: 4 bytes)

**Value**: Width × Height (for single-byte-per-pixel format)
**Range**: 0-4,294,967,295 bytes
**Purpose**: Size of image data following header
**Validation**: Must match file size: `file_size = 25 + payload_len`

```c
uint32_t payload_len = width * height;  // little-endian
```

### Header CRC32 (Offset: 17, Size: 4 bytes)

**Algorithm**: CRC32 (polynomial: 0x04C11DB7)
**Input**: Header bytes 0-16 (magic through payload_len)
**Purpose**: Error detection for header
**Validation**: Computed CRC32 must match stored value

```c
// Calculated as CRC32 of bytes [0:17)
uint32_t header_crc32 = calculate_crc32(header, 17);
```

**CRC32 Calculation (C-like pseudocode)**:
```c
uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}
```

---

## Payload Section

### Structure

Raw pixel data immediately following the 21-byte header.

**Format**:
- **Black & White**: 1 byte per pixel (256 grayscale values)
- **6-Color**: 1 byte per pixel (custom 6-color palette)

**Size**: Exactly `width × height` bytes

### Pixel Ordering

Pixels stored in **row-major order** (left-to-right, top-to-bottom):

```
Row 0: [0,0] [1,0] [2,0] ... [W-1,0]
Row 1: [0,1] [1,1] [2,1] ... [W-1,1]
...
Row H-1: [0,H-1] [1,H-1] ... [W-1,H-1]
```

### Black & White Format

**Byte Value**: 0-255 (grayscale)
- `0x00` = Black
- `0xFF` = White
- Intermediate values = Gray levels

### 6-Color Format

**Byte Value**: 0-5 (palette index)
```
0 = Black
1 = Dark Gray
2 = Light Gray
3 = White
4 = Red
5 = Yellow
```

---

## CRC32 Payload Section

### Structure

**Size**: 4 bytes (little-endian)
**Location**: Immediately after payload
**Offset**: 21 + payload_len

### Calculation

CRC32 checksum of the entire payload (width × height bytes).

```c
// Payload CRC32
uint32_t payload_crc32 = calculate_crc32(payload, payload_len);
```

### Validation

During file loading:
1. Read entire file into buffer
2. Calculate CRC32 of payload section
3. Compare with stored CRC32 value
4. Reject if mismatch detected

---

## File Validation Process

### Step 1: Magic Number Check

```c
uint32_t magic = *(uint32_t*)(buffer + 0);
if (magic != 0x50465231) {
    return ERROR_INVALID_MAGIC;
}
```

### Step 2: Header CRC32 Validation

```c
uint32_t stored_header_crc = *(uint32_t*)(buffer + 17);
uint32_t calculated_crc = crc32(buffer, 17);
if (stored_header_crc != calculated_crc) {
    return ERROR_HEADER_CRC_FAILED;
}
```

### Step 3: Dimension Validation

```c
uint16_t width = *(uint16_t*)(buffer + 7);
uint16_t height = *(uint16_t*)(buffer + 9);
if (width != EXPECTED_WIDTH || height != EXPECTED_HEIGHT) {
    return ERROR_DIMENSION_MISMATCH;
}
```

### Step 4: Payload CRC32 Validation

```c
uint32_t payload_len = *(uint32_t*)(buffer + 13);
uint32_t stored_payload_crc = *(uint32_t*)(buffer + 21 + payload_len);
uint8_t* payload = buffer + 21;
uint32_t calculated_crc = crc32(payload, payload_len);
if (stored_payload_crc != calculated_crc) {
    return ERROR_PAYLOAD_CRC_FAILED;
}
```

### Step 5: File Size Validation

```c
size_t expected_size = 21 + payload_len + 4;
if (actual_file_size != expected_size) {
    return ERROR_FILE_SIZE_MISMATCH;
}
```

---

## Creating PFR1 Files

### Using the Rust Tool

```bash
# Process image and generate .pfr1
photoframe-processor \
  -i ~/Photos/image.jpg \
  -o ~/output \
  -t bw \
  --output-format pfr1
```

### Manual Creation (Pseudocode)

```c
void create_pfr1_file(const char* output_path,
                     uint16_t width, uint16_t height,
                     uint8_t rotation,
                     uint8_t* pixel_data) {
    
    // Create buffer
    size_t total_size = 21 + (width * height) + 4;
    uint8_t* buffer = malloc(total_size);
    
    // Fill header
    *(uint32_t*)(buffer + 0) = 0x50465231;           // Magic
    *(uint8_t*)(buffer + 4) = 1;                      // Version
    *(uint16_t*)(buffer + 5) = 21;                    // Header length
    *(uint16_t*)(buffer + 7) = width;                 // Width
    *(uint16_t*)(buffer + 9) = height;                // Height
    *(uint8_t*)(buffer + 11) = rotation;              // Rotation
    *(uint8_t*)(buffer + 12) = 0;                     // Color mode (BW)
    *(uint32_t*)(buffer + 13) = width * height;       // Payload length
    
    // Calculate and store header CRC32
    uint32_t hcrc = crc32(buffer, 17);
    *(uint32_t*)(buffer + 17) = hcrc;
    
    // Copy payload
    memcpy(buffer + 21, pixel_data, width * height);
    
    // Calculate and store payload CRC32
    uint32_t pcrc = crc32(buffer + 21, width * height);
    *(uint32_t*)(buffer + 21 + width * height) = pcrc;
    
    // Write to file
    FILE* f = fopen(output_path, "wb");
    fwrite(buffer, 1, total_size, f);
    fclose(f);
    
    free(buffer);
}
```

---

## Endianness

**All multi-byte fields use LITTLE-ENDIAN byte order** (native to ESP32 and most desktop systems).

When reading/writing:
```c
// Read
uint16_t value = *(uint16_t*)&buffer[offset];

// Write
*(uint16_t*)&buffer[offset] = value;
```

---

## Compatibility

### Version 1 Specification

- **Created**: January 2025
- **Last Updated**: January 2025
- **Status**: Stable
- **Breaking Changes**: None expected for v1.x

### Future Compatibility

If format needs to change:
1. Increment `version` field in header
2. Readers can gracefully reject unknown versions
3. Extend header by using `header_len` field
4. Maintain backward compatibility where possible

---

## Usage in Transfer

### Bluetooth Transfer

1. **File Loading**: Read complete PFR1 file
2. **Validation**: Verify both CRC32 checksums
3. **Fragmentation**: Split payload into MTU-sized chunks
4. **Transmission**: Send header + chunks via BLE
5. **Reception**: Device rebuilds file in PSRAM
6. **Verification**: Device validates both CRC32 checksums
7. **Application**: Device applies rotation and renders to display

### SD Card Storage

1. **File Saving**: Write complete PFR1 file to SD card
2. **Verification**: Read back and validate checksums
3. **Persistence**: File remains on SD for recovery
4. **Playback**: On next boot, device loads from SD and displays

### Flash Storage

1. **Compression**: PFR1 can be stored in compressed form
2. **Caching**: Common images cached in ESP32 flash
3. **Quick Load**: Reduces Bluetooth transfer for repeated images

---

## Tools and Utilities

### Validate PFR1 File (Shell Script)

```bash
photoframe-processor --validate Input-File.pfr1
```
---

## Limitations and Considerations

### Memory Requirements
- **ESP32 PSRAM**: Minimum `width × height` bytes for image buffer
- **Desktop Storage**: Minimal (typically < 1 MB for standard displays)
- **Transfer Buffer**: ~500-1024 bytes for BLE chunks

### Size Limits
- **Maximum Resolution**: Limited by PSRAM size (typically 8 MB)
- **Practical Maximum**: 1024×1024 pixels (1 MB) for most devices
- **Standard**: 800×480 pixels (384 KB)

### Transfer Considerations
- **Large Files**: May timeout on slow Bluetooth connections
- **Battery Drain**: Bluetooth transfer consumes ~50-100 mA
- **Interference**: 2.4 GHz WiFi can affect Bluetooth performance

---

## Version History

| Version | Date     | Changes         | Status |
| ------- | -------- | --------------- | ------ |
| 1.0     | Jan 2025 | Initial release | Stable |

---

**Last Updated**: January 2025  
**Format Version**: 1.0  
**Status**: Stable
