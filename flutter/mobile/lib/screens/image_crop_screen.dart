import 'dart:async';
import 'dart:io';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart' hide Orientation;
import 'package:image/image.dart' as img;
import 'package:path_provider/path_provider.dart';
import 'package:photoframe/screens/dithering_screen.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe_common/photoframe_common.dart';

class ImageCropScreen extends StatefulWidget {
  final File imageFile;
  final BoardConfig boardConfig;

  const ImageCropScreen({super.key, required this.imageFile, required this.boardConfig});

  @override
  State<ImageCropScreen> createState() => _ImageCropScreenState();
}

class _ImageCropScreenState extends State<ImageCropScreen> {
  static const double _minUserScale = 1.0;
  static const double _maxUserScale = 4.0;

  Size? _imageSize;
  ui.Image? _decodedImage;
  Offset _offset = Offset.zero;
  double _userScale = 1.0;
  double _startScale = 1.0;
  bool _isRotated = false;
  double? _currentAspect;
  Size? _lastCropSize;
  Offset? _lastCropCenter;
  double? _lastMinScale;
  bool _isSaving = false;

  @override
  void initState() {
    super.initState();
    _resolveImageSize();
    _isRotated = widget.boardConfig.displayRotation.value % 2 == 1;
    _currentAspect = _targetAspect;
  }

  @override
  void dispose() {
    _decodedImage?.dispose();
    super.dispose();
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

      // Decode the image
      final bytes = await widget.imageFile.readAsBytes();
      final codec = await ui.instantiateImageCodec(bytes);
      final frameInfo = await codec.getNextFrame();
      final decodedImage = frameInfo.image;

      if (mounted) {
        setState(() {
          _imageSize = size;
          _decodedImage = decodedImage;
        });
      }
    } catch (e) {
      logger.severe('Failed to resolve image size: $e');
    }
  }

  void _onScaleStart(ScaleStartDetails details) {
    _startScale = _userScale;
  }

  void _onScaleUpdate(ScaleUpdateDetails details, Size cropSize, Size imageSize) {
    final newUserScale = (_startScale * details.scale).clamp(_minUserScale, _maxUserScale).toDouble();

    // Accumula il pan durante la gesture
    final dx = details.focalPointDelta.dx;
    final dy = details.focalPointDelta.dy;
    final updatedOffset = Offset(_offset.dx + dx, _offset.dy + dy);

    // Calcola minScale: scala minima per riempire il crop frame
    final minScale = math.max(cropSize.width / imageSize.width, cropSize.height / imageSize.height);
    // Dimensione immagine scalata finale
    final displaySize = Size(imageSize.width * minScale * newUserScale, imageSize.height * minScale * newUserScale);

    // Debug clamping calculation
    final clamped = _clampOffset(updatedOffset, displaySize, cropSize);

    // Check if image borders are inside crop area
    final imageLeft = clamped.dx - displaySize.width / 2;
    final imageRight = clamped.dx + displaySize.width / 2;
    final imageTop = clamped.dy - displaySize.height / 2;
    final imageBottom = clamped.dy + displaySize.height / 2;
    final cropLeft = -cropSize.width / 2;
    final cropRight = cropSize.width / 2;
    final cropTop = -cropSize.height / 2;
    final cropBottom = cropSize.height / 2;

    if (imageLeft > cropLeft || imageRight < cropRight || imageTop > cropTop || imageBottom < cropBottom) {
      // Image does not completely cover crop area
    }

    setState(() {
      _userScale = newUserScale;
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

  void _resetTransform() {
    setState(() {
      _userScale = 1.0;
      _offset = Offset.zero;
    });
  }

  Future<void> _continue() async {
    if (_isSaving) {
      return;
    }

    final cropSize = _lastCropSize;
    final cropCenter = _lastCropCenter;
    final imageSize = _imageSize;
    final minScale = _lastMinScale;

    if (cropSize == null || cropCenter == null || imageSize == null || minScale == null) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Crop data not ready yet'), backgroundColor: Colors.orange));
      }
      return;
    }

    setState(() {
      _isSaving = true;
    });

    try {
      final croppedFile = await _saveCroppedImage(
        cropSize: cropSize,
        cropCenter: cropCenter,
        imageSize: imageSize,
        minScale: minScale,
        isRotated: _isRotated,
      );

      if (!mounted) {
        return;
      }

      Navigator.of(context).push(
        MaterialPageRoute(
          builder: (_) => DitheringScreen(
            croppedImageFile: croppedFile,
            boardConfig: widget.boardConfig,
            currentOrientation: _isRotated ? Orientation.portrait : Orientation.landscape,
          ),
        ),
      );
    } catch (e) {
      logger.severe('Failed to save cropped image: $e');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to save cropped image: $e'), backgroundColor: Colors.red));
      }
    } finally {
      if (mounted) {
        setState(() {
          _isSaving = false;
        });
      }
    }
  }

  Future<File> _saveCroppedImage({
    required Size cropSize,
    required Offset cropCenter,
    required Size imageSize,
    required double minScale,
    required bool isRotated,
  }) async {
    final finalScale = minScale * _userScale;
    final scaledSize = Size(imageSize.width * finalScale, imageSize.height * finalScale);
    final scaledImageTopLeft = Offset(cropCenter.dx + _offset.dx - scaledSize.width / 2, cropCenter.dy + _offset.dy - scaledSize.height / 2);
    final cropRect = Rect.fromCenter(center: cropCenter, width: cropSize.width, height: cropSize.height);
    final relative = Offset(cropRect.left - scaledImageTopLeft.dx, cropRect.top - scaledImageTopLeft.dy);

    var srcX = relative.dx / finalScale;
    var srcY = relative.dy / finalScale;
    var srcW = cropSize.width / finalScale;
    var srcH = cropSize.height / finalScale;

    // Clamp to image bounds
    if (srcX < 0) srcX = 0;
    if (srcY < 0) srcY = 0;
    if (srcX + srcW > imageSize.width) srcW = imageSize.width - srcX;
    if (srcY + srcH > imageSize.height) srcH = imageSize.height - srcY;

    final srcXInt = srcX.floor();
    final srcYInt = srcY.floor();
    final srcWInt = srcW.ceil();
    final srcHInt = srcH.ceil();

    final bytes = await widget.imageFile.readAsBytes();
    final decoded = img.decodeImage(bytes);
    if (decoded == null) {
      throw Exception('Failed to decode image');
    }

    final cropped = img.copyCrop(
      decoded,
      x: srcXInt.clamp(0, decoded.width - 1),
      y: srcYInt.clamp(0, decoded.height - 1),
      width: srcWInt.clamp(1, decoded.width),
      height: srcHInt.clamp(1, decoded.height),
    );

    debugPrint('cropped image size: ${cropped.width}x${cropped.height}');

    final displayWidth = isRotated ? widget.boardConfig.displayHeight : widget.boardConfig.displayWidth;
    final displayHeight = isRotated ? widget.boardConfig.displayWidth : widget.boardConfig.displayHeight;

    final resized = img.copyResize(cropped, width: displayWidth, height: displayHeight, interpolation: img.Interpolation.cubic);

    debugPrint('resized image size: ${resized.width}x${resized.height}');

    final tempDir = await getTemporaryDirectory();
    final timestamp = DateTime.now().millisecondsSinceEpoch;
    final file = File('${tempDir.path}/crop_$timestamp.png');

    debugPrint('Saving cropped image to ${file.path}');

    await file.writeAsBytes(img.encodePng(resized));

    return file;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Crop & Rotate'), elevation: 0, backgroundColor: Colors.white, foregroundColor: Colors.black),
      body: Stack(
        children: [
          Column(
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
                        final imageSize = _imageSize!;
                        final minScale = math.max(cropSize.width / imageSize.width, cropSize.height / imageSize.height);
                        final finalScale = minScale * _userScale;
                        final displaySize = Size(imageSize.width * finalScale, imageSize.height * finalScale);

                        _lastCropSize = cropSize;
                        _lastCropCenter = cropCenter;
                        _lastMinScale = minScale;

                        // Ensure offset stays valid when crop size changes (e.g., during rotation)
                        WidgetsBinding.instance.addPostFrameCallback((_) {
                          if (!mounted || _imageSize == null) return;
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
                            if (_imageSize != null && _decodedImage != null)
                              Positioned.fill(
                                child: GestureDetector(
                                  behavior: HitTestBehavior.translucent,
                                  onScaleStart: _onScaleStart,
                                  onScaleUpdate: (details) => _onScaleUpdate(details, cropSize, imageSize),
                                  child: CustomPaint(
                                    painter: ImageCropPainter(
                                      image: _decodedImage!,
                                      imageSize: imageSize,
                                      cropSize: cropSize,
                                      cropCenter: cropCenter,
                                      offset: _offset,
                                      scale: finalScale,
                                    ),
                                    size: Size.infinite,
                                  ),
                                ),
                              )
                            else
                              const Positioned.fill(child: Center(child: CircularProgressIndicator())),

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
          if (_isSaving)
            Positioned.fill(
              child: AbsorbPointer(
                child: Container(
                  color: Colors.black54,
                  child: const Center(child: SizedBox(width: 40, height: 40, child: CircularProgressIndicator())),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class ImageCropPainter extends CustomPainter {
  final ui.Image image;
  final Size imageSize;
  final Size cropSize;
  final Offset cropCenter;
  final Offset offset;
  final double scale;

  ImageCropPainter({
    required this.image,
    required this.imageSize,
    required this.cropSize,
    required this.cropCenter,
    required this.offset,
    required this.scale,
  });

  @override
  void paint(Canvas canvas, Size size) {
    // Calcola la dimensione scalata dell'immagine
    final scaledImageWidth = imageSize.width * scale;
    final scaledImageHeight = imageSize.height * scale;

    // Posizione dell'immagine scalata (centrata)
    final imageX = offset.dx + size.width / 2 - scaledImageWidth / 2;
    final imageY = offset.dy + size.height / 2 - scaledImageHeight / 2;

    // Rect di destinazione dove rendere l'immagine scalata
    final dstRect = Rect.fromLTWH(imageX, imageY, scaledImageWidth, scaledImageHeight);

    // Rect dell'immagine originale (uso tutta l'immagine)
    final srcRect = Rect.fromLTWH(0, 0, imageSize.width.toDouble(), imageSize.height.toDouble());

    // Disegna l'immagine
    canvas.drawImageRect(image, srcRect, dstRect, Paint());
  }

  @override
  bool shouldRepaint(ImageCropPainter oldDelegate) {
    return oldDelegate.offset != offset || oldDelegate.scale != scale || oldDelegate.image != image;
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
        borderPaint
          ..color = Colors.white.withValues(alpha: .5)
          ..strokeWidth = 1,
      );
      // Horizontal lines
      canvas.drawLine(
        Offset(cropRect.left, cropRect.top + i * thirdHeight),
        Offset(cropRect.right, cropRect.top + i * thirdHeight),
        borderPaint
          ..color = Colors.white.withValues(alpha: .5)
          ..strokeWidth = 1,
      );
    }
  }

  @override
  bool shouldRepaint(covariant CropOverlayPainter oldDelegate) {
    return cropRect != oldDelegate.cropRect;
  }
}
