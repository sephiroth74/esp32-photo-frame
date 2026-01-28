# Bluetooth Image Transfer - Complete Guide

## 📋 Overview

Complete guide for transferring images to the ESP32 Photo Frame via Bluetooth Low Energy (BLE). This document covers the entire workflow from image preparation to successful transfer, including three different methods:

1. **Flutter Mobile App** - Android/iOS companion app
2. **Flutter Desktop App** - macOS native application
3. **Rust Command-Line Tool** - `bt-client` for scripting and automation

---

## 🎯 Prerequisites

### Device Setup
- ESP32 Photo Frame with Bluetooth mode enabled (`ENABLE_BT_IMAGE` in firmware)
- Bluetooth capability enabled on device
- Battery charged (at least 20% recommended)
- Device name in format `PhotoFrame-XXXX` (where XXXX is last 4 MAC chars)
- **LittleFS filesystem uploaded** with default image (`default.pfr1`) for fallback display
  - Without this, device shows error on timeout or transfer failure
  - Use PlatformIO: `platformio run --target uploadfs` to upload filesystem

### Software Requirements
- **Mobile**: Android 6.0+ or iOS 12.0+
- **Desktop**: macOS 11.0+ with Flutter 3.10.3+
- **CLI**: Rust 1.70+ and `photoframe-processor` with Bluetooth features

### Network
- No WiFi required
- Bluetooth 5.0+ recommended (but 4.2 is compatible)
- 5-10 meters effective range typical

---

## 📐 Image Format Specification

The ESP32 Photo Frame uses a custom binary format called **PFR1** (Photo Frame Rust 1) for image transfer. This format includes metadata, error detection, and display configuration in a single file.

**For detailed PFR1 format specification, see:** [BINARY_FILE_FORMAT.md](BINARY_FILE_FORMAT.md)

### Quick Reference

| Aspect                      | Details                   |
| --------------------------- | ------------------------- |
| **Format Name**             | PFR1 (Photo Frame Rust 1) |
| **Header Size**             | 21 bytes                  |
| **Magic Number**            | 0x50465231 ('PFR1')       |
| **Error Detection**         | CRC32 (header + payload)  |
| **Supported Colors**        | Black & White, 6-Color    |
| **Standard Size (800×480)** | 384,025 bytes total       |

### Key Components

- **Header (21 bytes)**: Magic, version, dimensions, rotation, color mode, payload size, CRC32
- **Payload (variable)**: Raw pixel data (width × height bytes)
- **CRC32 (4 bytes)**: Payload checksum for error detection

**See [BINARY_FILE_FORMAT.md](BINARY_FILE_FORMAT.md) for:**
- Complete format specification
- Byte-by-byte field details
- CRC32 validation process
- File creation examples
- Validation tools and utilities

---

## 🚀 Method 1: Flutter Mobile App

### Installation

#### Android
1. Clone or download the project:
   ```bash
   cd flutter/mobile
   flutter pub get
   ```

2. Build APK:
   ```bash
   ./scripts/build_rust_macos.sh
   ./scripts/build_rust_android.sh
   flutter build apk --release
   ```

3. Install on device:
   ```bash
   flutter install
   ```

#### iOS
1. Setup iOS dependencies:
   ```bash
   cd flutter/mobile
   flutter pub get
   cd ios
   pod install
   cd ..
   ```

2. Build and run:
   ```bash
   flutter run -d ios
   ```

### Usage Workflow

#### Step 1: Prepare Your Image
- Select or capture a photo in your device's gallery
- Supported formats: JPEG, PNG
- Recommended size: Match your display (800×480 for standard frame)
- Portrait mode images are supported and will be handled automatically

#### Step 2: Launch the App
1. Open the Photo Frame Bluetooth app
2. App automatically scans for nearby Photo Frame devices
3. Devices appear as `PhotoFrame-XXXX` in the available devices list

#### Step 3: Select Device
1. Tap on your Photo Frame device in the list
2. Connection indicator changes to show connection status
3. Wait for "Connected" message (typically 2-3 seconds)

#### Step 4: Select Image
1. Tap "Choose Image" button
2. Select image from gallery or take a new photo
3. Preview displays selected image with size information
4. Confirm selection

#### Step 5: Configure Settings
- **Rotation**: Select desired display orientation (0°, 90°, 180°, 270°)
- **Color Mode**: Choose between Black & White or 6-Color (if supported)
- **Brightness**: Adjust image brightness slider (optional)
- **Contrast**: Adjust image contrast slider (optional)

#### Step 6: Transfer Image
1. Tap "Upload to Device" button
2. Progress bar shows transfer progress
3. Transfer typically takes 5-30 seconds depending on:
   - Image size
   - Bluetooth signal strength
   - Device MTU (Maximum Transfer Unit)
4. Success message displays when transfer is complete

#### Step 7: Verify
- Photo Frame display shows the new image
- Device automatically returns to sleep after successful transfer
- Battery indicator shows remaining charge

### Troubleshooting Mobile App

| Issue                   | Solution                                                        |
| ----------------------- | --------------------------------------------------------------- |
| Device not appearing    | Turn Bluetooth on/off, restart app, check device is in BT mode  |
| Connection drops        | Move closer to device, reduce interference, try again           |
| Transfer fails          | Ensure image is not corrupted, try smaller image, check battery |
| Image shows incorrectly | Verify rotation setting matches your orientation                |

---

## 🖥️ Method 2: Flutter Desktop App (macOS)

### Installation

#### Prerequisites
```bash
# Ensure Flutter is installed
flutter --version

# Verify Xcode is installed
xcode-select --install

# Verify Rust is installed (for processor)
rustc --version
```

#### Build Process

1. Clone repository:
   ```bash
   cd flutter/desktop
   flutter pub get
   ```

2. Generate JSON serialization:
   ```bash
   flutter pub run build_runner build
   ```

3. Build for macOS:
   ```bash
   flutter build macos --release
   ```

4. Run application:
   ```bash
   flutter run -d macos
   ```

Application location after build:
```
build/macos/Build/Products/Release/photoframe_flutter.app
```

### Application Features

#### Interface Sections

**Bluetooth Controls**
- Device scanner with real-time discovery
- Connection status indicator
- Device selection dropdown
- Automatic device name filtering

**Image Selection**
- File picker for input directory
- Preview panel showing selected image
- Image dimensions and file size display
- Format validation

**Image Processing**
- Display type selection (Black & White, 6-Color)
- Target orientation picker (Landscape, Portrait)
- Dithering method selection (Floyd-Steinberg, etc.)
- Brightness and contrast sliders
- Auto-optimize toggle

**Transfer Controls**
- Progress indicator
- Transfer speed display
- Estimated time remaining
- Cancel button (during transfer)

### Usage Workflow

#### Step 1: Launch Application
```bash
open build/macos/Build/Products/Release/photoframe_flutter.app
```

#### Step 2: Scan for Devices
1. Click "Scan for Devices"
2. Wait 3-5 seconds for discovery to complete
3. Available Photo Frame devices appear in list
4. Look for `PhotoFrame-XXXX` format

#### Step 3: Connect to Device
1. Select device from list
2. Click "Connect"
3. Status changes to "Connected" when ready
4. Device shows connection indication

#### Step 4: Select Input Image
1. Click folder icon next to "Input Image"
2. Navigate to image file
3. Select image (JPEG, PNG, BMP supported)
4. Preview displays in window

#### Step 5: Configure Processing
1. **Display Type**: Select "Black & White" or "6-Color"
2. **Target Orientation**:
   - Landscape: 800×480 horizontal
   - Portrait: 480×800 vertical
3. **Dithering Method**: Floyd-Steinberg recommended for quality
4. **Auto-Optimize**: Enabled by default (adjust sliders to disable)
5. **Brightness/Contrast**: Fine-tune as needed

#### Step 6: Transfer Image
1. Click "Upload to Device"
2. Window shows:
   - Real-time progress bar
   - Bytes transferred / Total bytes
   - Transfer speed (KB/s)
   - Estimated time remaining
3. Do not close window or disconnect device
4. Transfer complete message appears

#### Step 7: Verify Transfer
1. Photo Frame display updates with new image
2. Device returns to sleep
3. Battery percentage shown on display

### Advanced Features

#### Configuration Persistence
- Window size and position saved automatically
- Last used image path remembered
- Processing settings stored between sessions

#### Theme Support
- Automatic light/dark mode detection
- Native macOS appearance
- System accent color integration

### Troubleshooting Desktop App

| Issue                   | Solution                                                           |
| ----------------------- | ------------------------------------------------------------------ |
| Bluetooth not available | Enable Bluetooth in System Preferences → Bluetooth                 |
| Device not found        | Check device is in Bluetooth mode, power cycle device              |
| Transfer is slow        | Check signal strength, reduce interference, restart Bluetooth      |
| Image quality poor      | Use PNG source, increase dithering strength, disable auto-optimize |

---

## 💻 Method 3: Rust Command-Line Tool (bt-client)

### Installation

#### Prerequisites
```bash
# Install Rust (if not already installed)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Verify installation
rustc --version
cargo --version
```

#### Build Process

1. Navigate to project:
   ```bash
   cd rust/photoframe-processor
   ```

2. Build with Bluetooth feature:
   ```bash
   cargo build --release --features bluetooth
   ```

3. Binaries created in `target/release/`:
   - `photoframe-processor` - Image processor
   - `bt-client` - Bluetooth tool

#### Installation for System-Wide Access
```bash
# Copy to system path
cp target/release/photoframe-processor /usr/local/bin/
cp target/release/bt-client /usr/local/bin/

# Verify installation
photoframe-processor --version
bt-client --help
```

### Workflow: Prepare and Upload Images

#### Complete Example Workflow

```bash
# Step 1: Process images from source directory
photoframe-processor \
  -i ~/Photos/Vacation \
  -o ~/processed_images \
  -t bw \
  --output-format pfr1 \

# Step 2: Scan for available devices
bt-client scan

# Output:
# Found PhotoFrame devices:
# 1. PhotoFrame-ABC1 (MAC: AA:BB:CC:DD:EE:01) - Signal: -45 dBm
# 2. PhotoFrame-XYZ9 (MAC: AA:BB:CC:DD:EE:02) - Signal: -62 dBm

# Step 3: Upload image to device
bt-client upload \
  -f ~/processed_images/bin/image_001.pfr1 \
  -d PhotoFrame-ABC1 \
  --orientation 0
```

### Command Reference

#### photoframe-processor

Process images from source to PFR1 binary format:

```bash
photoframe-processor [OPTIONS] -i <INPUT> -o <OUTPUT>
```

**Common Options:**

**See:** [RUST_PHOTOFRAME_PROCESSOR.md](RUST_PHOTOFRAME_PROCESSOR.md)

**Examples:**

Black & White processing:
```bash
photoframe-processor \
  -i ~/Pictures \
  -o ~/output \
  -t bw \
  --output-format bin
```

6-Color with EXIF auto-detect:
```bash
photoframe-processor \
  -i ~/Photos \
  -o ~/output \
  -t 6c \
  --auto \
  --output-format bin,jpg \
  --rotation-auto
```

Portrait pairing:
```bash
photoframe-processor \
  -i ~/Portraits \
  -o ~/output \
  -t bw \
  --portrait-pairing \
  --output-format bin
```

#### bt-client

Scan devices and upload images via Bluetooth:

```bash
bt-client <COMMAND> [OPTIONS]
```

**Commands:**

**scan** - List available Photo Frame devices:
```bash
bt-client scan [OPTIONS]
```

Example:
```bash
bt-client scan
# Output:
# PhotoFrame devices found:
# PhotoFrame-ABC1  (AA:BB:CC:DD:EE:01)  Signal: -42 dBm
# PhotoFrame-XYZ9  (AA:BB:CC:DD:EE:02)  Signal: -65 dBm
```

**upload** - Upload image to device:
```bash
bt-client upload [OPTIONS] -f <FILE>

Required Options:
  -f, --file <FILE>        Path to .pfr1 binary file

Optional Options:
  -d, --device <NAME>      Device name (PhotoFrame-XXXX or MAC address)
                          (auto-select first available if omitted)
  --orientation <VALUE>   Display rotation: 0, 1, 2, 3
                         (default: 0)
```

Example with auto-select:
```bash
bt-client upload -f ~/output/image.pfr1
# Auto-selects first available device
```

Example with specific device and orientation:
```bash
bt-client upload \
  -f ~/output/image.pfr1 \
  -d PhotoFrame-ABC1 \
  --orientation 1
```


### Troubleshooting CLI Tool

| Issue                              | Solution                                                            |
| ---------------------------------- | ------------------------------------------------------------------- |
| `bt-client: command not found`     | Add to PATH: `export PATH="$PATH:~/cargo/bin"`                      |
| Device not found                   | Check Bluetooth is on, run `bt-client scan` to verify               |
| Upload fails with timeout          | Increase timeout: `--timeout 60`                                    |
| Device disconnects during transfer | Use slower upload: `--speed-optimization false`                     |
| Permission denied on macOS         | Grant Bluetooth permission: System Preferences → Security & Privacy |

---

## 🔧 Technical Details

### Bluetooth Configuration

#### BLE Characteristics
```
Service UUID: Custom (defined in firmware)

Characteristics:
1. Config (Write)
   - 19 bytes: rotation, dimensions, image size, timestamp
   - Validated before image transfer

2. Image Data (Write)
   - Variable size chunks
   - MTU-dependent (typically 500-1024 bytes)
   - CRC16-MODBUS validated

3. Status (Read/Notify)
   - Current transfer progress
   - Battery level
   - Error codes
```

#### Transfer Protocol

```
Time    Event                   Data Size
───────────────────────────────────────────
T0      Device discovers frame  ~10ms
T1      Connect BLE             ~500ms
T2      Exchange config         19 bytes
T3      Stream image data       W × H bytes
T4      Validate payload CRC    4 bytes
T5      Apply to display        ~2-5 seconds
T6      Return to sleep         <100ms
```

### Image Processing Pipeline

#### Flow Diagram

```
┌─────────────────┐
│  Source Image   │
│  (JPEG/PNG)     │
└────────┬────────┘
         │
         ▼
┌──────────────────────────┐
│  EXIF Analysis           │
│  - Orientation detection │
│  - Dimension reading     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Resize & Scale          │
│  - Lanczos3 filtering    │
│  - Aspect ratio handling │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Color Processing        │
│  - Auto color correction │
│  - Saturation adjustment │
│  - Level optimization    │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Dithering               │
│  - Floyd-Steinberg       │
│  - Ordered (Bayer)       │
│  - Custom patterns       │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Format Conversion       │
│  - BW: 1 byte/pixel      │
│  - 6C: 1 byte/pixel      │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  PFR1 Packaging          │
│  - Header creation       │
│  - CRC32 calculation     │
│  - Binary file output    │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Bluetooth Transfer      │
│  - MTU fragmentation     │
│  - Retry on error        │
│  - Progress reporting    │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Device Validation       │
│  - Header CRC verify     │
│  - Payload CRC verify    │
│  - Dimension check       │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  Display Rendering       │
│  - Buffer update         │
│  - Rotation apply        │
│  - E-paper refresh       │
└──────────────────────────┘
```

### Battery Management During Transfer

Device monitors battery during entire Bluetooth session:

- **Critical** (<5%): Abort transfer, show error, sleep indefinitely
- **Low** (5-20%): Show warning, continue if possible
- **Normal** (>20%): Full functionality

Device returns to sleep after successful transfer to conserve battery.

---

## 🐛 Troubleshooting Common Issues

### Connection Issues

**Problem**: Device not appearing in device list
```
Cause: Device not in Bluetooth mode or BLE advertising disabled
Fix:  - Verify device is in BT_MODE (check firmware compilation)
      - Restart device (power cycle)
      - Check device's Bluetooth hardware
```

**Problem**: Connection fails intermittently
```
Cause: Bluetooth interference or signal weakness
Fix:  - Move closer to device (within 2 meters)
      - Remove WiFi router nearby
      - Use 2.4 GHz band only (not 5 GHz)
      - Try again after 30 seconds
```

### Transfer Issues

**Problem**: Transfer aborts halfway
```
Cause: MTU size mismatch or signal loss
Fix:  - Ensure stable proximity to device
      - Reduce MTU size if available
      - Use CLI `--speed-optimization false`
      - Check battery isn't critically low
```

**Problem**: Image displays incorrectly
```
Cause: Rotation setting or color mode mismatch
Fix:  - Verify rotation matches display orientation
      - Confirm color mode (BW vs 6C) matches device
      - Check image isn't corrupted (test with small image)
```

### Performance Issues

| Symptom            | Typical Cause           | Solution                  |
| ------------------ | ----------------------- | ------------------------- |
| Slow transfer      | WiFi interference       | Move away from router     |
| High battery drain | Long Bluetooth session  | Disconnect after transfer |
| Poor image quality | Wrong dithering method  | Test different methods    |
| Blurry image       | Incorrect resize filter | Use Lanczos3 (default)    |

---

## 📚 Additional Resources

### Related Documentation
- [rust-photoframe-processor.md](rust-photoframe-processor.md) - Rust tool documentation

### Development
- **Bluetooth Protocol**: See `src/bt_protocol.h` in platformio/
- **BLE Manager**: See `src/bluetooth_image_manager.h` in platformio/
- **Rust Implementation**: See `src/bluetooth.rs` in rust/photoframe-processor/

### Support
- Issue tracker: GitHub issues
- Discussions: GitHub discussions
- Device info: Check device name on display during Bluetooth mode

---

## 📝 Summary

| Method          | Speed | Ease      | Features                           | Best For                        |
| --------------- | ----- | --------- | ---------------------------------- | ------------------------------- |
| **Mobile App**  | Fast  | Very Easy | User-friendly, visual feedback     | Casual users                    |
| **Desktop App** | Fast  | Easy      | Native interface, batch processing | macOS users, processing control |
| **CLI Tool**    | Fast  | Medium    | Automation, scripting, batch       | Power users, integration        |

Choose the method that best fits your workflow and technical comfort level. All three methods produce identical results on the device.

---

**Last Updated**: January 2025  
**Version**: 1.0  
**Status**: Complete
