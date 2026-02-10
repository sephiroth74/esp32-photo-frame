import 'dart:io';

import 'package:flutter/material.dart';
import 'package:photoframe/models/library_models.dart';
import 'package:photoframe/models/ws_messages.dart';

class DitheringScreen extends StatefulWidget {
  final File croppedImageFile;
  final BoardConfig boardConfig;

  const DitheringScreen({super.key, required this.croppedImageFile, required this.boardConfig});

  @override
  State<DitheringScreen> createState() => _DitheringScreenState();
}

enum NavigationTab { effects, brightness, contrast, saturation }

enum AdjustmentType { brightness, contrast, saturation }

class _DitheringScreenState extends State<DitheringScreen> with TickerProviderStateMixin {
  late final bool _supportsSixColors;
  late DitheringMethod _selectedMethod;
  late ColorMode _selectedColorMode;

  NavigationTab? _activeTab;

  double _brightness = 1.0;
  double _contrast = 1.0;
  double _saturation = 1.0;

  @override
  void initState() {
    super.initState();
    _supportsSixColors = _resolveSupportsSixColors(widget.boardConfig.displayType);
    _selectedMethod = DitheringMethod.floydSteinberg;
    _selectedColorMode = _supportsSixColors ? ColorMode.sixColors : ColorMode.blackAndWhite;
  }

  @override
  void dispose() {
    // Clean up temporary cropped image file when leaving this screen
    widget.croppedImageFile.delete().catchError((_) {});
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

  void _setSliderValue(AdjustmentType type, double value) {
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
  }

  Widget _buildPanel() {
    if (_activeTab == null) {
      return const SizedBox.shrink();
    }

    if (_activeTab == NavigationTab.effects) {
      final options = _getAvailableOptions();
      return _EffectsPanel(
        options: options,
        supportsSixColors: _supportsSixColors,
        selected: _DitheringOption(method: _selectedMethod, colorMode: _selectedColorMode),
        onSelect: _onSelectEffect,
        imageFile: widget.croppedImageFile,
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
      onChanged: (value) => _setSliderValue(type, value),
      onChangeEnd: (value) => _setSliderValue(type, value),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Dithering & Effects'),
        centerTitle: true,
        backgroundColor: Colors.white,
        foregroundColor: Colors.black,
        elevation: 0,
      ),
      body: Column(
        children: [
          Expanded(
            child: Container(
              color: Colors.black,
              child: Center(child: Image.file(widget.croppedImageFile, fit: BoxFit.cover)),
            ),
          ),
          AnimatedSize(
            duration: const Duration(milliseconds: 200),
            curve: Curves.easeInOut,
            child: SizedBox(
              height: _activeTab == null
                  ? 0
                  : _activeTab == NavigationTab.effects
                  ? 140
                  : 90,
              child: _buildPanel(),
            ),
          ),
          LayoutBuilder(
            builder: (context, constraints) {
              final showLabel = constraints.maxWidth >= 360;
              return Container(
                padding: const EdgeInsets.symmetric(vertical: 8),
                decoration: BoxDecoration(
                  color: Colors.white,
                  border: Border(top: BorderSide(color: Colors.grey.shade300)),
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
              decoration: BoxDecoration(
                color: Colors.white,
                boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: .08), blurRadius: 12, offset: const Offset(0, -2))],
              ),
              child: Row(
                children: [
                  Expanded(
                    child: OutlinedButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: ElevatedButton(
                      style: ElevatedButton.styleFrom(backgroundColor: Colors.blue, disabledBackgroundColor: Colors.grey[300]),
                      onPressed: () {
                        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Next step not implemented yet')));
                      },
                      child: const Text(
                        'Continue',
                        style: TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
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

  const _EffectsPanel({
    required this.options,
    required this.supportsSixColors,
    required this.selected,
    required this.onSelect,
    required this.imageFile,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.grey.shade100,
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
        child: Row(
          children: [
            for (int i = 0; i < options.length; i++) ...[
              if (i > 0 && supportsSixColors && options[i - 1].colorMode == ColorMode.sixColors && options[i].colorMode == ColorMode.blackAndWhite)
                Container(width: 1, height: 80, margin: const EdgeInsets.symmetric(horizontal: 8), color: Colors.grey.shade400),
              _EffectPreviewCard(option: options[i], isSelected: options[i] == selected, onTap: () => onSelect(options[i]), imageFile: imageFile),
            ],
          ],
        ),
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
    final borderColor = isSelected ? Colors.blue : Colors.grey.shade400;
    final borderWidth = isSelected ? 2.5 : 1.0;

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
              child: ClipRRect(
                borderRadius: const BorderRadius.vertical(top: Radius.circular(7)),
                child: Image.file(imageFile, fit: BoxFit.cover),
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
            const SizedBox(height: 2),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
              decoration: BoxDecoration(
                color: option.colorMode == ColorMode.sixColors ? Colors.blue.shade50 : Colors.grey.shade200,
                borderRadius: BorderRadius.circular(10),
              ),
              child: Text(
                option.modeLabel,
                style: TextStyle(
                  fontSize: 9,
                  fontWeight: FontWeight.bold,
                  color: option.colorMode == ColorMode.sixColors ? Colors.blue.shade700 : Colors.grey.shade700,
                ),
              ),
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
      color: Colors.grey.shade100,
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
    final color = isActive ? Colors.blue : Colors.grey.shade600;

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
