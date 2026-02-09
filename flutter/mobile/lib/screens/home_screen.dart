import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'dart:ui' as ui;
import 'package:image_picker/image_picker.dart';
import 'package:provider/provider.dart';

import '../services/gallery_service.dart';
import '../services/bin_parser.dart';
import '../services/thumbnail_cache_service.dart';
import '../state/image_processing_state.dart';
import '../models/processing_models.dart';
import '../main.dart' as main_app;
import 'processing_wizard.dart';
import 'gallery_detail_screen.dart';
import 'websocket_upload_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late Future<List<GeneratedImage>> _galleryFuture;
  bool _selectionMode = false;
  final Set<String> _selectedFilenames = {};
  StreamSubscription? _deepLinkSubscription;

  @override
  void initState() {
    super.initState();
    _galleryFuture = GalleryService.getGalleryImages();

    // Listen to deep link events
    _deepLinkSubscription = main_app.deepLinkHandler.qrCodeDataStream.listen((deviceInfo) {
      _handleDeepLink(deviceInfo);
    });
  }

  @override
  void dispose() {
    _deepLinkSubscription?.cancel();
    super.dispose();
  }

  Future<void> _handleDeepLink(dynamic deviceInfo) async {
    if (!mounted) return;

    // Show dialog to select which image to upload
    final images = await GalleryService.getGalleryImages();

    if (!mounted) return;

    if (images.isEmpty) {
      // No images available, prompt user to create one
      showDialog(
        context: context,
        builder: (context) => AlertDialog(
          title: const Text('No Images Available'),
          content: const Text(
            'You need to process an image before you can upload it to your device. '
            'Would you like to select an image to process now?',
          ),
          actions: [
            TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
            FilledButton(
              onPressed: () {
                Navigator.of(context).pop();
                _pickImageAndStartWizard(context);
              },
              child: const Text('Select Image'),
            ),
          ],
        ),
      );
      return;
    }

    // Show dialog to select image
    final selectedImage = await showDialog<GeneratedImage>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Select Image to Upload'),
        content: SizedBox(
          width: double.maxFinite,
          child: ListView.builder(
            shrinkWrap: true,
            itemCount: images.length,
            itemBuilder: (context, index) {
              final image = images[index];
              return ListTile(
                leading: const Icon(Icons.image),
                title: Text(image.displayName),
                subtitle: Text('${(image.file.lengthSync() / 1024).toStringAsFixed(1)} KB'),
                onTap: () => Navigator.of(context).pop(image),
              );
            },
          ),
        ),
        actions: [TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel'))],
      ),
    );

    if (selectedImage != null && mounted) {
      // Read processing job from the image metadata
      final bytes = await selectedImage.file.readAsBytes();
      final header = BinParser.parseHeader(bytes);

      if (header != null && mounted) {
        // Create a minimal ProcessingJob from header
        final job = ProcessingJob(targetResolution: Size(header.width.toDouble(), header.height.toDouble()), rotation: header.rotation == 1 ? 90 : 0);

        // Navigate to WebSocket upload screen
        Navigator.of(context).push(
          MaterialPageRoute(
            builder: (_) => WebSocketUploadScreen(
              pfr1File: selectedImage.file,
              job: job,
              deviceIp: deviceInfo.ip,
              deviceSsid: deviceInfo.ssid,
              devicePort: deviceInfo.port,
            ),
          ),
        );
      }
    }
  }

  void _refreshGallery() {
    setState(() {
      _galleryFuture = GalleryService.getGalleryImages();
    });
  }

  void _enterSelection(GeneratedImage image) {
    setState(() {
      _selectionMode = true;
      _selectedFilenames.add(image.filename);
    });
  }

  void _toggleSelection(GeneratedImage image) {
    setState(() {
      if (_selectedFilenames.contains(image.filename)) {
        _selectedFilenames.remove(image.filename);
      } else {
        _selectedFilenames.add(image.filename);
      }

      if (_selectedFilenames.isEmpty) {
        _selectionMode = false;
      }
    });
  }

  void _exitSelectionMode() {
    setState(() {
      _selectionMode = false;
      _selectedFilenames.clear();
    });
  }

  Future<void> _confirmDeleteSelected(BuildContext context) async {
    if (_selectedFilenames.isEmpty) return;

    final count = _selectedFilenames.length;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete selected images?'),
        content: Text('This will permanently delete $count item${count == 1 ? '' : 's'}.'),
        actions: [
          TextButton(onPressed: () => Navigator.of(context).pop(false), child: const Text('Cancel')),
          FilledButton.icon(onPressed: () => Navigator.of(context).pop(true), icon: const Icon(Icons.delete), label: const Text('Delete')),
        ],
      ),
    );

    if (confirmed != true) return;

    final images = await GalleryService.getGalleryImages();
    final toDelete = images.where((image) => _selectedFilenames.contains(image.filename)).toList();
    for (final image in toDelete) {
      await GalleryService.deleteImage(image);
    }

    if (!mounted) return;
    _exitSelectionMode();
    _refreshGallery();
  }

  Future<void> _pickImageAndStartWizard(BuildContext context) async {
    final state = context.read<ImageProcessingState>();
    await state.pickImage(ImageSource.gallery);

    if (!context.mounted) return;

    if (state.imageFile != null) {
      Navigator.of(context).push(MaterialPageRoute(builder: (_) => ProcessingWizardScreen(imageFile: state.imageFile!))).then((_) {
        _refreshGallery();
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(_selectionMode ? 'Multiple selection' : 'PhotoFrame Gallery'),
        leading: _selectionMode ? IconButton(icon: const Icon(Icons.close), tooltip: 'Exit selection', onPressed: _exitSelectionMode) : null,
        actions: _selectionMode
            ? [
                IconButton(
                  icon: const Icon(Icons.delete),
                  tooltip: 'Delete selected',
                  onPressed: _selectedFilenames.isEmpty ? null : () => _confirmDeleteSelected(context),
                ),
              ]
            : [IconButton(icon: const Icon(Icons.refresh), onPressed: _refreshGallery, tooltip: 'Refresh gallery')],
      ),
      body: FutureBuilder<List<GeneratedImage>>(
        future: _galleryFuture,
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          }

          final images = snapshot.data ?? [];

          if (images.isEmpty) {
            return Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.image_not_supported_outlined, size: 72, color: Colors.grey),
                  const SizedBox(height: 16),
                  const Text('No generated images yet', style: TextStyle(fontSize: 16, color: Colors.grey)),
                  const SizedBox(height: 24),
                  FilledButton.icon(
                    onPressed: () => _pickImageAndStartWizard(context),
                    icon: const Icon(Icons.add),
                    label: const Text('Create First Image'),
                  ),
                ],
              ),
            );
          }

          return GridView.builder(
            padding: const EdgeInsets.all(8),
            gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
              crossAxisCount: 3,
              crossAxisSpacing: 8,
              mainAxisSpacing: 8,
              childAspectRatio: 1.0,
            ),
            itemCount: images.length,
            itemBuilder: (context, index) {
              final image = images[index];
              final isSelected = _selectedFilenames.contains(image.filename);
              return _GalleryGridItem(
                image: image,
                onImageDeleted: _refreshGallery,
                selectionMode: _selectionMode,
                selected: isSelected,
                onTap: () {
                  if (_selectionMode) {
                    _toggleSelection(image);
                  } else {
                    Navigator.of(
                      context,
                    ).push(MaterialPageRoute(builder: (context) => GalleryDetailScreen(image: image))).then((_) => _refreshGallery());
                  }
                },
                onLongPress: () {
                  if (_selectionMode) {
                    _toggleSelection(image);
                  } else {
                    _enterSelection(image);
                  }
                },
              );
            },
          );
        },
      ),
      floatingActionButton: _selectionMode
          ? null
          : FloatingActionButton.extended(
              onPressed: () => _pickImageAndStartWizard(context),
              icon: const Icon(Icons.add_photo_alternate),
              label: const Text('New Image'),
            ),
    );
  }
}

class _GalleryGridItem extends StatefulWidget {
  final GeneratedImage image;
  final VoidCallback onImageDeleted;
  final bool selectionMode;
  final bool selected;
  final VoidCallback onTap;
  final VoidCallback onLongPress;

  const _GalleryGridItem({
    required this.image,
    required this.onImageDeleted,
    required this.selectionMode,
    required this.selected,
    required this.onTap,
    required this.onLongPress,
  });

  @override
  State<_GalleryGridItem> createState() => _GalleryGridItemState();
}

class _GalleryGridItemState extends State<_GalleryGridItem> {
  late Future<Image?> _imageFuture;

  @override
  void initState() {
    super.initState();
    _imageFuture = _loadPreview();
  }

  Future<Image?> _loadPreview() async {
    try {
      // Try to load from cache first
      final cachedThumbnail = await ThumbnailCacheService.getCachedThumbnail(widget.image.file);
      if (cachedThumbnail != null) {
        final cachedBytes = await cachedThumbnail.readAsBytes();
        return Image.memory(cachedBytes, fit: BoxFit.cover, width: double.infinity, height: double.infinity);
      }

      // Load from .pfr1 file
      final bytes = await widget.image.file.readAsBytes();
      final header = BinParser.parseHeader(bytes);
      final decodedImage = await BinParser.decodeToImage(bytes);

      if (decodedImage != null && header != null) {
        // Apply rotation from header
        final rotatedImage = await _applyRotation(decodedImage, header.rotation);

        // Cache the thumbnail
        final rotatedBytes = await rotatedImage.toByteData(format: ui.ImageByteFormat.png);
        if (rotatedBytes != null) {
          await ThumbnailCacheService.saveThumbnail(widget.image.file, rotatedBytes.buffer.asUint8List());
        }

        return Image(image: _UiImageProvider(rotatedImage), fit: BoxFit.cover, width: double.infinity, height: double.infinity);
      }
    } catch (e) {
      // Fallback to placeholder if decoding fails
    }
    return null;
  }

  Future<ui.Image> _applyRotation(ui.Image image, int rotation) async {
    final quarterTurns = (4 - (rotation % 4)) % 4; // Counter-clockwise rotation

    if (quarterTurns == 0) {
      return image;
    }

    final recorder = ui.PictureRecorder();
    final canvas = ui.Canvas(recorder);

    // Calculate dimensions after rotation
    final width = quarterTurns % 2 == 0 ? image.width : image.height;
    final height = quarterTurns % 2 == 0 ? image.height : image.width;

    canvas.translate(width / 2, height / 2);
    canvas.rotate(quarterTurns * 1.5708); // π/2 radians per quarter turn
    canvas.translate(-image.width / 2, -image.height / 2);
    canvas.drawImage(image, ui.Offset.zero, ui.Paint());

    final picture = recorder.endRecording();
    return picture.toImage(width.toInt(), height.toInt());
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: widget.onTap,
      onLongPress: widget.onLongPress,
      child: ClipRRect(
        borderRadius: BorderRadius.circular(8),
        child: Container(
          color: Colors.grey[100],
          child: Stack(
            children: [
              FutureBuilder<Image?>(
                future: _imageFuture,
                builder: (context, snapshot) {
                  if (snapshot.connectionState == ConnectionState.waiting) {
                    return Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          const SizedBox(width: 40, height: 40, child: CircularProgressIndicator(strokeWidth: 2)),
                          const SizedBox(height: 8),
                          Text(
                            widget.image.displayName,
                            maxLines: 2,
                            overflow: TextOverflow.ellipsis,
                            textAlign: TextAlign.center,
                            style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w500),
                          ),
                        ],
                      ),
                    );
                  }

                  if (snapshot.hasData && snapshot.data != null) {
                    return SizedBox.expand(child: snapshot.data!);
                  }

                  // Fallback placeholder
                  return Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        const Icon(Icons.image, size: 48, color: Colors.grey),
                        const SizedBox(height: 8),
                        Padding(
                          padding: const EdgeInsets.symmetric(horizontal: 8.0),
                          child: Text(
                            widget.image.displayName,
                            maxLines: 2,
                            overflow: TextOverflow.ellipsis,
                            textAlign: TextAlign.center,
                            style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w500),
                          ),
                        ),
                        const SizedBox(height: 4),
                        Text('.pfr1', style: TextStyle(fontSize: 10, color: Colors.grey[600])),
                      ],
                    ),
                  );
                },
              ),
              if (widget.selectionMode)
                Positioned.fill(
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 150),
                    decoration: BoxDecoration(
                      color: widget.selected ? Colors.black.withOpacity(0.35) : Colors.transparent,
                      border: Border.all(color: widget.selected ? Theme.of(context).colorScheme.primary : Colors.transparent, width: 2),
                    ),
                  ),
                ),
              if (widget.selectionMode)
                Positioned(
                  top: 6,
                  right: 6,
                  child: Container(
                    width: 24,
                    height: 24,
                    decoration: BoxDecoration(
                      color: widget.selected ? Theme.of(context).colorScheme.primary : Colors.white70,
                      shape: BoxShape.circle,
                      border: Border.all(color: Colors.white, width: 2),
                    ),
                    child: widget.selected ? const Icon(Icons.check, size: 16, color: Colors.white) : const SizedBox.shrink(),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}

class _UiImageProvider extends ImageProvider<_UiImageProvider> {
  final ui.Image image;

  _UiImageProvider(this.image);

  @override
  Future<_UiImageProvider> obtainKey(ImageConfiguration configuration) {
    return SynchronousFuture<_UiImageProvider>(this);
  }

  @override
  ImageStreamCompleter loadImage(_UiImageProvider key, ImageDecoderCallback decode) {
    return OneFrameImageStreamCompleter(_loadImage());
  }

  Future<ImageInfo> _loadImage() async {
    return ImageInfo(image: image);
  }
}
