import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/processing_models.dart';
import '../../state/image_processing_state.dart';
import '../../utils/app_logger.dart';
import 'preview_card.dart';
import 'adjustment_slider.dart';
import 'wizard_utils.dart';

class DitheringStep extends StatefulWidget {
  final ProcessingJob job;
  final File file;

  const DitheringStep({super.key, required this.job, required this.file});

  @override
  State<DitheringStep> createState() => _DitheringStepState();
}

class _DitheringStepState extends State<DitheringStep> {
  bool _busy = false;
  int _previewGeneration = 0;

  Future<void> _updatePreview(ImageProcessingState state) async {
    final current = ++_previewGeneration;
    logger.fine('DitheringStep: Updating preview (generation $current)');
    setState(() => _busy = true);
    await state.renderDitheringPreview();
    if (!mounted || current != _previewGeneration) {
      logger.fine('DitheringStep: Preview generation $current cancelled or widget disposed');
      return;
    }
    setState(() => _busy = false);
    logger.fine('DitheringStep: Preview updated');
  }

  void _showFullSizePreview(BuildContext context, ImageProcessingState state) {
    if (state.ditherPreview == null) return;

    showDialog(
      context: context,
      builder: (context) => Dialog(
        backgroundColor: Colors.black,
        child: Stack(
          children: [
            InteractiveViewer(minScale: 0.5, maxScale: 4.0, child: Center(child: Image.memory(state.ditherPreview!))),
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
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ImageProcessingState>(
      builder: (context, state, _) {
        final methods = DitheringMethod.values;
        final selected = methods.contains(widget.job.dithering) ? widget.job.dithering : DitheringMethod.floydSteinberg;
        // if enum changed during hot reload, normalize once
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!methods.contains(widget.job.dithering)) {
            state.updateDithering(selected);
          }
        });
        // ensure preview on enter
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (state.ditherPreview == null && !_busy) {
            _updatePreview(state);
          }
        });
        return Column(
          children: [
            // Fixed image preview at the top
            Container(
              padding: const EdgeInsets.all(16),
              child: GestureDetector(
                onLongPress: () => _showFullSizePreview(context, state),
                child: Stack(
                  alignment: Alignment.center,
                  children: [
                    PreviewCard(
                      job: widget.job,
                      file: widget.file,
                      bytes: state.ditherPreview,
                      showAnnotation: false,
                      applyTransforms: false,
                      maxHeight: MediaQuery.of(context).size.height * 0.35,
                      showCard: false,
                    ),
                    if (_busy)
                      Container(
                        padding: const EdgeInsets.all(12),
                        decoration: BoxDecoration(color: Colors.black.withAlpha(179), borderRadius: BorderRadius.circular(12)),
                        child: const SizedBox(width: 32, height: 32, child: CircularProgressIndicator(strokeWidth: 3, color: Colors.white)),
                      ),
                  ],
                ),
              ),
            ),
            // Scrollable controls section
            Expanded(
              child: SingleChildScrollView(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const SizedBox(height: 24),
                    Text('Dithering Method', style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 8),
                    DropdownButton<DitheringMethod>(
                      value: selected,
                      isExpanded: true,
                      items: DitheringMethod.values.map((method) => DropdownMenuItem(value: method, child: Text(labelForDithering(method)))).toList(),
                      onChanged: (method) {
                        if (method != null) {
                          state.updateDithering(method);
                          _updatePreview(state);
                        }
                      },
                    ),
                    const SizedBox(height: 24),
                    Text('Display Type', style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 8),
                    SegmentedButton<DisplayType>(
                      segments: const [
                        ButtonSegment(value: DisplayType.sixColors, label: Text('6 Colors')),
                        ButtonSegment(value: DisplayType.blackAndWhite, label: Text('B/W')),
                      ],
                      selected: {widget.job.displayType},
                      onSelectionChanged: (Set<DisplayType> newSelection) {
                        state.updateDisplayType(newSelection.first);
                        _updatePreview(state);
                      },
                    ),
                    const SizedBox(height: 24),
                    Text('Dithering Intensity', style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 16),
                    AdjustmentSlider(
                      label: 'Strength',
                      value: widget.job.ditherStrength,
                      min: 0.0,
                      max: 2.0,
                      onChanged: (value) {
                        state.updateAdjustments(ditherStrength: value);
                      },
                      onChangeEnd: (value) {
                        _updatePreview(state);
                      },
                    ),
                    const SizedBox(height: 24),
                    Text('Color Adjustments', style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 16),
                    AdjustmentSlider(
                      label: 'Saturation',
                      value: widget.job.saturation,
                      min: 0.0,
                      max: 3.0,
                      onChanged: (value) {
                        state.updateAdjustments(saturation: value);
                      },
                      onChangeEnd: (value) {
                        _updatePreview(state);
                      },
                    ),
                    AdjustmentSlider(
                      label: 'Contrast',
                      value: widget.job.contrast,
                      min: 0.0,
                      max: 3.0,
                      onChanged: (value) {
                        state.updateAdjustments(contrast: value);
                      },
                      onChangeEnd: (value) {
                        _updatePreview(state);
                      },
                    ),
                    AdjustmentSlider(
                      label: 'Brightness',
                      value: widget.job.brightness,
                      min: 0.0,
                      max: 3.0,
                      onChanged: (value) {
                        state.updateAdjustments(brightness: value);
                      },
                      onChangeEnd: (value) {
                        _updatePreview(state);
                      },
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
