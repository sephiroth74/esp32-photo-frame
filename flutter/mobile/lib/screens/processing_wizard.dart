import 'dart:io';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:share_plus/share_plus.dart';

import '../state/image_processing_state.dart';
import '../utils/app_logger.dart';

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
    logger.fine('Wizard: Initialized with image ${widget.imageFile.path}');
    _pageController = PageController();
  }

  @override
  void dispose() {
    logger.fine('Wizard: Disposing');
    _pageController.dispose();
    super.dispose();
  }

  String _getTitleForStep(int step) {
    const titles = ['Crop & Rotate', 'Dithering', 'Review'];
    if (step >= 0 && step < titles.length) {
      return '${titles[step]} (${step + 1} of 3)';
    }
    return '';
  }

  Future<void> _finishWizard() async {
    logger.info('Wizard: Finishing - generating and saving .pfr1 file');

    // Show loading indicator
    if (!mounted) return;
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => const Center(
        child: Card(
          child: Padding(
            padding: EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [CircularProgressIndicator(), SizedBox(height: 16), Text('Generating .pfr1 file...')],
            ),
          ),
        ),
      ),
    );

    try {
      final state = context.read<ImageProcessingState>();

      // Generate binary data if not already generated
      final binaryData = state.binaryData ?? await state.generateBinaryData();
      if (!mounted) return;
      Navigator.of(context).pop(); // Close loading dialog

      if (binaryData == null) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Unable to generate .pfr1 file')));
        return;
      }

      // Save to gallery
      final savedFile = await state.savePfr1ToGallery();
      if (savedFile != null) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('File saved to gallery: ${savedFile.path}')));
          logger.info('Wizard: File saved successfully to gallery');
        }
      } else {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Error saving file')));
        }
        return;
      }

      // Close the wizard
      if (mounted) {
        Navigator.of(context).pop();
      }
    } catch (e, stackTrace) {
      logger.severe('Wizard: Error finishing wizard', e, stackTrace);
      if (mounted) {
        Navigator.of(context).pop(); // Close dialog if open
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Errore: $e')));
      }
    }
  }

  Future<void> _onShareBin(ImageProcessingState state) async {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => const Center(
        child: Card(
          child: Padding(
            padding: EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [CircularProgressIndicator(), SizedBox(height: 16), Text('Preparing .pfr1 file...')],
            ),
          ),
        ),
      ),
    );

    final binaryData = await state.generateBinaryData();

    if (!mounted) return;
    Navigator.of(context).pop();

    if (binaryData == null) {
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Unable to generate .pfr1 file')));
      return;
    }

    try {
      final tempFile = File('${Directory.systemTemp.path}/photoframe_${DateTime.now().millisecondsSinceEpoch}.pfr1');
      await tempFile.writeAsBytes(binaryData, flush: true);
      await Share.shareXFiles([XFile(tempFile.path)], text: 'PhotoFrame .pfr1 ready for desktop test');
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Sharing failed: $e')));
      }
    }
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
            actions: _currentStep == 2
                ? [IconButton(icon: const Icon(Icons.share), tooltip: 'Share .pfr1', onPressed: () => _onShareBin(state))]
                : [],
          ),
          body: Column(
            children: [
              LinearProgressIndicator(value: (_currentStep + 1) / 3, minHeight: 4),
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
                    // RotationCropStep(job: job, file: widget.imageFile),
                    // DitheringStep(job: job, file: processed),
                    // ReviewStep(job: job, file: processed),
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
                          logger.fine('Wizard: Back pressed from step $_currentStep');
                          // If returning from Dithering to Crop, clear stale intermediate (forces regeneration on next Next press)
                          if (_currentStep == 1) {
                            logger.fine('Wizard: Clearing dithering data when returning to Crop');
                            context.read<ImageProcessingState>().clearDitheringData();
                          }
                          _pageController.previousPage(duration: const Duration(milliseconds: 300), curve: Curves.easeInOut);
                        }
                      : null,
                  icon: const Icon(Icons.arrow_back),
                  label: const Text('Back'),
                ),
                FilledButton.icon(
                  onPressed: () async {
                    if (_currentStep < 2) {
                      logger.fine('Wizard: Next pressed from step $_currentStep');
                      if (_currentStep == 0) {
                        logger.fine('Wizard: Rendering intermediate crop');
                        // Render intermediate image before moving to next step
                        await context.read<ImageProcessingState>().renderIntermediateCrop(widget.imageFile);
                      } else if (_currentStep == 1) {
                        logger.fine('Wizard: Saving dithering preview');
                        // Save dithering preview as intermediate for next steps
                        await context.read<ImageProcessingState>().saveDitherPreview();
                      }
                      _pageController.nextPage(duration: const Duration(milliseconds: 300), curve: Curves.easeInOut);
                    } else {
                      logger.info('Wizard: Finishing wizard');
                      _finishWizard();
                    }
                  },
                  icon: Icon(_currentStep < 2 ? Icons.arrow_forward : Icons.check),
                  label: Text(_currentStep < 2 ? 'Next' : 'Finish'),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}
