# Photoframe Dithering Library

Shared Rust library for dithering operations used by both the Rust processor and Flutter mobile app.

## Features

- Floyd-Steinberg dithering
- Atkinson dithering
- Stucki dithering
- Jarvis-Judice-Ninke dithering
- Ordered (Bayer) dithering
- Perceptual color matching
- Gamma correction for uniform results
- FFI interface for Flutter/Dart

## Building

### For Rust usage

```bash
cargo build --release
```

### For Flutter/macOS

```bash
cd ../../flutter/mobile
./scripts/build_rust_macos.sh
```

### For Flutter/Android and iOS

```bash
cd ../../flutter/mobile
export ANDROID_NDK_HOME=/path/to/ndk
./scripts/build_rust_android.sh
```

```bash
./scripts/build_rust_ios.sh
```

