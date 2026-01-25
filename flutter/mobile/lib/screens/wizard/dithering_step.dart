import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/processing_models.dart';
import '../../state/image_processing_state.dart';
import 'preview_card.dart';
import 'adjustment_slider.dart';
import 'wizard_utils.dart';
// duplicate import removed

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
    setState(() => _busy = true);
    await state.renderDitheringPreview();
    if (!mounted || current != _previewGeneration) return;
    setState(() => _busy = false);
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
        return SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Stack(
                alignment: Alignment.center,
                children: [
                  PreviewCard(
                    job: widget.job,
                    file: widget.file,
                    bytes: state.ditherPreview,
                    showAnnotation: false,
                    applyTransforms: false,
                    maxHeight: MediaQuery.of(context).size.height * 0.4,
                  ),
                  if (_busy)
                    Container(
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(color: Colors.black.withOpacity(0.7), borderRadius: BorderRadius.circular(12)),
                      child: const SizedBox(
                        width: 32,
                        height: 32,
                        child: CircularProgressIndicator(strokeWidth: 3, color: Colors.white),
                      ),
                    ),
                ],
              ),
              const SizedBox(height: 24),
              Text('Dithering Method', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              DropdownButton<DitheringMethod>(
                value: selected,
                isExpanded: true,
                items: DitheringMethod.values
                    .map((method) => DropdownMenuItem(value: method, child: Text(labelForDithering(method))))
                    .toList(),
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
                max: 1.5,
                onChanged: (value) {
                  state.updateAdjustments(ditherStrength: value);
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
                max: 2.0,
                onChanged: (value) {
                  state.updateAdjustments(saturation: value);
                  _updatePreview(state);
                },
              ),
              AdjustmentSlider(
                label: 'Contrast',
                value: widget.job.contrast,
                min: 0.5,
                max: 2.0,
                onChanged: (value) {
                  state.updateAdjustments(contrast: value);
                  _updatePreview(state);
                },
              ),
              AdjustmentSlider(
                label: 'Brightness',
                value: widget.job.brightness,
                min: 0.5,
                max: 2.0,
                onChanged: (value) {
                  state.updateAdjustments(brightness: value);
                  _updatePreview(state);
                },
              ),
            ],
          ),
        );
      },
    );
  }
}
