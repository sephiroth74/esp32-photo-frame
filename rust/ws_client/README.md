# PhotoFrame WebSocket Client

> Interactive WebSocket client for ESP32 Photo Frame devices

A command-line tool for communicating with ESP32-based PhotoFrame devices over WebSocket. 
This tool provides an interactive menu-driven interface for managing PhotoFrame devices.

## Features

- **Interactive Menu**: Dialoguer-based menu with arrow key navigation
- **Get Configuration**: Retrieve device configuration and status
- **Image Upload**: Transfer `.pfr1` binary image files via WebSocket with progress tracking
- **Orientation Control**: Set display rotation (0-3) during upload
- **Shutdown Control**: Put device into deep sleep mode
- **Error Reporting**: Display server errors in real-time (e.g., filesystem errors, upload failures)

## Installation

### Prerequisites

- Rust 1.75+ (edition 2024)
- ESP32 PhotoFrame device running WebSocket server
- Network connectivity to the device (WiFi or AP mode)

### Build from Source

```bash
# Clone the repository
git clone <repository-url>
cd ws_client

# Build release version
cargo build --release
****
# The binary will be in target/release/ws_client
```

## Usage

### Connect to Device

Start an interactive session with your PhotoFrame device:

```bash
ws_client connect 192.168.4.1:81
```

The client will connect and display an interactive menu with the following options:

1. **Get device configuration** - Retrieve and display device info, battery status, display specs
2. **Upload image file** - Transfer a `.pfr1` image with orientation control
3. **Shutdown device** - Put the device into deep sleep mode
4. **Disconnect and exit** - Close the connection

### Error Messages

The client automatically displays server errors in red, including:
- Filesystem errors (e.g., "File system is not mounted")
- Upload failures (e.g., "Failed to open temp file")
- Size limit violations (e.g., "File size limit exceeded")
- Write errors (e.g., "Write failed")


### Orientation Values

The `orientation` flag accepts values 0-3:

- `0` - Landscape (default)
- `1` - Portrait
- `2` - Landscape (reverse/180°)
- `3` - Portrait (reverse/180°)

## File Format

The tool expects binary image files in the `.pfr1` (Photo Frame Rev1) format. These files contain:

- Header with image dimensions, timestamp, and CRC
- Binary image data
- Metadata for device configuration

## Error Handling

The tool provides clear error messages for common issues:

- Device not found or unreachable
- Invalid file format or missing file
- Connection timeout
- Invalid orientation value

## Troubleshooting

### Device Not Found

- Ensure the PhotoFrame device is powered on
- Check that on your computer is connected to the same network as the device Wifi SSID

### Upload Fails

- Verify the file is in `.pfr1` format
- Check that the device has sufficient storage
- Ensure no other application is connected to the device

## Related Projects

- `photoframe-lib` - Shared library for PhotoFrame binary format
- ESP32 PhotoFrame firmware - The embedded device firmware

## Author

Alessandro Crugnola (@sephiroth74)
