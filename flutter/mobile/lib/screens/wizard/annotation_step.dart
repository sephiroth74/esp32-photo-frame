import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/processing_models.dart';
import '../../state/image_processing_state.dart';
import 'preview_card.dart';
import 'adjustment_slider.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';

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
        return Column(
          children: [
            // Fixed preview at top
            Container(
              padding: const EdgeInsets.all(16),
              child: GestureDetector(
                onLongPress: () {
                  showDialog(
                    context: context,
                    builder: (context) => Dialog(
                      backgroundColor: Colors.black,
                      child: Stack(
                        children: [
                          InteractiveViewer(
                            minScale: 0.5,
                            maxScale: 4.0,
                            child: Center(
                              child: PreviewCard(
                                job: widget.job,
                                file: widget.file,
                                showAnnotation: true,
                                applyTransforms: false,
                                maxHeight: MediaQuery.of(context).size.height * 0.85,
                                showCard: false,
                              ),
                            ),
                          ),
                          Positioned(
                            top: 8,
                            right: 8,
                            child: IconButton(
                              icon: const Icon(Icons.close, color: Colors.white),
                              onPressed: () => Navigator.of(context).pop(),
                            ),
                          ),
                        ],
                      ),
                    ),
                  );
                },
                child: PreviewCard(
                  job: widget.job,
                  file: widget.file,
                  showAnnotation: true,
                  applyTransforms: false,
                  maxHeight: MediaQuery.of(context).size.height * 0.30,
                  showCard: false,
                ),
              ),
            ),
            // Scrollable controls
            Expanded(
              child: SingleChildScrollView(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Annotation Text', style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 8),
                    TextField(
                      controller: _textController,
                      maxLines: 1,
                      maxLength: 30,
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
                      min: 6.0,
                      max: 16.0,
                      onChanged: (value) {
                        state.updateAnnotation(fontSize: value.toInt());
                      },
                    ),
                    const SizedBox(height: 24),
                    _ColorPickerLauncher(
                      label: 'Text Color',
                      color: widget.job.annotation.textColor,
                      onPick: (c) => state.updateAnnotation(textColor: c),
                    ),
                    const SizedBox(height: 16),
                    _ColorPickerLauncher(
                      label: 'Background Color',
                      color: widget.job.annotation.backgroundColor,
                      onPick: (c) => state.updateAnnotation(backgroundColor: c),
                    ),
                  ],
                ),
              ),
            ),
          ],
        );
      },
    );
  }
}

class _ColorPickerLauncher extends StatelessWidget {
  final String label;
  final Color color;
  final ValueChanged<Color> onPick;

  const _ColorPickerLauncher({required this.label, required this.color, required this.onPick});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Expanded(child: Text(label, style: Theme.of(context).textTheme.titleMedium)),
        InkWell(
          onTap: () async {
            Color current = color;
            await showDialog(
              context: context,
              builder: (ctx) {
                return AlertDialog(
                  title: Text(label),
                  content: SingleChildScrollView(
                    child: ColorPicker(
                      pickerColor: current,
                      enableAlpha: true,
                      labelTypes: const [],
                      onColorChanged: (c) {
                        current = c;
                        onPick(c); // live update
                      },
                      displayThumbColor: true,
                    ),
                  ),
                  actions: [TextButton(onPressed: () => Navigator.of(ctx).pop(), child: const Text('Done'))],
                );
              },
            );
          },
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: Colors.black12),
              color: color,
            ),
            child: Row(children: [const Icon(Icons.color_lens, size: 18), const SizedBox(width: 8), const Text('Pick')]),
          ),
        ),
      ],
    );
  }
}
