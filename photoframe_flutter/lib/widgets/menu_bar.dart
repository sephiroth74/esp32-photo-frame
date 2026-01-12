import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:file_picker/file_picker.dart';
import '../providers/processing_provider.dart';

class AppMenuBar extends StatelessWidget {
  final Widget child;

  const AppMenuBar({super.key, required this.child});

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();

    return PlatformMenuBar(
      menus: [
        PlatformMenu(
          label: 'File',
          menus: [
            PlatformMenuItem(
              label: 'New Configuration',
              shortcut: const SingleActivator(LogicalKeyboardKey.keyN, meta: true),
              onSelected: () {
                _handleNewConfiguration(context);
              },
            ),
            PlatformMenuItem(
              label: 'Open Configuration...',
              shortcut: const SingleActivator(LogicalKeyboardKey.keyO, meta: true),
              onSelected: () {
                _handleOpenConfiguration(context);
              },
            ),
            if (provider.recentFiles.isNotEmpty) ...[
              PlatformMenu(
                label: 'Open Recent',
                menus: [
                  ...provider.recentFiles.map((recentFile) => PlatformMenuItem(
                        label: recentFile.name,
                        onSelected: () {
                          provider.loadRecentFile(recentFile);
                        },
                      )),
                  PlatformMenuItem(
                    label: 'Clear Recent',
                    onSelected: () {
                      provider.clearRecentFiles();
                    },
                  ),
                ],
              ),
            ],
            PlatformMenuItem(
              label: 'Save Configuration',
              shortcut: const SingleActivator(LogicalKeyboardKey.keyS, meta: true),
              onSelected: provider.currentProfilePath != null
                  ? () {
                      _handleSaveConfiguration(context);
                    }
                  : null,
            ),
            PlatformMenuItem(
              label: 'Save Configuration As...',
              shortcut: const SingleActivator(LogicalKeyboardKey.keyS,
                  meta: true, shift: true),
              onSelected: () {
                _handleSaveConfigurationAs(context);
              },
            ),
            PlatformMenuItem(
              label: 'Export Configuration...',
              onSelected: () {
                _handleExportConfiguration(context);
              },
            ),
          ],
        ),
        PlatformMenu(
          label: 'Edit',
          menus: [
            PlatformMenuItem(
              label: 'Select Input Directory...',
              onSelected: () async {
                final provider = context.read<ProcessingProvider>();
                final path = await FilePicker.platform.getDirectoryPath();
                if (path != null) {
                  provider.updateConfig(provider.config.copyWith(inputPath: path));
                }
              },
            ),
            PlatformMenuItem(
              label: 'Select Output Directory...',
              onSelected: () async {
                final provider = context.read<ProcessingProvider>();
                final path = await FilePicker.platform.getDirectoryPath();
                if (path != null) {
                  provider.updateConfig(provider.config.copyWith(outputPath: path));
                }
              },
            ),
            PlatformMenuItem(
              label: 'Select Processor Binary...',
              onSelected: () async {
                final provider = context.read<ProcessingProvider>();
                final result = await FilePicker.platform.pickFiles(
                  type: FileType.custom,
                  allowedExtensions: [''],
                  dialogTitle: 'Select photoframe-processor binary',
                );
                if (result != null && result.files.single.path != null) {
                  provider.updateConfig(
                    provider.config.copyWith(
                      processorBinaryPath: result.files.single.path,
                    ),
                  );
                }
              },
            ),
          ],
        ),
        PlatformMenu(
          label: 'Process',
          menus: [
            PlatformMenuItem(
              label: 'Start Processing',
              shortcut: const SingleActivator(LogicalKeyboardKey.keyR, meta: true),
              onSelected: !provider.isProcessing
                  ? () {
                      provider.startProcessing();
                    }
                  : null,
            ),
          ],
        ),
      ],
      child: child,
    );
  }

  Future<void> _handleNewConfiguration(BuildContext context) async {
    final provider = context.read<ProcessingProvider>();

    if (provider.hasUnsavedChanges) {
      final shouldContinue = await _showUnsavedChangesDialog(context);
      if (!shouldContinue) return;
    }

    provider.newProfile();
  }

  Future<void> _handleOpenConfiguration(BuildContext context) async {
    final provider = context.read<ProcessingProvider>();

    if (provider.hasUnsavedChanges) {
      final shouldContinue = await _showUnsavedChangesDialog(context);
      if (!shouldContinue) return;
    }

    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['pfconfig'],
      dialogTitle: 'Open Configuration',
    );

    if (result != null && result.files.single.path != null) {
      await provider.loadProfile(result.files.single.path!);
    }
  }

  Future<void> _handleSaveConfiguration(BuildContext context) async {
    final provider = context.read<ProcessingProvider>();

    if (provider.currentProfilePath != null) {
      await provider.saveProfile(filePath: provider.currentProfilePath);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Configuration saved')),
        );
      }
    }
  }

  Future<void> _handleSaveConfigurationAs(BuildContext context) async {
    final provider = context.read<ProcessingProvider>();

    final result = await FilePicker.platform.saveFile(
      dialogTitle: 'Save Configuration As',
      fileName: '${provider.currentProfileName ?? 'config'}.pfconfig',
      type: FileType.custom,
      allowedExtensions: ['pfconfig'],
    );

    if (result != null) {
      await provider.saveProfile(filePath: result);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Configuration saved')),
        );
      }
    }
  }

  Future<void> _handleExportConfiguration(BuildContext context) async {
    final provider = context.read<ProcessingProvider>();

    final result = await FilePicker.platform.saveFile(
      dialogTitle: 'Export Configuration',
      fileName: '${provider.currentProfileName ?? 'config'}.pfconfig',
      type: FileType.custom,
      allowedExtensions: ['pfconfig'],
    );

    if (result != null) {
      await provider.saveProfile(filePath: result);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Configuration exported')),
        );
      }
    }
  }

  Future<bool> _showUnsavedChangesDialog(BuildContext context) async {
    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Unsaved Changes'),
        content: const Text(
          'You have unsaved changes. Do you want to discard them?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Discard'),
          ),
        ],
      ),
    );

    return result ?? false;
  }
}