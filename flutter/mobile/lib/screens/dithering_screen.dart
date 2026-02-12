import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart' hide Orientation;
import 'package:image/image.dart' as img;
import 'package:path_provider/path_provider.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:photoframe/screens/upload_screen.dart';
import 'package:photoframe/services/binary_converter.dart';
import 'package:photoframe/services/dithering_processor.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';
import 'package:photoframe/widgets/dithering_method_icon.dart';

class DitheringScreen extends StatefulWidget {
  final File croppedImageFile;
  final BoardConfig boardConfig;
  final Orientation currentOrientation;

  const DitheringScreen({super.key, required this.croppedImageFile, required this.boardConfig, this.currentOrientation = Orientation.landscape});

  @override
  State<DitheringScreen> createState() => _DitheringScreenState();
}

enum NavigationTab { effects, brightness, contrast, saturation }

enum AdjustmentType { brightness, contrast, saturation }

class _DitherPreviewArgs {
  final Uint8List imageBytes;
  final DisplayType displayType;
  final DitheringMethod method;
  final double brightness;
  final double contrast;
  final double saturation;
  final double ditherStrength;

  const _DitherPreviewArgs({
    required this.imageBytes,
    required this.displayType,
    required this.method,
    required this.brightness,
    required this.contrast,
    required this.saturation,
    required this.ditherStrength,
  });
}

Uint8List? _computeDitherPreview(_DitherPreviewArgs args) {
  return DitheringProcessor.apply(
    args.imageBytes,
    args.displayType,
    args.method,
    saturation: args.saturation,
    contrast: args.contrast,
    brightness: args.brightness,
    ditherStrength: args.ditherStrength,
  );
}

class _DitheringScreenState extends State<DitheringScreen> with TickerProviderStateMixin {
  late final bool _supportsSixColors;
  late DitheringMethod _selectedMethod;
  late ColorMode _selectedColorMode;

  NavigationTab? _activeTab;
  bool _isProcessing = false;
  bool _isGeneratingPreview = false;
  Uint8List? _previewDithered;

  double _brightness = 1.1;
  double _contrast = 0.9;
  double _saturation = 1.0;
  double _ditherStrength = 1.0;

  @override
  void initState() {
    super.initState();
    _supportsSixColors = _resolveSupportsSixColors(widget.boardConfig.displayType);
    _selectedMethod = DitheringMethod.floydSteinberg;
    _selectedColorMode = _supportsSixColors ? ColorMode.sixColors : ColorMode.blackAndWhite;
    // Generate initial preview
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _updatePreviewDithering();
    });
  }

  @override
  void dispose() {
    // Clean up temporary cropped image file when leaving this screen
    widget.croppedImageFile.delete().catchError((_) {
      logger.warning('Failed to delete temporary cropped image file: ${widget.croppedImageFile.path}');
    });
    super.dispose();
  }

  bool _resolveSupportsSixColors(DisplayType displayType) {
    return switch (displayType) {
      DisplayType.sixColors => true,
      _ => false,
    };
  }

  List<_DitheringOption> _getAvailableOptions() {
    final methods = DitheringMethod.values;
    final options = <_DitheringOption>[];

    if (_supportsSixColors) {
      for (final method in methods) {
        options.add(_DitheringOption(method: method, colorMode: ColorMode.sixColors));
      }
    }

    for (final method in methods) {
      options.add(_DitheringOption(method: method, colorMode: ColorMode.blackAndWhite));
    }

    return options;
  }

  void _onSelectEffect(_DitheringOption option) {
    setState(() {
      _selectedMethod = option.method;
      _selectedColorMode = option.colorMode;
    });
    _updatePreviewDithering();
  }

  void _onNavTap(NavigationTab tab) {
    setState(() {
      _activeTab = _activeTab == tab ? null : tab;
    });
  }

  double _getSliderValue(AdjustmentType type) {
    switch (type) {
      case AdjustmentType.brightness:
        return _brightness;
      case AdjustmentType.contrast:
        return _contrast;
      case AdjustmentType.saturation:
        return _saturation;
    }
  }

  void _setSliderValue(AdjustmentType type, double value, bool isFinal) {
    setState(() {
      switch (type) {
        case AdjustmentType.brightness:
          _brightness = value;
          break;
        case AdjustmentType.contrast:
          _contrast = value;
          break;
        case AdjustmentType.saturation:
          _saturation = value;
          break;
      }
    });
    if (isFinal) {
      _updatePreviewDithering();
    }
  }

  void _setDitherStrength(double value, bool isFinal) {
    setState(() {
      _ditherStrength = value;
    });
    if (isFinal) {
      _updatePreviewDithering();
    }
  }

  Future<void> _updatePreviewDithering() async {
    if (_isGeneratingPreview) return;

    try {
      setState(() {
        _isGeneratingPreview = true;
      });

      final imageBytes = await widget.croppedImageFile.readAsBytes();
      logger.fine(
        'Computing dithering preview: method=$_selectedMethod, colorMode=$_selectedColorMode, brightness=$_brightness, contrast=$_contrast, saturation=$_saturation, strength=$_ditherStrength',
      );

      final displayType = _selectedColorMode == ColorMode.sixColors ? DisplayType.sixColors : DisplayType.blackAndWhite;

      final args = _DitherPreviewArgs(
        imageBytes: imageBytes,
        displayType: displayType,
        method: _selectedMethod,
        brightness: _brightness,
        contrast: _contrast,
        saturation: _saturation,
        ditherStrength: _ditherStrength,
      );

      final ditheredBytes = await compute(_computeDitherPreview, args);

      if (!mounted) return;

      setState(() {
        _previewDithered = ditheredBytes;
        _isGeneratingPreview = false;
      });

      logger.fine('Preview dithering computed: ${ditheredBytes?.length ?? 0} bytes');
    } catch (e, stackTrace) {
      logger.warning('Failed to compute preview dithering: $e', e, stackTrace);
      if (mounted) {
        setState(() {
          _isGeneratingPreview = false;
        });
      }
    }
  }

  Future<void> _applyDithering() async {
    if (_isProcessing) return;

    setState(() {
      _isProcessing = true;
    });

    try {
      logger.fine(
        'Applying dithering: method=$_selectedMethod, colorMode=$_selectedColorMode, brightness=$_brightness, contrast=$_contrast, saturation=$_saturation, strength=$_ditherStrength',
      );

      // Read the cropped image
      final imageBytes = await widget.croppedImageFile.readAsBytes();
      logger.fine('Loaded cropped image: ${imageBytes.length} bytes');

      // Apply dithering with effects
      final displayType = _selectedColorMode.toDisplayType();

      final ditheredBytes = DitheringProcessor.apply(
        imageBytes,
        displayType,
        _selectedMethod,
        saturation: _saturation,
        contrast: _contrast,
        brightness: _brightness,
        ditherStrength: _ditherStrength,
      );

      if (!mounted) return;

      logger.info('Dithering applied successfully: ${ditheredBytes.length} bytes');

      // Decode dithered image for rotation
      final ditheredImage = img.decodeImage(ditheredBytes);
      if (ditheredImage == null) {
        throw Exception('Failed to decode dithered image');
      }

      logger.fine('Dithered image decoded: ${ditheredImage.width}x${ditheredImage.height}');

      // Rotate image back to landscape mode based on current orientation
      final int rotationAngle = widget.currentOrientation.toDegrees();
      logger.fine('Current image orientation is ${widget.currentOrientation.name}, applying rotation: $rotationAngle°');

      img.Image rotatedImage = ditheredImage;
      if (rotationAngle != 0) {
        logger.fine('Rotating image by $rotationAngle° to landscape mode');
        rotatedImage = img.copyRotate(ditheredImage, angle: rotationAngle);
      }

      logger.fine('Rotated image: ${rotatedImage.width}x${rotatedImage.height}');

      // Encode back to PNG bytes
      final rotatedBytes = img.encodePng(rotatedImage);

      // Convert to binary format using BinaryConverter
      logger.fine('Converting to binary format for ESP32');
      final binaryData = BinaryConverter.convertToBinary(rotatedBytes, colorMode: _selectedColorMode, orientation: widget.currentOrientation);

      if (binaryData == null) {
        throw Exception('Failed to convert image to binary format');
      }

      logger.info('Binary conversion successful: ${binaryData.length} bytes');

      // Save as .pfr1 file in temp directory
      final tempDir = await getTemporaryDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      final baseFilename = '${tempDir.path}/photoframe_$timestamp';

      final pfrFile = File('$baseFilename.pfr1');

      // Save binary data as .pfr1
      await pfrFile.writeAsBytes(binaryData);
      logger.info('PFR1 file saved: ${pfrFile.path}');

      if (!mounted) return;

      // Navigate to UploadScreen
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(
          builder: (_) => UploadScreen(pfrFile: pfrFile, currentOrientation: widget.currentOrientation),
        ),
      );
    } catch (e, stackTrace) {
      logger.severe('Failed to apply dithering and convert to binary: $e', e, stackTrace);
      if (mounted) {
        final colors = ThemeColors(context);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to process image: $e'), backgroundColor: colors.error));
      }
    } finally {
      if (mounted) {
        setState(() {
          _isProcessing = false;
        });
      }
    }
  }

  Widget _buildPanel() {
    if (_activeTab == null) {
      return const SizedBox.shrink();
    }

    if (_activeTab == NavigationTab.effects) {
      final options = _getAvailableOptions();
      final colors = ThemeColors(context);
      return _EffectsPanel(
        options: options,
        supportsSixColors: _supportsSixColors,
        selected: _DitheringOption(method: _selectedMethod, colorMode: _selectedColorMode),
        onSelect: _onSelectEffect,
        imageFile: widget.croppedImageFile,
        ditherStrength: _ditherStrength,
        onStrengthChanged: (value) => _setDitherStrength(value, false),
        onStrengthChangeEnd: (value) => _setDitherStrength(value, true),
        colors: colors,
      );
    }

    final type = switch (_activeTab) {
      NavigationTab.brightness => AdjustmentType.brightness,
      NavigationTab.contrast => AdjustmentType.contrast,
      NavigationTab.saturation => AdjustmentType.saturation,
      _ => AdjustmentType.brightness,
    };

    return _AdjustmentPanel(
      type: type,
      value: _getSliderValue(type),
      onChanged: (value) => _setSliderValue(type, value, false),
      onChangeEnd: (value) => _setSliderValue(type, value, true),
    );
  }

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Dithering & Effects'),
        centerTitle: true,
        backgroundColor: colors.appBarBackground,
        foregroundColor: colors.appBarForeground,
        elevation: 0,
      ),
      body: Column(
        children: [
          Expanded(
            child: Container(
              color: Colors.black,
              child: Stack(
                alignment: Alignment.center,
                children: [
                  // Image preview layer
                  Stack(
                    alignment: Alignment.center,
                    children: [
                      Image.file(widget.croppedImageFile, fit: BoxFit.cover),
                      if (_previewDithered != null) Image.memory(_previewDithered!, fit: BoxFit.cover),
                      if (_isGeneratingPreview)
                        Container(
                          decoration: BoxDecoration(color: Colors.black.withValues(alpha: 0.5), shape: BoxShape.circle),
                          padding: const EdgeInsets.all(16),
                          child: const CircularProgressIndicator(valueColor: AlwaysStoppedAnimation<Color>(Colors.white)),
                        ),
                    ],
                  ),
                  // Overlay panel
                  Positioned(
                    left: 0,
                    right: 0,
                    bottom: 0,
                    child: AnimatedSize(
                      duration: const Duration(milliseconds: 200),
                      curve: Curves.easeInOut,
                      child: SizedBox(
                        height: _activeTab == null
                            ? 0
                            : _activeTab == NavigationTab.effects
                            ? 185
                            : 90,
                        child: _buildPanel(),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
          LayoutBuilder(
            builder: (context, constraints) {
              final showLabel = constraints.maxWidth >= 360;
              return Container(
                padding: const EdgeInsets.symmetric(vertical: 8),
                decoration: BoxDecoration(
                  color: colors.surface,
                  border: Border(top: BorderSide(color: colors.borderLight)),
                  boxShadow: const [BoxShadow(color: Colors.black12, blurRadius: 6, offset: Offset(0, -2))],
                ),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _NavBarItem(
                      icon: Icons.auto_fix_high,
                      label: 'Effects',
                      showLabel: showLabel,
                      isActive: _activeTab == NavigationTab.effects,
                      onTap: () => _onNavTap(NavigationTab.effects),
                    ),
                    _NavBarItem(
                      icon: Icons.brightness_6,
                      label: 'Brightness',
                      showLabel: showLabel,
                      isActive: _activeTab == NavigationTab.brightness,
                      onTap: () => _onNavTap(NavigationTab.brightness),
                    ),
                    _NavBarItem(
                      icon: Icons.contrast,
                      label: 'Contrast',
                      showLabel: showLabel,
                      isActive: _activeTab == NavigationTab.contrast,
                      onTap: () => _onNavTap(NavigationTab.contrast),
                    ),
                    _NavBarItem(
                      icon: Icons.palette,
                      label: 'Saturation',
                      showLabel: showLabel,
                      isActive: _activeTab == NavigationTab.saturation,
                      onTap: () => _onNavTap(NavigationTab.saturation),
                    ),
                  ],
                ),
              );
            },
          ),
          SafeArea(
            top: false,
            child: Container(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
              decoration: BoxDecoration(color: colors.surface),
              child: Row(
                children: [
                  Expanded(
                    child: OutlinedButton(onPressed: _isProcessing ? null : () => Navigator.of(context).pop(), child: const Text('Cancel')),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: ElevatedButton(
                      style: ElevatedButton.styleFrom(backgroundColor: colors.primary, disabledBackgroundColor: colors.disabled),
                      onPressed: _isProcessing ? null : _applyDithering,
                      child: _isProcessing
                          ? const SizedBox(
                              height: 20,
                              width: 20,
                              child: CircularProgressIndicator(strokeWidth: 2, valueColor: AlwaysStoppedAnimation(Colors.white)),
                            )
                          : Text(
                              'Continue',
                              style: TextStyle(color: colors.onError, fontWeight: FontWeight.bold),
                            ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _DitheringOption {
  final DitheringMethod method;
  final ColorMode colorMode;

  const _DitheringOption({required this.method, required this.colorMode});

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is _DitheringOption && other.method == method && other.colorMode == colorMode;
  }

  @override
  int get hashCode => Object.hash(method, colorMode);

  String get label {
    switch (method) {
      case DitheringMethod.floydSteinberg:
        return 'Floyd-Steinberg';
      case DitheringMethod.atkinson:
        return 'Atkinson';
      case DitheringMethod.stucki:
        return 'Stucki';
      case DitheringMethod.jarvisJudiceNinke:
        return 'Jarvis-Judice';
      case DitheringMethod.ordered:
        return 'Ordered';
    }
  }

  String get modeLabel => colorMode == ColorMode.sixColors ? '6C' : 'BW';
}

class _EffectsPanel extends StatelessWidget {
  final List<_DitheringOption> options;
  final bool supportsSixColors;
  final _DitheringOption selected;
  final ValueChanged<_DitheringOption> onSelect;
  final File imageFile;
  final double ditherStrength;
  final ValueChanged<double> onStrengthChanged;
  final ValueChanged<double> onStrengthChangeEnd;
  final ThemeColors colors;

  const _EffectsPanel({
    required this.options,
    required this.supportsSixColors,
    required this.selected,
    required this.onSelect,
    required this.imageFile,
    required this.ditherStrength,
    required this.onStrengthChanged,
    required this.onStrengthChangeEnd,
    required this.colors,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(backgroundBlendMode: BlendMode.screen, color: Colors.white.withValues(alpha: 0.8)),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(8, 8, 8, 0),
            child: Column(
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Slider(
                        value: ditherStrength,
                        min: 0.0,
                        max: 2.0,
                        divisions: 40,
                        onChanged: onStrengthChanged,
                        onChangeEnd: onStrengthChangeEnd,
                      ),
                    ),
                    const SizedBox(width: 6),
                    Text(ditherStrength.toStringAsFixed(2), style: const TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(width: 6),
                  ],
                ),
              ],
            ),
          ),
          Expanded(
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
              child: Row(
                children: [
                  for (int i = 0; i < options.length; i++) ...[
                    if (i > 0 &&
                        supportsSixColors &&
                        options[i - 1].colorMode == ColorMode.sixColors &&
                        options[i].colorMode == ColorMode.blackAndWhite)
                      Container(width: 1, height: 80, margin: const EdgeInsets.symmetric(horizontal: 8), color: colors.borderLight),
                    _EffectPreviewCard(
                      option: options[i],
                      isSelected: options[i] == selected,
                      onTap: () => onSelect(options[i]),
                      imageFile: imageFile,
                    ),
                  ],
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _EffectPreviewCard extends StatelessWidget {
  final _DitheringOption option;
  final bool isSelected;
  final VoidCallback onTap;
  final File imageFile;

  const _EffectPreviewCard({required this.option, required this.isSelected, required this.onTap, required this.imageFile});

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    final borderColor = isSelected ? colors.primary : colors.borderLight;
    final borderWidth = isSelected ? 2.5 : 1.0;
    final iconColor = option.colorMode == ColorMode.sixColors ? colors.surface : colors.textHint;
    final grayScale = option.colorMode == ColorMode.blackAndWhite;
    Image image;
    if (grayScale) {
      image = Image.file(imageFile, fit: BoxFit.cover, color: Colors.grey, colorBlendMode: BlendMode.saturation, width: 90, height: 70);
    } else {
      image = Image.file(imageFile, fit: BoxFit.cover, width: 90, height: 70);
    }

    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(8),
      child: Container(
        width: 90,
        margin: const EdgeInsets.symmetric(horizontal: 6),
        decoration: BoxDecoration(
          color: Colors.white,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: borderColor, width: borderWidth),
        ),
        child: Column(
          children: [
            Container(
              height: 70,
              width: 90,
              decoration: const BoxDecoration(
                color: Colors.black,
                borderRadius: BorderRadius.vertical(top: Radius.circular(7)),
              ),
              child: Stack(
                alignment: Alignment.center,
                children: [
                  ClipRRect(
                    borderRadius: const BorderRadius.vertical(top: Radius.circular(7)),
                    child: image,
                  ),
                  // Icon overlay with semi-transparent background
                  Container(
                    decoration: BoxDecoration(color: Colors.black.withValues(alpha: 0.4), shape: BoxShape.circle),
                    padding: const EdgeInsets.all(4),
                    child: DitheringMethodIcon(method: option.method, color: iconColor, size: 40),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 6),
            Text(
              option.label,
              style: TextStyle(fontSize: 10, fontWeight: isSelected ? FontWeight.bold : FontWeight.normal),
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}

class _AdjustmentPanel extends StatelessWidget {
  final AdjustmentType type;
  final double value;
  final ValueChanged<double> onChanged;
  final ValueChanged<double> onChangeEnd;

  const _AdjustmentPanel({required this.type, required this.value, required this.onChanged, required this.onChangeEnd});

  String get _label {
    switch (type) {
      case AdjustmentType.brightness:
        return 'Brightness';
      case AdjustmentType.contrast:
        return 'Contrast';
      case AdjustmentType.saturation:
        return 'Saturation';
    }
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(backgroundBlendMode: BlendMode.screen, color: Colors.white.withValues(alpha: 0.8)),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Text('$_label: ${value.toStringAsFixed(2)}', style: const TextStyle(fontWeight: FontWeight.bold)),
          Slider(value: value, min: 0.0, max: 2.0, divisions: 40, onChanged: onChanged, onChangeEnd: onChangeEnd),
        ],
      ),
    );
  }
}

class _NavBarItem extends StatelessWidget {
  final IconData icon;
  final String label;
  final bool showLabel;
  final bool isActive;
  final VoidCallback onTap;

  const _NavBarItem({required this.icon, required this.label, required this.showLabel, required this.isActive, required this.onTap});

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    final color = isActive ? colors.primary : colors.textSecondary;

    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(8),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, color: color, size: 24),
            if (showLabel) ...[
              const SizedBox(height: 4),
              Text(
                label,
                style: TextStyle(fontSize: 11, color: color, fontWeight: isActive ? FontWeight.bold : FontWeight.normal),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
