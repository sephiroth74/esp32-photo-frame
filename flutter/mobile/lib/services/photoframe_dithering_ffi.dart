import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

/// FFI result structure matching Rust
final class DitheringResult extends ffi.Struct {
  @ffi.Bool()
  external bool success;

  @ffi.Uint32()
  external int width;

  @ffi.Uint32()
  external int height;

  external ffi.Pointer<ffi.Uint8> dataPtr;

  @ffi.Size()
  external int dataLen;
}

// Typedef per la funzione Rust
typedef PhotoframeDummyFunctionNative = ffi.Uint32 Function();
typedef PhotoframeDummyFunctionDart = int Function();

/// Typedef for dithering_apply FFI function
typedef DitheringApplyNative =
    DitheringResult Function(
      ffi.Pointer<ffi.Uint8> imageData,
      ffi.Size imageLen,
      ffi.Pointer<ffi.Char> method,
      ffi.Uint8 displayType,
      ffi.Float ditherStrength,
      ffi.Float saturation,
      ffi.Float contrast,
      ffi.Float brightness,
    );

typedef DitheringApplyDart =
    DitheringResult Function(
      ffi.Pointer<ffi.Uint8> imageData,
      int imageLen,
      ffi.Pointer<ffi.Char> method,
      int displayType,
      double ditherStrength,
      double saturation,
      double contrast,
      double brightness,
    );

/// Typedef for free FFI function
typedef DitheringFreeNative = ffi.Void Function(ffi.Pointer<ffi.Uint8> ptr, ffi.Size len);

typedef DitheringFreeDart = void Function(ffi.Pointer<ffi.Uint8> ptr, int len);

/// Typedef for convert_with_processing FFI function
typedef ConvertWithProcessingNative =
    DitheringResult Function(ffi.Pointer<ffi.Uint8> imageData, ffi.Size imageLen, ffi.Uint8 processingType, ffi.Uint8 rotation);

typedef ConvertWithProcessingDart = DitheringResult Function(ffi.Pointer<ffi.Uint8> imageData, int imageLen, int processingType, int rotation);

class PhotoframeDithering {
  static final ffi.DynamicLibrary _dylib = _loadLibrary();

  static ffi.DynamicLibrary _loadLibrary() {
    if (Platform.isAndroid) {
      return ffi.DynamicLibrary.open('libphotoframe_lib.so');
    } else if (Platform.isIOS) {
      return ffi.DynamicLibrary.process();
    } else if (Platform.isMacOS) {
      return ffi.DynamicLibrary.open('libphotoframe_lib.dylib');
    } else if (Platform.isLinux) {
      return ffi.DynamicLibrary.open('libphotoframe_lib.so');
    } else if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('photoframe_lib.dll');
    }
    throw UnsupportedError('Unsupported platform');
  }

  static final DitheringApplyDart _apply = _dylib.lookup<ffi.NativeFunction<DitheringApplyNative>>('photoframe_dithering_apply').asFunction();

  static final DitheringFreeDart _free = _dylib.lookup<ffi.NativeFunction<DitheringFreeNative>>('photoframe_dithering_free').asFunction();

  static final PhotoframeDummyFunctionDart dummyFunction = _dylib
      .lookup<ffi.NativeFunction<PhotoframeDummyFunctionNative>>('photoframe_dummy_function')
      .asFunction();

  static final ConvertWithProcessingDart _convertWithProcessing = _dylib
      .lookup<ffi.NativeFunction<ConvertWithProcessingNative>>('photoframe_convert_with_processing')
      .asFunction();

  /// Apply dithering to image bytes
  ///
  /// Returns PNG bytes or null on error
  static Uint8List? apply({
    required Uint8List imageBytes,
    required String method,
    required int displayType,
    required double ditherStrength,
    double saturation = 1.0,
    double contrast = 1.0,
    double brightness = 1.0,
  }) {
    // Allocate native memory for input
    final inputPtr = malloc.allocate<ffi.Uint8>(imageBytes.length);
    final inputList = inputPtr.asTypedList(imageBytes.length);
    inputList.setAll(0, imageBytes);

    try {
      // Call FFI function
      final methodPtr = method.toNativeUtf8().cast<ffi.Char>();
      final result = _apply(inputPtr, imageBytes.length, methodPtr, displayType, ditherStrength, saturation, contrast, brightness);
      malloc.free(methodPtr);

      if (!result.success || result.dataPtr == ffi.nullptr) {
        return null;
      }

      // Copy result to Dart memory
      final outputList = result.dataPtr.asTypedList(result.dataLen);
      final output = Uint8List.fromList(outputList);

      // Free Rust-allocated memory
      _free(result.dataPtr, result.dataLen);

      return output;
    } finally {
      // Free input memory
      malloc.free(inputPtr);
    }
  }

  /// Convert image to binary format for ESP32 display with PFR1 header
  ///
  /// Returns binary bytes with PFR1 header or null on error
  /// processingType: 0 = BlackAndWhite, 1 = SixColors
  /// rotation: 0-3 (0=landscape, 1=portrait, 2=180°, 3=270°)
  static Uint8List? convertWithProcessing({required Uint8List imageBytes, required int processingType, required int rotation}) {
    // Allocate native memory for input
    final inputPtr = malloc.allocate<ffi.Uint8>(imageBytes.length);
    final inputList = inputPtr.asTypedList(imageBytes.length);
    inputList.setAll(0, imageBytes);

    try {
      // Call FFI function
      final result = _convertWithProcessing(inputPtr, imageBytes.length, processingType, rotation);

      if (!result.success || result.dataPtr == ffi.nullptr) {
        return null;
      }

      // Copy result to Dart memory
      final outputList = result.dataPtr.asTypedList(result.dataLen);
      final output = Uint8List.fromList(outputList);

      // Free Rust-allocated memory
      _free(result.dataPtr, result.dataLen);

      return output;
    } finally {
      // Free input memory
      malloc.free(inputPtr);
    }
  }
}
