# ESP32 Photo Frame Mobile App

The **Photo Frame Companion App** allows you to process and upload images directly to your e-paper frame from your iOS or Android device.

## Features

- **📸 Image Processing**: Select images from your gallery and convert them to the frame's native `.pfr1` format.
- **🎨 On-Device Dithering**: Uses the core Rust library (via FFI) to apply high-quality dithering (Floyd-Steinberg, Atkinson, etc.) directly on your phone.
- **📡 Bluetooth Upload**: Transfer processed images wirelessly to the frame (requires firmware with `ENABLE_BT_IMAGE`).
- **📏 Smart Crop**: Adjust crop and composition before processing.

## 🛠️ Build Prerequisites

This project uses the `photoframe_lib` Rust library via FFI (Foreign Function Interface) for heavy image processing tasks. You **must** compile the Rust library before building the Flutter app.

### 1. Rust Environment
Ensure you have Rust and Cargo installed, along with the necessary cross-compilation targets:
```bash
rustup target add aarch64-linux-android armv7-linux-androideabi i686-linux-android x86_64-linux-android
rustup target add aarch64-apple-ios x86_64-apple-ios aarch64-apple-ios-sim
```
*(The scripts below will attempt to install these targets automatically)*

### 2. Compile Rust Bindings

Run the appropriate script for your target platform from the `scripts/` directory:

**For Android:**
```bash
cd scripts
./build_rust_android.sh
```

**For macOS/iOS (Simulator):**
```bash
cd scripts
./build_rust_macos.sh
```

These scripts will:
1. Compile the `photoframe_lib` Rust crate.
2. Generate the necessary `.so` (Android) or `.dylib/.a` (iOS/macOS) library files.
3. Place them in the correct `jniLibs` or Flutter directory.

## 🚀 Running the App

Once the Rust libraries are compiled:

1. **Install Flutter Dependencies**:
   ```bash
   flutter pub get
   ```

2. **Run on Device**:
   ```bash
   flutter run
   ```

## Troubleshooting

- **"Library not found" errors**: Ensure you ran the build script corresponding to your target device/emulator.
- **Bluetooth Permissions**: On Android 12+, ensure you grant "Nearby Devices" permissions when prompted.
- **Connection Issues**: Make sure the Frame is in Bluetooth mode (Blue LED indicator active).
