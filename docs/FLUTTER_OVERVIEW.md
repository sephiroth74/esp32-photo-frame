# Flutter Applications Overview

The `flutter/` directory contains the cross-platform applications that serve as the user interface for the ESP32 Photo Frame ecosystem. These apps allow users to process images, manage configurations, and upload content to the frame.

## Project Structure

The Flutter workspace is divided into two main projects:

### 🖥️ Desktop Application (`flutter/desktop`)
**Location:** [`flutter/desktop`](../flutter/desktop/README.md)

A native-feeling desktop application for **macOS**, **Windows**, and **Linux**.
- **GUI for Rust Processor**: Acts as a user-friendly frontend for the Rust `processor` CLI.
- **Batch Processing**: Drag-and-drop support for processing multiple images at once.
- **Preview**: Visual preview of dithering and cropping results before saving.
- **Config Management**: Easy editing of the frame's `config.json`.

[👉 Read Desktop App Documentation](../flutter/desktop/README.md)

---

### 📱 Mobile Application (`flutter/mobile`)
**Location:** [`flutter/mobile`](../flutter/mobile/README.md)

A companion app for **iOS** and **Android**.
- **On-Device Processing**: Uses the Rust core libraries (via FFI) to dither and convert images directly on your phone.
- **Bluetooth Upload**: Wirelessly transfer images to the frame without needing a WiFi connection (requires `ENABLE_BT_IMAGE` firmware build).
- **Share Extension**: Send photos directly from your phone's gallery to the app.

[👉 Read Mobile App Documentation](../flutter/mobile/README.md)
