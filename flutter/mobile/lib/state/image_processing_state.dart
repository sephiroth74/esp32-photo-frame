import 'dart:io';
import 'dart:ui';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:image_picker/image_picker.dart';
import 'package:image/image.dart' as img;

import '../models/processing_models.dart';
import '../services/dithering_processor.dart';

class _DitherArgs {
  final Uint8List bytes;
  final DisplayType displayType;
  final DitheringMethod method;
  final double saturation;
  final double contrast;
  final double brightness;
  final double ditherStrength;

  const _DitherArgs({
    required this.bytes,
    required this.displayType,
    required this.method,
    required this.saturation,
    required this.contrast,
    required this.brightness,
    required this.ditherStrength,
  });
}

Uint8List? _computeDither(_DitherArgs args) {
  return DitheringProcessor.apply(
    args.bytes,
    args.displayType,
    args.method,
    saturation: args.saturation,
    contrast: args.contrast,
    brightness: args.brightness,
    ditherStrength: args.ditherStrength,
  );
}

class ImageProcessingState extends ChangeNotifier {
  XFile? _image;
  ProcessingJob _job = const ProcessingJob();
  File? _intermediateFile;
  Uint8List? _ditherPreview;

  XFile? get image => _image;
  ProcessingJob get job => _job;
  ProcessingJob get currentJob => _job;
  File? get imageFile => _image != null ? File(_image!.path) : null;
  File? get intermediateFile => _intermediateFile;
  Uint8List? get ditherPreview => _ditherPreview;

  Future<void> pickImage(ImageSource source) async {
    final picker = ImagePicker();
    final picked = await picker.pickImage(source: source, imageQuality: 100);
    if (picked != null) {
      _image = picked;
      notifyListeners();
    }
  }

  void clearImage() {
    _image = null;
    _job = const ProcessingJob();
    _intermediateFile = null;
    _ditherPreview = null;
    notifyListeners();
  }

  void updateRotation(int rotation) {
    _job = _job.copyWith(rotation: rotation);
    notifyListeners();
  }

  void updateCropZoom(double zoom) {
    _job = _job.copyWith(cropZoom: zoom);
    notifyListeners();
  }

  void updatePanOffset(Offset offset) {
    _job = _job.copyWith(panOffset: offset);
    notifyListeners();
  }

  void updateTargetResolution({bool landscape = false}) {
    _job = _job.copyWith(
      targetResolution: landscape ? const Resolution(800, 480) : const Resolution(480, 800),
      cropZoom: 1.0,
      panOffset: Offset.zero,
    );
    notifyListeners();
  }

  void updateDithering(DitheringMethod method) {
    _job = _job.copyWith(dithering: method);
    notifyListeners();
  }

  void updateDisplayType(DisplayType type) {
    _job = _job.copyWith(displayType: type);
    notifyListeners();
  }

  void updateAdjustments({double? saturation, double? contrast, double? brightness, double? ditherStrength}) {
    _job = _job.copyWith(
      saturation: saturation ?? _job.saturation,
      contrast: contrast ?? _job.contrast,
      brightness: brightness ?? _job.brightness,
      ditherStrength: ditherStrength ?? _job.ditherStrength,
    );
    notifyListeners();
  }

  void updateAnnotation({String? text, String? font, int? fontSize, Color? textColor, Color? backgroundColor}) {
    _job = _job.copyWith(
      annotation: _job.annotation.copyWith(
        text: text ?? _job.annotation.text,
        fontFamily: font ?? _job.annotation.fontFamily,
        fontSize: fontSize?.toDouble() ?? _job.annotation.fontSize,
        textColor: textColor ?? _job.annotation.textColor,
        backgroundColor: backgroundColor ?? _job.annotation.backgroundColor,
      ),
    );
    notifyListeners();
  }

  /// Renders an intermediate cropped image at the selected target resolution
  /// applying rotation, zoom and pan so subsequent steps can use it directly.
  Future<File?> renderIntermediateCrop(File source) async {
    try {
      final bytes = await source.readAsBytes();
      final codec = await instantiateImageCodec(bytes);
      final frame = await codec.getNextFrame();
      final uiImage = frame.image;

      final width = _job.targetResolution.width.toInt();
      final height = _job.targetResolution.height.toInt();

      final recorder = PictureRecorder();
      final canvas = Canvas(recorder);

      // Base scale to emulate BoxFit.cover
      final baseScale = math.max(width / uiImage.width, height / uiImage.height);
      final angleRad = (_job.rotation % 360) * math.pi / 180.0;

      // Apply transforms: center + pan -> rotate -> scale -> draw centered image
      canvas.translate(width / 2 + _job.panOffset.dx, height / 2 + _job.panOffset.dy);
      if (angleRad != 0) {
        canvas.rotate(angleRad);
      }
      canvas.scale(baseScale * _job.cropZoom);
      final srcRect = Rect.fromLTWH(0, 0, uiImage.width.toDouble(), uiImage.height.toDouble());
      final dstRect = Rect.fromLTWH(-uiImage.width / 2, -uiImage.height / 2, uiImage.width.toDouble(), uiImage.height.toDouble());
      canvas.drawImageRect(uiImage, srcRect, dstRect, Paint());

      final picture = recorder.endRecording();
      final outImage = await picture.toImage(width, height);

      // Encode as JPEG with high quality
      final byteData = await outImage.toByteData(format: ImageByteFormat.rawRgba);
      if (byteData == null) return null;

      // Convert to JPEG using image package
      final img.Image? intermediate = img.Image.fromBytes(
        width: width,
        height: height,
        bytes: byteData.buffer,
        format: img.Format.uint8,
        numChannels: 4,
      );
      if (intermediate == null) return null;

      final jpegBytes = img.encodeJpg(intermediate, quality: 95);

      final outFile = File('${Directory.systemTemp.path}/pf_intermediate_${DateTime.now().millisecondsSinceEpoch}.jpg');
      await outFile.writeAsBytes(jpegBytes);
      _intermediateFile = outFile;
      notifyListeners();
      return outFile;
    } catch (_) {
      return null;
    }
  }

  /// Renders dithering preview based on current job settings using the intermediate image
  Future<Uint8List?> renderDitheringPreview() async {
    try {
      final src = _intermediateFile ?? (_image != null ? File(_image!.path) : null);
      if (src == null) return null;
      final bytes = await src.readAsBytes();
      final args = _DitherArgs(
        bytes: bytes,
        displayType: _job.displayType,
        method: _job.dithering,
        saturation: _job.saturation,
        contrast: _job.contrast,
        brightness: _job.brightness,
        ditherStrength: _job.ditherStrength,
      );
      final out = await compute(_computeDither, args);
      if (out != null) {
        _ditherPreview = out;
        notifyListeners();
      }
      return out;
    } catch (_) {
      return null;
    }
  }
}
