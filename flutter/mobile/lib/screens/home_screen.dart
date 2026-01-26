import 'dart:io';

import 'package:flutter/material.dart';
import 'package:image_picker/image_picker.dart';
import 'package:provider/provider.dart';

import '../state/image_processing_state.dart';
import 'processing_wizard.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final state = context.watch<ImageProcessingState>();

    return Scaffold(
      appBar: AppBar(title: const Text('PhotoFrame Mobile')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Center(
          child: state.imageFile == null
              ? _EmptyState(onPick: () => _pickImage(context))
              : _SelectedImageView(file: state.imageFile!, onPlay: () => _startWizard(context), onDelete: state.clearImage),
        ),
      ),
      floatingActionButton: state.imageFile == null
          ? FloatingActionButton.extended(
              onPressed: () => _pickImage(context),
              icon: const Icon(Icons.photo_library_outlined),
              label: const Text('Scegli immagine'),
            )
          : null,
    );
  }

  Future<void> _pickImage(BuildContext context) async {
    final state = context.read<ImageProcessingState>();
    await state.pickImage(ImageSource.gallery);
  }

  void _startWizard(BuildContext context) {
    Navigator.of(context).push(MaterialPageRoute(builder: (_) => ProcessingWizardScreen(imageFile: context.read<ImageProcessingState>().imageFile!)));
  }
}

class _EmptyState extends StatelessWidget {
  final VoidCallback onPick;

  const _EmptyState({required this.onPick});

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        const Icon(Icons.photo_size_select_actual_outlined, size: 72, color: Colors.grey),
        const SizedBox(height: 16),
        const Text('Seleziona un\'immagine dalla galleria'),
        const SizedBox(height: 12),
        FilledButton.icon(onPressed: onPick, icon: const Icon(Icons.photo_library_outlined), label: const Text('Scegli immagine')),
      ],
    );
  }
}

class _SelectedImageView extends StatelessWidget {
  final File file;
  final VoidCallback onPlay;
  final VoidCallback onDelete;

  const _SelectedImageView({required this.file, required this.onPlay, required this.onDelete});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final double maxWidth = constraints.maxWidth.clamp(0, 600).toDouble();
        return SizedBox(
          width: maxWidth,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            mainAxisSize: MainAxisSize.max,
            children: [
              Padding(
                padding: const EdgeInsets.only(top: 12, bottom: 12),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  spacing: 8,
                  children: [
                    FilledButton.icon(onPressed: onPlay, icon: const Icon(Icons.play_arrow), label: const Text('Start')),
                    FilledButton.icon(
                      style: FilledButton.styleFrom(backgroundColor: Colors.red, foregroundColor: Colors.white),
                      onPressed: onDelete,
                      icon: const Icon(Icons.delete_outline),
                      label: const Text('Remove'),
                    ),
                  ],
                ),
              ),
              ClipRRect(
                borderRadius: BorderRadius.circular(16),
                child: AspectRatio(
                  aspectRatio: 4 / 3,
                  child: Image.file(file, fit: BoxFit.cover),
                ),
              ),
            ],
          ),
        );
      },
    );
  }
}
