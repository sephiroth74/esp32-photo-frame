# Bluetooth Image Transfer - Implementation Summary

## 📋 Overview

Complete implementation of Bluetooth Low Energy (BLE) image transfer mode for ESP32 photo frame. This feature allows the device to receive images wirelessly via BLE instead of using WiFi/Google Drive or SD card.

## ✅ Completed Components

### 1. Protocol Layer
**Files**: `bt_protocol.h/cpp`
- Binary protocol with magic number (0xBEEF) and version (1)
- Config structure: rotation (0-3), dimensions, image size, timestamp
- CRC16-MODBUS validation
- Size: 19 bytes config header

### 2. Error Management
**Files**: `errors.h` (15 new error codes)
- **Critical** (301-310, 354): Init failed, invalid config, invalid rotation, etc.
- **Informative** (350-354): Timeout, disconnection, partial transfer
- **Warning** (360-369): Low battery skip, configuration issues
- Severity classification system

### 3. Preferences Storage
**Files**: `preferences_helper.h/cpp`
- 6 new NVS methods for BT mode
- Stores: rotation, first_boot flag, image_available, last_error, retry_count
- Thread-safe with proper begin/end

### 4. Configuration Constants
**Files**: `config.h`
- `BT_FIRST_BOOT_TIMEOUT_MS` = 30 min
- `BT_LISTEN_TIMEOUT_MS` = 5 min
- `BT_BATTERY_CHECK_INTERVAL_MS` = 60 sec
- `BT_FALLBACK_IMAGE_PATH` = "/bt_images/last.bin"
- `BT_IMAGES_DIR` = "/bt_images"
- `EXPECTED_IMAGE_SIZE_BYTES` = EPD_WIDTH × EPD_HEIGHT

### 5. Battery Management
**Files**: `bt_utils.h/cpp`
- Battery checking with 3 threshold levels
- Critical battery handler (display + indefinite sleep)
- Low battery warning display
- Integrated with wakeup reason

### 6. BLE Manager
**Files**: `bluetooth_image_manager.h/cpp`

**Components**:
- `BTServerCallbacks` - Connection/disconnection events
- `BTConfigCallbacks` - Receives and validates configuration (19 bytes)
- `BTImageDataCallbacks` - Receives image data chunks

**Features**:
- BLE advertising with custom service UUID
- 3 characteristics: config, image_data, status
- Image buffer allocation (malloc)
- Chunk accumulation with memcpy
- Size validation during transfer
- Complete transfer detection

### 7. Image Persistence
**Files**: `bluetooth_image_manager.cpp`

**Function**: `saveImageToSdCard()`
- Initializes SD card if needed
- Creates `/bt_images` directory
- Writes to `/bt_images/last.bin`
- Chunk writing (4KB blocks)
- File verification after write
- Error handling and cleanup

### 8. Image Validation
**Files**: `io_utils.h/cpp`

**Function**: `validate_image_size_exact()`
- Validates **exact** byte count: width × height
- For 800×480: must be exactly 384,000 bytes
- Used in all 3 flows: Google Drive, SD Card, Bluetooth
- Returns `ImageFileTruncated` on mismatch

### 9. Main Integration
**Files**: `main.cpp`

**Function**: `setup_bluetooth_mode()`

**Workflow**:
1. Hardware init (RGB LED, battery reader)
2. Battery checks (critical → sleep, low → warning)
3. Display init + "waiting" message
4. Create `BluetoothImageManager`
5. Start BLE advertising
6. Wait loop with 60-sec battery checks
7. Detect transfer complete
8. **SD Operations** (BEFORE display):
   - Save image to SD
   - Save preferences (rotation, flags)
   - Close SD card
9. **Display Operations** (AFTER SD):
   - Init display hardware
   - Load image from SD
   - Apply rotation
   - Show image
10. Deep sleep indefinite (GPIO1 wake only)

### 10. Display Messages
**Files**: `bt_utils.cpp`, `main.cpp`

**Messages Implemented**:
- **Waiting (First Boot)**: "Primo Avvio / In attesa... / Timeout: 30 minuti"
- **Waiting (Subsequent)**: "In Attesa / In attesa... / Timeout: 5 minuti"
- **Timeout (First)**: "In Attesa di Immagine / Timeout: 30 minuti scaduti / Premere GPIO1..."
- **Timeout (Subsequent)**: "Timeout / Nessuna immagine ricevuta / Premere GPIO1..."

**Rendering**: GFX Canvas with centered text, multiple sizes, calculated bounds

### 11. Display Manager Enhancement
**Files**: `display_manager.h/cpp`
- New `image_source_t` enum value: `IMAGE_SOURCE_BLUETOOTH`
- `setImageSource()` / `getImageSource()` methods
- Used for status reporting on display

### 12. Diagnostic Test Suite
**Files**: `bt_diagnostic.h/cpp`, `docs/BT_DIAGNOSTIC.md`

**9 Test Suites** (45+ individual tests):
1. Protocol validation (magic, version, CRC, rotation)
2. Preferences storage (6 NVS operations)
3. Battery checking (thresholds, states)
4. Image size validation (exact matching)
5. Error classification (severity levels)
6. SD card operations (directory, files)
7. Display initialization (buffer, hardware, rotation)
8. Display messages (rendering, centering)
9. BLE manager (creation, state, cleanup)

**Features**:
- Comprehensive test macros (`TEST_START`, `TEST_ASSERT`, `TEST_END`)
- Pass/fail counters
- Summary statistics
- Safe execution (no BLE advertising)
- Similar to `ENABLE_DISPLAY_DIAGNOSTIC`

## 🔧 Build Flags

### Enable BT Mode
```cpp
#define ENABLE_BT_IMAGE  // in config.h
```

### Enable BT Diagnostic
```cpp
#define ENABLE_BT_IMAGE       // Required
#define ENABLE_BT_DIAGNOSTIC  // Runs test suite
```

## 📊 Statistics

### Code Added
- **New Files**: 8 (protocol, manager, utils, diagnostic + headers)
- **Modified Files**: 10 (main, errors, config, preferences, io_utils, display_manager, etc.)
- **Lines of Code**: ~2,500 (including tests and docs)
- **Error Codes**: 15 new codes (301-369)
- **Test Cases**: 45+ automated tests

### Memory Usage
- Config structure: 19 bytes
- Image buffer: 384,000 bytes (800×480 display)
- BLE stack: ~50KB (ESP32 standard)
- Total RAM: ~450KB for BT mode

## 🎯 Design Patterns

### 1. SD-Display Separation
**Critical**: SD and display share SPI bus and cannot be used simultaneously

**Pattern**:
```cpp
// Phase 1: SD Operations
sdCard.begin();
// ... SD operations ...
sdCard.end();
delay(100);  // Allow SPI bus to settle

// Phase 2: Display Operations
display_power_on();
display.init();
// ... display operations ...
```

### 2. Deep Sleep Strategy
- **Normal mode**: Timed sleep (next refresh)
- **BT critical battery**: Indefinite sleep (GPIO1 only)
- **BT timeout**: Indefinite sleep (GPIO1 only)
- **BT success**: Indefinite sleep (GPIO1 only)

**Rationale**: BT mode is user-triggered, not automatic refresh

### 3. Error Classification
- **IGNORE**: Log only, continue
- **WARNING**: Log + LED, use fallback
- **CRITICAL**: Log + display full error, halt

### 4. Battery Integration
- Check on startup
- Check every 60 seconds during wait
- Abort transfer if critical
- Display appropriate messages

## 🚧 Not Implemented (Future Work)

### 1. Factory Reset
**Requirement**: Long-press GPIO1 or dedicated button
**Actions**:
- Clear all BT preferences
- Delete `/bt_images/last.bin`
- Reset to first boot state
**Estimated**: 50-80 lines

### 2. BLE Client Application
**Options**:
- Mobile app (iOS/Android)
- Python script (BlueZ)
- Web Bluetooth (Chrome)

**Requirements**:
- Connect to ESP32 BLE server
- Send 19-byte config (magic, version, rotation, dimensions, size, timestamp, CRC)
- Send image data in chunks (≤512 bytes)
- Read status characteristic
- Handle disconnections

**Estimated**: 200-500 lines (depends on platform)

### 3. Chunk Protocol Enhancement
**Current**: Simple byte accumulation
**Future**: 
- Chunk headers with sequence numbers
- Per-chunk CRC validation
- Retry mechanism
- Progress feedback via status characteristic

### 4. Multi-Image Support
**Current**: Single image (`last.bin`)
**Future**:
- Image gallery in `/bt_images`
- Rotation through images
- Image metadata (timestamp, name)

## 📁 File Structure

```
include/
  ├── bt_diagnostic.h           # Diagnostic test declarations
  ├── bt_protocol.h             # Protocol definitions
  ├── bt_utils.h                # Battery/display utilities
  ├── bluetooth_image_manager.h # BLE manager interface
  ├── config.h                  # Constants and flags
  ├── display_manager.h         # Display enhancement
  ├── errors.h                  # Error codes
  ├── google_drive.h            # image_source_t enum
  ├── io_utils.h                # Validation functions
  └── preferences_helper.h      # NVS storage

src/
  ├── bt_diagnostic.cpp         # Test suite implementation
  ├── bt_protocol.cpp           # Protocol implementation
  ├── bt_utils.cpp              # Utilities implementation
  ├── bluetooth_image_manager.cpp # BLE manager + callbacks
  ├── display_manager.cpp       # Display enhancement
  ├── io_utils.cpp              # Validation implementation
  ├── main.cpp                  # Integration + workflow
  └── preferences_helper.cpp    # NVS implementation

docs/
  ├── BT_DIAGNOSTIC.md          # Diagnostic guide
  └── BT_IMPLEMENTATION.md      # This file
```

## 🔍 Testing

### Unit Tests (Diagnostic Mode)
```bash
# Enable in config.h:
#define ENABLE_BT_IMAGE
#define ENABLE_BT_DIAGNOSTIC

# Build and run
pio run -t upload
pio device monitor

# Expected: 45 tests, all pass
```

### Integration Test (Real Hardware)
```bash
# Enable in config.h:
#define ENABLE_BT_IMAGE
// #define ENABLE_BT_DIAGNOSTIC  // Commented out

# Build and run
pio run -t upload

# Use BLE client to:
# 1. Connect to "ESP32-PhotoFrame"
# 2. Send config (19 bytes)
# 3. Send image (384,000 bytes)
# 4. Verify image displays
# 5. Test GPIO1 wake from sleep
```

## 🐛 Known Limitations

1. **Single Image**: Only stores one image at a time
2. **No Resume**: Transfer interruption requires full restart
3. **No Progress**: Client doesn't see transfer progress
4. **No Validation Feedback**: Client doesn't know if CRC failed
5. **Fixed Display**: Hardcoded 800×480 (but configurable)

## 📝 Usage Example

### Enable BT Mode
1. Edit `include/config.h`:
   ```cpp
   #define ENABLE_BT_IMAGE
   ```

2. Build: `pio run -t upload`

3. Device behavior:
   - First boot: Wait 30 min for BLE connection
   - Show "In attesa di una nuova immagine..."
   - If timeout: Show message, sleep indefinitely
   - Press GPIO1: Wake and retry

### Send Image via BLE
```python
# Pseudo-code for client
import bluetooth

# 1. Connect
device = bluetooth.find("ESP32-PhotoFrame")
device.connect()

# 2. Send config
config = struct.pack('<HBBHHIHH',
    0xBEEF,      # magic
    1,           # version
    0,           # rotation (0-3)
    800,         # width
    480,         # height
    384000,      # size
    int(time.time()),  # timestamp
    0            # reserved
)
crc = calculate_crc16(config)
config += struct.pack('<H', crc)
device.write_characteristic(CONFIG_UUID, config)

# 3. Send image data
with open('image.bin', 'rb') as f:
    while chunk := f.read(512):
        device.write_characteristic(DATA_UUID, chunk)

# 4. Disconnect
device.disconnect()
```

## 🎉 Achievements

✅ Complete BLE stack integration  
✅ Robust error handling (3 severity levels)  
✅ Battery monitoring during transfer  
✅ Exact image size validation  
✅ SD-Display bus isolation  
✅ Comprehensive test suite (45+ tests)  
✅ Clear user feedback (waiting/timeout messages)  
✅ Deep sleep power management  
✅ NVS preferences persistence  
✅ Protocol with CRC validation  

## 📚 References

- ESP32 BLE Documentation
- Adafruit GFX Library
- Arduino SD Library
- CRC16-MODBUS Algorithm

## 👤 Author

Alessandro Crugnola - 2025

## 📄 License

MIT License - See project root
