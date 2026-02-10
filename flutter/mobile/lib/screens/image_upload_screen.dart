import 'dart:io';
import 'package:flutter/material.dart';
import 'package:image_picker/image_picker.dart';
import 'package:photoframe/models/library_models.dart';
import 'package:photoframe/models/ws_messages.dart';
import 'package:photoframe/screens/image_crop_screen.dart';
import 'package:photoframe/utils/app_logger.dart';

/// Image upload screen with BoardConfig info display and image selection
/// Allows user to pick an image from gallery and preview before upload
class ImageUploadScreen extends StatefulWidget {
  final BoardConfig boardConfig;

  const ImageUploadScreen({super.key, required this.boardConfig});

  @override
  State<ImageUploadScreen> createState() => _ImageUploadScreenState();
}

class _ImageUploadScreenState extends State<ImageUploadScreen> {
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
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to pick image: $e'), backgroundColor: Colors.red));
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
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Please select an image first'), backgroundColor: Colors.orange));
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
    return Scaffold(
      appBar: AppBar(title: const Text('Image Upload'), elevation: 0, backgroundColor: Colors.white, foregroundColor: Colors.black),
      body: Stack(
        children: [
          SingleChildScrollView(
            padding: const EdgeInsets.only(bottom: 96),
            child: Column(
              children: [
                // BoardConfig info section
                Container(
                  color: Colors.grey[100],
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text('Device Information', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                      const SizedBox(height: 12),
                      _buildInfoRow('Device', widget.boardConfig.board),
                      _buildInfoRow('Display', '${widget.boardConfig.displayWidth}×${widget.boardConfig.displayHeight}'),
                      _buildInfoRow('Type', widget.boardConfig.displayType.toJsonValue()),
                      _buildInfoRow('Rotation', '${widget.boardConfig.displayRotation} (${_rotationAngle(widget.boardConfig.displayRotation)}°)'),
                      if (widget.boardConfig.batteryLevel != null)
                        _buildInfoRow(
                          'Battery',
                          '${widget.boardConfig.batteryLevel}%${widget.boardConfig.batteryVoltageMv != null ? ' (${widget.boardConfig.batteryVoltageMv}mV)' : ''}',
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
                              border: Border.all(color: Colors.grey[300]!, width: 2),
                              borderRadius: BorderRadius.circular(8),
                              color: Colors.grey[50],
                            ),
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                Icon(Icons.image_outlined, size: 64, color: Colors.grey[400]),
                                const SizedBox(height: 12),
                                Text('Tap to select an image', style: TextStyle(fontSize: 16, color: Colors.grey[600])),
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
                                border: Border.all(color: Colors.grey[300]!, width: 2),
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
                                  decoration: BoxDecoration(color: Colors.red, shape: BoxShape.circle),
                                  padding: const EdgeInsets.all(8),
                                  child: const Icon(Icons.close, color: Colors.white, size: 20),
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
                                  decoration: BoxDecoration(color: Colors.blue, borderRadius: BorderRadius.circular(4)),
                                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                                  child: const Row(
                                    mainAxisSize: MainAxisSize.min,
                                    children: [
                                      Icon(Icons.edit, color: Colors.white, size: 16),
                                      SizedBox(width: 4),
                                      Text('Change', style: TextStyle(color: Colors.white, fontSize: 12)),
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
                  color: Colors.white,
                  boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: .08), blurRadius: 12, offset: const Offset(0, -2))],
                ),
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
                child: SizedBox(
                  width: double.infinity,
                  height: 48,
                  child: ElevatedButton(
                    onPressed: _selectedImage != null ? _continue : null,
                    style: ElevatedButton.styleFrom(backgroundColor: Colors.blue, disabledBackgroundColor: Colors.grey[300]),
                    child: const Text(
                      'Continue',
                      style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.white),
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

  Widget _buildInfoRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: TextStyle(fontSize: 14, color: Colors.grey[600])),
          Text(
            value,
            style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500, color: Colors.black),
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
