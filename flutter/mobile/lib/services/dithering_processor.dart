import 'dart:typed_data';

import '../models/processing_models.dart';
import '../utils/app_logger.dart';
import 'photoframe_dithering_ffi.dart';

class DitheringProcessor {
  /// Apply dithering using native Rust library via FFI
  static Uint8List apply(
    Uint8List inputBytes,
    DisplayType displayType,
    DitheringMethod method, {
    double ditherStrength = 1.0,
    double saturation = 1.0,
    double contrast = 1.0,
    double brightness = 1.0,
  }) {
    logger.fine(
      'Applying dithering: method=$method, displayType=$displayType, '
      'strength=$ditherStrength, sat=$saturation, contrast=$contrast, brightness=$brightness',
    );

    final stopwatch = Stopwatch()..start();

    // Map enums to FFI values
    final ffiMethod = _mapDitheringMethod(method);
    final ffiDisplay = _mapDisplayType(displayType);

    logger.fine('Calling Rust FFI with ${inputBytes.length} bytes');
    logger.fine('Dithering params: method=${ffiMethod.value}, display=${ffiDisplay.value}');

    // Call native Rust library
    final result = PhotoframeDithering.apply(
      imageBytes: inputBytes,
      method: ffiMethod.value,
      displayType: ffiDisplay.value,
      ditherStrength: ditherStrength,
      saturation: saturation,
      contrast: contrast,
      brightness: brightness,
    );

    stopwatch.stop();

    if (result != null) {
      logger.info('Dithering completed in ${stopwatch.elapsedMilliseconds}ms, output ${result.length} bytes');
    } else {
      logger.severe('Dithering failed, returning original image');
    }

    // Return result or original on error
    return result ?? inputBytes;
  }

  static DitheringMethodFFI _mapDitheringMethod(DitheringMethod method) {
    switch (method) {
      case DitheringMethod.floydSteinberg:
        return DitheringMethodFFI.floydSteinberg;
      case DitheringMethod.atkinson:
        return DitheringMethodFFI.atkinson;
      case DitheringMethod.stucki:
        return DitheringMethodFFI.stucki;
      case DitheringMethod.jarvisJudiceNinke:
        return DitheringMethodFFI.jarvisJudiceNinke;
      case DitheringMethod.ordered:
        return DitheringMethodFFI.ordered;
    }
  }

  static DisplayTypeFFI _mapDisplayType(DisplayType type) {
    switch (type) {
      case DisplayType.sixColors:
        return DisplayTypeFFI.sixColors;
      case DisplayType.blackAndWhite:
        return DisplayTypeFFI.blackAndWhite;
    }
  }
}
