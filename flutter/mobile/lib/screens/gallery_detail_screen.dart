import 'dart:async';
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import '../models/processing_models.dart';
import '../services/bin_parser.dart';
import '../services/gallery_service.dart';
import '../utils/app_logger.dart';
import 'websocket_upload_screen.dart';

final _logger = getLogger('GalleryDetailScreen');

class GalleryDetailScreen extends StatefulWidget {
  final GeneratedImage image;

  const GalleryDetailScreen({super.key, required this.image});

  @override
  State<GalleryDetailScreen> createState() => _GalleryDetailScreenState();
}

class _GalleryDetailScreenState extends State<GalleryDetailScreen> {
  late Future<(BinHeader?, Uint8List?)?> _imageDataFuture;
  late Future<ui.Image?> _previewImageFuture;

  @override
  void initState() {
    super.initState();
    _imageDataFuture = _loadImageData();
    _previewImageFuture = _imageDataFuture.then((data) async {
      if (data == null || data.$1 == null || data.$2 == null) {
        return null;
      }
      final header = data.$1!;
      final rgba = _decodeRgba(header, data.$2!);
      return _rgbaToImage(rgba, header.width, header.height);
    });
  }

  Future<(BinHeader?, Uint8List?)?> _loadImageData() async {
    try {
      final bytes = await widget.image.file.readAsBytes();
      final parsed = BinParser.parse(bytes);
      return (parsed.header, bytes);
    } catch (e) {
      _logger.warning('Failed to load image data: $e');
      return null;
    }
  }

  void _startWebSocketUpload(BuildContext context, BinHeader header) async {
    // Create a minimal ProcessingJob from the header
    final job = ProcessingJob(
      targetResolution: Resolution(header.width.toDouble(), header.height.toDouble()),
      rotation: header.rotation,
      displayType: header.colorMode == 0 ? DisplayType.blackAndWhite : DisplayType.sixColors,
    );

    // Use the .pfr1 file from gallery
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (context) => WebSocketUploadScreen(pfr1File: widget.image.file, job: job),
      ),
    );
  }

  Future<void> _deleteImage(BuildContext context) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Image'),
        content: Text('Are you sure you want to delete "${widget.image.displayName}"? This action cannot be undone.'),
        actions: [
          TextButton(onPressed: () => Navigator.of(context).pop(false), child: const Text('Cancel')),
          TextButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Delete', style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );

    if (confirmed == true) {
      try {
        await GalleryService.deleteImage(widget.image);
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Image deleted successfully')));
          Navigator.of(context).pop();
        }
      } catch (e) {
        _logger.severe('Failed to delete image', e);
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to delete image: $e')));
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Image Details'),
        leading: IconButton(icon: const Icon(Icons.arrow_back), onPressed: () => Navigator.of(context).pop()),
        actions: [IconButton(icon: const Icon(Icons.delete_outline), onPressed: () => _deleteImage(context), tooltip: 'Delete image')],
      ),
      body: FutureBuilder<(BinHeader?, Uint8List?)?>(
        future: _imageDataFuture,
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          }

          final data = snapshot.data;
          if (data == null || data.$1 == null) {
            return Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.error_outline, size: 64, color: Colors.grey),
                  const SizedBox(height: 16),
                  const Text('Unable to load image', style: TextStyle(fontSize: 16, color: Colors.grey)),
                  const SizedBox(height: 24),
                  FilledButton.icon(onPressed: () => Navigator.of(context).pop(), icon: const Icon(Icons.arrow_back), label: const Text('Go Back')),
                ],
              ),
            );
          }

          final header = data.$1!;
          final imageBytes = data.$2!;

          return Column(
            children: [
              // Preview section
              Expanded(
                child: SingleChildScrollView(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    children: [
                      // Image preview
                      FutureBuilder<ui.Image?>(
                        future: _previewImageFuture,
                        builder: (context, imgSnapshot) {
                          if (imgSnapshot.hasData && imgSnapshot.data != null) {
                            return ClipRRect(
                              borderRadius: BorderRadius.circular(12),
                              child: RotatedBox(
                                quarterTurns: (4 - (header.rotation % 4)) % 4,
                                child: RawImage(image: imgSnapshot.data, fit: BoxFit.contain),
                              ),
                            );
                          }
                          if (imgSnapshot.connectionState == ConnectionState.done) {
                            return const SizedBox(height: 200, child: Center(child: Text('Unable to load preview')));
                          }
                          return const SizedBox(height: 200, child: Center(child: CircularProgressIndicator()));
                        },
                      ),
                      const SizedBox(height: 32),

                      // Image info card
                      Card(
                        child: Padding(
                          padding: const EdgeInsets.all(16),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Row(
                                children: [
                                  const Icon(Icons.description_outlined),
                                  const SizedBox(width: 8),
                                  Text('Image Information', style: Theme.of(context).textTheme.titleMedium),
                                ],
                              ),
                              const SizedBox(height: 16),
                              _infoRow('Filename', widget.image.displayName),
                              _infoRow('Dimensions', '${header.width} × ${header.height}'),
                              _infoRow('Color Mode', header.colorMode == 0 ? 'Black & White' : '6 Colors'),
                              _infoRow('Rotation', '${header.rotation}'),
                              _infoRow('File Size', '${(imageBytes.length / 1024).toStringAsFixed(2)} KB'),
                              _infoRow('Created', _formatDate(widget.image.createdAt)),
                            ],
                          ),
                        ),
                      ),
                      const SizedBox(height: 16),

                      // File info
                      Card(
                        color: Colors.grey[100],
                        child: Padding(
                          padding: const EdgeInsets.all(16),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text('File Details', style: Theme.of(context).textTheme.titleSmall),
                              const SizedBox(height: 8),
                              Text('Path: ${widget.image.file.path}', style: const TextStyle(fontSize: 12, color: Colors.grey)),
                            ],
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),

              // Upload button
              Padding(
                padding: const EdgeInsets.all(16),
                child: SizedBox(
                  width: double.infinity,
                  child: FilledButton.icon(
                    onPressed: () => _startWebSocketUpload(context, header),
                    icon: const Icon(Icons.cloud_upload),
                    label: const Text('Upload via WebSocket'),
                    style: FilledButton.styleFrom(padding: const EdgeInsets.symmetric(vertical: 16)),
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }

  Widget _infoRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontSize: 14, color: Colors.grey)),
          Text(value, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500)),
        ],
      ),
    );
  }

  String _formatDate(DateTime dt) {
    return '${dt.year}-${dt.month.toString().padLeft(2, '0')}-${dt.day.toString().padLeft(2, '0')} '
        '${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
  }

  Uint8List _decodeRgba(BinHeader header, Uint8List data) {
    final parsed = BinParser.parse(data);
    return BinParser.decodeToRgba(parsed);
  }

  Future<ui.Image?> _rgbaToImage(Uint8List rgba, int width, int height) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(rgba, width, height, ui.PixelFormat.rgba8888, (img) => completer.complete(img));
    return completer.future;
  }
}
