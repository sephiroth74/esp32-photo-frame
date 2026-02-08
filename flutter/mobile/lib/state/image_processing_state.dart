import 'dart:io';
import 'dart:ui';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:image_picker/image_picker.dart';
import 'package:image/image.dart' as img;
import 'package:exif/exif.dart';

import '../models/processing_models.dart';
import '../services/dithering_processor.dart';
import '../services/photoframe_dithering_ffi.dart';
import '../services/gallery_service.dart';
import '../utils/app_logger.dart';

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
  Uint8List? _binaryData;

  XFile? get image => _image;
  ProcessingJob get job => _job;
  ProcessingJob get currentJob => _job;
  File? get imageFile => _image != null ? File(_image!.path) : null;
  File? get intermediateFile => _intermediateFile;
  Uint8List? get ditherPreview => _ditherPreview;
  Uint8List? get binaryData => _binaryData;

  Future<Uint8List?> getIntermediateFileAsBytes() async {
    if (_intermediateFile == null) {
      return null;
    }
    return _intermediateFile!.readAsBytes();
  }

  Future<void> pickImage(ImageSource source) async {
    logger.fine('Picking image from source: $source');
    final picker = ImagePicker();
    final picked = await picker.pickImage(source: source, imageQuality: 100);
    if (picked != null) {
      logger.info('Image picked: ${picked.path}');
      _image = picked;
      // Reset processing state for new image
      _intermediateFile = null;
      _ditherPreview = null;
      _binaryData = null;
      // Try to prefill annotation text with EXIF capture date
      try {
        logger.fine('Reading EXIF data');
        final bytes = await File(picked.path).readAsBytes();
        final exif = await readExifFromBytes(bytes);
        String? dateStr;
        // Prefer DateTimeOriginal; fallback to Image DateTime
        final dtOrig = exif['EXIF DateTimeOriginal']?.printable;
        final dtImage = exif['Image DateTime']?.printable;
        dateStr = dtOrig?.trim().isNotEmpty == true ? dtOrig : dtImage;
        if (dateStr != null && dateStr.trim().isNotEmpty) {
          // Use only the date portion and normalize: 'YYYY:MM:DD HH:MM:SS' -> 'YYYY-MM-DD'
          final parts = dateStr.split(' ');
          final dateOnly = parts.isNotEmpty ? parts.first : dateStr;
          final normalizedDate = dateOnly.replaceAll(':', '-');
          logger.fine('EXIF date found: $normalizedDate');
          if (_job.annotation.text.isEmpty) {
            _job = _job.copyWith(annotation: _job.annotation.copyWith(text: normalizedDate));
            logger.info('Prefilled annotation with EXIF date: $normalizedDate');
          }
        } else {
          logger.fine('No EXIF date found in image');
        }
      } catch (e) {
        logger.warning('Failed to read EXIF data: $e');
      }
      notifyListeners();
    } else {
      logger.fine('No image picked');
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
    // Invalidate intermediate file and dither preview when rotation changes (crop will be different)
    _intermediateFile = null;
    _ditherPreview = null;
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
    // Invalidate intermediate file and dither preview when target resolution changes (crop will be different)
    _intermediateFile = null;
    _ditherPreview = null;
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
    logger.fine(
      'Rendering intermediate crop: resolution=${_job.targetResolution}, rotation=${_job.rotation}, '
      'zoom=${_job.cropZoom}, pan=${_job.panOffset}',
    );
    final stopwatch = Stopwatch()..start();

    try {
      final bytes = await source.readAsBytes();
      logger.fine('Source image loaded: ${bytes.length} bytes');

      final codec = await instantiateImageCodec(bytes);
      final frame = await codec.getNextFrame();
      final uiImage = frame.image;
      logger.fine('Image decoded: ${uiImage.width}x${uiImage.height}');

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
      logger.fine('Canvas rendered to ${width}x$height');

      // Encode as JPEG with high quality
      final byteData = await outImage.toByteData(format: ImageByteFormat.rawRgba);
      if (byteData == null) {
        logger.severe('Failed to get byte data from rendered image');
        return null;
      }

      // Convert to JPEG using image package
      final img.Image intermediate = img.Image.fromBytes(
        width: width,
        height: height,
        bytes: byteData.buffer,
        format: img.Format.uint8,
        numChannels: 4,
      );
      if (intermediate == null) {
        logger.severe('Failed to create Image from bytes');
        return null;
      }

      final jpegBytes = img.encodeJpg(intermediate, quality: 95);

      final outFile = File('${Directory.systemTemp.path}/pf_intermediate_${DateTime.now().millisecondsSinceEpoch}.jpg');
      await outFile.writeAsBytes(jpegBytes);
      _intermediateFile = outFile;

      stopwatch.stop();
      logger.info('Intermediate crop saved: ${outFile.path} (${jpegBytes.length} bytes) in ${stopwatch.elapsedMilliseconds}ms');

      notifyListeners();
      return outFile;
    } catch (e, stackTrace) {
      logger.severe('Failed to render intermediate crop', e, stackTrace);
      return null;
    }
  }

  /// Renders dithering preview based on current job settings using the intermediate image
  Future<Uint8List?> renderDitheringPreview() async {
    logger.fine('Rendering dithering preview');
    final stopwatch = Stopwatch()..start();

    try {
      final src = _intermediateFile ?? (_image != null ? File(_image!.path) : null);
      if (src == null) {
        logger.warning('No source image available for dithering');
        return null;
      }

      logger.fine('Loading source for dithering: ${src.path}');
      final bytes = await src.readAsBytes();
      logger.fine('Source loaded: ${bytes.length} bytes');

      final args = _DitherArgs(
        bytes: bytes,
        displayType: _job.displayType,
        method: _job.dithering,
        saturation: _job.saturation,
        contrast: _job.contrast,
        brightness: _job.brightness,
        ditherStrength: _job.ditherStrength,
      );

      logger.fine('Starting dithering computation in isolate');
      final out = await compute(_computeDither, args);

      stopwatch.stop();

      if (out != null) {
        _ditherPreview = out;
        logger.info('Dithering preview completed: ${out.length} bytes in ${stopwatch.elapsedMilliseconds}ms');
        notifyListeners();
      } else {
        logger.severe('Dithering computation returned null');
      }
      return out;
    } catch (e, stackTrace) {
      logger.severe('Failed to render dithering preview', e, stackTrace);
      return null;
    }
  }

  /// Apply annotation to the intermediate image and save it
  Future<File?> renderIntermediateWithAnnotation() async {
    logger.fine('Applying annotation to intermediate image');
    final stopwatch = Stopwatch()..start();

    try {
      final src = _intermediateFile;
      if (src == null) {
        logger.warning('No intermediate file available for annotation');
        return null;
      }

      // If no annotation text, just keep the current intermediate
      if (_job.annotation.text.isEmpty) {
        logger.fine('No annotation text, skipping annotation rendering');
        return src;
      }

      final bytes = await src.readAsBytes();
      logger.fine('Loading intermediate: ${bytes.length} bytes');

      final codec = await instantiateImageCodec(bytes);
      final frame = await codec.getNextFrame();
      final uiImage = frame.image;
      logger.fine('Image decoded: ${uiImage.width}x${uiImage.height}');

      final recorder = PictureRecorder();
      final canvas = Canvas(recorder);

      // Draw the base image
      final srcRect = Rect.fromLTWH(0, 0, uiImage.width.toDouble(), uiImage.height.toDouble());
      canvas.drawImageRect(uiImage, srcRect, srcRect, Paint());

      // Draw annotation text at bottom-right
      // Match PreviewCard layout, scaling to target image resolution
      final scale = uiImage.height / 480.0;
      final paddingRight = 5.0 * scale;
      final paddingBottom = 5.0 * scale;
      final textPaddingH = 10.0 * scale;
      final textPaddingV = 6.0 * scale;
      final cornerRadius = 8.0 * scale;

      final textSpan = TextSpan(
        text: _job.annotation.text,
        style: TextStyle(color: _job.annotation.textColor, fontSize: _job.annotation.fontSize * scale, fontFamily: _job.annotation.fontFamily),
      );

      final textPainter = TextPainter(text: textSpan, textDirection: TextDirection.ltr, maxLines: 3, textAlign: TextAlign.right);
      textPainter.layout(maxWidth: uiImage.width - paddingRight - textPaddingH * 2);

      // Position at bottom-right with padding (matching PreviewCard)
      final bgWidth = textPainter.width + textPaddingH * 2;
      final bgHeight = textPainter.height + textPaddingV * 2;
      final bgX = uiImage.width - bgWidth - paddingRight;
      final bgY = uiImage.height - bgHeight - paddingBottom;

      // Draw background
      final bgRect = RRect.fromRectAndRadius(Rect.fromLTWH(bgX, bgY, bgWidth, bgHeight), Radius.circular(cornerRadius));
      canvas.drawRRect(bgRect, Paint()..color = _job.annotation.backgroundColor);

      // Draw text (centered in background)
      final textX = bgX + textPaddingH;
      final textY = bgY + textPaddingV;
      textPainter.paint(canvas, Offset(textX, textY));

      final picture = recorder.endRecording();
      final outImage = await picture.toImage(uiImage.width, uiImage.height);
      logger.fine('Annotation rendered on ${uiImage.width}x${uiImage.height} canvas');

      // Encode as PNG to preserve quality
      final byteData = await outImage.toByteData(format: ImageByteFormat.png);
      if (byteData == null) {
        logger.severe('Failed to get byte data from annotated image');
        return null;
      }

      final pngBytes = byteData.buffer.asUint8List();
      final outFile = File('${Directory.systemTemp.path}/pf_annotated_${DateTime.now().millisecondsSinceEpoch}.png');
      await outFile.writeAsBytes(pngBytes);
      _intermediateFile = outFile;

      stopwatch.stop();
      logger.info('Intermediate with annotation saved: ${outFile.path} (${pngBytes.length} bytes) in ${stopwatch.elapsedMilliseconds}ms');

      notifyListeners();
      return outFile;
    } catch (e, stackTrace) {
      logger.severe('Failed to render intermediate with annotation', e, stackTrace);
      return null;
    }
  }

  /// Persist current dithering preview to an intermediate file for subsequent steps
  Future<File?> saveDitherPreview() async {
    logger.fine('Saving dithering preview to file');
    try {
      if (_ditherPreview == null) {
        logger.warning('No dithering preview available to save');
        return null;
      }
      final outFile = File('${Directory.systemTemp.path}/pf_dither_${DateTime.now().millisecondsSinceEpoch}.png');
      await outFile.writeAsBytes(_ditherPreview!);
      _intermediateFile = outFile;
      logger.info('Dithering preview saved: ${outFile.path} (${_ditherPreview!.length} bytes)');
      notifyListeners();
      return outFile;
    } catch (e, stackTrace) {
      logger.severe('Failed to save dithering preview', e, stackTrace);
      return null;
    }
  }

  /// Clear intermediate and dither preview when returning to crop (prevents stale cache)
  void clearDitheringData() {
    logger.fine('Clearing dithering data (intermediate and preview)');
    _intermediateFile = null;
    _ditherPreview = null;
    notifyListeners();
  }

  /// Generate binary data for Bluetooth upload with PFR1 header
  Future<Uint8List?> generateBinaryData() async {
    logger.fine('Generating binary data for Bluetooth upload with PFR1 header');
    final stopwatch = Stopwatch()..start();

    try {
      // Prefer the processed intermediate (crop/annotation), fallback to original
      final srcFile = _intermediateFile ?? (_image != null ? File(_image!.path) : null);
      if (srcFile == null) {
        logger.warning('No image available for binary conversion');
        return null;
      }

      Uint8List bytes = await srcFile.readAsBytes();
      logger.fine('Loading image for binary conversion: ${bytes.length} bytes');

      // Determine processing type: 0=BW, 1=6C
      final processingType = _job.displayType == DisplayType.blackAndWhite ? 0 : 1;

      // For now the header rotation is always 0; rotation is sent via BLE config.
      int rotationValue = 0;

      // Check if target resolution is portrait (width < height).
      // Images MUST be rotated to landscape orientation before binary conversion (display buffer is always landscape).
      // Calculate total rotation: portrait->landscape + user-applied rotation
      final isPortrait = _job.targetResolution.width < _job.targetResolution.height;
      int totalRotationAngle = 0;

      if (isPortrait) {
        totalRotationAngle += 90; // Force portrait to landscape (90° rotation)
      }
      totalRotationAngle += _job.rotation; // Add user-applied rotation
      totalRotationAngle = totalRotationAngle % 360; // Normalize to 0-360

      // rotation is 0 to 3 (0°, 90°, 180°, 270°) in header, but we need angle in degrees for image rotation
      rotationValue = totalRotationAngle ~/ 90;

      if (totalRotationAngle > 0) {
        final decoded = img.decodeImage(bytes);
        if (decoded != null) {
          final rotated = img.copyRotate(decoded, angle: totalRotationAngle);
          bytes = Uint8List.fromList(img.encodePng(rotated));
          logger.info(
            'Rotated image for bin generation: ${rotated.width}x${rotated.height} (was: ${decoded.width}x${decoded.height}, total rotation: $totalRotationAngle°, isPortrait: $isPortrait, userRotation: ${_job.rotation}°)',
          );
        } else {
          logger.warning('Failed to decode image for rotation; proceeding without rotation');
        }
      }

      // Convert to .pfr1 with PFR1 header using Rust FFI (header rotation=0)
      final binary = PhotoframeDithering.convertWithProcessing(imageBytes: bytes, processingType: processingType, rotation: rotationValue);

      stopwatch.stop();

      if (binary != null) {
        _binaryData = binary;
        logger.info('Binary data with PFR1 header generated: ${binary.length} bytes in ${stopwatch.elapsedMilliseconds}ms');
        notifyListeners();
      } else {
        logger.severe('Binary conversion returned null');
      }

      return binary;
    } catch (e, stackTrace) {
      logger.severe('Failed to generate binary data', e, stackTrace);
      return null;
    }
  }

  /// Save the generated .pfr1 file to the gallery using the original image filename
  Future<File?> savePfr1ToGallery() async {
    if (_binaryData == null) {
      logger.warning('No binary data available to save');
      return null;
    }

    if (_image == null) {
      logger.warning('No original image available for naming');
      return null;
    }

    try {
      final originalImageName = _image!.name; // Get filename from XFile
      logger.info('Saving .pfr1 file to gallery with name: $originalImageName');

      final savedFile = await GalleryService.savePfr1File(binaryData: _binaryData!, originalImageName: originalImageName);

      if (savedFile != null) {
        logger.info('Successfully saved .pfr1 file to gallery: ${savedFile.path}');
      }

      return savedFile;
    } catch (e, stackTrace) {
      logger.severe('Failed to save .pfr1 file to gallery', e, stackTrace);
      return null;
    }
  }
}
