import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

/// FFI result structure matching Rust BinValidationResult
final class BinValidationResultNative extends ffi.Struct {
  @ffi.Bool()
  external bool success;

  @ffi.Uint8()
  external int version;

  @ffi.Uint16()
  external int headerLen;

  @ffi.Uint16()
  external int width;

  @ffi.Uint16()
  external int height;

  @ffi.Uint8()
  external int rotation;

  @ffi.Uint8()
  external int colorMode;

  @ffi.Uint32()
  external int payloadLen;

  @ffi.Uint32()
  external int payloadCrc32;
}

/// Dart-side wrapper for validation result
class BinValidationResult {
  final bool success;
  final int version;
  final int headerLen;
  final int width;
  final int height;
  final int rotation;
  final int colorMode;
  final int payloadLen;
  final int payloadCrc32;

  const BinValidationResult({
    required this.success,
    required this.version,
    required this.headerLen,
    required this.width,
    required this.height,
    required this.rotation,
    required this.colorMode,
    required this.payloadLen,
    required this.payloadCrc32,
  });

  factory BinValidationResult.fromNative(BinValidationResultNative native) {
    return BinValidationResult(
      success: native.success,
      version: native.version,
      headerLen: native.headerLen,
      width: native.width,
      height: native.height,
      rotation: native.rotation,
      colorMode: native.colorMode,
      payloadLen: native.payloadLen,
      payloadCrc32: native.payloadCrc32,
    );
  }

  @override
  String toString() {
    if (!success) {
      return 'BinValidationResult(failed)';
    }
    return 'BinValidationResult(v$version, ${width}x${height}, colorMode=$colorMode, rotation=$rotation)';
  }
}

/// Typedef for validate_bin FFI function
typedef ValidateBinNative = BinValidationResultNative Function(ffi.Pointer<ffi.Uint8> dataPtr, ffi.Size dataLen);

typedef ValidateBinDart = BinValidationResultNative Function(ffi.Pointer<ffi.Uint8> dataPtr, int dataLen);

/// FFI wrapper for PFR1 .bin file validation
class PhotoframeValidator {
  static final ffi.DynamicLibrary _dylib = _loadLibrary();

  static ffi.DynamicLibrary _loadLibrary() {
    if (Platform.isMacOS) {
      return ffi.DynamicLibrary.open('libphotoframe_lib.dylib');
    } else if (Platform.isLinux) {
      return ffi.DynamicLibrary.open('libphotoframe_lib.so');
    } else if (Platform.isWindows) {
      return ffi.DynamicLibrary.open('photoframe_lib.dll');
    }
    throw UnsupportedError('Unsupported platform for PFR1 validation');
  }

  static final ValidateBinDart _validateBin = _dylib.lookup<ffi.NativeFunction<ValidateBinNative>>('photoframe_validate_bin').asFunction();

  /// Validate a PFR1 .bin file
  ///
  /// Returns [BinValidationResult] with header metadata if valid.
  /// On error, success=false.
  static BinValidationResult validate(Uint8List data) {
    // Allocate native memory for input
    final inputPtr = malloc.allocate<ffi.Uint8>(data.length);
    final inputList = inputPtr.asTypedList(data.length);
    inputList.setAll(0, data);

    try {
      // Call FFI function
      final result = _validateBin(inputPtr, data.length);
      return BinValidationResult.fromNative(result);
    } finally {
      // Free input memory
      malloc.free(inputPtr);
    }
  }

  /// Validate a PFR1 .bin file from file path
  static BinValidationResult validateFile(String filePath) {
    try {
      final file = File(filePath);
      final data = file.readAsBytesSync();
      return validate(data);
    } catch (e) {
      // Return failed result if file read fails
      return BinValidationResult(
        success: false,
        version: 0,
        headerLen: 0,
        width: 0,
        height: 0,
        rotation: 0,
        colorMode: 0,
        payloadLen: 0,
        payloadCrc32: 0,
      );
    }
  }
}
