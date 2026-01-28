import 'dart:typed_data';

import '../models/processing_models.dart';
import '../utils/app_logger.dart';
import 'photoframe_dithering_ffi.dart';

class BinaryConverter {
  /// Convert image to binary format for ESP32 display using Rust library
  static Uint8List? convertToBinary(
    Uint8List imageBytes,
    DisplayType displayType, {
    double saturation = 1.0,
    double contrast = 0.0,
    double brightness = 0.0,
    int rotation = 0,
  }) {
    logger.fine(
      'Converting image to binary: displayType=$displayType, '
      'sat=$saturation, contrast=$contrast, brightness=$brightness',
    );

    final stopwatch = Stopwatch()..start();

    // Map enum to processing type expected by FFI: 0 = BW, 1 = 6C
    final processingType = displayType == DisplayType.blackAndWhite ? 0 : 1;

    logger.fine('Calling Rust FFI convert_with_processing with ${imageBytes.length} bytes');

    // Call native Rust library (rotation fixed to 0; rotation handled via BLE config)
    final result = PhotoframeDithering.convertWithProcessing(imageBytes: imageBytes, processingType: processingType, rotation: rotation);

    stopwatch.stop();

    if (result != null) {
      logger.info('Binary conversion completed in ${stopwatch.elapsedMilliseconds}ms, output ${result.length} bytes');
    } else {
      logger.severe('Binary conversion failed');
    }

    return result;
  }
}
