import 'dart:typed_data';

import 'package:photoframe_common/photoframe_common.dart';

/// Local model for parsed .pfr1 binary data (used in UI layer)
class BinHeader {
  static const int magic = 0x50465231; // 'PFR1' LE
  static const int headerSize = 21;

  final int version;
  final int headerLen;
  final int width;
  final int height;
  final Orientation orientation;
  final ColorMode colorMode;
  final int payloadLen;
  final int headerCrc32;

  const BinHeader({
    required this.version,
    required this.headerLen,
    required this.width,
    required this.height,
    required this.orientation,
    required this.colorMode,
    required this.payloadLen,
    required this.headerCrc32,
  });

  int getWidth() => orientation.value % 2 == 0 ? width : height;
  int getHeight() => orientation.value % 2 == 0 ? height : width;
}

/// Local model for parsed .pfr1 binary data (used in UI layer)
class ParsedBin {
  final BinHeader header;
  final Uint8List payload;
  final int payloadCrc32;

  const ParsedBin(this.header, this.payload, this.payloadCrc32);

  Uint8List decodeToRgba() {
    final w = header.width;
    final h = header.height;
    if (payload.lengthInBytes < w * h) {
      // Pad with white
      final padded = Uint8List(w * h);
      padded.fillRange(0, padded.length, 0xFF);
      padded.setRange(0, payload.lengthInBytes, payload);
      return _mapPixels(padded, w, h, header.colorMode.toInt());
    }
    return _mapPixels(payload, w, h, header.colorMode.toInt());
  }

  static Uint8List _mapPixels(Uint8List src, int width, int height, int colorMode) {
    final out = Uint8List(width * height * 4);
    if (colorMode == 0) {
      // Black & White: 0x00 = black, 0xFF = white
      for (int i = 0; i < width * height; i++) {
        final v = src[i];
        final o = i * 4;
        if (v >= 0x80) {
          out[o] = 0xFF;
          out[o + 1] = 0xFF;
          out[o + 2] = 0xFF;
          out[o + 3] = 0xFF;
        } else {
          out[o] = 0x00;
          out[o + 1] = 0x00;
          out[o + 2] = 0x00;
          out[o + 3] = 0xFF;
        }
      }
      return out;
    }
    // 6-color mode1 codes
    for (int i = 0; i < width * height; i++) {
      final b = src[i];
      int r = 0, g = 0, bl = 0;
      switch (b) {
        case 0xFF: // white
          r = g = bl = 0xFF;
          break;
        case 0xFC: // yellow
          r = 0xFF;
          g = 0xFF;
          bl = 0x00;
          break;
        case 0xE0: // red
          r = 0xFF;
          g = 0x00;
          bl = 0x00;
          break;
        case 0x1C: // green
          r = 0x00;
          g = 0xFF;
          bl = 0x00;
          break;
        case 0x03: // blue
          r = 0x00;
          g = 0x00;
          bl = 0xFF;
          break;
        case 0x00: // black
        default:
          r = g = bl = 0x00;
          break;
      }
      final o = i * 4;
      out[o] = r;
      out[o + 1] = g;
      out[o + 2] = bl;
      out[o + 3] = 0xFF;
    }
    return out;
  }
}

/// Local model for data needed to display a .pfr1 image (used in UI layer)
class BinParser {
  static const int _magic = BinHeader.magic;
  static const int _headerSize = BinHeader.headerSize;

  static ParsedBin parse(Uint8List data) {
    if (data.lengthInBytes < _headerSize + 4) {
      throw FormatException('BIN too small: ${data.lengthInBytes}');
    }

    final magic = _le32(data, 0);
    if (magic != _magic) {
      throw FormatException('Invalid BIN magic 0x${magic.toRadixString(16)}');
    }
    final version = data[4];
    final headerLen = _le16(data, 5);
    if (headerLen != _headerSize) {
      throw FormatException('Unexpected header length $headerLen');
    }
    final width = _le16(data, 7);
    final height = _le16(data, 9);
    final rotation = data[11];
    final color = data[12];
    final payloadLen = _le32(data, 13);
    final headerCrc = _le32(data, 17);

    if (width <= 0 || height <= 0) {
      throw FormatException('Invalid dimensions ${width}x$height');
    }

    if (payloadLen <= 0) {
      throw FormatException('Invalid payload length $payloadLen');
    }

    final calcHeaderCrc = _crc32(data.sublist(0, _headerSize - 4));
    if (calcHeaderCrc != headerCrc) {
      throw FormatException('Header CRC mismatch');
    }

    final total = _headerSize + payloadLen + 4;
    if (data.lengthInBytes < total) {
      throw FormatException('BIN truncated: have ${data.lengthInBytes}, need $total');
    }

    final payload = Uint8List.sublistView(data, _headerSize, _headerSize + payloadLen);
    final payloadCrc = _le32(data, _headerSize + payloadLen);

    return ParsedBin(
      BinHeader(
        version: version,
        headerLen: headerLen,
        width: width,
        height: height,
        orientation: orientationFromInt(rotation),
        colorMode: colorModeFromInt(color),
        payloadLen: payloadLen,
        headerCrc32: headerCrc,
      ),
      payload,
      payloadCrc,
    );
  }

  static int _le16(Uint8List d, int off) => d[off] | (d[off + 1] << 8);

  static int _le32(Uint8List d, int off) => d[off] | (d[off + 1] << 8) | (d[off + 2] << 16) | (d[off + 3] << 24);

  // Simple CRC32 implementation (poly 0xEDB88320)
  static final List<int> _crcTable = _makeCrcTable();

  static List<int> _makeCrcTable() {
    final t = List<int>.filled(256, 0);
    for (int n = 0; n < 256; n++) {
      int c = n;
      for (int k = 0; k < 8; k++) {
        c = (c & 1) != 0 ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
      }
      t[n] = c;
    }
    return t;
  }

  static int _crc32(Uint8List data) {
    int c = 0xFFFFFFFF;
    for (final b in data) {
      c = _crcTable[(c ^ b) & 0xFF] ^ (c >>> 8);
    }
    return (c ^ 0xFFFFFFFF) & 0xFFFFFFFF;
  }
}
