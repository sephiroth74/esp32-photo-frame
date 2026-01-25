import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../../models/processing_models.dart';

class PreviewCard extends StatelessWidget {
  final ProcessingJob job;
  final File file;
  final Uint8List? bytes;
  final bool showAnnotation;
  final bool applyTransforms;
  final double? maxHeight;

  const PreviewCard({
    super.key,
    required this.job,
    required this.file,
    this.bytes,
    required this.showAnnotation,
    this.applyTransforms = true,
    this.maxHeight,
  });

  @override
  Widget build(BuildContext context) {
    final imageWidget = bytes != null
        ? Image.memory(bytes!, fit: BoxFit.cover, alignment: Alignment.center)
        : Image.file(file, fit: BoxFit.cover, alignment: Alignment.center);

    return SizedBox(
      width: double.infinity,
      height: maxHeight,
      child: Card(
        elevation: 0,
        clipBehavior: Clip.antiAlias,
        child: AspectRatio(
          aspectRatio: job.targetResolution.width / job.targetResolution.height,
          child: Stack(
            alignment: Alignment.center,
            children: [
              if (applyTransforms)
                Transform(
                  alignment: Alignment.center,
                  transform: Matrix4.identity()
                    ..translate(job.panOffset.dx, job.panOffset.dy)
                    ..scale(job.cropZoom),
                  child: RotatedBox(quarterTurns: job.rotation ~/ 90, child: imageWidget),
                )
              else
                imageWidget,
              if (showAnnotation && job.annotation.text.isNotEmpty)
                Container(
                  decoration: BoxDecoration(color: job.annotation.backgroundColor, borderRadius: BorderRadius.circular(8)),
                  padding: const EdgeInsets.all(12),
                  margin: const EdgeInsets.all(16),
                  child: Text(
                    job.annotation.text,
                    style: TextStyle(
                      color: job.annotation.textColor,
                      fontSize: job.annotation.fontSize,
                      fontFamily: job.annotation.fontFamily,
                    ),
                    textAlign: TextAlign.center,
                    maxLines: 5,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
