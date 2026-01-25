import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/processing_models.dart';
import '../../state/image_processing_state.dart';
import 'preview_card.dart';
import 'adjustment_slider.dart';
import 'color_dropdown.dart';

class AnnotationStep extends StatefulWidget {
  final ProcessingJob job;
  final File file;

  const AnnotationStep({super.key, required this.job, required this.file});

  @override
  State<AnnotationStep> createState() => _AnnotationStepState();
}

class _AnnotationStepState extends State<AnnotationStep> {
  late TextEditingController _textController;

  @override
  void initState() {
    super.initState();
    _textController = TextEditingController(text: widget.job.annotation.text);
  }

  @override
  void dispose() {
    _textController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ImageProcessingState>(
      builder: (context, state, _) {
        return SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PreviewCard(job: widget.job, file: widget.file, showAnnotation: true, applyTransforms: false),
              const SizedBox(height: 24),
              Text('Annotation Text', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              TextField(
                controller: _textController,
                maxLines: 3,
                decoration: InputDecoration(
                  hintText: 'Enter annotation text...',
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                ),
                onChanged: (text) {
                  state.updateAnnotation(text: text);
                },
              ),
              const SizedBox(height: 24),
              Text('Font', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              DropdownButton<String>(
                value: widget.job.annotation.fontFamily,
                isExpanded: true,
                items: [
                  'Arial',
                  'Times',
                  'Courier',
                  'Helvetica',
                  'Roboto',
                ].map((font) => DropdownMenuItem(value: font, child: Text(font))).toList(),
                onChanged: (font) {
                  if (font != null) {
                    state.updateAnnotation(font: font);
                  }
                },
              ),
              const SizedBox(height: 24),
              AdjustmentSlider(
                label: 'Font Size',
                value: widget.job.annotation.fontSize,
                min: 8.0,
                max: 48.0,
                onChanged: (value) {
                  state.updateAnnotation(fontSize: value.toInt());
                },
              ),
              const SizedBox(height: 24),
              Text('Text Color', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              ColorDropdown(
                value: widget.job.annotation.textColor,
                onChanged: (color) {
                  state.updateAnnotation(textColor: color);
                },
              ),
              const SizedBox(height: 16),
              Text('Background Color', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              ColorDropdown(
                value: widget.job.annotation.backgroundColor,
                onChanged: (color) {
                  state.updateAnnotation(backgroundColor: color);
                },
              ),
            ],
          ),
        );
      },
    );
  }
}
