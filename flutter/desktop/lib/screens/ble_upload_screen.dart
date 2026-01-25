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
        final fileLabel = ble.binPath != null
            ? ble.binPath!.split('/').last
            : 'No file selected';
        final deviceLabel =
            ble.deviceInfo?.name ??
            ble.connected?.platformName ??
            'No device selected';

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
                      child: Text(
                        'Bluetooth Upload',
                        style: TextStyle(
                          fontSize: 13,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
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
                                          final found = await ble.scan(
                                            timeout: const Duration(seconds: 6),
                                          );
                                          if (!found && context.mounted) {
                                            showAppKitDialog(
                                              context: context,
                                              barrierDismissible: true,
                                              builder: (context) => AppKitDialog(
                                                title: const Text(
                                                  'Nessun device trovato',
                                                ),
                                                message: (context) => const Text(
                                                  'Non sono stati rilevati frame Bluetooth. Assicurati che il frame sia acceso e in pairing, poi riprova.',
                                                ),
                                                primaryButton: AppKitButton(
                                                  size:
                                                      AppKitControlSize.regular,
                                                  type:
                                                      AppKitButtonType.primary,
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
                                              builder: (context) =>
                                                  AppKitDialog(
                                                    title: const Text(
                                                      'Errore Bluetooth',
                                                    ),
                                                    message: (context) =>
                                                        SelectableText(
                                                          e.toString(),
                                                          style:
                                                              const TextStyle(
                                                                fontSize: 13,
                                                              ),
                                                        ),
                                                    primaryButton: AppKitButton(
                                                      size: AppKitControlSize
                                                          .regular,
                                                      type: AppKitButtonType
                                                          .primary,
                                                      onTap: () {
                                                        Navigator.of(
                                                          context,
                                                        ).pop();
                                                      },
                                                      child: const Text('Ok'),
                                                    ),
                                                  ),
                                            );
                                          }
                                        }
                                      },
                                child: Text(
                                  ble.scanning
                                      ? 'Scanning…'
                                      : 'Scan for Devices',
                                ),
                              ),
                              const SizedBox(width: 8),
                              AppKitButton(
                                size: AppKitControlSize.regular,
                                type: AppKitButtonType.secondary,
                                onTap: ble.connected != null
                                    ? ble.disconnect
                                    : null,
                                child: const Text('Disconnect'),
                              ),
                              const SizedBox(width: 12),
                              Expanded(
                                child: Text(
                                  ble.status.isNotEmpty
                                      ? ble.status
                                      : 'Select a device and choose a .bin file to upload',
                                  style: TextStyle(
                                    fontSize: 12,
                                    color: Colors.grey[700],
                                  ),
                                  maxLines: 2,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 12),
                          _DeviceList(ble: ble),
                          if (ble.deviceInfo != null) ...[
                            const SizedBox(height: 12),
                            _DeviceInfo(ble: ble),
                          ],
                          const SizedBox(height: 12),
                          Row(
                            children: [
                              const SizedBox(
                                width: 100,
                                child: Text('Selected File:'),
                              ),
                              Expanded(
                                child: Text(
                                  fileLabel,
                                  style: const TextStyle(
                                    fontSize: 13,
                                    fontWeight: FontWeight.w500,
                                  ),
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                              const SizedBox(width: 8),
                              AppKitButton(
                                size: AppKitControlSize.regular,
                                onTap: () async {
                                  final result = await FilePicker.platform
                                      .pickFiles(
                                        type: FileType.custom,
                                        allowedExtensions: const ['bin'],
                                        initialDirectory:
                                            FilePickerHistory.initialDir(
                                              'bleBin',
                                            ),
                                      );
                                  if (result != null &&
                                      result.files.single.path != null) {
                                    final selectedPath =
                                        result.files.single.path!;
                                    FilePickerHistory.rememberFile(
                                      'bleBin',
                                      selectedPath,
                                    );
                                    await ble.pickBin(selectedPath);
                                  }
                                },
                                child: const Text('Choose .bin'),
                              ),
                            ],
                          ),
                          const SizedBox(height: 12),
                          Row(
                            children: [
                              const SizedBox(
                                width: 100,
                                child: Text('Rotation:'),
                              ),
                              AppKitPopupButton<int>(
                                selectedItem: ble.rotation,
                                onItemSelected: (value) {
                                  if (value != null) {
                                    ble.setRotation(value);
                                  }
                                },
                                items: const [
                                  AppKitContextMenuItem(
                                    value: 0,
                                    child: Text('0° Landscape'),
                                  ),
                                  AppKitContextMenuItem(
                                    value: 1,
                                    child: Text('90° Portrait'),
                                  ),
                                  AppKitContextMenuItem(
                                    value: 2,
                                    child: Text('180° Landscape'),
                                  ),
                                  AppKitContextMenuItem(
                                    value: 3,
                                    child: Text('270° Portrait'),
                                  ),
                                ],
                              ),
                              const SizedBox(width: 12),
                              Text(
                                'Device: $deviceLabel',
                                style: TextStyle(
                                  fontSize: 12,
                                  color: Colors.grey[700],
                                ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 12),
                          Container(
                            height: ble.rotation % 2 == 0 ? 480 : 800,
                            width: double.infinity,
                            decoration: BoxDecoration(
                              color: Colors.grey[100],
                              borderRadius: BorderRadius.circular(8),
                              border: Border.all(color: Colors.grey[300]!),
                            ),
                            child: ble.previewImage != null
                                ? RotatedBox(
                                    quarterTurns: -ble.rotation,
                                    child: RawImage(
                                      image: ble.previewImage,
                                      fit: BoxFit.contain,
                                    ),
                                  )
                                : Center(
                                    child: Text(
                                      'Preview will appear after selecting a .bin file (uses device dimensions ${ble.deviceInfo?.width ?? 800}x${ble.deviceInfo?.height ?? 480}).',
                                      textAlign: TextAlign.center,
                                      style: TextStyle(
                                        fontSize: 12,
                                        color: Colors.grey[700],
                                      ),
                                    ),
                                  ),
                          ),
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
                child: Text(
                  ble.error!,
                  style: const TextStyle(color: Colors.red, fontSize: 12),
                ),
              ),
            Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                SizedBox(
                  height: 20,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      if (ble.uploading || ble.progress > 0) ...[
                        AppKitProgressBar(value: ble.progress),
                        const SizedBox(height: 6),
                        Text(
                          'Upload progress: ${(ble.progress * 100).toStringAsFixed(0)}%',
                          style: const TextStyle(fontSize: 12),
                        ),
                      ],
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
                                    message: (context) => SelectableText(
                                      errorMsg,
                                      style: const TextStyle(fontSize: 13),
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
                            }
                          : null,
                      child: Text(
                        ble.uploading ? 'Uploading…' : 'Upload to Device',
                      ),
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
    if (ble.devices.isEmpty) {
      return Container(
        width: double.infinity,
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: Colors.grey[100],
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: Colors.grey[300]!),
        ),
        child: const Text(
          'No devices found yet. Tap Scan to refresh.',
          style: TextStyle(fontSize: 12),
        ),
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: ble.devices.map((result) {
        final name = result.device.platformName.isNotEmpty
            ? result.device.platformName
            : result.device.remoteId.str;
        final isConnected = ble.connected?.remoteId == result.device.remoteId;
        return Container(
          margin: const EdgeInsets.only(bottom: 6),
          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
          decoration: BoxDecoration(
            color: isConnected ? Colors.green.withAlpha(13) : Colors.grey[50],
            borderRadius: BorderRadius.circular(8),
            border: Border.all(
              color: isConnected
                  ? Colors.green.withAlpha(80)
                  : Colors.grey[300]!,
            ),
          ),
          child: Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      name,
                      style: const TextStyle(
                        fontSize: 13,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    Text(
                      result.device.remoteId.str,
                      style: TextStyle(fontSize: 11, color: Colors.grey[700]),
                    ),
                  ],
                ),
              ),
              Text('RSSI ${result.rssi}', style: const TextStyle(fontSize: 12)),
              const SizedBox(width: 8),
              AppKitButton(
                size: AppKitControlSize.small,
                onTap: ble.connecting ? null : () => ble.selectDevice(result),
                child: Text(isConnected ? 'Connected' : 'Connect'),
              ),
            ],
          ),
        );
      }).toList(),
    );
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
          _InfoChip(
            label: 'Display',
            value: info.displayType == 1 ? '6-color' : 'B/W',
          ),
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
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: Colors.grey[300]!),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withAlpha(8),
            blurRadius: 4,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(label, style: TextStyle(fontSize: 12, color: Colors.grey[700])),
          const SizedBox(width: 6),
          Text(
            value,
            style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
          ),
        ],
      ),
    );
  }
}
