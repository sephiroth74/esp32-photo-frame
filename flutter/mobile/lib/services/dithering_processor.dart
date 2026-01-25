import 'dart:typed_data';

import 'package:image/image.dart' as img;

import '../models/processing_models.dart';

class DitheringProcessor {
  static List<img.ColorRgb8> _paletteFor(DisplayType type) {
    if (type == DisplayType.blackAndWhite) {
      return [img.ColorRgb8(0, 0, 0), img.ColorRgb8(255, 255, 255)];
    }
    // Six-color e-paper common palette
    return [
      img.ColorRgb8(0, 0, 0), // black
      img.ColorRgb8(255, 255, 255), // white
      img.ColorRgb8(255, 0, 0), // red
      img.ColorRgb8(255, 255, 0), // yellow
      img.ColorRgb8(0, 255, 0), // green
      img.ColorRgb8(0, 0, 255), // blue
    ];
  }

  static img.ColorRgb8 _closest(img.ColorRgb8 c, List<img.ColorRgb8> palette) {
    int bestIdx = 0;
    double bestDist = double.infinity;
    for (int i = 0; i < palette.length; i++) {
      final p = palette[i];
      // Use perceptually weighted distance (R:30%, G:59%, B:11%)
      final dr = (c.r - p.r).toDouble();
      final dg = (c.g - p.g).toDouble();
      final db = (c.b - p.b).toDouble();
      final dist = (dr * dr * 0.3 + dg * dg * 0.59 + db * db * 0.11);
      if (dist < bestDist) {
        bestDist = dist;
        bestIdx = i;
      }
    }
    return palette[bestIdx];
  }

  static double _applyGamma(double value) {
    // Apply gamma 2.2 for perceptually uniform dithering
    final normalized = value / 255.0;
    final corrected = normalized * normalized * normalized; // approx gamma 2.2
    return corrected * 255.0;
  }

  static Uint8List apply(
    Uint8List inputBytes,
    DisplayType displayType,
    DitheringMethod method, {
    double ditherStrength = 1.0,
    double saturation = 1.0,
    double contrast = 1.0,
    double brightness = 1.0,
  }) {
    final palette = _paletteFor(displayType);
    final src = img.decodeImage(inputBytes);
    if (src == null) return inputBytes;

    // Convert to working float buffers
    final w = src.width;
    final h = src.height;
    final r = List.generate(h, (y) => List<double>.filled(w, 0));
    final g = List.generate(h, (y) => List<double>.filled(w, 0));
    final b = List.generate(h, (y) => List<double>.filled(w, 0));

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        final px = src.getPixel(x, y);
        // Apply brightness and contrast around mid-point 128
        double rr = (px.r.toDouble() * brightness);
        double gg = (px.g.toDouble() * brightness);
        double bb = (px.b.toDouble() * brightness);

        rr = (rr - 128.0) * contrast + 128.0;
        gg = (gg - 128.0) * contrast + 128.0;
        bb = (bb - 128.0) * contrast + 128.0;

        // Apply saturation toward luma
        final lum = 0.299 * rr + 0.587 * gg + 0.114 * bb;
        rr = lum + (rr - lum) * saturation;
        gg = lum + (gg - lum) * saturation;
        bb = lum + (bb - lum) * saturation;

        // Apply gamma correction for perceptually uniform dithering
        r[y][x] = _applyGamma(rr.clamp(0.0, 255.0));
        g[y][x] = _applyGamma(gg.clamp(0.0, 255.0));
        b[y][x] = _applyGamma(bb.clamp(0.0, 255.0));
      }
    }

    final out = img.Image(width: w, height: h);

    void putPixel(int x, int y, img.ColorRgb8 c) {
      out.setPixelRgb(x, y, c.r, c.g, c.b);
    }

    //

    img.ColorRgb8 quantize(int x, int y) {
      final cr = r[y][x].clamp(0.0, 255.0).toInt();
      final cg = g[y][x].clamp(0.0, 255.0).toInt();
      final cb = b[y][x].clamp(0.0, 255.0).toInt();
      return _closest(img.ColorRgb8(cr, cg, cb), palette);
    }

    switch (method) {
      case DitheringMethod.floydSteinberg:
        // Floyd-Steinberg diffusion, divisor 16
        const fsWeights = [
          [0, 0, 0, 7],
          [3, 5, 1, 0],
        ];
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            final c = quantize(x, y);
            putPixel(x, y, c);
            final er = (r[y][x] - c.r) * ditherStrength;
            final eg = (g[y][x] - c.g) * ditherStrength;
            final eb = (b[y][x] - c.b) * ditherStrength;

            void add(int dx, int dy, double weight, double divisor) {
              final nx = x + dx;
              final ny = y + dy;
              if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                final factor = weight / divisor;
                r[ny][nx] += er * factor;
                g[ny][nx] += eg * factor;
                b[ny][nx] += eb * factor;
              }
            }

            add(1, 0, 7, 16);
            add(-1, 1, 3, 16);
            add(0, 1, 5, 16);
            add(1, 1, 1, 16);
          }
        }
        break;
      case DitheringMethod.atkinson:
        // Atkinson distribution factors (1/8) - diffuses only 75% of error
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            final c = quantize(x, y);
            putPixel(x, y, c);
            final er = (r[y][x] - c.r) * ditherStrength;
            final eg = (g[y][x] - c.g) * ditherStrength;
            final eb = (b[y][x] - c.b) * ditherStrength;
            const factor = 1.0 / 8.0;
            void add(int dx, int dy) {
              final nx = x + dx;
              final ny = y + dy;
              if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                r[ny][nx] += er * factor;
                g[ny][nx] += eg * factor;
                b[ny][nx] += eb * factor;
              }
            }

            add(1, 0);
            add(2, 0);
            add(-1, 1);
            add(0, 1);
            add(1, 1);
            add(0, 2);
          }
        }
        break;
      case DitheringMethod.stucki:
        // Stucki weights over 5x3 matrix, divisor 42
        const List<List<int>> weights = [
          [0, 0, 0, 8, 4],
          [2, 4, 8, 4, 2],
          [1, 2, 4, 2, 1],
        ];
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            final c = quantize(x, y);
            putPixel(x, y, c);
            final er = (r[y][x] - c.r) * ditherStrength;
            final eg = (g[y][x] - c.g) * ditherStrength;
            final eb = (b[y][x] - c.b) * ditherStrength;
            for (int dy = 0; dy < 3; dy++) {
              for (int dx = -2; dx <= 2; dx++) {
                final wx = dx + 2;
                final weight = weights[dy][wx];
                if (weight == 0) continue;
                final nx = x + dx;
                final ny = y + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                  final factor = weight / 42.0;
                  r[ny][nx] += er * factor;
                  g[ny][nx] += eg * factor;
                  b[ny][nx] += eb * factor;
                }
              }
            }
          }
        }
        break;
      case DitheringMethod.jarvisJudiceNinke:
        // JJN weights, divisor 48
        const List<List<int>> weights = [
          [0, 0, 0, 7, 5],
          [3, 5, 7, 5, 3],
          [1, 3, 5, 3, 1],
        ];
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            final c = quantize(x, y);
            putPixel(x, y, c);
            final er = (r[y][x] - c.r) * ditherStrength;
            final eg = (g[y][x] - c.g) * ditherStrength;
            final eb = (b[y][x] - c.b) * ditherStrength;
            for (int dy = 0; dy < 3; dy++) {
              for (int dx = -2; dx <= 2; dx++) {
                final wx = dx + 2;
                final weight = weights[dy][wx];
                if (weight == 0) continue;
                final nx = x + dx;
                final ny = y + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                  final factor = weight / 48.0;
                  r[ny][nx] += er * factor;
                  g[ny][nx] += eg * factor;
                  b[ny][nx] += eb * factor;
                }
              }
            }
          }
        }
        break;
      case DitheringMethod.ordered:
        // 4x4 Bayer matrix for ordered dithering
        const bayer4 = [
          [0, 8, 2, 10],
          [12, 4, 14, 6],
          [3, 11, 1, 9],
          [15, 7, 13, 5],
        ];
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            final threshold = (bayer4[y % 4][x % 4] / 16.0 - 0.5) * 255 * ditherStrength;
            final adjusted = img.ColorRgb8(
              (r[y][x] + threshold).clamp(0.0, 255.0).toInt(),
              (g[y][x] + threshold).clamp(0.0, 255.0).toInt(),
              (b[y][x] + threshold).clamp(0.0, 255.0).toInt(),
            );
            final c = _closest(adjusted, palette);
            putPixel(x, y, c);
          }
        }
        break;
    }

    final outBytes = img.encodePng(out);
    return Uint8List.fromList(outBytes);
  }
}
