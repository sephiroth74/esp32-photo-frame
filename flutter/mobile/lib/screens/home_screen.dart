import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'dart:ui' as ui;
import 'package:image_picker/image_picker.dart';
import 'package:provider/provider.dart';

import '../services/gallery_service.dart';
import '../services/bin_parser.dart';
import '../state/image_processing_state.dart';
import 'processing_wizard.dart';
import 'gallery_detail_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late Future<List<GeneratedImage>> _galleryFuture;

  @override
  void initState() {
    super.initState();
    _galleryFuture = GalleryService.getGalleryImages();
  }

  void _refreshGallery() {
    setState(() {
      _galleryFuture = GalleryService.getGalleryImages();
    });
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
        title: const Text('PhotoFrame Gallery'),
        actions: [IconButton(icon: const Icon(Icons.refresh), onPressed: _refreshGallery, tooltip: 'Refresh gallery')],
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
              return _GalleryGridItem(image: image, onImageDeleted: _refreshGallery);
            },
          );
        },
      ),
      floatingActionButton: FloatingActionButton.extended(
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

  const _GalleryGridItem({required this.image, required this.onImageDeleted});

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
      final bytes = await widget.image.file.readAsBytes();
      final decodedImage = await BinParser.decodeToImage(bytes);
      if (decodedImage != null) {
        return Image(image: _UiImageProvider(decodedImage), fit: BoxFit.cover, width: double.infinity, height: double.infinity);
      }
    } catch (e) {
      // Fallback to placeholder if decoding fails
    }
    return null;
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: () {
        Navigator.of(
          context,
        ).push(MaterialPageRoute(builder: (context) => GalleryDetailScreen(image: widget.image))).then((_) => widget.onImageDeleted());
      },
      onLongPress: () {
        // TODO: Implement multi-select mode for batch deletion
      },
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
