import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:file_picker/file_picker.dart';
import '../providers/processing_provider.dart';
import '../models/processing_config.dart';
import '../services/font_service.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return AppKitScaffold(
      toolBar: AppKitToolBar(title: const Text('ESP32 Photo Frame Processor'), titleWidth: 250),
      children: [
        AppKitContentArea(
          builder: (context, scrollController) {
            return SingleChildScrollView(
              controller: scrollController,
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _FileSelectionSection(),
                  const SizedBox(height: 20),
                  _ProcessorBinarySection(),
                  const SizedBox(height: 20),
                  _DisplaySettingsSection(),
                  const SizedBox(height: 20),
                  _PeopleDetectionSection(),
                  const SizedBox(height: 20),
                  _DitheringSettingsSection(),
                  const SizedBox(height: 20),
                  _OutputFormatsSection(),
                  const SizedBox(height: 20),
                  _AnnotationSettingsSection(),
                  const SizedBox(height: 20),
                  _DividerSettingsSection(),
                  const SizedBox(height: 20),
                  _AdvancedOptionsSection(),
                  const SizedBox(height: 20),
                  _ProcessButtonSection(),
                ],
              ),
            );
          },
        ),
      ],
    );
  }
}

class _FileSelectionSection extends StatelessWidget {
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
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              // Input path
              Row(
                children: [
                  const SizedBox(width: 100, child: Text('Input:')),
                  Expanded(
                    child: AppKitTextField(
                      controller: TextEditingController(text: config.inputPath),
                      maxLines: 1,
                      placeholder: 'Select input directory...',
                      onChanged: (value) {
                        provider.updateConfig(config.copyWith(inputPath: value));
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  AppKitButton(
                    size: AppKitControlSize.regular,
                    onTap: () async {
                      final path = await FilePicker.platform.getDirectoryPath();
                      if (path != null) {
                        provider.updateConfig(config.copyWith(inputPath: path));
                      }
                    },
                    child: const Text('Browse...'),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              // Output path
              Row(
                children: [
                  const SizedBox(width: 100, child: Text('Output:')),
                  Expanded(
                    child: AppKitTextField(
                      maxLines: 1,
                      controller: TextEditingController(text: config.outputPath),
                      placeholder: 'Select output directory...',
                      onChanged: (value) {
                        provider.updateConfig(config.copyWith(outputPath: value));
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  AppKitButton(
                    size: AppKitControlSize.regular,
                    onTap: () async {
                      final path = await FilePicker.platform.getDirectoryPath();
                      if (path != null) {
                        provider.updateConfig(config.copyWith(outputPath: path));
                      }
                    },
                    child: const Text('Browse...'),
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
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  const SizedBox(width: 100, child: Text('Binary Path:')),
                  Expanded(
                    child: AppKitTextField(
                      maxLines: 1,
                      controller: TextEditingController(text: config.processorBinaryPath ?? ''),
                      placeholder: 'Path to photoframe-processor binary (optional)',
                      onChanged: (value) {
                        provider.updateConfig(config.copyWith(processorBinaryPath: value.isEmpty ? null : value));
                      },
                    ),
                  ),
                  const SizedBox(width: 8),
                  AppKitButton(
                    size: AppKitControlSize.regular,
                    onTap: () async {
                      final result = await FilePicker.platform.pickFiles();
                      if (result != null && result.files.single.path != null) {
                        provider.updateConfig(config.copyWith(processorBinaryPath: result.files.single.path));
                      }
                    },
                    child: const Text('Browse...'),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                'ℹ If not specified, the app will search in common locations',
                style: TextStyle(fontSize: 12, color: Colors.grey[600]),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _DisplaySettingsSection extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Display Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  // Display Type
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('Display Type:'),
                        const SizedBox(width: 4),
                        AppKitPopupButton<DisplayType>(
                          selectedItem: config.displayType,
                          onItemSelected: (value) {
                            if (value != null) {
                              provider.updateConfig(config.copyWith(displayType: value));
                            }
                          },
                          items: const [
                            AppKitContextMenuItem(value: DisplayType.blackWhite, child: Text('Black & White')),
                            AppKitContextMenuItem(value: DisplayType.sixColor, child: Text('6-Color')),
                            AppKitContextMenuItem(value: DisplayType.sevenColor, child: Text('7-Color')),
                          ],
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(width: 20),
                  // Target Orientation
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('Target Orientation:'),
                        const SizedBox(width: 4),
                        AppKitPopupButton<TargetOrientation>(
                          selectedItem: config.orientation,
                          onItemSelected: (value) {
                            if (value != null) {
                              provider.updateConfig(config.copyWith(orientation: value));
                            }
                          },
                          items: const [
                            AppKitContextMenuItem(value: TargetOrientation.landscape, child: Text('Landscape')),
                            AppKitContextMenuItem(value: TargetOrientation.portrait, child: Text('Portrait')),
                          ],
                        ),
                      ],
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                'ℹ Images will be pre-rotated to match the physical display orientation',
                style: TextStyle(fontSize: 12, color: Colors.grey[600]),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _PeopleDetectionSection extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('People Detection (AI)', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  AppKitSwitch(
                    checked: config.detectPeople,
                    onChanged: (value) {
                      provider.updateConfig(config.copyWith(detectPeople: value));
                    },
                  ),
                  const SizedBox(width: 8),
                  const Text('Enable person detection for smart cropping'),
                ],
              ),
              if (config.detectPeople) ...[
                const SizedBox(height: 12),
                Text('ℹ Uses YOLO11 model for accurate subject detection', style: TextStyle(fontSize: 12, color: Colors.grey[600])),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Confidence: ${config.confidenceThreshold.toStringAsFixed(2)}'),
                          AppKitSlider(
                            value: config.confidenceThreshold,
                            min: 0.3,
                            max: 0.95,
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
                // Python Script Path
                Row(
                  children: [
                    const SizedBox(width: 120, child: Text('Python Script:')),
                    Expanded(
                      child: AppKitTextField(
                        maxLines: 1,
                        controller: TextEditingController(text: config.pythonScriptPath ?? ''),
                        placeholder: 'Path to find_subject.py (optional)',
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(pythonScriptPath: value.isEmpty ? null : value));
                        },
                      ),
                    ),
                    const SizedBox(width: 8),
                    AppKitButton(
                      size: AppKitControlSize.regular,
                      onTap: () async {
                        final result = await FilePicker.platform.pickFiles(type: FileType.custom, allowedExtensions: ['py']);
                        if (result != null && result.files.single.path != null) {
                          provider.updateConfig(config.copyWith(pythonScriptPath: result.files.single.path));
                        }
                      },
                      child: const Text('Browse...'),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                // Python Path
                Row(
                  children: [
                    const SizedBox(width: 120, child: Text('Python Binary:')),
                    Expanded(
                      child: AppKitTextField(
                        maxLines: 1,
                        controller: TextEditingController(text: config.pythonPath ?? ''),
                        placeholder: 'Path to python3 binary (optional)',
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(pythonPath: value.isEmpty ? null : value));
                        },
                      ),
                    ),
                    const SizedBox(width: 8),
                    AppKitButton(
                      size: AppKitControlSize.regular,
                      onTap: () async {
                        final result = await FilePicker.platform.pickFiles();
                        if (result != null && result.files.single.path != null) {
                          provider.updateConfig(config.copyWith(pythonPath: result.files.single.path));
                        }
                      },
                      child: const Text('Browse...'),
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

class _DitheringSettingsSection extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Dithering Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.standardScrollBox,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  AppKitSwitch(
                    checked: config.autoOptimize,
                    onChanged: (value) {
                      provider.updateConfig(config.copyWith(autoOptimize: value));
                    },
                  ),
                  const SizedBox(width: 8),
                  const Text('Auto-optimize (automatic per-image optimization)'),
                ],
              ),
              if (!config.autoOptimize) ...[
                const SizedBox(height: 12),
                Row(
                  children: [
                    const Text('Method:'),
                    const SizedBox(height: 4, width: 4),
                    AppKitPopupButton<DitherMethod>(
                      style: AppKitPopupButtonStyle.bevel,
                      selectedItem: config.ditherMethod,
                      onItemSelected: (value) {
                        if (value != null) {
                          provider.updateConfig(config.copyWith(ditherMethod: value));
                        }
                      },
                      items: const [
                        AppKitContextMenuItem(value: DitherMethod.floydSteinberg, child: Text('Floyd-Steinberg')),
                        AppKitContextMenuItem(value: DitherMethod.atkinson, child: Text('Atkinson')),
                        AppKitContextMenuItem(value: DitherMethod.stucki, child: Text('Stucki')),
                        AppKitContextMenuItem(value: DitherMethod.jarvisJudiceNinke, child: Text('Jarvis')),
                        AppKitContextMenuItem(value: DitherMethod.ordered, child: Text('Ordered/Bayer')),
                      ],
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Strength: ${config.ditherStrength.toStringAsFixed(2)}'),
                          AppKitSlider(
                            value: config.ditherStrength,
                            min: 0.5,
                            max: 1.5,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(ditherStrength: value));
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
                          Text('Contrast: ${config.contrast.toStringAsFixed(2)}'),
                          AppKitSlider(
                            value: config.contrast + 1.0,
                            min: 0.0,
                            max: 2.0,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(contrast: value - 1.0));
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
                          Text('Brightness: ${config.brightness.toStringAsFixed(2)}'),
                          AppKitSlider(
                            value: config.brightness + 1.0,
                            min: 0.0,
                            max: 2.0,
                            onChanged: (value) {
                              provider.updateConfig(config.copyWith(brightness: value - 1.0));
                            },
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ] else ...[
                const SizedBox(height: 8),
                Text(
                  '(Auto-optimize will select optimal parameters for each image)',
                  style: TextStyle(fontSize: 12, color: Colors.grey[600]),
                ),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _OutputFormatsSection extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Output Formats', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
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
                      AppKitCheckbox(
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
                      AppKitCheckbox(
                        value: config.outputBin,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(outputBin: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('Binary (BIN)'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      AppKitCheckbox(
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
                      AppKitCheckbox(
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
    setState(() {
      _systemFonts = fonts;
      _fontsLoaded = true;
    });
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
          child: Text('Annotation Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              Row(
                children: [
                  AppKitSwitch(
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
                      child: _fontsLoaded
                          ? AppKitPopupButton<String>(
                              style: AppKitPopupButtonStyle.bevel,
                              selectedItem: _systemFonts.contains(config.font) ? config.font : null,
                              onItemSelected: (value) {
                                if (value != null) {
                                  provider.updateConfig(config.copyWith(font: value));
                                }
                              },
                              items: _systemFonts
                                  .map((font) => AppKitContextMenuItem(
                                        value: font,
                                        child: Text(font),
                                      ))
                                  .toList(),
                            )
                          : const AppKitProgressCircle(value: null),
                    ),
                    const SizedBox(width: 12),
                    const Text('Size:'),
                    const SizedBox(width: 8),
                    SizedBox(
                      width: 80,
                      child: AppKitTextField(
                        controller: TextEditingController(text: config.pointsize.toString()),
                        placeholder: '22',
                        keyboardType: TextInputType.number,
                        onChanged: (value) {
                          final size = int.tryParse(value);
                          if (size != null) {
                            provider.updateConfig(config.copyWith(pointsize: size));
                          }
                        },
                      ),
                    ),
                    const SizedBox(width: 12),
                    const Text('Background:'),
                    const SizedBox(width: 8),
                    SizedBox(
                      width: 120,
                      child: AppKitTextField(
                        controller: TextEditingController(text: config.annotateBackground),
                        placeholder: '#00000040',
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(annotateBackground: value));
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
  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final config = provider.config;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 12, bottom: 8),
          child: Text('Divider Settings', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ),
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.standardScrollBox,
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
                        const Text('Width:'),
                        const SizedBox(height: 4),
                        AppKitTextField(
                          controller: TextEditingController(text: config.dividerWidth.toString()),
                          placeholder: '3',
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
                        const Text('Color:'),
                        const SizedBox(height: 4),
                        AppKitTextField(
                          controller: TextEditingController(text: config.dividerColor),
                          placeholder: '#FFFFFF',
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
          ),
        ),
      ],
    );
  }
}

class _AdvancedOptionsSection extends StatelessWidget {
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
        AppKitGroupBox(
          style: AppKitGroupBoxStyle.roundedScrollBox,
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
                      AppKitCheckbox(
                        value: config.force,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(force: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('Force re-process'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      AppKitCheckbox(
                        value: config.dryRun,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(dryRun: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('Dry run'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      AppKitCheckbox(
                        value: config.debug,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(debug: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('Debug mode'),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      AppKitCheckbox(
                        value: config.report,
                        onChanged: (value) {
                          provider.updateConfig(config.copyWith(report: value));
                        },
                      ),
                      const SizedBox(width: 8),
                      const Text('Generate report'),
                    ],
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text('Parallel Jobs:'),
                        const SizedBox(height: 4),
                        AppKitTextField(
                          controller: TextEditingController(text: config.jobs == 0 ? 'Auto' : config.jobs.toString()),
                          placeholder: 'Auto',
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
                        const Text('File Extensions:'),
                        const SizedBox(height: 4),
                        AppKitTextField(
                          controller: TextEditingController(text: config.extensions),
                          placeholder: 'jpg,jpeg,png,heic',
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
  @override
  State<_ProcessButtonSection> createState() => _ProcessButtonSectionState();
}

class _ProcessButtonSectionState extends State<_ProcessButtonSection> {
  bool _dialogShown = false;

  void _showProgressDialog(BuildContext context, ProcessingProvider provider) {
    if (_dialogShown) return;
    _dialogShown = true;

    showAppKitDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) => _ProcessingDialog(provider: provider),
    ).then((_) {
      _dialogShown = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();

    // Show dialog when processing starts
    if (provider.isProcessing && !_dialogShown) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _showProgressDialog(context, provider);
      });
    }

    return Center(
      child: AppKitButton(
        size: AppKitControlSize.large,
        onTap: provider.isProcessing
            ? null
            : () {
                provider.startProcessing();
              },
        child: Text(provider.isProcessing ? 'Processing...' : 'Process Images'),
      ),
    );
  }
}

class _ProcessingDialog extends StatelessWidget {
  final ProcessingProvider provider;

  const _ProcessingDialog({required this.provider});

  @override
  Widget build(BuildContext context) {
    return Consumer<ProcessingProvider>(
      builder: (context, provider, child) {
        return AppKitDialog(
          constraints: const BoxConstraints(minWidth: 450, maxWidth: 450),
          title: const Text('Processing Images'),
          message: (context) => Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 8),
              AppKitProgressBar(value: provider.progress),
              const SizedBox(height: 12),
              Text(
                'Processing: ${provider.processedCount}/${provider.totalCount}',
                style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w500),
              ),
              const SizedBox(height: 8),
              Text(
                provider.currentFile.isNotEmpty ? 'Current: ${provider.currentFile}' : 'Initializing...',
                style: TextStyle(fontSize: 11, color: Colors.grey[600]),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              if (provider.errorMessage.isNotEmpty) ...[
                const SizedBox(height: 12),
                Text(
                  provider.errorMessage,
                  style: const TextStyle(color: Colors.red, fontSize: 12),
                ),
              ],
              if (provider.resultsMessage.isNotEmpty) ...[
                const SizedBox(height: 12),
                Text(
                  provider.resultsMessage,
                  style: const TextStyle(color: Colors.green, fontSize: 12),
                ),
              ],
            ],
          ),
          primaryButton: AppKitButton(
            size: AppKitControlSize.large,
            type: AppKitButtonType.primary,
            onTap: !provider.isProcessing
                ? () {
                    Navigator.of(context).pop();
                  }
                : null,
            child: const Text('Close'),
          ),
        );
      },
    );
  }
}

