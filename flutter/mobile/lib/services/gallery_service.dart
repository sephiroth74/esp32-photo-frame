import 'dart:io';
import 'dart:typed_data';

import 'package:path_provider/path_provider.dart';

import '../utils/app_logger.dart';

final _logger = getLogger('GalleryService');

class GeneratedImage {
  final String filename;
  final String displayName;
  final DateTime createdAt;
  final File file;

  GeneratedImage({required this.filename, required this.displayName, required this.createdAt, required this.file});
}

/// Service for managing generated .pfr1 files in the application's local gallery
class GalleryService {
  /// Get the gallery directory, creating it if it doesn't exist
  static Future<Directory> _getGalleryDirectory() async {
    final appDir = await getApplicationDocumentsDirectory();
    final galleryDir = Directory('${appDir.path}/photoframe_gallery');

    if (!await galleryDir.exists()) {
      await galleryDir.create(recursive: true);
      _logger.info('Created gallery directory: ${galleryDir.path}');
    }

    return galleryDir;
  }

  /// Save a .pfr1 file to the gallery with the original image filename
  /// Returns the saved file or null if save failed
  static Future<File?> savePfr1File({required Uint8List binaryData, required String originalImageName}) async {
    try {
      final galleryDir = await _getGalleryDirectory();

      // Extract base filename without extension
      final baseName = originalImageName.contains('.') ? originalImageName.substring(0, originalImageName.lastIndexOf('.')) : originalImageName;

      // Create filename: original_name.pfr1
      final filename = '$baseName.pfr1';
      final filepath = '${galleryDir.path}/$filename';

      // Check if file already exists - if so, add timestamp to make it unique
      File targetFile = File(filepath);
      if (await targetFile.exists()) {
        final timestamp = DateTime.now().millisecondsSinceEpoch;
        final uniqueFilename = '${baseName}_$timestamp.pfr1';
        targetFile = File('${galleryDir.path}/$uniqueFilename');
        _logger.info('File already exists, using unique filename: $uniqueFilename');
      }

      // Write binary data to file
      await targetFile.writeAsBytes(binaryData, flush: true);

      _logger.info('Saved .pfr1 file: ${targetFile.path} (${binaryData.length} bytes)');
      return targetFile;
    } catch (e, stackTrace) {
      _logger.severe('Failed to save .pfr1 file', e, stackTrace);
      return null;
    }
  }

  /// Get all generated images in the gallery
  static Future<List<GeneratedImage>> getGalleryImages() async {
    try {
      final galleryDir = await _getGalleryDirectory();
      final files = galleryDir.listSync().whereType<File>().where((f) => f.path.endsWith('.pfr1')).toList();

      final images = <GeneratedImage>[];
      for (final file in files) {
        final stat = await file.stat();
        final filename = file.path.split('/').last;

        images.add(GeneratedImage(filename: filename, displayName: filename.replaceAll('.pfr1', ''), createdAt: stat.modified, file: file));
      }

      // Sort by creation date (newest first)
      images.sort((a, b) => b.createdAt.compareTo(a.createdAt));

      return images;
    } catch (e, stackTrace) {
      _logger.severe('Failed to get gallery images', e, stackTrace);
      return [];
    }
  }

  /// Delete a file from the gallery
  static Future<bool> deleteImage(GeneratedImage image) async {
    try {
      if (await image.file.exists()) {
        await image.file.delete();
        _logger.info('Deleted image: ${image.filename}');
        return true;
      }
      return false;
    } catch (e, stackTrace) {
      _logger.severe('Failed to delete image', e, stackTrace);
      return false;
    }
  }

  /// Get the count of images in the gallery
  static Future<int> getImageCount() async {
    try {
      final images = await getGalleryImages();
      return images.length;
    } catch (e) {
      _logger.warning('Failed to get image count: $e');
      return 0;
    }
  }
}
