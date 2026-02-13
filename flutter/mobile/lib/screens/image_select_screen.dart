import 'dart:io';
import 'package:flutter/material.dart';
import 'package:image_picker/image_picker.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:photoframe/screens/image_crop_screen.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';
import 'package:photoframe/l10n/app_localizations.dart';

/// Image select screen with BoardConfig info display and image selection
/// Allows user to pick an image from gallery and preview before upload
class ImageSelectScreen extends StatefulWidget {
  final BoardConfig boardConfig;

  const ImageSelectScreen({super.key, required this.boardConfig});

  @override
  State<ImageSelectScreen> createState() => _ImageSelectScreenState();
}

class _ImageSelectScreenState extends State<ImageSelectScreen> {
  File? _selectedImage;
  final ImagePicker _imagePicker = ImagePicker();

  Future<void> _pickImage() async {
    try {
      final pickedFile = await _imagePicker.pickImage(source: ImageSource.gallery, imageQuality: 95);

      if (pickedFile != null) {
        setState(() {
          _selectedImage = File(pickedFile.path);
        });
        logger.info('✓ Image selected: ${pickedFile.name}');
      } else {
        logger.info('Image selection cancelled');
      }
    } catch (e) {
      logger.severe('Failed to pick image: $e');
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l10n.pickImageFailedMessage(e.toString())), backgroundColor: colors.error));
      }
    }
  }

  void _continue() {
    if (_selectedImage == null) {
      final l10n = AppLocalizations.of(context)!;
      final colors = ThemeColors(context);
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l10n.selectImageFirstMessage), backgroundColor: colors.warning));
      return;
    }

    logger.info('Continuing to crop with image: ${_selectedImage!.path}');
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (_) => ImageCropScreen(imageFile: _selectedImage!, boardConfig: widget.boardConfig),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    final l10n = AppLocalizations.of(context)!;
    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.imageSelectTitle),
        elevation: 0,
        backgroundColor: colors.appBarBackground,
        foregroundColor: colors.appBarForeground,
      ),
      body: Column(
        mainAxisSize: MainAxisSize.max,
        children: [
          Expanded(
            child: SingleChildScrollView(
              padding: const EdgeInsets.only(bottom: 96, left: 16, right: 16, top: 16),
              child: Column(
                children: [
                  // BoardConfig info section
                  Container(
                    decoration: BoxDecoration(
                      border: Border.all(color: colors.borderLight, width: 2),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(l10n.deviceInfoTitle, style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                        const SizedBox(height: 12),
                        _buildInfoRow(l10n.deviceLabel, widget.boardConfig.board, colors),
                        _buildInfoRow(l10n.displayLabel, '${widget.boardConfig.displayWidth}×${widget.boardConfig.displayHeight}', colors),
                        _buildInfoRow(l10n.typeLabel, widget.boardConfig.displayType.toJsonValue(), colors),
                        _buildInfoRow(
                          l10n.rotationLabel,
                          '${widget.boardConfig.displayRotation.name} (${_rotationAngle(widget.boardConfig.displayRotation.value, l10n)})',
                          colors,
                        ),
                        if (widget.boardConfig.batteryLevel != null)
                          _buildInfoRow(
                            l10n.batteryLabel,
                            '${widget.boardConfig.batteryLevel}%${widget.boardConfig.batteryVoltageMv != null ? ' (${widget.boardConfig.batteryVoltageMv}mV)' : ''}',
                            colors,
                          ),
                      ],
                    ),
                  ),

                  const SizedBox(height: 24),

                  // Image picker section
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.center,
                      children: [
                        FilledButton.icon(onPressed: _pickImage, label: Text(l10n.pickImageFromGallery), icon: const Icon(Icons.image)),
                        const SizedBox(height: 16),
                        // Image preview or picker button
                        if (_selectedImage != null)
                          Container(
                            width: double.infinity,
                            decoration: BoxDecoration(
                              border: Border.all(color: colors.borderMedium, width: 2),
                              borderRadius: BorderRadius.circular(8),
                            ),
                            child: Image.file(_selectedImage!, fit: BoxFit.cover),
                          ),
                      ],
                    ),
                  ),

                  const SizedBox(height: 24),
                ],
              ),
            ),
          ),
          Flexible(
            flex: 0,
            child: SafeArea(
              child: Container(
                decoration: BoxDecoration(color: colors.surface),
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
                child: SizedBox(
                  width: double.infinity,
                  height: 48,
                  child: FilledButton(
                    onPressed: _selectedImage != null ? _continue : null,
                    child: Text(l10n.continueAction, style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildInfoRow(String label, String value, ThemeColors colors) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: TextStyle(fontSize: 14, color: colors.textSecondary)),
          Text(
            value,
            style: TextStyle(fontSize: 14, fontWeight: FontWeight.w500, color: colors.textPrimary),
          ),
        ],
      ),
    );
  }

  String _rotationAngle(int rotation, AppLocalizations l10n) {
    switch (rotation) {
      case 0:
        return '0°';
      case 1:
        return '90°';
      case 2:
        return '180°';
      case 3:
        return '270°';
      default:
        return l10n.rotationUnknown;
    }
  }
}
