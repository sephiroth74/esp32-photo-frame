import 'dart:typed_data';

import 'package:photoframe_common/models/bin_model.dart';
import 'package:photoframe_common/photoframe_common.dart';

/// FFI validation result (used when validation succeeds)
import 'bin_validator_ffi.dart' show PhotoframeValidator, BinValidationResult;

/// Create BinHeader from FFI validation result
BinHeader _fromValidationResult(BinValidationResult result) {
  return BinHeader(
    version: result.version,
    headerLen: result.headerLen,
    width: result.width,
    height: result.height,
    orientation: orientationFromInt(result.rotation),
    colorMode: colorModeFromInt(result.colorMode),
    payloadLen: result.payloadLen,
    headerCrc32: 0, // Not available from validation result
  );
}

/// Parse using C FFI validation (preferred on desktop)
ParsedBin parseWithValidation(Uint8List data) {
  final result = PhotoframeValidator.validate(data);
  if (!result.success) {
    throw FormatException('BIN validation failed');
  }

  // Extract header
  final header = _fromValidationResult(result);

  // Extract payload
  final payloadStart = BinHeader.headerSize;
  final payloadEnd = payloadStart + result.payloadLen;
  if (data.length < payloadEnd + 4) {
    throw FormatException('BIN truncated: insufficient data');
  }
  final payload = Uint8List.sublistView(data, payloadStart, payloadEnd);

  return ParsedBin(header, payload, result.payloadCrc32);
}
