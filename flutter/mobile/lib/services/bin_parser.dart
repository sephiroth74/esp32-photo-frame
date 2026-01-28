import 'dart:async';
import 'dart:typed_data';
import 'dart:ui' as ui;

class BinHeader {
  final int version;
  final int headerLen;
  final int width;
  final int height;
  final int rotation;
  final int colorMode; // 0 = BW, 1 = 6C
  final int payloadLen;
  final int headerCrc32;

  const BinHeader({
    required this.version,
    required this.headerLen,
    required this.width,
    required this.height,
    required this.rotation,
    required this.colorMode,
    required this.payloadLen,
    required this.headerCrc32,
  });
}

class ParsedBin {
  final BinHeader header;
  final Uint8List payload;
  final int payloadCrc32;

  const ParsedBin(this.header, this.payload, this.payloadCrc32);
}

class BinParser {
  static const int _magic = 0x50465231; // 'PFR1' LE
  static const int _headerSize = 21;

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
        rotation: rotation,
        colorMode: color,
        payloadLen: payloadLen,
        headerCrc32: headerCrc,
      ),
      payload,
      payloadCrc,
    );
  }

  static Uint8List decodeToRgba(ParsedBin bin) {
    final w = bin.header.width;
    final h = bin.header.height;
    if (bin.payload.lengthInBytes < w * h) {
      // Pad with white
      final padded = Uint8List(w * h);
      padded.fillRange(0, padded.length, 0xFF);
      padded.setRange(0, bin.payload.lengthInBytes, bin.payload);
      return _mapPixels(padded, w, h, bin.header.colorMode);
    }
    return _mapPixels(bin.payload, w, h, bin.header.colorMode);
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
    // 6-color mode
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

  /// Decode a .pfr1 file to a Flutter Image
  static Future<ui.Image?> decodeToImage(Uint8List data) async {
    try {
      final parsed = parse(data);
      final rgba = decodeToRgba(parsed);
      return _rgbaToImage(rgba, parsed.header.width, parsed.header.height);
    } catch (e) {
      return null;
    }
  }

  static Future<ui.Image> _rgbaToImage(Uint8List rgba, int width, int height) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(rgba, width, height, ui.PixelFormat.rgba8888, (img) => completer.complete(img));
    return completer.future;
  }
}
