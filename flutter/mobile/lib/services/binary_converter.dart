import 'dart:typed_data';

import 'package:photoframe_common/photoframe_common.dart';

import '../utils/app_logger.dart';
import 'photoframe_dithering_ffi.dart';

class BinaryConverter {
  /// Convert image to binary format for ESP32 display using Rust library
  static Uint8List? convertToBinary(Uint8List imageBytes, {required ColorMode colorMode, required Orientation orientation}) {
    logger.fine('Converting image to binary: colorMode=$colorMode, orientation=$orientation');

    final stopwatch = Stopwatch()..start();

    logger.fine('Calling Rust FFI convert_with_processing with ${imageBytes.length} bytes');

    // Call native Rust library (rotation fixed to 0; rotation handled via BLE config)
    final result = PhotoframeDithering.convertWithProcessing(imageBytes: imageBytes, processingType: colorMode.value, rotation: orientation.toInt());

    stopwatch.stop();

    if (result != null) {
      logger.info('Binary conversion completed in ${stopwatch.elapsedMilliseconds}ms, output ${result.length} bytes');
    } else {
      logger.severe('Binary conversion failed');
    }

    return result;
  }
}
