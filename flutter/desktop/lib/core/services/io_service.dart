import 'dart:io';

class IoService {
  static Future<bool> commandExists(String command) async {
    if (Platform.isLinux) {
      return await Process.run('which', [command]).then((result) => result.exitCode == 0);
    } else if (Platform.isMacOS) {
      return await Process.run('which', [command]).then((result) => result.exitCode == 0);
    } else if (Platform.isWindows) {
      return await Process.run('where', [command]).then((result) => result.exitCode == 0);
    } else {
      throw UnsupportedError('Unsupported platform');
    }
  }
}
