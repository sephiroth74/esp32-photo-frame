import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:photoframe/models/ws_messages.dart';
import 'package:photoframe/utils/app_logger.dart';

class ImageCropScreen extends StatefulWidget {
  final File imageFile;
  final BoardConfig boardConfig;

  const ImageCropScreen({super.key, required this.imageFile, required this.boardConfig});

  @override
  State<ImageCropScreen> createState() => _ImageCropScreenState();
}

class _ImageCropScreenState extends State<ImageCropScreen> {
  static const double _maxScale = 4.0;
  static const double _minScale = 1.0;

  Size? _imageSize;
  Offset _offset = Offset.zero;
  Offset _startOffset = Offset.zero;
  double _userScale = 1.0;
  double _startScale = 1.0;
  bool _isRotated = false;
  double? _currentAspect;

  @override
  void initState() {
    super.initState();
    _resolveImageSize();
    _isRotated = widget.boardConfig.displayRotation % 2 == 1;
    _currentAspect = _targetAspect;
  }

  double get _baseAspect => widget.boardConfig.displayWidth / widget.boardConfig.displayHeight;

  double get _targetAspect {
    return _isRotated ? 1 / _baseAspect : _baseAspect;
  }

  Future<void> _resolveImageSize() async {
    final image = Image.file(widget.imageFile).image;
    final completer = Completer<Size>();
    final stream = image.resolve(const ImageConfiguration());
    late final ImageStreamListener listener;

    listener = ImageStreamListener(
      (ImageInfo info, bool _) {
        final size = Size(info.image.width.toDouble(), info.image.height.toDouble());
        completer.complete(size);
        stream.removeListener(listener);
      },
      onError: (Object error, StackTrace? stackTrace) {
        completer.completeError(error);
        stream.removeListener(listener);
      },
    );

    stream.addListener(listener);

    try {
      final size = await completer.future;
      if (mounted) {
        setState(() {
          _imageSize = size;
        });
      }
    } catch (e) {
      logger.severe('Failed to resolve image size: $e');
    }
  }

  void _onScaleStart(ScaleStartDetails details) {
    _startScale = _userScale;
    _startOffset = _offset;
  }

  void _onScaleUpdate(ScaleUpdateDetails details, Size cropSize) {
    final newScale = (_startScale * details.scale).clamp(_minScale, _maxScale).toDouble();

    // Accumula il pan durante la gesture
    final dx = details.focalPointDelta.dx;
    final dy = details.focalPointDelta.dy;
    final updatedOffset = Offset(_offset.dx + dx, _offset.dy + dy);

    // Il clamping basato sulla nuova scala
    final displaySize = Size(cropSize.width * newScale, cropSize.height * newScale);
    final clamped = _clampOffset(updatedOffset, displaySize, cropSize);

    setState(() {
      _userScale = newScale;
      _offset = clamped;
    });
  }

  Offset _clampOffset(Offset offset, Size imageSize, Size cropSize) {
    final maxX = math.max(0, (imageSize.width - cropSize.width) / 2);
    final maxY = math.max(0, (imageSize.height - cropSize.height) / 2);

    return Offset(offset.dx.clamp(-maxX, maxX).toDouble(), offset.dy.clamp(-maxY, maxY).toDouble());
  }

  void _rotateFrame() {
    setState(() {
      _isRotated = !_isRotated;
      _userScale = 1.0;
      _offset = Offset.zero;
    });
  }

  void _ensureClamped(Offset clampedOffset) {
    if (clampedOffset == _offset) {
      return;
    }

    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) {
        return;
      }
      setState(() {
        _offset = clampedOffset;
      });
    });
  }

  void _resetTransform() {
    setState(() {
      _userScale = 1.0;
      _offset = Offset.zero;
    });
  }

  void _continue() {
    logger.info('Continue to next step (not implemented yet)');
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Crop & Rotate'), elevation: 0, backgroundColor: Colors.white, foregroundColor: Colors.black),
      body: Column(
        children: [
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) {
                if (_imageSize == null) {
                  return const Center(child: CircularProgressIndicator());
                }

                final maxWidth = constraints.maxWidth;
                final maxHeight = constraints.maxHeight;
                final maxCropWidth = maxWidth * 0.85;
                final maxCropHeight = maxHeight * 0.85;

                return TweenAnimationBuilder<double>(
                  duration: const Duration(milliseconds: 250),
                  curve: Curves.easeInOut,
                  tween: Tween<double>(begin: _currentAspect ?? _targetAspect, end: _targetAspect),
                  onEnd: () {
                    _currentAspect = _targetAspect;
                  },
                  builder: (context, animatedAspect, child) {
                    double cropWidth = maxCropWidth;
                    double cropHeight = cropWidth / animatedAspect;
                    if (cropHeight > maxCropHeight) {
                      cropHeight = maxCropHeight;
                      cropWidth = cropHeight * animatedAspect;
                    }

                    final cropSize = Size(cropWidth, cropHeight);
                    final cropCenter = Offset(maxWidth / 2, maxHeight / 2);
                    final cropRect = Rect.fromCenter(center: cropCenter, width: cropSize.width, height: cropSize.height);

                    // Ensure offset stays valid when crop size changes (e.g., during rotation)
                    WidgetsBinding.instance.addPostFrameCallback((_) {
                      if (!mounted) return;
                      final displaySize = Size(cropSize.width * _userScale, cropSize.height * _userScale);
                      final clamped = _clampOffset(_offset, displaySize, cropSize);
                      if (clamped != _offset) {
                        setState(() {
                          _offset = clamped;
                        });
                      }
                    });

                    return Stack(
                      fit: StackFit.expand,
                      children: [
                        // Black background
                        const ColoredBox(color: Colors.black),

                        // Gesture detector covering entire screen
                        Positioned.fill(
                          child: GestureDetector(
                            behavior: HitTestBehavior.translucent,
                            onScaleStart: _onScaleStart,
                            onScaleUpdate: (details) => _onScaleUpdate(details, cropSize),
                            child: Center(
                              child: Transform(
                                alignment: Alignment.center,
                                transform: Matrix4.identity()
                                  ..translate(_offset.dx, _offset.dy)
                                  ..scale(_userScale),
                                child: SizedBox(
                                  width: cropSize.width,
                                  height: cropSize.height,
                                  child: Image.file(widget.imageFile, fit: BoxFit.cover),
                                ),
                              ),
                            ),
                          ),
                        ),

                        // Crop frame overlay (border and darkened surroundings)
                        IgnorePointer(
                          child: CustomPaint(
                            painter: CropOverlayPainter(cropRect: cropRect),
                            size: Size.infinite,
                          ),
                        ),
                      ],
                    );
                  },
                );
              },
            ),
          ),
          SafeArea(
            top: false,
            child: Container(
              decoration: BoxDecoration(
                color: Colors.white,
                boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: .08), blurRadius: 12, offset: const Offset(0, -2))],
              ),
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
              child: Row(
                children: [
                  Expanded(
                    flex: 1,
                    child: OutlinedButton.icon(onPressed: _rotateFrame, icon: const Icon(Icons.rotate_right), label: const Text('Rotate')),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    flex: 1,
                    child: OutlinedButton(onPressed: _resetTransform, child: const Text('Reset')),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    flex: 1,
                    child: ElevatedButton(
                      onPressed: _continue,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.blue,
                        // shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                      ),
                      child: const Text(
                        'Continue',
                        style: TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class CropOverlayPainter extends CustomPainter {
  final Rect cropRect;

  CropOverlayPainter({required this.cropRect});

  @override
  void paint(Canvas canvas, Size size) {
    final overlayPaint = Paint()..color = Colors.black.withValues(alpha: .55);
    canvas.saveLayer(Offset.zero & size, Paint());
    canvas.drawRect(Offset.zero & size, overlayPaint);

    final holePaint = Paint()..blendMode = BlendMode.clear;
    canvas.drawRect(cropRect, holePaint);
    canvas.restore();

    final borderPaint = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;
    canvas.drawRect(cropRect, borderPaint);

    final thirdWidth = cropRect.width / 3;
    final thirdHeight = cropRect.height / 3;
    for (int i = 1; i <= 2; i++) {
      // Vertical lines
      canvas.drawLine(
        Offset(cropRect.left + i * thirdWidth, cropRect.top),
        Offset(cropRect.left + i * thirdWidth, cropRect.bottom),
        borderPaint..color = Colors.white.withValues(alpha: .5)..strokeWidth = 1,
      );
      // Horizontal lines
      canvas.drawLine(
        Offset(cropRect.left, cropRect.top + i * thirdHeight),
        Offset(cropRect.right, cropRect.top + i * thirdHeight),
        borderPaint..color = Colors.white.withValues(alpha: .5)..strokeWidth = 1,
      );
    }
  }

  @override
  bool shouldRepaint(covariant CropOverlayPainter oldDelegate) {
    return cropRect != oldDelegate.cropRect;
  }
}
