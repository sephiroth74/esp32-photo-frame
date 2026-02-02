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

### For Flutter/Android

```bash
cd ../../flutter/mobile
export ANDROID_NDK_HOME=/path/to/ndk
./scripts/build_rust_android.sh
```

## Usage in Rust

```rust
use photoframe_lib::{apply_dithering, DitheringMethod, DisplayType};

let result = apply_dithering(
    &image,
    DitheringMethod::FloydSteinberg,
    DisplayType::SixColors,
    0.8, // dither strength
);
```

## Usage in Flutter

```dart
import 'package:photoframe/services/photoframe_dithering_ffi.dart';

final result = PhotoframeDithering.apply(
  imageBytes: imageData,
  method: DitheringMethodFFI.floydSteinberg.value,
  displayType: DisplayTypeFFI.sixColors.value,
  ditherStrength: 0.8,
  saturation: 1.0,
  contrast: 1.0,
  brightness: 1.0,
);
```

## Usage: photoframe_convert_with_processing

This crate exposes a simple FFI entrypoint `photoframe_convert_with_processing` that converts image bytes to a device-friendly payload according to a `ProcessingType`.

- `processing_type` mapping:
  - `0` => BlackAndWhite (demo bitmap mode 1 codes: 0x00 black, 0xFF white)
  - `1` => SixColors (demo bitmap: u16 width LE, u16 height LE, then one byte per pixel palette index)

Below are short examples showing how to call the function from Rust (unsafe FFI call) and from Flutter/Dart via `dart:ffi`.

### Rust example (calling the FFI entrypoint)

```rust
use photoframe_lib::{photoframe_convert_with_processing, photoframe_dithering_free, DitheringResult};
use std::slice;

let image_bytes = std::fs::read("tests/data/default_portrait.jpg").expect("read image");

unsafe {
    // 0 => BlackAndWhite, 1 => SixColors
    let res: DitheringResult = photoframe_convert_with_processing(image_bytes.as_ptr(), image_bytes.len(), 1);

    if res.success {
        // Borrow the returned buffer as a slice (do not take ownership!)
        let payload: &[u8] = slice::from_raw_parts(res.data_ptr, res.data_len);
        println!("Converted payload length = {} ({}x{})", payload.len(), res.width, res.height);

        // Use `payload` (copy it if you need to keep it).

        // Free the buffer allocated by Rust
        photoframe_dithering_free(res.data_ptr, res.data_len);
    } else {
        eprintln!("Conversion failed");
    }
}
```

### Flutter / Dart example (via dart:ffi)

This is a minimal example sketch showing the FFI wiring. In production you should add error handling and avoid raw allocs where possible.

```dart
// Dart side (lib/ffi_bindings.dart)
import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:typed_data';

class DitheringResult extends ffi.Struct {
  @ffi.Uint8()
  external int success; // bool as u8
  @ffi.Uint32()
  external int width;
  @ffi.Uint32()
  external int height;
  external ffi.Pointer<ffi.Uint8> data_ptr;
  @ffi.Uint64()
  external int data_len;
}

typedef _ConvertFn = DitheringResult Function(ffi.Pointer<ffi.Uint8>, ffi.Uint64, ffi.Uint8);
typedef _Convert = DitheringResult Function(ffi.Pointer<ffi.Uint8>, int, int);

final dylib = ffi.DynamicLibrary.open('libphotoframe_lib.dylib'); // or .so on Android
final _convert = dylib.lookupFunction<_ConvertFn, _Convert>('photoframe_convert_with_processing');
final _free = dylib.lookupFunction<ffi.Void Function(ffi.Pointer<ffi.Uint8>, ffi.Uint64), void Function(ffi.Pointer<ffi.Uint8>, int)>('photoframe_dithering_free');

void convertExample(Uint8List imageBytes) {
  final ptr = ffi.calloc.allocate<ffi.Uint8>(imageBytes.length);
  final nativeBytes = ptr.asTypedList(imageBytes.length);
  nativeBytes.setAll(0, imageBytes);

  final result = _convert(ptr, imageBytes.length, 1); // 1 = SixColors

  if (result.success != 0) {
    final payload = result.data_ptr.asTypedList(result.data_len);
    // Copy payload into a Dart-managed buffer if you need to keep it
    final copied = Uint8List.fromList(payload);
    print('Converted payload length: ${copied.length}, dims: ${result.width}x${result.height}');

    // Free Rust-allocated memory
    _free(result.data_ptr, result.data_len);
  } else {
    print('Conversion failed');
  }

  ffi.calloc.free(ptr);
}
```

Notes
- The FFI function returns a heap-allocated buffer owned by Rust. Always call `photoframe_dithering_free(ptr, len)` after you finish using the buffer to avoid memory leaks.
- For `ProcessingType::BlackAndWhite` the returned binary is compact (bit-packed rows). For `SixColors` the demo wrapper contains a 4-byte header (width/height little-endian) followed by palette indices.

## PFR1 .pfr1 format (header + payload)

The library exposes helpers to build and parse PFR1 binaries via `build_bin_file` / `parse_bin_file` in `bin_format.rs`.

**Header layout (21 bytes):**
- 0-3   : Magic `PFR1` (0x50 0x46 0x52 0x31 little-endian)
- 4     : Version (u8)
- 5-6   : Header length (u16 LE), currently 21
- 7-8   : Width (u16 LE)
- 9-10  : Height (u16 LE)
- 11    : Rotation (u8, degrees mod 360; 0/90/180/270 expected)
- 12    : Color mode (u8) — 0 = BW, 1 = 6C
- 13-16 : Payload length (u32 LE)
- 17-20 : Header CRC32 (over bytes 0..16)

**Payload:** pixel payload as produced by the processing pipeline, followed by a 4-byte CRC32 of the payload.

**Rust example (build + parse):**
```rust
use photoframe_lib::bin_format::{build_bin_file, parse_bin_file, BIN_HEADER_SIZE};

let payload: Vec<u8> = vec![0u8; 1024];
let bin = build_bin_file(&payload, 800, 480, 0, 1, 1);
assert!(bin.len() >= BIN_HEADER_SIZE + payload.len() + 4);

let (hdr, data, crc) = parse_bin_file(&bin)?;
assert_eq!(hdr.width, 800);
assert_eq!(hdr.height, 480);
assert_eq!(hdr.rotation, 0);
assert_eq!(hdr.color_mode, 1);
assert_eq!(hdr.payload_len as usize, payload.len());
assert_eq!(data.len(), payload.len());
// crc matches the payload CRC32 appended after the payload
```

## Architecture

- `src/core.rs` - Core dithering algorithms
- `src/lib.rs` - FFI interface and Rust exports
- Compiled as both `rlib` (for Rust) and `cdylib` (for FFI)
