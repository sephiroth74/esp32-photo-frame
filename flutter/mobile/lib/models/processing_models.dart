import 'package:flutter/material.dart';

enum DisplayType { sixColors, blackAndWhite }

enum DitheringMethod { floydSteinberg, atkinson, stucki, jarvisJudiceNinke, ordered }

typedef Resolution = Size;

class AnnotationSettings {
  final String text;
  final String fontFamily;
  final double fontSize;
  final Color textColor;
  final Color backgroundColor;

  const AnnotationSettings({
    this.text = '',
    this.fontFamily = 'Roboto',
    this.fontSize = 16,
    this.textColor = Colors.white,
    this.backgroundColor = Colors.black,
  });

  AnnotationSettings copyWith({String? text, String? fontFamily, double? fontSize, Color? textColor, Color? backgroundColor}) {
    return AnnotationSettings(
      text: text ?? this.text,
      fontFamily: fontFamily ?? this.fontFamily,
      fontSize: fontSize ?? this.fontSize,
      textColor: textColor ?? this.textColor,
      backgroundColor: backgroundColor ?? this.backgroundColor,
    );
  }
}

class ProcessingJob {
  final int rotation;
  final Resolution targetResolution;
  final double cropZoom;
  final Offset panOffset;
  final DitheringMethod dithering;
  final DisplayType displayType;
  final double saturation;
  final double contrast;
  final double brightness;
  final double ditherStrength;
  final AnnotationSettings annotation;

  const ProcessingJob({
    this.rotation = 0,
    this.targetResolution = const Resolution(800, 480),
    this.cropZoom = 1.0,
    this.panOffset = Offset.zero,
    this.dithering = DitheringMethod.atkinson,
    this.displayType = DisplayType.sixColors,
    this.saturation = 1.2,
    this.contrast = 1.15,
    this.brightness = 1.2,
    this.ditherStrength = 1.05,
    this.annotation = const AnnotationSettings(),
  });

  ProcessingJob copyWith({
    int? rotation,
    Resolution? targetResolution,
    double? cropZoom,
    Offset? panOffset,
    DitheringMethod? dithering,
    DisplayType? displayType,
    double? saturation,
    double? contrast,
    double? brightness,
    double? ditherStrength,
    AnnotationSettings? annotation,
  }) {
    return ProcessingJob(
      rotation: rotation ?? this.rotation,
      targetResolution: targetResolution ?? this.targetResolution,
      cropZoom: cropZoom ?? this.cropZoom,
      panOffset: panOffset ?? this.panOffset,
      dithering: dithering ?? this.dithering,
      displayType: displayType ?? this.displayType,
      saturation: saturation ?? this.saturation,
      contrast: contrast ?? this.contrast,
      brightness: brightness ?? this.brightness,
      ditherStrength: ditherStrength ?? this.ditherStrength,
      annotation: annotation ?? this.annotation,
    );
  }

  bool get isPortrait => rotation % 180 != 0;
  bool get isLandscape => !isPortrait;
}
