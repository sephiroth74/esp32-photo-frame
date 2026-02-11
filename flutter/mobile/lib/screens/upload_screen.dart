import 'dart:io';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart' hide Orientation;
import 'package:photoframe_common/models/bin_model.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:share_plus/share_plus.dart';

import '../models/binary_model.dart';
import '../services/bin_parser.dart';

class UploadScreen extends StatefulWidget {
  final File pfrFile;
  final Orientation currentOrientation;

  const UploadScreen({super.key, required this.pfrFile, this.currentOrientation = Orientation.landscape});

  @override
  State<UploadScreen> createState() => _UploadScreenState();
}

class _UploadScreenState extends State<UploadScreen> {
  late final Future<Pfr1ViewData?> _pfrPreview;

  @override
  void initState() {
    super.initState();
    _pfrPreview = _loadPfrPreview();
  }

  Future<Pfr1ViewData?> _loadPfrPreview() async {
    try {
      final bytes = await widget.pfrFile.readAsBytes();
      final parsed = BinParser.parse(bytes);
      final image = await parsed.decodeToImage(bytes);
      if (image == null) {
        return null;
      }
      return Pfr1ViewData(parsed.header, image);
    } catch (e) {
      logger.severe('Failed to decode PFR1 preview: $e');
      return null;
    }
  }

  @override
  void dispose() {
    // Clean up temporary files when leaving this screen
    widget.pfrFile.delete().catchError((e) {
      logger.warning('Failed to delete PFR1 file: $e');
    });
    super.dispose();
  }

  Future<void> _shareFile() async {
    try {
      logger.fine('Sharing PFR1 file: ${widget.pfrFile.path}');

      await Share.shareXFiles([XFile(widget.pfrFile.path)], text: 'Photo Frame Binary');

      logger.info('File shared successfully');
    } catch (e) {
      logger.severe('Failed to share file: $e');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to share file: $e'), backgroundColor: Colors.red));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Preview'),
        centerTitle: true,
        backgroundColor: Colors.white,
        foregroundColor: Colors.black,
        elevation: 0,
        actions: [IconButton(icon: const Icon(Icons.share), tooltip: 'Share PFR1 file', onPressed: _shareFile)],
      ),
      body: Center(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Preview image (decoded from .pfr1)
              Container(
                decoration: BoxDecoration(
                  border: Border.all(color: Colors.grey.shade300),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(12),
                  child: FutureBuilder<Pfr1ViewData?>(
                    future: _pfrPreview,
                    builder: (context, snapshot) {
                      if (snapshot.connectionState == ConnectionState.waiting) {
                        return const SizedBox(height: 240, child: Center(child: CircularProgressIndicator()));
                      }

                      final data = snapshot.data;
                      if (data == null) {
                        return const SizedBox(height: 240, child: Center(child: Text('Unable to render .pfr1 preview')));
                      }

                      final angle = switch (data.header.orientation) {
                        Orientation.portrait => math.pi / 2,
                        Orientation.landscapeReverse => math.pi,
                        Orientation.portraitReverse => math.pi * 1.5,
                        Orientation.landscape => 0.0,
                      };

                      final aspectRatio = data.header.getWidth() / data.header.getHeight();

                      return AspectRatio(
                        aspectRatio: aspectRatio,
                        child: Transform.rotate(
                          angle: angle,
                          child: FittedBox(
                            fit: BoxFit.contain,
                            child: SizedBox(
                              width: data.header.getWidth().toDouble(),
                              height: data.header.getHeight().toDouble(),
                              child: RawImage(image: data.image),
                            ),
                          ),
                        ),
                      );
                    },
                  ),
                ),
              ),
              const SizedBox(height: 32),
              // File info
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(color: Colors.grey.shade100, borderRadius: BorderRadius.circular(12)),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('File Information', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    const SizedBox(height: 12),
                    _InfoRow(label: 'Filename', value: widget.pfrFile.path.split('/').last),
                    const SizedBox(height: 8),
                    _InfoRow(label: 'File size', value: '${(widget.pfrFile.lengthSync() / 1024).toStringAsFixed(2)} KB'),
                    const SizedBox(height: 8),
                    _InfoRow(label: 'Status', value: 'Ready to upload'),
                  ],
                ),
              ),
              const SizedBox(height: 32),
              // Temporary share button (will be replaced with upload)
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  onPressed: _shareFile,
                  icon: const Icon(Icons.share),
                  label: const Text('Share File'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.blue,
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(vertical: 16),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  final String label;
  final String value;

  const _InfoRow({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(label, style: TextStyle(color: Colors.grey.shade700, fontSize: 14)),
        Text(value, style: const TextStyle(fontWeight: FontWeight.w500, fontSize: 14)),
      ],
    );
  }
}
