import 'package:path/path.dart' as p;

class FilePickerHistory {
  static final Map<String, String> _lastDirs = {};

  static String? initialDir(String key) => _lastDirs[key];

  static void rememberDirectory(String key, String directoryPath) {
    _lastDirs[key] = directoryPath;
  }

  static void rememberFile(String key, String filePath) {
    _lastDirs[key] = p.dirname(filePath);
  }
}
