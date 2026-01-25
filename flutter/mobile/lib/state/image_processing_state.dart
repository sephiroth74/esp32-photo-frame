import 'dart:io';
import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:image_picker/image_picker.dart';

import '../models/processing_models.dart';

class ImageProcessingState extends ChangeNotifier {
  XFile? _image;
  ProcessingJob _job = const ProcessingJob();

  XFile? get image => _image;
  ProcessingJob get job => _job;
  ProcessingJob get currentJob => _job;
  File? get imageFile => _image != null ? File(_image!.path) : null;

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

  void updateAdjustments({double? saturation, double? contrast, double? brightness}) {
    _job = _job.copyWith(
      saturation: saturation ?? _job.saturation,
      contrast: contrast ?? _job.contrast,
      brightness: brightness ?? _job.brightness,
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
}
