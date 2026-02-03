# Rust Components Overview

The `rust/` directory contains the high-performance core of the ESP32 Photo Frame ecosystem. These components handle the heavy lifting of image processing, binary format generation (`.pfr1`), and device communication.

Built with Rust, these tools ensure type safety, memory safety, and native performance across all supported platforms (macOS, Windows, Linux).

## Project Structure

The Rust workspace is divided into several crates, each serving a specific purpose:

### 📚 Core Library (`photoframe_lib`)
**Location:** [`rust/photoframe_lib`](../rust/photoframe_lib/README.md)

This is the foundational library used by all other Rust components. It implements:
- The **`.pfr1` binary format** specification (headers, payloads, CRC32 validation).
- **Dithering algorithms** (Floyd-Steinberg, Atkinson, Sierra, etc.) to convert images for e-paper.
- Color palette mapping for 6-color and Black/White displays.

[👉 Read `photoframe_lib` Documentation](../rust/photoframe_lib/README.md)

---

### ⚙️ Image Processor (`processor`)
**Location:** [`rust/processor`](../rust/processor/README.md)

The official Command Line Interface (CLI) tool for the project. It powers the Flutter Desktop App but can also be used standalone for batch processing.
- Handles image resizing, cropping, and padding.
- Integrates **InsightFace** for smart face-centered cropping.
- Generates valid `.pfr1` files ready for the ESP32.

[👉 Read `processor` Documentation](../rust/processor/README.md)

---

### 🧠 InsightFace (`insightface_rs`)
**Location:** [`rust/insightface_rs`](../rust/insightface_rs/README.md)

A specialized Rust crate providing bindings and logic for the **InsightFace** deep learning model.
- Used by the `processor` to detect faces in images.
- Ensures that when cropping landscape photos for a portrait frame (or vice versa), the subjects remain in the frame.

[👉 Read `insightface_rs` Documentation](../rust/insightface_rs/README.md)

---

### 📡 Bluetooth Uploader (`bt_uploader`)
**Location:** [`rust/bt_uploader`](../rust/bt_uploader/README.md)

A utility tool for communicating with the photo frame via Bluetooth Low Energy (BLE).
- Allows uploading `.pfr1` files directly to the device without WiFi.
- Primarily used for testing transfer protocols and integration with the firmware's `ENABLE_BT_IMAGE` mode.

[👉 Read `bt_uploader` Documentation](../rust/bt_uploader/README.md)
