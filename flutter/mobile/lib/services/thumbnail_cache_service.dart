import 'dart:io';
import 'dart:typed_data';
import 'package:path_provider/path_provider.dart';
import 'package:crypto/crypto.dart';
import '../utils/app_logger.dart';

final _logger = getLogger('ThumbnailCacheService');

/// Service for caching thumbnails locally to avoid regenerating them
class ThumbnailCacheService {
  static const _thumbnailDir = 'pfr1_thumbnails';

  /// Get cached thumbnail file path for a given .pfr1 file
  static Future<File?> getCachedThumbnail(File pfr1File) async {
    try {
      final cacheDir = await _getThumbnailDirectory();
      final hash = md5.convert(pfr1File.path.codeUnits).toString();
      final cachedFile = File('${cacheDir.path}/$hash.jpg');

      if (await cachedFile.exists()) {
        _logger.fine('Thumbnail cache hit for ${pfr1File.path}');
        return cachedFile;
      }
      return null;
    } catch (e) {
      _logger.warning('Failed to check thumbnail cache: $e');
      return null;
    }
  }

  /// Save thumbnail bytes to cache
  static Future<File?> saveThumbnail(File pfr1File, Uint8List bytes) async {
    try {
      final cacheDir = await _getThumbnailDirectory();
      final hash = md5.convert(pfr1File.path.codeUnits).toString();
      final cachedFile = File('${cacheDir.path}/$hash.jpg');

      await cachedFile.writeAsBytes(bytes, flush: true);
      _logger.fine('Thumbnail cached for ${pfr1File.path}');
      return cachedFile;
    } catch (e) {
      _logger.warning('Failed to save thumbnail: $e');
      return null;
    }
  }

  /// Clear all cached thumbnails
  static Future<bool> clearCache() async {
    try {
      final cacheDir = await _getThumbnailDirectory();
      if (await cacheDir.exists()) {
        await cacheDir.delete(recursive: true);
        _logger.info('Thumbnail cache cleared');
        return true;
      }
      return false;
    } catch (e) {
      _logger.warning('Failed to clear thumbnail cache: $e');
      return false;
    }
  }

  /// Get the thumbnail cache directory, creating it if necessary
  static Future<Directory> _getThumbnailDirectory() async {
    final appDir = await getApplicationDocumentsDirectory();
    final cacheDir = Directory('${appDir.path}/$_thumbnailDir');

    if (!await cacheDir.exists()) {
      await cacheDir.create(recursive: true);
      _logger.fine('Created thumbnail cache directory');
    }

    return cacheDir;
  }
}
