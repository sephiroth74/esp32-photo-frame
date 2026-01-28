import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/processing_models.dart';
import '../../state/image_processing_state.dart';
import '../../utils/app_logger.dart';
import '../ble_upload_screen.dart';
import 'preview_card.dart';

final _logger = getLogger('ReviewStep');

class ReviewStep extends StatelessWidget {
  final ProcessingJob job;
  final File file;

  const ReviewStep({super.key, required this.job, required this.file});

  void _startBluetoothUpload(BuildContext context) async {
    final state = context.read<ImageProcessingState>();

    // Show loading indicator
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => const Center(
        child: Card(
          child: Padding(
            padding: EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [CircularProgressIndicator(), SizedBox(height: 16), Text('Generating binary data...')],
            ),
          ),
        ),
      ),
    );

    // Generate binary data
    final binaryData = await state.generateBinaryData();

    if (!context.mounted) return;
    Navigator.of(context).pop(); // Close loading dialog

    if (binaryData == null) {
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Failed to generate binary data')));
      return;
    }

    // Save .pfr1 file to gallery before uploading
    final savedFile = await state.savePfr1ToGallery();
    if (savedFile != null) {
      _logger.info('File saved to gallery: ${savedFile.path}');
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('File saved to gallery: ${savedFile.path}')));
      }
    } else {
      _logger.warning('Failed to save file to gallery, but continuing with upload');
    }

    // Navigate to Bluetooth upload wizard
    final intermediateFile = state.intermediateFile;
    if (intermediateFile != null && context.mounted) {
      Navigator.of(context).push(
        MaterialPageRoute(
          builder: (context) => BleUploadScreen(image: intermediateFile, job: job),
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ImageProcessingState>(
      builder: (context, state, _) {
        // Show intermediate file with annotation
        return FutureBuilder<Uint8List?>(
          future: state.getIntermediateFileAsBytes(),
          builder: (context, snapshot) {
            final previewBytes = snapshot.data;

            // Debug logging
            if (snapshot.connectionState == ConnectionState.done) {
              if (previewBytes != null) {
                _logger.info('Review: Showing intermediate file (${previewBytes.length} bytes)');
              } else {
                _logger.warning('Review: No intermediate file available, showing fallback PreviewCard');
                if (state.intermediateFile == null) {
                  _logger.warning('Review: state.intermediateFile is null!');
                } else {
                  _logger.info('Review: state.intermediateFile exists: ${state.intermediateFile!.path}');
                }
              }
            }

            return Column(
              children: [
                Expanded(
                  child: Center(
                    child: SingleChildScrollView(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          if (previewBytes != null)
                            ConstrainedBox(
                              constraints: BoxConstraints(maxHeight: MediaQuery.of(context).size.height * 0.6),
                              child: AspectRatio(
                                aspectRatio: job.targetResolution.width / job.targetResolution.height,
                                child: ClipRRect(
                                  borderRadius: BorderRadius.circular(12),
                                  child: Image.memory(previewBytes, fit: BoxFit.contain),
                                ),
                              ),
                            )
                          else
                            PreviewCard(
                              job: job,
                              file: file,
                              showAnnotation: true,
                              applyTransforms: false,
                              maxHeight: MediaQuery.of(context).size.height * 0.6,
                              showCard: false,
                            ),
                        ],
                      ),
                    ),
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.all(16),
                  child: SizedBox(
                    width: double.infinity,
                    child: FilledButton.icon(
                      onPressed: () => _startBluetoothUpload(context),
                      icon: const Icon(Icons.bluetooth),
                      label: const Text('Upload via Bluetooth'),
                      style: FilledButton.styleFrom(padding: const EdgeInsets.symmetric(vertical: 16)),
                    ),
                  ),
                ),
              ],
            );
          },
        );
      },
    );
  }
}
