import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/processing_models.dart';
import '../../state/image_processing_state.dart';

class RotationCropStep extends StatefulWidget {
  final ProcessingJob job;
  final File file;

  const RotationCropStep({super.key, required this.job, required this.file});

  @override
  State<RotationCropStep> createState() => _RotationCropStepState();
}

class _RotationCropStepState extends State<RotationCropStep> {
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
                    final isLandscape = widget.job.targetResolution.width > widget.job.targetResolution.height;
                    final targetAspectRatio = isLandscape ? 800 / 480 : 480 / 800;

                    double containerWidth = constraints.maxWidth;
                    double containerHeight = containerWidth / targetAspectRatio;

                    if (containerHeight > constraints.maxHeight - 80) {
                      containerHeight = constraints.maxHeight - 80;
                      containerWidth = containerHeight * targetAspectRatio;
                    }

                    return Center(
                      child: GestureDetector(
                        onScaleUpdate: (details) {
                          setState(() {
                            final scaleAdjustment = 1.0 + (details.scale - 1.0) * 0.3;
                            _cropZoom = (widget.job.cropZoom * scaleAdjustment).clamp(1.0, 5.0);

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
                      // Reset zoom/pan to avoid any borders after rotation change
                      setState(() {
                        _cropZoom = 1.0;
                        _panOffset = Offset.zero;
                      });
                      state.updateRotation(newRotation);
                      state.updateCropZoom(_cropZoom);
                      state.updatePanOffset(_panOffset);
                    },
                    icon: const Icon(Icons.rotate_left),
                  ),
                  Text('${widget.job.rotation}°', style: Theme.of(context).textTheme.bodyMedium),
                  IconButton(
                    onPressed: () {
                      final newRotation = (widget.job.rotation + 90) % 360;
                      // Reset zoom/pan to avoid any borders after rotation change
                      setState(() {
                        _cropZoom = 1.0;
                        _panOffset = Offset.zero;
                      });
                      state.updateRotation(newRotation);
                      state.updateCropZoom(_cropZoom);
                      state.updatePanOffset(_panOffset);
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
                  // Reset zoom/pan when toggling orientation to avoid borders
                  setState(() {
                    _cropZoom = 1.0;
                    _panOffset = Offset.zero;
                  });
                  state.updateTargetResolution(landscape: newSelection.first);
                  state.updateCropZoom(_cropZoom);
                  state.updatePanOffset(_panOffset);
                },
              ),
            ],
          ),
        );
      },
    );
  }
}
