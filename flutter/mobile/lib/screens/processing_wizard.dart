import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/image_processing_state.dart';
import 'wizard/rotation_crop_step.dart';
import 'wizard/dithering_step.dart';
import 'wizard/annotation_step.dart';
import 'wizard/review_step.dart';

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
        final processed = state.intermediateFile ?? widget.imageFile;

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
                    RotationCropStep(job: job, file: widget.imageFile),
                    DitheringStep(job: job, file: processed),
                    AnnotationStep(job: job, file: processed),
                    ReviewStep(job: job, file: processed),
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
                  onPressed: () async {
                    if (_currentStep < 3) {
                      if (_currentStep == 0) {
                        // Render intermediate image before moving to next step
                        await context.read<ImageProcessingState>().renderIntermediateCrop(widget.imageFile);
                      }
                      _pageController.nextPage(duration: const Duration(milliseconds: 300), curve: Curves.easeInOut);
                    } else {
                      _finishWizard();
                    }
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
