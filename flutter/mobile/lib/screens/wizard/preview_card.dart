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
  final bool showCard;

  const PreviewCard({
    super.key,
    required this.job,
    required this.file,
    this.bytes,
    required this.showAnnotation,
    this.applyTransforms = true,
    this.maxHeight,
    this.showCard = true,
  });

  @override
  Widget build(BuildContext context) {
    final imageWidget = bytes != null
        ? Image.memory(bytes!, fit: BoxFit.cover, alignment: Alignment.center)
        : Image.file(file, fit: BoxFit.cover, alignment: Alignment.center);

    final BorderRadius radius = BorderRadius.circular(12);

    Widget buildStackContent({required bool clipImage}) {
      // Build image + overlay together so overlay is anchored to image bounds
      Widget imageWithOverlay = Stack(
        children: [
          Positioned.fill(child: imageWidget),
          if (showAnnotation && job.annotation.text.isNotEmpty)
            Positioned(
              right: 5,
              bottom: 5,
              child: Container(
                decoration: BoxDecoration(color: job.annotation.backgroundColor, borderRadius: BorderRadius.circular(8)),
                padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                child: Text(
                  job.annotation.text,
                  style: TextStyle(color: job.annotation.textColor, fontSize: job.annotation.fontSize, fontFamily: job.annotation.fontFamily),
                  textAlign: TextAlign.right,
                  maxLines: 3,
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ),
        ],
      );

      if (applyTransforms) {
        imageWithOverlay = Transform(
          alignment: Alignment.center,
          transform: Matrix4.identity()
            // ignore: deprecated_member_use
            ..translate(job.panOffset.dx, job.panOffset.dy)
            // ignore: deprecated_member_use
            ..scale(job.cropZoom),
          child: RotatedBox(quarterTurns: job.rotation ~/ 90, child: imageWithOverlay),
        );
      }

      final Widget imageLayer = clipImage ? ClipRRect(borderRadius: radius, child: imageWithOverlay) : imageWithOverlay;

      return Stack(children: [imageLayer]);
    }

    final stackContent = buildStackContent(clipImage: !showCard);

    if (showCard) {
      final content = AspectRatio(aspectRatio: job.targetResolution.width / job.targetResolution.height, child: stackContent);
      return SizedBox(
        width: double.infinity,
        height: maxHeight,
        child: Card(elevation: 0, clipBehavior: Clip.antiAlias, child: content),
      );
    } else {
      // For non-card (showCard: false), constrain height then apply aspect ratio
      return ConstrainedBox(
        constraints: maxHeight != null ? BoxConstraints(maxHeight: maxHeight!) : const BoxConstraints(),
        child: AspectRatio(
          aspectRatio: job.targetResolution.width / job.targetResolution.height,
          child: ClipRRect(borderRadius: radius, child: stackContent),
        ),
      );
    }
  }
}
