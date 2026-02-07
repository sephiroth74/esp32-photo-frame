import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../core/models/processing_config.dart';
import '../core/providers/processing_provider.dart';
import '../core/providers/widget_factory_provider.dart';
import '../core/services/file_picker_history.dart';
import '../core/services/font_service.dart';
import '../presentation/abstractions/widget_abstractions.dart';
import '../widgets/report_summary_widget.dart';

class ProcessingScreen extends StatelessWidget {
  const ProcessingScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Expanded(
          child: SingleChildScrollView(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: const [
                _FileSelectionSection(),
                SizedBox(height: 20),
                _ProcessorBinarySection(),
                SizedBox(height: 20),
                _DisplaySettingsSection(),
                SizedBox(height: 20),
                _PeopleDetectionSection(),
                SizedBox(height: 20),
                _DitheringSettingsSection(),
                SizedBox(height: 20),
                _OutputFormatsSection(),
                SizedBox(height: 20),
                _AnnotationSettingsSection(),
                SizedBox(height: 20),
                _DividerSettingsSection(),
                SizedBox(height: 20),
                _AdvancedOptionsSection(),
                SizedBox(height: 20),
              ],
            ),
          ),
        ),
        const SizedBox(height: 8),
        const _ProcessButtonSection(),
        const SizedBox(height: 8),
      ],
    );
  }
}

class _FileSelectionSection extends StatelessWidget {
  const _FileSelectionSection();

  Widget _buildButton(BuildContext context, {required String label, required VoidCallback? onPressed}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.button(label: label, onPressed: onPressed, size: PlatformButtonSize.medium);
  }

  Widget _buildTextField(BuildContext context, {String? placeholder, TextEditingController? controller, ValueChanged<String>? onChanged}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.textField(placeholder: placeholder, controller: controller, onChanged: onChanged, maxLines: 1);
  }

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('File Selection', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  const SizedBox(width: 100, child: Text('Input:')),
                  Expanded(
                    child: _buildTextField(
                      context,
                      placeholder: 'Select input directory...',
                      controller: TextEditingController(text: config.inputPath),
                      onChanged: (value) {
                        provider.updateConfig(config.copyWith(inputPath: value));
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  _buildButton(
                    context,
                    label: 'Browse...',
                    onPressed: () async {
                      final path = await FilePicker.platform.getDirectoryPath(initialDirectory: FilePickerHistory.initialDir('inputDir'));
                      if (path != null) {
                        FilePickerHistory.rememberDirectory('inputDir', path);
                        provider.updateConfig(config.copyWith(inputPath: path));
                      }
                    },
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  const SizedBox(width: 100, child: Text('Output:')),
                  Expanded(
                    child: _buildTextField(
                      context,
                      placeholder: 'Select output directory...',
                      controller: TextEditingController(text: config.outputPath),
                      onChanged: (value) {
                        provider.updateConfig(config.copyWith(outputPath: value));
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  _buildButton(
                    context,
                    label: 'Browse...',
                    onPressed: () async {
                      final path = await FilePicker.platform.getDirectoryPath(initialDirectory: FilePickerHistory.initialDir('outputDir'));
                      if (path != null) {
                        FilePickerHistory.rememberDirectory('outputDir', path);
                        provider.updateConfig(config.copyWith(outputPath: path));
                      }
                    },
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _ProcessorBinarySection extends StatelessWidget {
  const _ProcessorBinarySection();

  Widget _buildButton(BuildContext context, {required String label, required VoidCallback? onPressed}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.button(label: label, onPressed: onPressed, size: PlatformButtonSize.medium);
  }

  Widget _buildTextField(BuildContext context, {String? placeholder, TextEditingController? controller, ValueChanged<String>? onChanged}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.textField(placeholder: placeholder, controller: controller, onChanged: onChanged, maxLines: 1);
  }

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Processor Binary', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  const SizedBox(width: 100, child: Text('Binary Path:')),
                  Expanded(
                    child: _buildTextField(
                      context,
                      placeholder: 'Path to processor binary (optional)',
                      controller: TextEditingController(text: config.processorBinaryPath ?? ''),
                      onChanged: (value) {
                        provider.updateConfig(config.copyWith(processorBinaryPath: value.isEmpty ? null : value));
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  _buildButton(
                    context,
                    label: 'Browse...',
                    onPressed: () async {
                      final result = await FilePicker.platform.pickFiles(initialDirectory: FilePickerHistory.initialDir('processorBinary'));
                      if (result != null && result.files.single.path != null) {
                        final selectedPath = result.files.single.path!;
                        FilePickerHistory.rememberFile('processorBinary', selectedPath);
                        provider.updateConfig(config.copyWith(processorBinaryPath: selectedPath));
                      }
                    },
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  const Icon(Icons.info_outline, size: 16, color: Colors.grey),
                  const SizedBox(width: 4),
                  Text('If not specified, the app will search in common locations', style: TextStyle(fontSize: 12, color: Colors.grey[600])),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _DisplaySettingsSection extends StatelessWidget {
  const _DisplaySettingsSection();

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;
    final factory = context.read<WidgetFactoryProvider>().factory;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Display Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.center,
                      children: [
                        const Text('Display Type:'),
                        const SizedBox(width: 8),

                        factory.popupMenu<DisplayType>(
                          label: Text(config.displayType.name),
                          selectedItem: config.displayType,
                          onSelected: (value) {
                            if (value != null) {
                              provider.updateConfig(config.copyWith(displayType: value));
                            }
                          },
                          items: [
                            factory.popupMenuItem<DisplayType>(value: DisplayType.blackWhite, label: 'Black & White'),
                            factory.popupMenuItem<DisplayType>(value: DisplayType.sixColor, label: '6-Color'),
                          ],
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(width: 20),
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.center,
                      children: [
                        const Text('Target Orientation:'),
                        const SizedBox(width: 8),
                        factory.popupMenu<TargetOrientation>(
                          label: Text(config.orientation.name),
                          style: PlatformPopupMenuStyle.plain,
                          selectedItem: config.orientation,
                          onSelected: (value) {
                            if (value != null) {
                              provider.updateConfig(config.copyWith(orientation: value));
                            }
                          },
                          items: [
                            factory.popupMenuItem<TargetOrientation>(value: TargetOrientation.landscape, label: 'Landscape'),
                            factory.popupMenuItem<TargetOrientation>(value: TargetOrientation.portrait, label: 'Portrait'),
                            factory.popupMenuItem<TargetOrientation>(value: TargetOrientation.landscapeReverse, label: 'Landscape Reverse'),
                            factory.popupMenuItem<TargetOrientation>(value: TargetOrientation.portraitReverse, label: 'Portrait Reverse'),
                          ],
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _PeopleDetectionSection extends StatelessWidget {
  const _PeopleDetectionSection();

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final factory = context.read<WidgetFactoryProvider>().factory;
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('People Detection (AI)', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  factory.switchWidget(
                    checked: config.detectPeople,
                    onChanged: (value) => provider.updateConfig(config.copyWith(detectPeople: value)),
                  ),
                  const SizedBox(width: 8),
                  const Text('Person detection'),
                ],
              ),
              if (config.detectPeople) ...[
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Confidence: ${config.confidenceThreshold.toStringAsFixed(2)}'),
                          factory.slider(
                            value: config.confidenceThreshold.clamp(0.3, 0.9),
                            stops: [0.3, 0.35, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9],
                            min: 0.3,
                            max: 0.9,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(confidenceThreshold: value));
                            },
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 12),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _DitheringSettingsSection extends StatelessWidget {
  const _DitheringSettingsSection();

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final factory = context.read<WidgetFactoryProvider>().factory;
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Dithering Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  factory.switchWidget(
                    checked: config.autoColorCorrect,
                    onChanged: (value) {
                      provider.updateConfig(config.copyWith(autoColorCorrect: value));
                    },
                  ),
                  const SizedBox(width: 8),
                  const Text('Auto-color correction'),
                  const SizedBox(height: 8, width: 16),
                  factory.switchWidget(
                    checked: config.autoOptimize,
                    onChanged: (value) {
                      provider.updateConfig(config.copyWith(autoOptimize: value));
                    },
                  ),
                  const SizedBox(width: 8),
                  const Text('Auto-optimize'),
                ],
              ),
              if (!config.autoOptimize) ...[
                const SizedBox(height: 24),
                _buildGroupBox(
                  context,
                  child: Row(
                    mainAxisSize: MainAxisSize.max,
                    children: [
                      const Text('Method:'),
                      const SizedBox(height: 4, width: 8),
                      factory.popupMenu<DitherMethod>(
                        label: Text(config.ditherMethod.name),
                        style: PlatformPopupMenuStyle.bevel,
                        selectedItem: config.ditherMethod,
                        onSelected: (value) {
                          if (value != null) {
                            provider.updateConfig(config.copyWith(ditherMethod: value));
                          }
                        },
                        items: [
                          factory.popupMenuItem<DitherMethod>(value: DitherMethod.floydSteinberg, label: 'Floyd-Steinberg'),
                          factory.popupMenuItem<DitherMethod>(value: DitherMethod.atkinson, label: 'Atkinson'),
                          factory.popupMenuItem<DitherMethod>(value: DitherMethod.stucki, label: 'Stucki'),
                          factory.popupMenuItem<DitherMethod>(value: DitherMethod.jarvisJudiceNinke, label: 'Jarvis'),
                          factory.popupMenuItem<DitherMethod>(value: DitherMethod.ordered, label: 'Ordered/Bayer'),
                        ],
                      ),
                      const SizedBox(width: 24),
                      Expanded(
                        child: Row(
                          mainAxisSize: MainAxisSize.max,
                          children: [
                            Text('Strength: ${config.ditherStrength}'),
                            const SizedBox(width: 8),
                            Expanded(
                              child: factory.slider(
                                value: config.ditherStrength.toDouble(),
                                min: 0.0,
                                max: 200.0,
                                onChanged: (value) {
                                  provider.updateConfig(config.copyWith(ditherStrength: value.round()));
                                },
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Contrast: ${config.contrast}'),
                          factory.slider(
                            value: (config.contrast + 100).toDouble(),
                            min: 0.0,
                            max: 200.0,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(contrast: value.round() - 100));
                            },
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(width: 20),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Brightness: ${config.brightness}'),
                          factory.slider(
                            value: (config.brightness + 100).toDouble(),
                            min: 0.0,
                            max: 200.0,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(brightness: value.round() - 100));
                            },
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Saturation: ${config.saturation}'),
                          factory.slider(
                            value: config.saturation.toDouble(),
                            min: 0.0,
                            max: 200.0,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(saturation: value.round()));
                            },
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ] else ...[
                const SizedBox(height: 8),
                Text('(Auto-optimize will select optimal parameters for each image)', style: TextStyle(fontSize: 12, color: Colors.grey[600])),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _OutputFormatsSection extends StatelessWidget {
  const _OutputFormatsSection();

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final factory = context.read<WidgetFactoryProvider>().factory;
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Output Formats', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Wrap(
                spacing: 16,
                runSpacing: 8,
                children: [
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      factory.checkbox(
                        value: config.outputBmp,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(outputBmp: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('BMP'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      factory.checkbox(
                        value: config.outputBin,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(outputBin: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('Binary (PFR1)'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      factory.checkbox(
                        value: config.outputJpg,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(outputJpg: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('JPEG'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      factory.checkbox(
                        value: config.outputPng,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(outputPng: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('PNG'),
                    ],
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _AnnotationSettingsSection extends StatefulWidget {
  const _AnnotationSettingsSection();

  @override
  State<_AnnotationSettingsSection> createState() => _AnnotationSettingsSectionState();
}

class _AnnotationSettingsSectionState extends State<_AnnotationSettingsSection> {
  List<String> _systemFonts = [];
  bool _fontsLoaded = false;

  @override
  void initState() {
    super.initState();
    _loadSystemFonts();
  }

  Future<void> _loadSystemFonts() async {
    final fonts = await FontService.getSystemFonts();
    if (!mounted) return;
    setState(() {
      _systemFonts = fonts;
      _fontsLoaded = true;
    });
  }

  Widget _buildTextField(
    BuildContext context, {
    String? placeholder,
    TextEditingController? controller,
    ValueChanged<String>? onChanged,
    TextInputType keyboardType = TextInputType.text,
  }) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.textField(placeholder: placeholder, controller: controller, onChanged: onChanged, maxLines: 1, keyboardType: keyboardType);
  }

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final factory = context.read<WidgetFactoryProvider>().factory;
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Annotation Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  factory.switchWidget(
                    checked: config.annotate,
                    onChanged: (value) {
                      provider.updateConfig(config.copyWith(annotate: value));
                    },
                  ),
                  const SizedBox(width: 8),
                  const Text('Add date/time annotation to images'),
                ],
              ),
              if (config.annotate) ...[
                const SizedBox(height: 12),
                Row(
                  children: [
                    const Text('Font:'),
                    const SizedBox(width: 8),
                    Expanded(
                      flex: 0,
                      child: _fontsLoaded
                          ? factory.popupMenu<String>(
                              style: PlatformPopupMenuStyle.bevel,
                              label: _systemFonts.contains(config.font) ? Text(config.font) : null,
                              selectedItem: _systemFonts.contains(config.font) ? config.font : null,
                              onSelected: (value) {
                                if (value != null) {
                                  provider.updateConfig(config.copyWith(font: value));
                                }
                              },
                              items: _systemFonts.map((font) => factory.popupMenuItem<String>(value: font, label: font)).toList(),
                            )
                          : factory.circularProgress(value: null),
                    ),
                    const SizedBox(width: 12),
                    const Text('Size:'),
                    const SizedBox(width: 8),
                    SizedBox(
                      width: 80,
                      child: _buildTextField(
                        context,
                        placeholder: '22',
                        controller: TextEditingController(text: config.fontSize.toString()),
                        keyboardType: TextInputType.number,
                        onChanged: (value) {
                          final size = int.tryParse(value);
                          if (size != null) {
                            provider.updateConfig(config.copyWith(fontSize: size));
                          }
                        },
                      ),
                    ),
                    const SizedBox(width: 12),
                    const Text('Background:'),
                    const SizedBox(width: 8),
                    SizedBox(
                      width: 120,
                      child: _buildTextField(
                        context,
                        placeholder: '#40000000',
                        controller: TextEditingController(text: config.annotationBackground),
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(annotationBackground: value));
                        },
                      ),
                    ),
                  ],
                ),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _DividerSettingsSection extends StatelessWidget {
  const _DividerSettingsSection();

  Widget _buildTextField(
    BuildContext context, {
    String? placeholder,
    TextEditingController? controller,
    ValueChanged<String>? onChanged,
    TextInputType keyboardType = TextInputType.text,
  }) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.textField(placeholder: placeholder, controller: controller, onChanged: onChanged, maxLines: 1, keyboardType: keyboardType);
  }

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Pairing Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  factory.switchWidget(
                    checked: config.noPairing,
                    onChanged: (value) {
                      provider.updateConfig(config.copyWith(noPairing: value));
                    },
                  ),
                  const SizedBox(width: 8),
                  const Text('Disable automatic pairing of images'),
                ],
              ),
              if (!config.noPairing) ...[
                const SizedBox(height: 16),
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Divider Width:'),
                          const SizedBox(height: 4),
                          _buildTextField(
                            context,
                            placeholder: '3',
                            controller: TextEditingController(text: config.dividerWidth.toString()),
                            keyboardType: TextInputType.number,
                            onChanged: (value) {
                              final width = int.tryParse(value);
                              if (width != null) {
                                provider.updateConfig(config.copyWith(dividerWidth: width));
                              }
                            },
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Divider Color:'),
                          const SizedBox(height: 4),
                          _buildTextField(
                            context,
                            placeholder: '#FFFFFF',
                            controller: TextEditingController(text: config.dividerColor),
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(dividerColor: value));
                            },
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                Text('ℹ Divider is drawn between paired portrait images', style: TextStyle(fontSize: 12, color: Colors.grey[600])),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _AdvancedOptionsSection extends StatelessWidget {
  const _AdvancedOptionsSection();

  Widget _buildTextField(
    BuildContext context, {
    String? placeholder,
    TextEditingController? controller,
    ValueChanged<String>? onChanged,
    TextInputType keyboardType = TextInputType.text,
  }) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.textField(placeholder: placeholder, controller: controller, onChanged: onChanged, maxLines: 1, keyboardType: keyboardType);
  }

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Advanced Options', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        _buildGroupBox(
          context,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('Parallel Jobs:'),
                        const SizedBox(height: 4),
                        _buildTextField(
                          context,
                          placeholder: 'Auto',
                          controller: TextEditingController(text: config.jobs == 0 ? 'Auto' : config.jobs.toString()),
                          keyboardType: TextInputType.number,
                          onChanged: (value) {
                            if (value.toLowerCase() == 'auto' || value.isEmpty) {
                              provider.updateConfig(config.copyWith(jobs: 0));
                            } else {
                              final jobs = int.tryParse(value);
                              if (jobs != null) {
                                provider.updateConfig(config.copyWith(jobs: jobs));
                              }
                            }
                          },
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('File Extensions: (comma separated)'),
                        const SizedBox(height: 4),
                        _buildTextField(
                          context,
                          placeholder: 'jpg,jpeg,png,heic',
                          controller: TextEditingController(text: config.extensions),
                          onChanged: (value) {
                            provider.updateConfig(config.copyWith(extensions: value));
                          },
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _ProcessButtonSection extends StatefulWidget {
  const _ProcessButtonSection();

  @override
  State<_ProcessButtonSection> createState() => _ProcessButtonSectionState();
}

class _ProcessButtonSectionState extends State<_ProcessButtonSection> {
  bool _dialogShown = false;

  void _showProgressDialog(BuildContext context, ProcessingProvider provider) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    if (_dialogShown) return;
    _dialogShown = true;

    factory
        .openDialog(
          context: context,
          barrierDismissible: false,
          builder: (context, factory) => _ProcessingDialog(provider: provider),
        )
        .then((_) {
          _dialogShown = false;
        });
  }

  Widget _buildButton(BuildContext context, {required String label, required VoidCallback? onPressed}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.button(label: label, onPressed: onPressed, size: PlatformButtonSize.large);
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();

    if (provider.isProcessing && !_dialogShown) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _showProgressDialog(context, provider);
      });
    }

    return Row(
      children: [
        Spacer(flex: 1),
        Center(
          child: _buildButton(
            context,
            label: provider.isProcessing ? 'Processing...' : 'Process Images',
            onPressed: provider.isProcessing
                ? null
                : () {
                    provider.startProcessing();
                  },
          ),
        ),
      ],
    );
  }
}

class _ProcessingDialog extends StatelessWidget {
  final ProcessingProvider provider;

  const _ProcessingDialog({required this.provider});

  String _getProcessingCompleteSummary(ProcessingProvider provider) {
    final total = provider.totalCount;
    final processed = provider.processedCount;
    final failed = total - processed;

    if (failed == 0) {
      return 'Processing complete ($processed images processed)';
    } else if (failed == 1) {
      return 'Processing complete ($processed images processed, 1 failed)';
    } else {
      return 'Processing complete ($processed images processed, $failed failed)';
    }
  }

  void _showReportDialog(BuildContext context, ProcessingProvider provider) {
    final factory = context.read<WidgetFactoryProvider>().factory;

    factory.openDialog(
      context: context,
      barrierDismissible: true,
      builder: (context, factory) {
        return factory.dialog(
          constraints: const BoxConstraints(minWidth: 400, maxWidth: 500, maxHeight: 600),
          title: 'Processing Report',
          content: (context) => SingleChildScrollView(child: ReportSummaryWidget(summary: provider.lastSummary)),
          actions: [PlatformDialogAction(label: 'Close', onPressed: () => Navigator.of(context).pop())],
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return Consumer<ProcessingProvider>(
      builder: (context, provider, child) {
        return factory.dialog(
          constraints: BoxConstraints(minWidth: 500, maxWidth: 500, minHeight: 300, maxHeight: 600),
          title: 'Processing Images',
          content: (context) => Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Text(
                'Phase: ${provider.currentPhase}',
                style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w500, color: Colors.grey),
              ),
              const SizedBox(height: 8),
              factory.progress(value: provider.progress),
              const SizedBox(height: 12),
              Text(
                provider.isSaving ? 'Saving images...' : 'Processing: ${provider.processedCount}/${provider.totalCount}',
                style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w500),
              ),
              if (provider.errorMessage.isNotEmpty) ...[
                const SizedBox(height: 12),
                Text(provider.errorMessage, style: const TextStyle(color: Colors.red, fontSize: 12)),
              ],
              if (!provider.isProcessing && !provider.isSaving) ...[
                const SizedBox(height: 16),
                const Divider(),
                const SizedBox(height: 12),
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.green.withAlpha(13),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.green.withAlpha(51)),
                  ),
                  child: Column(
                    children: [
                      Row(
                        children: [
                          const Icon(Icons.check_circle, color: Colors.green, size: 20),
                          const SizedBox(width: 8),
                          Expanded(
                            child: Text(_getProcessingCompleteSummary(provider), style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w500)),
                          ),
                        ],
                      ),
                      const SizedBox(height: 12),
                      factory.button(
                        label: 'View Full Report',
                        onPressed: provider.lastSummary != null
                            ? () {
                                _showReportDialog(context, provider);
                              }
                            : null,
                        style: PlatformButtonStyle.primary,
                      ),
                    ],
                  ),
                ),
              ],
            ],
          ),
          actions: [PlatformDialogAction(label: 'Close', onPressed: !provider.isProcessing ? () => Navigator.of(context).pop() : null)],
        );
      },
    );
  }
}
