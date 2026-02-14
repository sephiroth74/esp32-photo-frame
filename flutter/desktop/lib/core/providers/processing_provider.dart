import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';
import 'package:photoframe_common/photoframe_common.dart';

import '../models/config_profile.dart';
import '../models/processing_config.dart';
import '../models/processor_message.dart';
import '../services/config_profile_service.dart';

class ProcessingProvider with ChangeNotifier {
  ProcessingConfig _config = const ProcessingConfig(inputPath: '', outputPath: '', outputBin: true, outputJpg: true);
  String? _currentProfilePath;
  String? _currentProfileName;
  bool _hasUnsavedChanges = false;
  List<RecentFile> _recentFiles = [];

  bool _isProcessing = false;
  bool _isSaving = false;
  double _progress = 0.0;
  int _processedCount = 0;
  int _totalCount = 0;
  String _currentFile = '';
  String _errorMessage = '';
  String _currentPhase = 'Starting';
  ProcessorSummary? _lastSummary;

  ProcessingConfig get config => _config;
  String? get currentProfilePath => _currentProfilePath;
  String? get currentProfileName => _currentProfileName;
  bool get hasUnsavedChanges => _hasUnsavedChanges;
  List<RecentFile> get recentFiles => _recentFiles;
  bool get isProcessing => _isProcessing;
  bool get isSaving => _isSaving;
  double? get progress => _progress;
  int get processedCount => lastSummary?.processed ?? _processedCount;
  int get totalCount => lastSummary?.totalOutputImages ?? _totalCount;
  String get currentFile => _currentFile;
  String get errorMessage => _errorMessage;
  ProcessorSummary? get lastSummary => _lastSummary;
  String get currentPhase => _currentPhase;

  ProcessingProvider() {
    _loadRecentFiles();
  }

  void updateConfig(ProcessingConfig newConfig) {
    _config = newConfig;
    _hasUnsavedChanges = true;
    notifyListeners();
    _saveConfig();
  }

  Future<void> _loadRecentFiles() async {
    final prefs = await ConfigProfileService.loadPreferences();
    _recentFiles = prefs.recentFiles;
    notifyListeners();
  }

  Future<void> clearRecentFiles() async {
    await ConfigProfileService.clearRecentFiles();
    _recentFiles = [];
    notifyListeners();
  }

  Future<void> saveProfile({String? filePath, String? name}) async {
    try {
      final savedPath = await ConfigProfileService.saveProfile(_config, filePath: filePath, name: name);
      _currentProfilePath = savedPath;
      _currentProfileName = name ?? filePath?.split('/').last.replaceAll('.pfconfig', '') ?? 'Untitled';
      _hasUnsavedChanges = false;
      await _loadRecentFiles();
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Failed to save profile: $e';
      notifyListeners();
    }
  }

  Future<void> loadProfile(String filePath) async {
    try {
      final profile = await ConfigProfileService.loadProfile(filePath);
      _config = profile.config;
      _currentProfilePath = filePath;
      _currentProfileName = profile.name;
      _hasUnsavedChanges = false;
      await _loadRecentFiles();
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Failed to load profile: $e';
      notifyListeners();
    }
  }

  Future<void> loadRecentFile(RecentFile recentFile) async {
    await loadProfile(recentFile.path);
  }

  void newProfile() {
    _config = const ProcessingConfig(inputPath: '', outputPath: '');
    _currentProfilePath = null;
    _currentProfileName = null;
    _hasUnsavedChanges = false;
    notifyListeners();
  }

  Future<void> loadConfig() async {
    debugPrint('Loading config...');

    try {
      final directory = await getApplicationSupportDirectory();
      final file = File('${directory.path}/config.json');
      if (await file.exists()) {
        final json = jsonDecode(await file.readAsString());
        _config = ProcessingConfig.fromJson(json);
        debugPrint('Config loaded: ${_config.toJson()}');
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
    _currentFile = '';
    notifyListeners();

    try {
      // Build command arguments
      final args = _buildCommandArgs();

      // Find processor binary
      final binary = await _findProcessorBinary();
      if (binary == null) {
        throw Exception('processor binary not found');
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
        debugPrint('STDOUT: ${line.substring(0, line.length > 100 ? 100 : line.length)}'); // Debug all output
        try {
          final json = jsonDecode(line);

          // Try to parse as ProcessorMessage first
          try {
            final message = ProcessorMessage.fromJson(json);
            debugPrint('✓ Parsed processor message: ${message.type}');

            if (message is ProgressMessage) {
              _currentPhase = message.phaseName;
              _currentFile = message.message;
              _processedCount = message.current;
              _totalCount = message.total;

              if (message.phase == ProcessorMessagePhase.saving) {
                _isSaving = true;
              } else {
                _isSaving = false;
              }

              if (_totalCount > 0) {
                _progress = _processedCount / _totalCount;
              }
              notifyListeners();
            } else if (message is ProcessorCompleteMessage) {
              _lastSummary = message.summary;
              _processedCount = message.processed;
              _totalCount = message.totalFiles;
              if (_totalCount > 0) {
                _progress = _processedCount / _totalCount;
              }
              notifyListeners();
            }
          } catch (e) {
            debugPrint('Failed to parse as ProcessorMessage: $e');
          }
        } catch (e) {
          debugPrint('Failed to parse JSON: $line');
          debugPrint('Error: $e');
        }
      });

      // Listen to stderr for errors
      process.stderr.transform(utf8.decoder).transform(const LineSplitter()).listen((line) {
        debugPrint('STDERR: $line');
      });

      final exitCode = await process.exitCode;
      debugPrint('Process exit code: $exitCode');

      if (exitCode != 0) {
        _errorMessage = 'Processing failed with exit code $exitCode';
      }
    } catch (e) {
      _errorMessage = 'Processing failed: $e';
      debugPrint('Processing error: $e');
    } finally {
      debugPrint('=== PROCESSING COMPLETE ===');
      _isProcessing = false;
      _isSaving = false;
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
    if (_config.outputBin) formats.add('pfr1');
    if (_config.outputJpg) formats.add('jpg');
    if (_config.outputPng) formats.add('png');
    if (formats.isNotEmpty) {
      args.addAll(['--output-format', formats.join(',')]);
    }

    // Dithering
    if (_config.autoOptimize) {
      args.add('--auto-optimize');
    } else {
      debugPrint('Adding brightness=${_config.brightness}, contrast=${_config.contrast}, saturation=${_config.saturation}');
      args.addAll([
        '--dithering',
        _ditherMethodToString(_config.ditherMethod),
        '--dither-strength',
        _config.ditherStrength.toString(),
        '--contrast',
        _config.contrast.toString(),
        '--brightness',
        _config.brightness.toString(),
        '--saturation',
        _config.saturation.toString(),
      ]);
    }

    // Optional flags
    if (_config.autoColorCorrect) args.add('--auto-color');
    if (_config.detectPeople) {
      args.add('--detect-people');
      args.add('--confidence=${_config.confidenceThreshold}');
    }
    if (_config.annotate) {
      args.add('--annotate');
      args.addAll(['--font', _config.font, '--font-size', _config.fontSize.toString(), '--annotation_background', _config.annotationBackground]);
    }
    // Add report generation
    args.add('--report');
    args.add('json');

    if (_config.jobs > 0) args.add('--jobs=${_config.jobs}');

    // Pairing options
    if (_config.noPairing) {
      args.add('--no-pairing');
    } else {
      args.addAll(['--divider-width', _config.dividerWidth.toString(), '--divider-color', _config.dividerColor]);
    }

    args.addAll(['--extensions', _config.extensions]);

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
      '../rust/processor/target/release/processor',
      // System PATH
      'processor',
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
    return type.toJsonValue();
  }

  String _targetOrientationToString(Orientation orientation) {
    return orientation.toJsonValue();
  }

  String _ditherMethodToString(DitheringMethod method) {
    return method.toJsonValue();
  }
}
