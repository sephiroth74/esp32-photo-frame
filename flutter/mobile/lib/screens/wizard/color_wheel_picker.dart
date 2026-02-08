import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/painting.dart';
import 'dart:ui' as ui;

class ColorWheelPicker extends StatefulWidget {
  final Color value;
  final ValueChanged<Color> onChanged;
  final String? label;

  const ColorWheelPicker({super.key, required this.value, required this.onChanged, this.label});

  @override
  State<ColorWheelPicker> createState() => _ColorWheelPickerState();
}

class _ColorWheelPickerState extends State<ColorWheelPicker> {
  late HSVColor _hsv;
  late double _alpha;

  @override
  void initState() {
    super.initState();
    _hsv = HSVColor.fromColor(widget.value);
    _alpha = widget.value.a;
  }

  void _updateColor(HSVColor hsv, [double? alpha]) {
    setState(() {
      _hsv = hsv;
      if (alpha != null) _alpha = alpha;
    });
    widget.onChanged(_hsv.toColor().withValues(alpha: _alpha));
  }

  @override
  Widget build(BuildContext context) {
    final svBoxSize = 180.0;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (widget.label != null)
          Padding(
            padding: const EdgeInsets.only(bottom: 8),
            child: Text(widget.label!, style: Theme.of(context).textTheme.titleMedium),
          ),
        // Hue slider
        Row(
          children: [
            Expanded(
              child: Slider(min: 0, max: 360, value: _hsv.hue, onChanged: (h) => _updateColor(_hsv.withHue(h))),
            ),
            Container(
              width: 24,
              height: 24,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: HSVColor.fromAHSV(1, _hsv.hue, 1, 1).toColor(),
                border: Border.all(color: Colors.black12),
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        // Saturation/Value square picker
        SizedBox(
          width: svBoxSize,
          height: svBoxSize,
          child: GestureDetector(
            onPanDown: (d) => _handleSVGesture(d.localPosition, svBoxSize),
            onPanUpdate: (d) => _handleSVGesture(d.localPosition, svBoxSize),
            child: DecoratedBox(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: Colors.black12),
                gradient: LinearGradient(
                  begin: Alignment.centerLeft,
                  end: Alignment.centerRight,
                  colors: [Colors.white, HSVColor.fromAHSV(1, _hsv.hue, 1, 1).toColor()],
                ),
              ),
              child: Stack(
                children: [
                  Container(
                    decoration: const BoxDecoration(
                      gradient: LinearGradient(begin: Alignment.topCenter, end: Alignment.bottomCenter, colors: [Colors.transparent, Colors.black]),
                    ),
                  ),
                  Positioned(
                    left: (_hsv.saturation * (svBoxSize - 1)).clamp(0, svBoxSize - 1),
                    top: ((1 - _hsv.value) * (svBoxSize - 1)).clamp(0, svBoxSize - 1),
                    child: Container(
                      width: 12,
                      height: 12,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        border: Border.all(color: Colors.white, width: 2),
                        boxShadow: const [BoxShadow(color: Colors.black26, blurRadius: 2)],
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        const SizedBox(height: 12),
        // Alpha slider with checkerboard background
        Row(
          children: [
            Expanded(
              child: Container(
                decoration: _checkerboardDecoration(),
                child: Slider(min: 0, max: 1, value: _alpha, onChanged: (a) => _updateColor(_hsv, a)),
              ),
            ),
            Container(
              width: 24,
              height: 24,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(4),
                border: Border.all(color: Colors.black12),
                color: _hsv.toColor().withValues(alpha: _alpha),
              ),
            ),
          ],
        ),
      ],
    );
  }

  void _handleSVGesture(Offset p, double size) {
    final s = (p.dx / size).clamp(0.0, 1.0);
    final v = (1 - (p.dy / size)).clamp(0.0, 1.0);
    _updateColor(_hsv.withSaturation(s).withValue(v));
  }

  Decoration _checkerboardDecoration() {
    const size = 8.0;
    return BoxDecoration(
      image: DecorationImage(
        repeat: ImageRepeat.repeat,
        image: _CheckerboardImage(size: size),
      ),
    );
  }
}

class _CheckerboardImage extends ImageProvider<_CheckerboardImage> {
  final double size;
  const _CheckerboardImage({required this.size});

  @override
  Future<_CheckerboardImage> obtainKey(ImageConfiguration configuration) => SynchronousFuture<_CheckerboardImage>(this);

  @override
  ImageStreamCompleter loadImage(_CheckerboardImage key, ImageDecoderCallback decode) {
    final recorder = ui.PictureRecorder();
    final canvas = ui.Canvas(recorder);
    final paintLight = Paint()..color = const Color(0xFFE0E0E0);
    final paintDark = Paint()..color = const Color(0xFFBDBDBD);

    for (int y = 0; y < 32; y++) {
      for (int x = 0; x < 32; x++) {
        final rect = Rect.fromLTWH(x * size, y * size, size, size);
        canvas.drawRect(rect, ((x + y) % 2 == 0) ? paintLight : paintDark);
      }
    }
    final picture = recorder.endRecording();
    final image = picture.toImageSync((size * 32).toInt(), (size * 32).toInt());
    return OneFrameImageStreamCompleter(Future.value(ImageInfo(image: image)));
  }

  @override
  bool operator ==(Object other) => other is _CheckerboardImage && other.size == size;
  @override
  int get hashCode => size.hashCode;
}
