# PhotoFrame BLE Client

> Bluetooth Low Energy (BLE) uploader for ESP32 Photo Frame devices

A command-line tool for communicating with ESP32-based PhotoFrame devices over Bluetooth Low Energy. This tool allows you to scan for nearby PhotoFrame devices and upload binary image files directly to the device.

## Features

- **Device Scanning**: Automatically discover PhotoFrame devices in range
- **Image Upload**: Transfer `.pfr1` binary image files via BLE
- **Orientation Control**: Set display rotation (0-3) during upload

## Installation

### Prerequisites

- Rust 1.75+ (edition 2024)
- Bluetooth adapter (BLE 4.0 or higher)
- Platform-specific requirements:
  - **macOS**: No additional dependencies
  - **Linux**: BlueZ and D-Bus (`libbluetooth-dev`, `libdbus-1-dev`)
  - **Windows**: Windows 10+ with Bluetooth support

### Build from Source

```bash
# Clone the repository
git clone <repository-url>
cd bt_uploader

# Build release version
cargo build --release

# The binary will be in target/release/bt_uploader
```

## Usage

### Scan for Devices

Scan for available PhotoFrame devices in range:

```bash
bt_uploader scan
```

This will display a list of discovered devices with their names and MAC addresses.

### Upload an Image

Upload a binary image file (`.pfr1` format) to a PhotoFrame device:

```bash
# Upload to first available device
bt_uploader upload -f image.pfr1

# Upload to specific device by name
bt_uploader upload -f image.pfr1 -d PhotoFrame-ABC123

# Upload to device by MAC address
bt_uploader upload -f image.pfr1 -d AA:BB:CC:DD:EE:FF

# Upload with specific orientation
bt_uploader upload -f image.pfr1 -d PhotoFrame-ABC123 --orientation 1

# Verbose output
bt_uploader upload -f image.pfr1 -d PhotoFrame-ABC123 -v
```

### Orientation Values

The `--orientation` flag accepts values 0-3:

- `0` - Landscape (default)
- `1` - Portrait
- `2` - Landscape (reverse/180°)
- `3` - Portrait (reverse/180°)

## File Format

The tool expects binary image files in the `.pfr1` (Photo Frame Rev1) format. These files contain:

- Header with image dimensions, timestamp, and CRC
- Binary image data
- Metadata for device configuration

## Architecture

The project consists of two main modules:

- **main.rs**: CLI interface and argument parsing (using `clap`)
- **bluetooth.rs**: BLE communication logic (using `btleplug`)

## BLE Protocol

The tool communicates with PhotoFrame devices using a custom BLE protocol:

## Error Handling

The tool provides clear error messages for common issues:

- Device not found or unreachable
- Invalid file format or missing file
- Connection timeout
- Invalid orientation value
- Bluetooth adapter issues

## Development

### Run in Debug Mode

```bash
cargo run -- scan
cargo run -- upload -f test.pfr1 -v
```

## Troubleshooting

### Device Not Found

- Ensure the PhotoFrame device is powered on
- Check that Bluetooth is enabled on your computer
- Move closer to the device (BLE has limited range)
- Run a scan first to verify the device is visible

### Upload Fails

- Verify the file is in `.pfr1` format
- Check that the device has sufficient storage
- Ensure no other application is connected to the device
- Try uploading with verbose mode (`-v`) for more details

### Linux Permissions

On Linux, you may need to add your user to the `bluetooth` group:

```bash
sudo usermod -aG bluetooth $USER
```

Or run with elevated privileges:

```bash
sudo ./bt_uploader upload -f image.pfr1
```

## Related Projects

- `photoframe-lib` - Shared library for PhotoFrame binary format
- ESP32 PhotoFrame firmware - The embedded device firmware

## Author

Alessandro Crugnola (@sephiroth74)
