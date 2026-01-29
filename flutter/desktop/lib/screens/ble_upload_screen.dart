import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../providers/ble_provider.dart';
import '../services/file_picker_history.dart';

class BleUploadScreen extends StatelessWidget {
  const BleUploadScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<BleUploadState>(
      builder: (context, ble, _) {
        final theme = AppKitTheme.of(context);
        final fileLabel = ble.binPath != null ? ble.binPath!.split('/').last : 'No file selected';
        final deviceLabel = ble.deviceInfo?.name ?? ble.connected?.platformName ?? 'No device selected';

        return Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: SingleChildScrollView(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Padding(
                      padding: const EdgeInsets.only(left: 12, bottom: 8),
                      child: Text('Bluetooth Upload', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
                    ),
                    AppKitGroupBox(
                      style: AppKitGroupBoxStyle.roundedScrollBox,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const SizedBox(height: 8),
                          Row(
                            children: [
                              AppKitButton(
                                size: AppKitControlSize.regular,
                                onTap: ble.scanning
                                    ? null
                                    : () async {
                                        try {
                                          final found = await ble.scan(timeout: const Duration(seconds: 6));
                                          if (!found && context.mounted) {
                                            showAppKitDialog(
                                              context: context,
                                              barrierDismissible: true,
                                              builder: (context) => AppKitDialog(
                                                title: const Text('No Devices Found'),
                                                message: (context) => const Text(
                                                  'No compatible devices were found during the scan. Please ensure your device is powered on and in range, then try again.',
                                                ),
                                                primaryButton: AppKitButton(
                                                  size: AppKitControlSize.regular,
                                                  type: AppKitButtonType.primary,
                                                  onTap: () {
                                                    Navigator.of(context).pop();
                                                  },
                                                  child: const Text('Ok'),
                                                ),
                                              ),
                                            );
                                          }
                                        } catch (e) {
                                          if (context.mounted) {
                                            showAppKitDialog(
                                              context: context,
                                              barrierDismissible: true,
                                              builder: (context) => AppKitDialog(
                                                title: const Text('Bluetooth Scan Error'),
                                                message: (context) => SelectableText(e.toString(), style: const TextStyle(fontSize: 13)),
                                                primaryButton: AppKitButton(
                                                  size: AppKitControlSize.regular,
                                                  type: AppKitButtonType.primary,
                                                  onTap: () {
                                                    Navigator.of(context).pop();
                                                  },
                                                  child: const Text('Ok'),
                                                ),
                                              ),
                                            );
                                          }
                                        }
                                      },
                                child: Text(ble.scanning ? 'Scanning…' : 'Scan for Devices'),
                              ),
                              const SizedBox(width: 8),
                              AppKitButton(
                                size: AppKitControlSize.regular,
                                type: AppKitButtonType.secondary,
                                onTap: ble.connected != null ? ble.disconnect : null,
                                child: const Text('Disconnect'),
                              ),
                              const SizedBox(width: 12),
                              Expanded(
                                child: Text(
                                  ble.status.isNotEmpty ? ble.status : 'Select a device and choose a .pfr1 file to upload',
                                  style: TextStyle(fontSize: 12, color: Colors.grey[700]),
                                  maxLines: 2,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 12),
                          _DeviceList(ble: ble),
                          if (ble.deviceInfo != null) ...[const SizedBox(height: 12), _DeviceInfo(ble: ble)],
                          const SizedBox(height: 12),
                          Row(
                            children: [
                              const SizedBox(width: 100, child: Text('Selected File:')),
                              Expanded(
                                child: Text(
                                  fileLabel,
                                  style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w500),
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                              const SizedBox(width: 8),
                              AppKitButton(
                                size: AppKitControlSize.regular,
                                onTap: () async {
                                  final result = await FilePicker.pickFiles(
                                    type: FileType.custom,
                                    allowedExtensions: const ['pfr1'],
                                    initialDirectory: FilePickerHistory.initialDir('bleBin'),
                                  );
                                  if (result != null && result.files.single.path != null) {
                                    final selectedPath = result.files.single.path!;
                                    FilePickerHistory.rememberFile('bleBin', selectedPath);
                                    await ble.pickBin(selectedPath);
                                  }
                                },
                                child: const Text('Choose .pfr1'),
                              ),
                            ],
                          ),
                          const SizedBox(height: 12),
                          Row(
                            children: [
                              const SizedBox(width: 100, child: Text('Rotation:')),
                              AppKitPopupButton<int>(
                                selectedItem: ble.rotation,
                                onItemSelected: (value) {
                                  if (value != null) {
                                    ble.setRotation(value);
                                  }
                                },
                                items: const [
                                  AppKitContextMenuItem(value: 0, child: Text('0° Landscape')),
                                  AppKitContextMenuItem(value: 1, child: Text('90° Portrait')),
                                  AppKitContextMenuItem(value: 2, child: Text('180° Landscape')),
                                  AppKitContextMenuItem(value: 3, child: Text('270° Portrait')),
                                ],
                              ),
                              const SizedBox(width: 12),
                              Text('Device: $deviceLabel', style: TextStyle(fontSize: 12, color: Colors.grey[700])),
                            ],
                          ),
                          const SizedBox(height: 12),

                          if (ble.previewImage != null) ...[
                            AppKitGroupBox(
                              style: AppKitGroupBoxStyle.roundedScrollBox,
                              height: ble.rotation % 2 == 0 ? ble.binHeader!.height.toDouble() : ble.binHeader!.width.toDouble(),
                              width: double.infinity,
                              child: ble.previewImage != null
                                  ? RotatedBox(
                                      quarterTurns: -ble.rotation,
                                      child: RawImage(image: ble.previewImage, fit: BoxFit.contain),
                                    )
                                  : Center(
                                      child: Text(
                                        'Preview will appear after selecting a .bin file (uses device dimensions ${ble.deviceInfo?.width ?? 800}x${ble.deviceInfo?.height ?? 480}).',
                                        textAlign: TextAlign.center,
                                        style: TextStyle(fontSize: 12, color: Colors.grey[700]),
                                      ),
                                    ),
                            ),
                            const SizedBox(height: 12),
                            _HeaderCard(ble: ble),
                          ] else ...[
                            AppKitGroupBox(
                              style: AppKitGroupBoxStyle.roundedScrollBox,
                              height: 480,
                              width: double.infinity,
                              child: Center(
                                child: Text(
                                  'Preview will appear after selecting a .bin file (uses device dimensions ${ble.deviceInfo?.width ?? 800}x${ble.deviceInfo?.height ?? 480}).',
                                  textAlign: TextAlign.center,
                                  style: TextStyle(fontSize: 12, color: Colors.grey[700]),
                                ),
                              ),
                            ),
                          ],
                          const SizedBox(height: 12),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
            if (ble.error != null)
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: Text(ble.error!, style: const TextStyle(color: Colors.red, fontSize: 12)),
              ),
            Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                SizedBox(
                  height: 40,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.center,
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      if (ble.uploading) ...[AppKitProgressBar(value: ble.progress)] else ...[const SizedBox.shrink()],
                    ],
                  ),
                ),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Spacer(flex: 1),
                    AppKitButton(
                      type: AppKitButtonType.primary,
                      size: AppKitControlSize.large,
                      onTap: ble.canUpload && !ble.uploading
                          ? () async {
                              final errorMsg = await ble.upload();
                              if (errorMsg != null && context.mounted) {
                                showAppKitDialog(
                                  context: context,
                                  barrierDismissible: true,
                                  builder: (context) => AppKitDialog(
                                    title: const Text('Errore Upload'),
                                    message: (context) => SelectableText(errorMsg, style: const TextStyle(fontSize: 13)),
                                    primaryButton: AppKitButton(
                                      size: AppKitControlSize.regular,
                                      type: AppKitButtonType.primary,
                                      onTap: () {
                                        Navigator.of(context).pop();
                                      },
                                      child: const Text('Ok'),
                                    ),
                                  ),
                                );
                              }
                            }
                          : null,
                      child: Text(ble.uploading ? 'Uploading…' : 'Upload to Device'),
                    ),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 12),
          ],
        );
      },
    );
  }
}

class _DeviceList extends StatelessWidget {
  final BleUploadState ble;

  const _DeviceList({required this.ble});

  @override
  Widget build(BuildContext context) {
    final theme = AppKitTheme.of(context);
    if (ble.devices.isEmpty) {
      return AppKitGroupBox(
        style: AppKitGroupBoxStyle.roundedScrollBox,
        width: double.infinity,
        padding: const EdgeInsets.all(12),
        child: Row(
          children: [
            if (ble.scanning) ...[const AppKitProgressCircle(size: 16), const SizedBox(width: 8, height: 16)],
            const SizedBox(width: 0, height: 16),
            const Text('No devices found yet. Tap Scan to refresh.', style: TextStyle(fontSize: 12)),
          ],
        ),
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: ble.devices.map((result) {
        final name = result.device.platformName.isNotEmpty ? result.device.platformName : result.device.remoteId.str;
        final isConnected = ble.connected?.remoteId == result.device.remoteId;
        return Container(
          margin: const EdgeInsets.only(bottom: 6),
          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
          decoration: BoxDecoration(
            color: isConnected ? AppKitColors.systemGreen.resolveWithContext(context).withAlpha(10) : null,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: isConnected ? AppKitColors.appleGreen.withAlpha(80) : AppKitColors.windowFrameColor),
          ),
          child: Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(name, style: theme.typography.body.copyWith(fontWeight: FontWeight.w600)),
                    Text(result.device.remoteId.str, style: theme.typography.caption1),
                  ],
                ),
              ),
              Text('RSSI ${result.rssi}', style: const TextStyle(fontSize: 12)),
              const SizedBox(width: 8),
              AppKitButton(
                size: AppKitControlSize.small,
                onTap: (ble.connecting || ble.connected != null) ? null : () => ble.selectDevice(result),
                child: Text(isConnected ? 'Connected' : 'Connect'),
              ),
            ],
          ),
        );
      }).toList(),
    );
  }
}

class _HeaderCard extends StatelessWidget {
  final BleUploadState ble;
  const _HeaderCard({required this.ble});

  @override
  Widget build(BuildContext context) {
    final h = ble.binHeader;
    final theme = AppKitTheme.of(context);

    return AppKitGroupBox(
      style: AppKitGroupBoxStyle.roundedScrollBox,
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: const [
              Icon(Icons.description_outlined, size: 18),
              SizedBox(width: 6),
              Text('Header', style: TextStyle(fontWeight: FontWeight.w600)),
            ],
          ),
          const SizedBox(height: 8),
          if (h == null)
            const Text('Select a .bin file to see header details.', style: TextStyle(fontSize: 12))
          else ...[
            _row('Magic', 'PFR1 (0x50465231)'),
            _row('Version', h.version.toString()),
            _row('Header len', '${h.headerLen} bytes'),
            _row('Size', '${h.width} x ${h.height}'),
            _row('Rotation', _rotationLabel(h.rotation)),
            _row('Color mode', _colorModeLabel(h.colorMode)),
            _row('Image size', '${h.payloadLen} bytes'),
            _row('Header CRC32', '0x${h.headerCrc32.toRadixString(16).padLeft(8, '0')}'),
            _row('Total file size', '${h.headerLen + h.payloadLen + 4} bytes'),
          ],
        ],
      ),
    );
  }

  static Widget _row(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontSize: 12, color: Colors.grey)),
          Text(value, style: const TextStyle(fontSize: 12)),
        ],
      ),
    );
  }

  static String _rotationLabel(int v) {
    switch (v % 4) {
      case 0:
        return '0° (landscape)';
      case 1:
        return '90° (portrait)';
      case 2:
        return '180°';
      case 3:
        return '270°';
      default:
        return '$v';
    }
  }

  static String _colorModeLabel(int v) {
    switch (v) {
      case 0:
        return 'Black & White';
      case 1:
        return 'Six Colors';
      default:
        return 'Unknown ($v)';
    }
  }
}

class _DeviceInfo extends StatelessWidget {
  final BleUploadState ble;

  const _DeviceInfo({required this.ble});

  @override
  Widget build(BuildContext context) {
    final info = ble.deviceInfo!;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.blue.withAlpha(10),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.blue.withAlpha(60)),
      ),
      child: Wrap(
        spacing: 12,
        runSpacing: 8,
        children: [
          _InfoChip(label: 'Display', value: info.displayType == 1 ? '6-color' : 'B/W'),
          _InfoChip(label: 'Resolution', value: '${info.width}x${info.height}'),
          _InfoChip(label: 'Rotation', value: _rotationName(info.rotation)),
          _InfoChip(label: 'MTU', value: info.mtu.toString()),
        ],
      ),
    );
  }

  String _rotationName(int r) {
    switch (r) {
      case 1:
        return '90°';
      case 2:
        return '180°';
      case 3:
        return '270°';
      default:
        return '0°';
    }
  }
}

class _InfoChip extends StatelessWidget {
  final String label;
  final String value;

  const _InfoChip({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    final theme = AppKitTheme.of(context);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: theme.controlBackgroundColor,
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: AppKitColors.windowFrameColor),
        boxShadow: [BoxShadow(color: AppKitColors.shadowColor.withAlpha(8), blurRadius: 4, offset: const Offset(0, 2))],
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
            label,
            style: theme.typography.callout.copyWith(color: theme.typography.callout.color?.withAlpha(127), fontWeight: FontWeight.w500),
          ),
          const SizedBox(width: 6),
          Text(value, style: theme.typography.callout.copyWith(fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }
}
