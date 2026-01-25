import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/processing_models.dart';
import '../state/image_processing_state.dart';

class ProcessingWizardScreen extends StatefulWidget {
  final File imageFile;

  const ProcessingWizardScreen({super.key, required this.imageFile});

  @override
  State<ProcessingWizardScreen> createState() => _ProcessingWizardScreenState();
}

class _ProcessingWizardScreenState extends State<ProcessingWizardScreen> {
  late PageController _pageController;
  int _currentStep = 0;

  @override
  void initState() {
    super.initState();
    _pageController = PageController();
  }

  @override
  void dispose() {
    _pageController.dispose();
    super.dispose();
  }

  String _getTitleForStep(int step) {
    const titles = ['Crop & Rotate', 'Dithering', 'Annotation', 'Review'];
    if (step >= 0 && step < titles.length) {
      return titles[step];
    }
    return '';
  }

  void _finishWizard() {
    Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ImageProcessingState>(
      builder: (context, state, _) {
        final job = state.currentJob;

        return Scaffold(
          appBar: AppBar(
            title: Text(_getTitleForStep(_currentStep)),
            elevation: 0,
            backgroundColor: Theme.of(context).colorScheme.primary,
            foregroundColor: Theme.of(context).colorScheme.onPrimary,
          ),
          body: Column(
            children: [
              LinearProgressIndicator(value: (_currentStep + 1) / 4, minHeight: 4),
              Padding(
                padding: const EdgeInsets.all(16),
                child: Text('Step ${_currentStep + 1} of 4', style: Theme.of(context).textTheme.bodySmall),
              ),
              Expanded(
                child: PageView(
                  controller: _pageController,
                  physics: const NeverScrollableScrollPhysics(),
                  onPageChanged: (index) {
                    setState(() {
                      _currentStep = index;
                    });
                  },
                  children: [
                    _RotationCropStep(job: job, file: widget.imageFile),
                    _DitheringStep(job: job, file: widget.imageFile),
                    _AnnotationStep(job: job, file: widget.imageFile),
                    _ReviewStep(job: job, file: widget.imageFile),
                  ],
                ),
              ),
            ],
          ),
          bottomNavigationBar: Padding(
            padding: const EdgeInsets.all(16),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                ElevatedButton.icon(
                  onPressed: _currentStep > 0
                      ? () {
                          _pageController.previousPage(duration: const Duration(milliseconds: 300), curve: Curves.easeInOut);
                        }
                      : null,
                  icon: const Icon(Icons.arrow_back),
                  label: const Text('Back'),
                ),
                FilledButton.icon(
                  onPressed: _currentStep < 3
                      ? () {
                          _pageController.nextPage(duration: const Duration(milliseconds: 300), curve: Curves.easeInOut);
                        }
                      : () {
                          _finishWizard();
                        },
                  icon: Icon(_currentStep < 3 ? Icons.arrow_forward : Icons.check),
                  label: Text(_currentStep < 3 ? 'Next' : 'Finish'),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

class _RotationCropStep extends StatefulWidget {
  final ProcessingJob job;
  final File file;

  const _RotationCropStep({required this.job, required this.file});

  @override
  State<_RotationCropStep> createState() => _RotationCropStepState();
}

class _RotationCropStepState extends State<_RotationCropStep> {
  late Offset _panOffset;
  late double _cropZoom;

  @override
  void initState() {
    super.initState();
    _panOffset = widget.job.panOffset;
    _cropZoom = widget.job.cropZoom;
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ImageProcessingState>(
      builder: (context, state, _) {
        return Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            children: [
              Expanded(
                child: LayoutBuilder(
                  builder: (context, constraints) {
                    // Aspect ratio in base alla risoluzione target: 800x480 landscape, 480x800 portrait
                    final isLandscape = widget.job.targetResolution.width > widget.job.targetResolution.height;
                    final targetAspectRatio = isLandscape ? 800 / 480 : 480 / 800;

                    // Calcola dimensioni basate su aspect ratio
                    double containerWidth = constraints.maxWidth;
                    double containerHeight = containerWidth / targetAspectRatio;

                    // Se l'altezza supera lo spazio disponibile, ricalcola in base all'altezza
                    if (containerHeight > constraints.maxHeight - 80) {
                      containerHeight = constraints.maxHeight - 80;
                      containerWidth = containerHeight * targetAspectRatio;
                    }

                    return Center(
                      child: GestureDetector(
                        onScaleUpdate: (details) {
                          setState(() {
                            // Zoom più lento: usa solo il 30% della scala per rallentare
                            final scaleAdjustment = 1.0 + (details.scale - 1.0) * 0.3;
                            _cropZoom = (widget.job.cropZoom * scaleAdjustment).clamp(1.0, 5.0);

                            // Pan con limitazione basata su zoom
                            final dx = details.focalPointDelta.dx;
                            final dy = details.focalPointDelta.dy;
                            final maxPanX = containerWidth * (_cropZoom - 1.0) / 2;
                            final maxPanY = containerHeight * (_cropZoom - 1.0) / 2;

                            _panOffset = Offset(
                              (_panOffset.dx + dx).clamp(-maxPanX, maxPanX),
                              (_panOffset.dy + dy).clamp(-maxPanY, maxPanY),
                            );
                          });
                          state.updateCropZoom(_cropZoom);
                          state.updatePanOffset(_panOffset);
                        },
                        onDoubleTap: () {
                          setState(() {
                            _cropZoom = 1.0;
                            _panOffset = Offset.zero;
                          });
                          state.updateCropZoom(_cropZoom);
                          state.updatePanOffset(_panOffset);
                        },
                        child: SizedBox(
                          width: containerWidth,
                          height: containerHeight,
                          child: Container(
                            decoration: BoxDecoration(
                              color: Colors.black,
                              border: Border.all(color: Theme.of(context).colorScheme.primary, width: 2),
                            ),
                            clipBehavior: Clip.antiAlias,
                            child: Stack(
                              alignment: Alignment.center,
                              children: [
                                // Immagine con fit contenuto nel box
                                SizedBox.expand(
                                  child: Transform(
                                    alignment: Alignment.center,
                                    transform: Matrix4.identity()
                                      ..translate(_panOffset.dx, _panOffset.dy)
                                      ..scale(_cropZoom),
                                    child: RotatedBox(
                                      quarterTurns: widget.job.rotation ~/ 90,
                                      child: Image.file(widget.file, fit: BoxFit.cover, alignment: Alignment.center),
                                    ),
                                  ),
                                ),
                                // Guida visiva di cropping
                                IgnorePointer(
                                  child: Container(
                                    decoration: BoxDecoration(
                                      border: Border.all(color: Colors.yellow.withOpacity(0.5), width: 1, style: BorderStyle.solid),
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                      ),
                    );
                  },
                ),
              ),
              const SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  IconButton(
                    onPressed: () {
                      final newRotation = (widget.job.rotation - 90) % 360;
                      state.updateRotation(newRotation);
                    },
                    icon: const Icon(Icons.rotate_left),
                  ),
                  Text('${widget.job.rotation}°', style: Theme.of(context).textTheme.bodyMedium),
                  IconButton(
                    onPressed: () {
                      final newRotation = (widget.job.rotation + 90) % 360;
                      state.updateRotation(newRotation);
                    },
                    icon: const Icon(Icons.rotate_right),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              SegmentedButton<bool>(
                segments: const [
                  ButtonSegment(value: false, label: Text('Portrait')),
                  ButtonSegment(value: true, label: Text('Landscape')),
                ],
                selected: {widget.job.targetResolution.width > widget.job.targetResolution.height},
                onSelectionChanged: (Set<bool> newSelection) {
                  state.updateTargetResolution(landscape: newSelection.first);
                },
              ),
            ],
          ),
        );
      },
    );
  }
}

class _DitheringStep extends StatelessWidget {
  final ProcessingJob job;
  final File file;

  const _DitheringStep({required this.job, required this.file});

  @override
  Widget build(BuildContext context) {
    return Consumer<ImageProcessingState>(
      builder: (context, state, _) {
        return SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _PreviewCard(job: job, file: file, showAnnotation: false),
              const SizedBox(height: 24),
              Text('Dithering Method', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              DropdownButton<DitheringMethod>(
                value: job.dithering,
                isExpanded: true,
                items: DitheringMethod.values
                    .map((method) => DropdownMenuItem(value: method, child: Text(_labelForDithering(method))))
                    .toList(),
                onChanged: (method) {
                  if (method != null) {
                    state.updateDithering(method);
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
                selected: {job.displayType},
                onSelectionChanged: (Set<DisplayType> newSelection) {
                  state.updateDisplayType(newSelection.first);
                },
              ),
              const SizedBox(height: 24),
              Text('Color Adjustments', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 16),
              _AdjustmentSlider(
                label: 'Saturation',
                value: job.saturation,
                min: 0.0,
                max: 2.0,
                onChanged: (value) {
                  state.updateAdjustments(saturation: value);
                },
              ),
              _AdjustmentSlider(
                label: 'Contrast',
                value: job.contrast,
                min: 0.5,
                max: 2.0,
                onChanged: (value) {
                  state.updateAdjustments(contrast: value);
                },
              ),
              _AdjustmentSlider(
                label: 'Brightness',
                value: job.brightness,
                min: 0.5,
                max: 2.0,
                onChanged: (value) {
                  state.updateAdjustments(brightness: value);
                },
              ),
            ],
          ),
        );
      },
    );
  }
}

class _AnnotationStep extends StatefulWidget {
  final ProcessingJob job;
  final File file;

  const _AnnotationStep({required this.job, required this.file});

  @override
  State<_AnnotationStep> createState() => _AnnotationStepState();
}

class _AnnotationStepState extends State<_AnnotationStep> {
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
              _PreviewCard(job: widget.job, file: widget.file, showAnnotation: true),
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
                ].map((font) => DropdownMenuItem(value: font, child: Text(font))).toList(),
                onChanged: (font) {
                  if (font != null) {
                    state.updateAnnotation(font: font);
                  }
                },
              ),
              const SizedBox(height: 24),
              _AdjustmentSlider(
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
              _ColorDropdown(
                value: widget.job.annotation.textColor,
                onChanged: (color) {
                  state.updateAnnotation(textColor: color);
                },
              ),
              const SizedBox(height: 16),
              Text('Background Color', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              _ColorDropdown(
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

class _ReviewStep extends StatelessWidget {
  final ProcessingJob job;
  final File file;

  const _ReviewStep({required this.job, required this.file});

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _PreviewCard(job: job, file: file, showAnnotation: true),
          const SizedBox(height: 24),
          Text('Processing Summary', style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 16),
          _SummaryTile(label: 'Rotation', value: '${job.rotation}°'),
          _SummaryTile(label: 'Zoom', value: '${job.cropZoom.toStringAsFixed(2)}x'),
          _SummaryTile(label: 'Dithering', value: _labelForDithering(job.dithering)),
          _SummaryTile(
            label: 'Display Type',
            value: switch (job.displayType) {
              DisplayType.sixColors => '6 Colors',
              DisplayType.blackAndWhite => 'B/W',
            },
          ),
          _SummaryTile(label: 'Saturation', value: '${(job.saturation * 100).toStringAsFixed(0)}%'),
          _SummaryTile(label: 'Contrast', value: '${(job.contrast * 100).toStringAsFixed(0)}%'),
          _SummaryTile(label: 'Brightness', value: '${(job.brightness * 100).toStringAsFixed(0)}%'),
          if (job.annotation.text.isNotEmpty) ...[
            const SizedBox(height: 16),
            _SummaryTile(label: 'Annotation', value: job.annotation.text),
          ],
        ],
      ),
    );
  }
}

class _PreviewCard extends StatelessWidget {
  final ProcessingJob job;
  final File file;
  final bool showAnnotation;

  const _PreviewCard({required this.job, required this.file, required this.showAnnotation});

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 4,
      child: AspectRatio(
        aspectRatio: job.targetResolution.width / job.targetResolution.height,
        child: ColoredBox(
          color: Colors.black,
          child: Stack(
          alignment: Alignment.center,
          children: [
            Transform(
              alignment: Alignment.center,
              transform: Matrix4.identity()
                ..translate(job.panOffset.dx, job.panOffset.dy)
                ..scale(job.cropZoom),
              child: RotatedBox(
                quarterTurns: job.rotation ~/ 90,
                child: Image.file(
                  file,
                  fit: BoxFit.cover,
                  alignment: Alignment.center,
                ),
              ),
            ),
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

class _AdjustmentSlider extends StatelessWidget {
  final String label;
  final double value;
  final double min;
  final double max;
  final ValueChanged<double> onChanged;

  const _AdjustmentSlider({
    required this.label,
    required this.value,
    required this.min,
    required this.max,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(label),
            Text(value.toStringAsFixed(2), style: Theme.of(context).textTheme.bodySmall),
          ],
        ),
        Slider(value: value, min: min, max: max, onChanged: onChanged),
        const SizedBox(height: 16),
      ],
    );
  }
}

class _ColorDropdown extends StatelessWidget {
  final Color value;
  final ValueChanged<Color> onChanged;

  const _ColorDropdown({required this.value, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    final colorMap = {
      Colors.white: 'White',
      Colors.black: 'Black',
      Colors.red: 'Red',
      Colors.green: 'Green',
      Colors.blue: 'Blue',
      Colors.yellow: 'Yellow',
    };

    return DropdownButton<Color>(
      value: value,
      isExpanded: true,
      items: colorMap.entries
          .map(
            (entry) => DropdownMenuItem(
              value: entry.key,
              child: Row(
                children: [
                  Container(
                    width: 20,
                    height: 20,
                    decoration: BoxDecoration(
                      color: entry.key,
                      border: Border.all(color: Colors.grey),
                      borderRadius: BorderRadius.circular(4),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Text(entry.value),
                ],
              ),
            ),
          )
          .toList(),
      onChanged: (color) {
        if (color != null) {
          onChanged(color);
        }
      },
    );
  }
}

class _SummaryTile extends StatelessWidget {
  final String label;
  final String value;

  const _SummaryTile({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: Theme.of(context).textTheme.bodyMedium),
          Text(value, style: Theme.of(context).textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.bold)),
        ],
      ),
    );
  }
}

String _labelForDithering(DitheringMethod method) {
  return switch (method) {
    DitheringMethod.none => 'None',
    DitheringMethod.ordered => 'Ordered',
    DitheringMethod.floydSteinberg => 'Floyd-Steinberg',
    DitheringMethod.bayerMatrix => 'Bayer Matrix',
    DitheringMethod.sierra => 'Sierra',
  };
}
