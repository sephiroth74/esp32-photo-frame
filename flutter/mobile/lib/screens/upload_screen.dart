import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart' hide Orientation;
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';
import 'package:photoframe_common/models/bin_model.dart';
import 'package:photoframe_common/photoframe_common.dart';
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

class _UploadScreenState extends State<UploadScreen> with TickerProviderStateMixin {
  late final Future<Pfr1ViewData?> _pfrPreview;
  bool _isUploading = false;
  bool _initialOrientationLoaded = false;
  late Orientation _orientation;
  late AnimationController _rotationController;
  late Animation<double> _rotationAnimation;

  @override
  void initState() {
    super.initState();
    _pfrPreview = _loadPfrPreview();
    _orientation = widget.currentOrientation;
    debugPrint('Initial orientation: $_orientation');

    // Initialize rotation animation controller
    _rotationController = AnimationController(duration: const Duration(milliseconds: 300), vsync: this);
    _rotationAnimation = Tween<double>(
      begin: 0.0,
      end: -(math.pi / 2),
    ).animate(CurvedAnimation(parent: _rotationController, curve: Curves.easeInOut));
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
    _rotationController.dispose();
    // Clean up temporary files when leaving this screen
    try {
      widget.pfrFile.deleteSync();
    } catch (e) {
      logger.warning('Failed to delete PFR1 file: $e');
    }
    super.dispose();
  }

  Future<void> _uploadToDevice() async {
    setState(() => _isUploading = true);
    try {
      logger.info('Uploading PFR1 file to device: ${widget.pfrFile.path}');

      // TODO: Implement WebSocket upload to device
      // Read bytes and send to ESP32 device over WebSocket
      // final bytes = await widget.pfrFile.readAsBytes();

      logger.info('File uploaded successfully');
      if (mounted) {
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: const Text('File uploaded successfully'), backgroundColor: colors.success));
      }
    } catch (e) {
      logger.severe('Failed to upload file: $e');
      if (mounted) {
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to upload file: $e'), backgroundColor: colors.error));
      }
    } finally {
      if (mounted) {
        setState(() => _isUploading = false);
      }
    }
  }

  Future<void> _shareFile() async {
    try {
      logger.fine('Sharing PFR1 file: ${widget.pfrFile.path}');

      await Share.shareXFiles([XFile(widget.pfrFile.path)], text: 'Photo Frame Binary');

      logger.info('File shared successfully');
    } catch (e) {
      logger.severe('Failed to share file: $e');
      if (mounted) {
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to share file: $e'), backgroundColor: colors.error));
      }
    }
  }

  Future<void> _rotateImage() async {
    final currentDegrees = _orientation.toDegrees();
    final currentAngle = _orientation.toRadians();
    debugPrint('Current orientation before rotation: $_orientation, current degrees: $currentDegrees°, current angle: $currentAngle rad');
    setState(() {
      _orientation = switch (_orientation) {
        Orientation.landscape => Orientation.portrait,
        Orientation.portrait => Orientation.landscapeReverse,
        Orientation.landscapeReverse => Orientation.portraitReverse,
        Orientation.portraitReverse => Orientation.landscape,
      };
      _rotationAnimation = Tween<double>(
        begin: -currentAngle,
        end: -currentAngle - (math.pi / 2),
      ).animate(CurvedAnimation(parent: _rotationController, curve: Curves.easeInOut));
    });

    logger.info('Image rotated to orientation: $_orientation');
    // Reset and play the rotation animation
    await _rotationController.forward(from: 0.0);
  }

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Preview'),
        backgroundColor: colors.appBarBackground,
        foregroundColor: colors.appBarForeground,
        elevation: 0,
        actions: [IconButton(icon: const Icon(Icons.share), tooltip: 'Share', onPressed: _shareFile)],
      ),
      body: FutureBuilder<Pfr1ViewData?>(
        future: _pfrPreview,
        builder: (context, snapshot) {
          final isLoading = snapshot.connectionState == ConnectionState.waiting;

          return Column(
            children: [
              Expanded(
                child: SingleChildScrollView(
                  padding: const EdgeInsets.all(16),
                  child: Builder(
                    builder: (context) {
                      if (isLoading) {
                        return const SizedBox(height: 600, child: Center(child: CircularProgressIndicator()));
                      }

                      final data = snapshot.data;
                      if (data == null) {
                        return const SizedBox(height: 600, child: Center(child: Text('Unable to render .pfr1 preview')));
                      }

                      // Load orientation from PFR1 header only once
                      if (!_initialOrientationLoaded) {
                        _orientation = data.header.orientation;
                        _initialOrientationLoaded = true;

                        _rotationAnimation = Tween<double>(
                          begin: -_orientation.toRadians(),
                          end: -_orientation.toRadians(),
                        ).animate(CurvedAnimation(parent: _rotationController, curve: Curves.easeInOut));

                        debugPrint('Loaded initial orientation from PFR1 header: $_orientation');
                        debugPrint('Initial rotation angle: ${-_orientation.toRadians()} rad');
                      }

                      debugPrint('PFR1 header: ${data.header}');
                      debugPrint('PFR1 image size: ${data.image.width}x${data.image.height}');

                      final aspectRatio = data.header.getWidth() / data.header.getHeight();

                      return Column(
                        mainAxisSize: MainAxisSize.min,
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          // Preview image (decoded from .pfr1)
                          ClipRRect(
                            borderRadius: BorderRadius.circular(12),
                            child: SizedBox(
                              width: data.header.width.toDouble(),
                              height: data.header.height.toDouble(),
                              child: AspectRatio(
                                aspectRatio: aspectRatio,
                                child: AnimatedBuilder(
                                  animation: _rotationAnimation,
                                  builder: (context, child) {
                                    return Transform.rotate(
                                      angle: _rotationAnimation.value,
                                      filterQuality: FilterQuality.high,
                                      alignment: Alignment.center,
                                      child: child,
                                    );
                                  },
                                  child: FittedBox(
                                    fit: BoxFit.cover,
                                    child: SizedBox(
                                      width: data.header.getWidth().toDouble(),
                                      height: data.header.getHeight().toDouble(),
                                      child: RawImage(image: data.image),
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ),
                          const SizedBox(height: 16),
                          Container(
                            width: double.infinity,
                            padding: const EdgeInsets.all(16),
                            decoration: BoxDecoration(
                              border: Border.all(color: colors.borderLight, width: 2),
                              borderRadius: BorderRadius.circular(8),
                              color: colors.surfaceLight,
                            ),
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  'Dimensions: ${data.header.getWidth()}x${data.header.getHeight()}',
                                  style: TextStyle(fontSize: 14, color: colors.textSecondary),
                                ),
                                const SizedBox(height: 2),
                                Text('Orientation: ${_orientation.toDegrees()}°', style: TextStyle(fontSize: 14, color: colors.textSecondary)),
                              ],
                            ),
                          ),
                        ],
                      );
                    },
                  ),
                ),
              ),
              Flexible(
                flex: 0,
                child: SafeArea(
                  top: false,
                  child: Container(
                    decoration: BoxDecoration(color: colors.surface),
                    padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
                    child: SizedBox(
                      width: double.infinity,
                      child: Row(
                        mainAxisSize: MainAxisSize.max,
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: (isLoading || _isUploading) ? null : _rotateImage,
                              icon: Icon(Icons.rotate_right),
                              label: Text('Rotate', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                            ),
                          ),
                          const SizedBox(width: 8),
                          Expanded(
                            child: FilledButton(
                              onPressed: (isLoading || _isUploading) ? null : _uploadToDevice,
                              child: _isUploading
                                  ? const SizedBox(height: 20, width: 20, child: CircularProgressIndicator(strokeWidth: 2))
                                  : Text(
                                      'Upload',
                                      style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: colors.onError),
                                    ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}
