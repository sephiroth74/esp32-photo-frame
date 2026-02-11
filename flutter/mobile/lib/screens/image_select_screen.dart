import 'dart:io';
import 'package:flutter/material.dart';
import 'package:image_picker/image_picker.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:photoframe/screens/image_crop_screen.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';

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
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to pick image: $e'), backgroundColor: colors.error));
      }
    }
  }

  void _clearImage() {
    setState(() {
      _selectedImage = null;
    });
    logger.info('Image selection cleared');
  }

  void _continue() {
    if (_selectedImage == null) {
      final colors = ThemeColors(context);
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: const Text('Please select an image first'), backgroundColor: colors.warning));
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
    return Scaffold(
      appBar: AppBar(
        title: const Text('Image Select'),
        elevation: 0,
        backgroundColor: colors.appBarBackground,
        foregroundColor: colors.appBarForeground,
      ),
      body: Stack(
        children: [
          SingleChildScrollView(
            padding: const EdgeInsets.only(bottom: 96),
            child: Column(
              children: [
                // BoardConfig info section
                Container(
                  color: colors.background,
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text('Device Information', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                      const SizedBox(height: 12),
                      _buildInfoRow('Device', widget.boardConfig.board, colors),
                      _buildInfoRow('Display', '${widget.boardConfig.displayWidth}×${widget.boardConfig.displayHeight}', colors),
                      _buildInfoRow('Type', widget.boardConfig.displayType.toJsonValue(), colors),
                      _buildInfoRow(
                        'Rotation',
                        '${widget.boardConfig.displayRotation.name} (${_rotationAngle(widget.boardConfig.displayRotation.value)})',
                        colors,
                      ),
                      if (widget.boardConfig.batteryLevel != null)
                        _buildInfoRow(
                          'Battery',
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
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text('Select Image', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                      const SizedBox(height: 16),

                      // Image preview or picker button
                      if (_selectedImage == null)
                        GestureDetector(
                          onTap: _pickImage,
                          child: Container(
                            width: double.infinity,
                            height: 200,
                            decoration: BoxDecoration(
                              border: Border.all(color: colors.borderLight, width: 2),
                              borderRadius: BorderRadius.circular(8),
                              color: colors.surfaceLight,
                            ),
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                Icon(Icons.image_outlined, size: 64, color: colors.textHint),
                                const SizedBox(height: 12),
                                Text('Tap to select an image', style: TextStyle(fontSize: 16, color: colors.textSecondary)),
                              ],
                            ),
                          ),
                        )
                      else
                        Stack(
                          children: [
                            // Image preview
                            Container(
                              width: double.infinity,
                              decoration: BoxDecoration(
                                border: Border.all(color: colors.borderLight, width: 2),
                                borderRadius: BorderRadius.circular(8),
                              ),
                              child: Image.file(_selectedImage!, fit: BoxFit.cover),
                            ),

                            // Clear button
                            Positioned(
                              top: 8,
                              right: 8,
                              child: GestureDetector(
                                onTap: _clearImage,
                                child: Container(
                                  decoration: BoxDecoration(color: colors.error, shape: BoxShape.circle),
                                  padding: const EdgeInsets.all(8),
                                  child: Icon(Icons.close, color: colors.onError, size: 20),
                                ),
                              ),
                            ),

                            // Select another button
                            Positioned(
                              bottom: 8,
                              left: 8,
                              child: GestureDetector(
                                onTap: _pickImage,
                                child: Container(
                                  decoration: BoxDecoration(color: colors.primary, borderRadius: BorderRadius.circular(4)),
                                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                                  child: Row(
                                    mainAxisSize: MainAxisSize.min,
                                    children: [
                                      Icon(Icons.edit, color: colors.onError, size: 16),
                                      const SizedBox(width: 4),
                                      Text('Change', style: TextStyle(color: colors.onError, fontSize: 12)),
                                    ],
                                  ),
                                ),
                              ),
                            ),
                          ],
                        ),
                    ],
                  ),
                ),

                const SizedBox(height: 24),
              ],
            ),
          ),
          Positioned(
            left: 0,
            right: 0,
            bottom: 0,
            child: SafeArea(
              top: false,
              child: Container(
                decoration: BoxDecoration(
                  color: colors.surface,
                  boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: .08), blurRadius: 12, offset: const Offset(0, -2))],
                ),
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
                child: SizedBox(
                  width: double.infinity,
                  height: 48,
                  child: ElevatedButton(
                    onPressed: _selectedImage != null ? _continue : null,
                    style: ElevatedButton.styleFrom(backgroundColor: colors.primary, disabledBackgroundColor: colors.disabled),
                    child: Text(
                      'Continue',
                      style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: colors.onError),
                    ),
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

  String _rotationAngle(int rotation) {
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
        return 'Unknown';
    }
  }
}
