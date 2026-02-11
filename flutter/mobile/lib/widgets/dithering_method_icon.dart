import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:photoframe_common/photoframe_common.dart';

/// Widget factory that returns an icon for a dithering method.
///
/// Each algorithm has a unique visual representation:
/// - Floyd-Steinberg: Drop with 4-directional diffusion arrows
/// - Atkinson: Compact 6-pointed star
/// - Stucki: Wide radiating sun with many rays
/// - Jarvis-Judice-Ninke: Precise diamond shape
/// - Ordered: Regular 3x3 grid pattern
class DitheringMethodIcon extends StatelessWidget {
  final DitheringMethod method;
  final Color color;
  final double size;

  const DitheringMethodIcon({super.key, required this.method, this.color = Colors.black, this.size = 64});

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: _DitheringMethodPainter(method: method, color: color),
      size: Size(size, size),
    );
  }
}

class _DitheringMethodPainter extends CustomPainter {
  final DitheringMethod method;
  final Color color;

  _DitheringMethodPainter({required this.method, required this.color});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final paint = Paint()
      ..color = color
      ..strokeWidth = 2
      ..style = PaintingStyle.stroke;

    final fillPaint = Paint()
      ..color = color
      ..style = PaintingStyle.fill;

    switch (method) {
      case DitheringMethod.floydSteinberg:
        _paintFloydSteinberg(canvas, center, size, paint, fillPaint);
      case DitheringMethod.atkinson:
        _paintAtkinson(canvas, center, size, paint, fillPaint);
      case DitheringMethod.stucki:
        _paintStucki(canvas, center, size, paint, fillPaint);
      case DitheringMethod.jarvisJudiceNinke:
        _paintJarvisJudiceNinke(canvas, center, size, paint, fillPaint);
      case DitheringMethod.ordered:
        _paintOrdered(canvas, center, size, paint, fillPaint);
    }
  }

  /// Floyd-Steinberg: Drop shape with 4 directional diffusion arrows
  void _paintFloydSteinberg(Canvas canvas, Offset center, Size size, Paint paint, Paint fillPaint) {
    const radius = 8.0;

    // Draw center drop
    canvas.drawCircle(center, radius, fillPaint);

    // Draw 4 diffusion arrows pointing to corners
    final arrowLength = 12.0;
    final directions = [
      Offset(1, 1), // bottom-right
      Offset(-1, 1), // bottom-left
      Offset(1, -1), // top-right
      Offset(-1, -1), // top-left
    ];

    for (final dir in directions) {
      final normalized = dir / dir.distance;
      final start = center + normalized * (radius + 2);
      final end = center + normalized * (radius + arrowLength);
      canvas.drawLine(start, end, paint);

      // Arrow head
      final arrowSize = 2.0;
      final perpendicular = Offset(-normalized.dy, normalized.dx);
      canvas.drawLine(end, end - normalized * 2 - perpendicular * arrowSize, paint);
      canvas.drawLine(end, end - normalized * 2 + perpendicular * arrowSize, paint);
    }
  }

  /// Atkinson: Compact 6-pointed star
  void _paintAtkinson(Canvas canvas, Offset center, Size size, Paint paint, Paint fillPaint) {
    const radius = 10.0;
    const innerRadius = 5.0;

    final path = Path();
    for (int i = 0; i < 12; i++) {
      final angle = (i * 30) * math.pi / 180;
      final r = i % 2 == 0 ? radius : innerRadius;
      final x = center.dx + r * math.cos(angle);
      final y = center.dy + r * math.sin(angle);

      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    path.close();
    canvas.drawPath(path, fillPaint);
  }

  /// Stucki: Wide radiating sun with many rays
  void _paintStucki(Canvas canvas, Offset center, Size size, Paint paint, Paint fillPaint) {
    // Center circle
    canvas.drawCircle(center, 4.0, fillPaint);

    // Draw 12 rays in all directions
    const rayCount = 12;
    const innerRadius = 6.0;
    const outerRadius = 14.0;

    for (int i = 0; i < rayCount; i++) {
      final angle = (i * 360 / rayCount) * math.pi / 180;
      final startX = center.dx + innerRadius * math.cos(angle);
      final startY = center.dy + innerRadius * math.sin(angle);
      final endX = center.dx + outerRadius * math.cos(angle);
      final endY = center.dy + outerRadius * math.sin(angle);

      canvas.drawLine(Offset(startX, startY), Offset(endX, endY), paint);
    }
  }

  /// Jarvis-Judice-Ninke: Precise diamond shape
  void _paintJarvisJudiceNinke(Canvas canvas, Offset center, Size size, Paint paint, Paint fillPaint) {
    const halfSize = 8.0;

    final path = Path();
    path.moveTo(center.dx, center.dy - halfSize); // top
    path.lineTo(center.dx + halfSize, center.dy); // right
    path.lineTo(center.dx, center.dy + halfSize); // bottom
    path.lineTo(center.dx - halfSize, center.dy); // left
    path.close();

    canvas.drawPath(path, fillPaint);

    // Add inner diamond for detail
    const innerSize = 4.0;
    final innerPath = Path();
    innerPath.moveTo(center.dx, center.dy - innerSize);
    innerPath.lineTo(center.dx + innerSize, center.dy);
    innerPath.lineTo(center.dx, center.dy + innerSize);
    innerPath.lineTo(center.dx - innerSize, center.dy);
    innerPath.close();

    canvas.drawPath(
      innerPath,
      Paint()
        ..color = color
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5,
    );
  }

  /// Ordered: Regular 3x3 grid pattern (Bayer matrix)
  void _paintOrdered(Canvas canvas, Offset center, Size size, Paint paint, Paint fillPaint) {
    const cellSize = 4.0;
    const gridSize = 3;
    const totalSize = cellSize * gridSize;
    const startX = -totalSize / 2;
    const startY = -totalSize / 2;

    // Draw varying intensity squares to represent ordered dithering
    final intensities = [
      [1, 3, 0],
      [2, 4, 1],
      [0, 2, 3],
    ];

    for (int row = 0; row < gridSize; row++) {
      for (int col = 0; col < gridSize; col++) {
        final intensity = intensities[row][col];
        final alpha = (intensity / 4.0).clamp(0.3, 1.0);

        final fillColor = color.withOpacity(alpha);
        final cellPaint = Paint()
          ..color = fillColor
          ..style = PaintingStyle.fill;

        final x = center.dx + startX + col * cellSize;
        final y = center.dy + startY + row * cellSize;

        canvas.drawRect(Rect.fromLTWH(x, y, cellSize, cellSize), cellPaint);

        // Draw border
        canvas.drawRect(
          Rect.fromLTWH(x, y, cellSize, cellSize),
          Paint()
            ..color = color
            ..style = PaintingStyle.stroke
            ..strokeWidth = 0.5,
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _DitheringMethodPainter oldDelegate) {
    return oldDelegate.color != color || oldDelegate.method != method;
  }
}
