import 'dart:io';

import 'package:flutter/material.dart';

import '../../models/processing_models.dart';
import 'preview_card.dart';
import 'summary_tile.dart';
import 'wizard_utils.dart';

class ReviewStep extends StatelessWidget {
  final ProcessingJob job;
  final File file;

  const ReviewStep({super.key, required this.job, required this.file});

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          PreviewCard(job: job, file: file, showAnnotation: true, applyTransforms: false),
          const SizedBox(height: 24),
          Text('Processing Summary', style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 16),
          SummaryTile(label: 'Rotation', value: '${job.rotation}°'),
          SummaryTile(label: 'Zoom', value: '${job.cropZoom.toStringAsFixed(2)}x'),
          SummaryTile(label: 'Dithering', value: labelForDithering(job.dithering)),
          SummaryTile(
            label: 'Display Type',
            value: switch (job.displayType) {
              DisplayType.sixColors => '6 Colors',
              DisplayType.blackAndWhite => 'B/W',
            },
          ),
          SummaryTile(label: 'Saturation', value: '${(job.saturation * 100).toStringAsFixed(0)}%'),
          SummaryTile(label: 'Contrast', value: '${(job.contrast * 100).toStringAsFixed(0)}%'),
          SummaryTile(label: 'Brightness', value: '${(job.brightness * 100).toStringAsFixed(0)}%'),
          if (job.annotation.text.isNotEmpty) ...[
            const SizedBox(height: 16),
            SummaryTile(label: 'Annotation', value: job.annotation.text),
          ],
        ],
      ),
    );
  }
}
