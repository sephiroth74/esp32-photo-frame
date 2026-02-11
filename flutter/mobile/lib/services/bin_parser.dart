import 'dart:async';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:photoframe_common/models/bin_model.dart';

extension ParsedBinExtensions on ParsedBin {
  /// Decode a .pfr1 file to a Flutter Image
  Future<ui.Image?> decodeToImage(Uint8List data) async {
    try {
      final parsed = BinParser.parse(data);
      final rgba = decodeToRgba();
      return _rgbaToImage(rgba, parsed.header.width, parsed.header.height);
    } catch (e) {
      return null;
    }
  }

  Future<ui.Image> _rgbaToImage(Uint8List rgba, int width, int height) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(rgba, width, height, ui.PixelFormat.rgba8888, (img) => completer.complete(img));
    return completer.future;
  }
}
