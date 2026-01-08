import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';
import '../models/processing_config.dart';

class ProcessingProvider with ChangeNotifier {
  ProcessingConfig _config = const ProcessingConfig(inputPath: '', outputPath: '');

  bool _isProcessing = false;
  double _progress = 0.0;
  int _processedCount = 0;
  int _totalCount = 0;
  String _currentFile = '';
  String _resultsMessage = '';
  String _errorMessage = '';

  ProcessingConfig get config => _config;
  bool get isProcessing => _isProcessing;
  double get progress => _progress;
  int get processedCount => _processedCount;
  int get totalCount => _totalCount;
  String get currentFile => _currentFile;
  String get resultsMessage => _resultsMessage;
  String get errorMessage => _errorMessage;

  ProcessingProvider() {
    _loadConfig();
  }

  void updateConfig(ProcessingConfig newConfig) {
    _config = newConfig;
    notifyListeners();
    _saveConfig();
  }

  Future<void> _loadConfig() async {
    try {
      final directory = await getApplicationSupportDirectory();
      final file = File('${directory.path}/config.json');
      if (await file.exists()) {
        final json = jsonDecode(await file.readAsString());
        _config = ProcessingConfig.fromJson(json);
        notifyListeners();
      }
    } catch (e) {
      debugPrint('Failed to load config: $e');
    }
  }

  Future<void> _saveConfig() async {
    try {
      final directory = await getApplicationSupportDirectory();
      await directory.create(recursive: true);
      final file = File('${directory.path}/config.json');
      await file.writeAsString(jsonEncode(_config.toJson()));
    } catch (e) {
      debugPrint('Failed to save config: $e');
    }
  }

  Future<void> startProcessing() async {
    if (_config.inputPath.isEmpty || _config.outputPath.isEmpty) {
      _errorMessage = 'Please select input and output directories';
      notifyListeners();
      return;
    }

    // Validate at least one output format
    if (!_config.outputBmp && !_config.outputBin && !_config.outputJpg && !_config.outputPng) {
      _errorMessage = 'Please select at least one output format';
      notifyListeners();
      return;
    }

    _isProcessing = true;
    _progress = 0.0;
    _processedCount = 0;
    _totalCount = 0;
    _errorMessage = '';
    _resultsMessage = '';
    _currentFile = '';
    notifyListeners();

    try {
      // Build command arguments
      final args = _buildCommandArgs();

      // Find photoframe-processor binary
      final binary = await _findProcessorBinary();
      if (binary == null) {
        throw Exception('photoframe-processor binary not found');
      }

      // Log the full command for debugging
      debugPrint('=== EXECUTING COMMAND ===');
      debugPrint('Binary: $binary');
      debugPrint('Command: $binary ${args.join(' ')}');
      debugPrint('========================');

      // Run process with JSON progress output
      final process = await Process.start(binary, args);

      // Listen to stdout for JSON progress
      process.stdout.transform(utf8.decoder).transform(const LineSplitter()).listen((line) {
        try {
          final json = jsonDecode(line);
          final type = json['type'];

          if (type == 'progress') {
            _processedCount = json['current'] ?? 0;
            _totalCount = json['total'] ?? 0;
            _currentFile = json['message'] ?? '';
            if (_totalCount > 0) {
              _progress = _processedCount / _totalCount;
            }
            notifyListeners();
          } else if (type == 'complete') {
            final success = json['success'] ?? 0;
            final failed = json['failed'] ?? 0;
            final skipped = json['skipped'] ?? 0;

            if (failed == 0 && skipped == 0) {
              _resultsMessage = '✓ Successfully processed $success images';
            } else if (failed == 0) {
              _resultsMessage = '✓ Processed $success images ($skipped skipped)';
            } else {
              _resultsMessage = 'Processed ${success + failed} images ($success succeeded, $failed failed, $skipped skipped)';
            }
          }
        } catch (e) {
          debugPrint('Failed to parse JSON: $line');
        }
      });

      // Listen to stderr for errors
      process.stderr.transform(utf8.decoder).transform(const LineSplitter()).listen((line) {
        debugPrint('stderr: $line');
      });

      final exitCode = await process.exitCode;

      if (exitCode != 0) {
        _errorMessage = 'Processing failed with exit code $exitCode';
      }
    } catch (e) {
      _errorMessage = 'Processing failed: $e';
    } finally {
      _isProcessing = false;
      notifyListeners();
    }
  }

  List<String> _buildCommandArgs() {
    final args = <String>[
      '-i',
      _config.inputPath,
      '-o',
      _config.outputPath,
      '-t',
      _displayTypeToString(_config.displayType),
      '--orientation',
      _targetOrientationToString(_config.orientation),
      '--json-progress',
    ];

    // Output formats
    final formats = <String>[];
    if (_config.outputBmp) formats.add('bmp');
    if (_config.outputBin) formats.add('bin');
    if (_config.outputJpg) formats.add('jpg');
    if (_config.outputPng) formats.add('png');
    if (formats.isNotEmpty) {
      args.addAll(['--output-format', formats.join(',')]);
    }

    // Dithering
    if (_config.autoOptimize) {
      args.add('--auto-optimize');
    } else {
      debugPrint('Adding brightness=${_config.brightness}, contrast=${_config.contrast}');
      args.addAll([
        '--dithering',
        _ditherMethodToString(_config.ditherMethod),
        '--dither-strength=${_config.ditherStrength}',
        '--contrast=${_config.contrast}',
        '--brightness=${_config.brightness}',
      ]);
    }

    // Optional flags
    if (_config.autoColorCorrect) args.add('--auto-color');
    if (_config.detectPeople) {
      args.add('--detect-people');
      if (_config.pythonScriptPath != null) {
        args.addAll(['--python-script', _config.pythonScriptPath!]);
      }
      if (_config.pythonPath != null) {
        args.addAll(['--python-path', _config.pythonPath!]);
      }
      args.add('--confidence=${_config.confidenceThreshold}');
    }
    if (_config.annotate) {
      args.add('--annotate');
      args.addAll([
        '--font',
        _config.font,
        '--pointsize=${_config.pointsize}',
        '--annotate_background',
        _config.annotateBackground,
      ]);
    }
    if (_config.force) args.add('--force');
    if (_config.dryRun) args.add('--dry-run');
    if (_config.debug) args.add('--debug');
    if (_config.report) args.add('--report');
    if (_config.jobs > 0) args.add('--jobs=${_config.jobs}');

    args.addAll([
      '--divider-width=${_config.dividerWidth}',
      '--divider-color',
      _config.dividerColor,
      '--extensions',
      _config.extensions,
    ]);

    return args;
  }

  Future<String?> _findProcessorBinary() async {
    // If user has configured a custom path, use it
    if (_config.processorBinaryPath != null && _config.processorBinaryPath!.isNotEmpty) {
      final file = File(_config.processorBinaryPath!);
      if (await file.exists()) {
        return file.absolute.path;
      }
    }

    // Try common locations
    final locations = [
      // Relative to Flutter app
      '../rust/photoframe-processor/target/release/photoframe-processor',
      // Absolute path in project
      '/Users/alessandro/Documents/git/sephiroth74/arduino/esp32-photo-frame/rust/photoframe-processor/target/release/photoframe-processor',
      // System PATH
      'photoframe-processor',
    ];

    for (final location in locations) {
      try {
        final result = await Process.run('which', [location]);
        if (result.exitCode == 0) {
          return result.stdout.toString().trim();
        }
      } catch (e) {
        // Continue to next location
      }

      // Check if file exists directly
      final file = File(location);
      if (await file.exists()) {
        return file.absolute.path;
      }
    }

    return null;
  }

  String _displayTypeToString(DisplayType type) {
    switch (type) {
      case DisplayType.blackWhite:
        return 'bw';
      case DisplayType.sixColor:
        return '6c';
      case DisplayType.sevenColor:
        return '7c';
    }
  }

  String _targetOrientationToString(TargetOrientation orientation) {
    switch (orientation) {
      case TargetOrientation.landscape:
        return 'landscape';
      case TargetOrientation.portrait:
        return 'portrait';
    }
  }

  String _ditherMethodToString(DitherMethod method) {
    switch (method) {
      case DitherMethod.floydSteinberg:
        return 'floyd-steinberg';
      case DitherMethod.atkinson:
        return 'atkinson';
      case DitherMethod.stucki:
        return 'stucki';
      case DitherMethod.jarvisJudiceNinke:
        return 'jarvis';
      case DitherMethod.ordered:
        return 'ordered';
    }
  }
}
