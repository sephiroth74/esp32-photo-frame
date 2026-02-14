import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart' hide Orientation;
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';
import 'package:photoframe_common/models/bin_model.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:share_plus/share_plus.dart';

import '../l10n/app_localizations.dart';
import '../models/binary_model.dart';
import '../services/bin_parser.dart';
import '../services/ws_connection_service.dart';

class UploadScreen extends StatefulWidget {
  final File pfrFile;
  final Orientation currentOrientation;

  const UploadScreen({super.key, required this.pfrFile, this.currentOrientation = Orientation.landscape});

  @override
  State<UploadScreen> createState() => _UploadScreenState();
}

class _UploadScreenState extends State<UploadScreen> with TickerProviderStateMixin {
  static final logger = getLogger('UploadScreen');
  late final Future<Pfr1ViewData?> _pfrPreview;
  bool _isUploading = false;
  bool _isDisplayReady = true;
  bool _initialOrientationLoaded = false;
  late Orientation _orientation;
  late AnimationController _rotationController;
  late Animation<double> _rotationAnimation;
  bool _isConnected = false;
  double _uploadProgress = 0.0;
  StreamSubscription? _uploadProgressSubscription;
  StreamSubscription? _displayReadySubscription;

  @override
  void initState() {
    super.initState();
    _pfrPreview = _loadPfrPreview();
    _orientation = widget.currentOrientation;
    logger.info('Initial orientation: $_orientation');

    // Initialize rotation animation controller
    _rotationController = AnimationController(duration: const Duration(milliseconds: 300), vsync: this);
    _rotationAnimation = Tween<double>(
      begin: 0.0,
      end: -(math.pi / 2),
    ).animate(CurvedAnimation(parent: _rotationController, curve: Curves.easeInOut));

    // Check initial connection state
    _isConnected = WsConnectionService().isConnected;

    // Listen to connection state changes and show snackbar on disconnection
    WsConnectionService().connectionStateStream.listen((isConnected) {
      if (mounted) {
        setState(() {
          _isConnected = isConnected;
        });

        if (!isConnected && _isConnected != isConnected) {
          logger.warning('Connection lost');
          final l10n = AppLocalizations.of(context)!;
          final colors = ThemeColors(context);
          ScaffoldMessenger.of(
            context,
          ).showSnackBar(SnackBar(content: Text(l10n.connectionLostMessage), backgroundColor: colors.error, duration: const Duration(seconds: 4)));
        }
      }
    });
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
    _uploadProgressSubscription?.cancel();
    // Clean up temporary files when leaving this screen
    try {
      widget.pfrFile.deleteSync();
    } catch (e) {
      logger.warning('Failed to delete PFR1 file: $e');
    }
    super.dispose();
  }

  Future<void> _uploadToDevice() async {
    if (!WsConnectionService().isConnected) {
      logger.warning('Not connected to device');
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l10n.deviceNotConnectedMessage), backgroundColor: colors.error));
      }
      return;
    }

    setState(() {
      _isDisplayReady = true;
      _isUploading = true;
      _uploadProgress = 0.0;
    });

    // Subscribe to upload progress stream
    _uploadProgressSubscription = WsConnectionService().uploadProgress.listen((progress) {
      if (mounted) {
        setState(() {
          _uploadProgress = progress;
        });
      }
    });

    _displayReadySubscription = WsConnectionService().isDisplayReadyStream.listen((readyInfo) {
      logger.config('Received display_ready message: $readyInfo');
      if (mounted) {
        setState(() {
          _isDisplayReady = readyInfo;
        });
      }
    });

    try {
      logger.info('Uploading PFR1 file to device: ${widget.pfrFile.path}');

      // Upload file with current orientation
      await WsConnectionService().uploadImage(pfrFilePath: widget.pfrFile.path, orientation: _orientation.value);

      logger.info('File uploaded successfully');
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        final colors = ThemeColors(context);
        final boardConfig = WsConnectionService().boardConfig;
        showDialog(
          context: context,
          builder: (dialogContext) => AlertDialog(
            icon: Icon(Icons.check_circle, color: colors.success, size: 48),
            title: Text(l10n.uploadSuccessTitle),
            content: Text(l10n.uploadSuccessMessage),
            actions: [
              FilledButton(
                style: FilledButton.styleFrom(backgroundColor: colors.primary),
                onPressed: () {
                  WsConnectionService().sendShutdown();
                  WsConnectionService().disconnect();
                  Navigator.of(dialogContext).pop();
                  Navigator.of(context).popUntil((route) => route.isFirst);
                },
                child: Text(l10n.okAction),
              ),
              OutlinedButton(
                onPressed: () {
                  Navigator.of(dialogContext).pop();
                  if (boardConfig != null) {
                    Navigator.of(
                      context,
                    ).popUntil((route) => route.settings.name == '/image_select'); // Pop back to home before navigating to image select
                  } else {
                    logger.warning('Board config is null, cannot navigate to image select screen');
                    Navigator.of(context).popUntil((route) => route.isFirst);
                  }
                },
                child: Text(l10n.newImageAction),
              ),
            ],
          ),
        );
      }
    } catch (e) {
      logger.severe('Failed to upload file: $e');
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        final colors = ThemeColors(context);
        showDialog(
          context: context,
          builder: (context) => AlertDialog(
            icon: Icon(Icons.error_outline, color: colors.error, size: 48),
            title: Text(l10n.uploadFailedTitle),
            content: Text(l10n.uploadFailedMessage(e.toString())),
            actions: [TextButton(onPressed: () => Navigator.of(context).pop(), child: Text(l10n.okAction))],
          ),
        );
      }
    } finally {
      await _uploadProgressSubscription?.cancel();
      await _displayReadySubscription?.cancel();

      _uploadProgressSubscription = null;
      _displayReadySubscription = null;

      if (mounted) {
        setState(() {
          _isUploading = false;
          _uploadProgress = 0.0;
          _isDisplayReady = true;
        });
      }
    }
  }

  Future<void> _shareFile() async {
    try {
      logger.fine('Sharing PFR1 file: ${widget.pfrFile.path}');

      final l10n = AppLocalizations.of(context)!;
      await Share.shareXFiles([XFile(widget.pfrFile.path)], text: l10n.photoFrameBinaryShareText);

      logger.info('File shared successfully');
    } catch (e) {
      logger.severe('Failed to share file: $e');
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l10n.shareFailedMessage(e.toString())), backgroundColor: colors.error));
      }
    }
  }

  Future<void> _rotateImage() async {
    final currentDegrees = _orientation.toDegrees();
    final currentAngle = _orientation.toRadians();
    logger.info('Current orientation before rotation: $_orientation, current degrees: $currentDegrees°, current angle: $currentAngle rad');
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

  Future<void> _retryConnection() async {
    logger.info('Retrying WebSocket connection...');

    try {
      final boardConfig = WsConnectionService().boardConfig;
      if (boardConfig == null) {
        logger.severe('Cannot retry connection: board config is null');
        throw Exception('Board configuration is not available. Please go back to the home screen and reconnect to the device.');
      }

      try {
        WsConnectionService().disconnect();
        await WsConnectionService().connect(host: boardConfig.ipAddress, port: boardConfig.ipPort);
        logger.info('Reconnection attempt finished');
      } catch (e) {
        throw Exception('Failed to reconnect: $e');
      }
    } catch (e) {
      logger.severe('Reconnection failed: $e');
      if (mounted) {
        // show an alert dialog
        final l10n = AppLocalizations.of(context)!;
        showDialog(
          context: context,
          builder: (context) => AlertDialog(
            title: Text(l10n.reconnectionFailedTitle),
            content: Text(l10n.reconnectionFailedMessage(e.toString())),
            actions: [TextButton(onPressed: () => Navigator.of(context).pop(), child: Text(l10n.okAction))],
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    final l10n = AppLocalizations.of(context)!;
    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.uploadTitle),
        backgroundColor: colors.appBarBackground,
        foregroundColor: colors.appBarForeground,
        elevation: 0,
        actions: [IconButton(icon: const Icon(Icons.share), tooltip: l10n.shareTooltip, onPressed: _shareFile)],
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
                        return SizedBox(height: 600, child: Center(child: Text(l10n.unableToRenderPreview)));
                      }

                      // Load orientation from PFR1 header only once
                      if (!_initialOrientationLoaded) {
                        _orientation = data.header.orientation;
                        _initialOrientationLoaded = true;

                        _rotationAnimation = Tween<double>(
                          begin: -_orientation.toRadians(),
                          end: -_orientation.toRadians(),
                        ).animate(CurvedAnimation(parent: _rotationController, curve: Curves.easeInOut));

                        logger.info('PFR1 header: ${data.header}');
                        logger.info('PFR1 image size: ${data.image.width}x${data.image.height}');
                        logger.info('Loaded initial orientation from PFR1 header: $_orientation');
                        logger.info('Initial rotation angle: ${-_orientation.toRadians()} rad');
                      }

                      final aspectRatio = data.header.getWidth() / data.header.getHeight();

                      return Column(
                        mainAxisSize: MainAxisSize.min,
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          // Preview image (decoded from .pfr1)
                          ClipRRect(
                            child: SizedBox(
                              height: data.header.height.toDouble(),
                              width: double.infinity,
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
                                      height: data.header.getHeight().toDouble(),
                                      child: RawImage(image: data.image),
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ),
                          const SizedBox(height: 8),
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
                                  l10n.dimensionsLabel(data.header.getWidth(), data.header.getHeight()),
                                  style: TextStyle(fontSize: 14, color: colors.textSecondary),
                                ),
                                const SizedBox(height: 2),
                                Text(l10n.orientationLabel(_orientation.toDegrees()), style: TextStyle(fontSize: 14, color: colors.textSecondary)),
                              ],
                            ),
                          ),
                          // show a box when the connection is lost
                          if (!_isConnected) ...[
                            Container(
                              width: double.infinity,
                              margin: const EdgeInsets.only(top: 16),
                              padding: const EdgeInsets.all(16),
                              decoration: BoxDecoration(
                                border: Border.all(color: colors.error, width: 2),
                                borderRadius: BorderRadius.circular(8),
                              ),
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(l10n.deviceNotConnectedMessage, style: TextStyle(fontSize: 14, color: colors.textSecondary)),
                                  const SizedBox(height: 16),
                                  FilledButton.icon(
                                    onPressed: _retryConnection,
                                    label: Text(l10n.retryAction),
                                    icon: const Icon(Icons.refresh),
                                    style: FilledButton.styleFrom(backgroundColor: colors.error),
                                  ),
                                ],
                              ),
                            ),
                          ],
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
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          // Upload progress indicator
                          if (_isUploading)
                            Column(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                LinearProgressIndicator(
                                  value: _isDisplayReady == false ? null : _uploadProgress,
                                  backgroundColor: colors.disabled,
                                  valueColor: AlwaysStoppedAnimation<Color>(colors.primary),
                                ),
                                const SizedBox(height: 8),
                                Text(
                                  l10n.uploadingProgress((_uploadProgress * 100).toStringAsFixed(0)),
                                  style: TextStyle(fontSize: 12, color: colors.textSecondary),
                                ),
                                const SizedBox(height: 8),
                              ],
                            ),
                          // Buttons row
                          Row(
                            mainAxisSize: MainAxisSize.max,
                            mainAxisAlignment: MainAxisAlignment.spaceBetween,
                            children: [
                              Expanded(
                                child: OutlinedButton.icon(
                                  onPressed: (isLoading || _isUploading) ? null : _rotateImage,
                                  icon: Icon(Icons.rotate_right),
                                  label: Text(l10n.rotateAction, style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                                ),
                              ),
                              const SizedBox(width: 8),
                              Expanded(
                                child: FilledButton(
                                  onPressed: (isLoading || _isUploading) ? null : _uploadToDevice,
                                  child: _isUploading
                                      ? const SizedBox(height: 20, width: 20, child: CircularProgressIndicator(strokeWidth: 2))
                                      : Text(l10n.uploadAction, style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                                ),
                              ),
                            ],
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
