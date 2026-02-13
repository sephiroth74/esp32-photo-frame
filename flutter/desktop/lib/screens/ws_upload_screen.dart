import 'package:desktop_drop/desktop_drop.dart';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:photoframe_flutter/core/services/preferences.dart';
import 'package:photoframe_flutter/presentation/widget_factory.dart';
import 'package:provider/provider.dart';

import '../core/providers/widget_factory_provider.dart';
import '../core/providers/ws_provider.dart';
import '../core/services/file_picker_history.dart';
import '../presentation/abstractions/widget_abstractions.dart';

class WsUploadScreen extends StatefulWidget {
  const WsUploadScreen({super.key});

  @override
  State<WsUploadScreen> createState() => _WsUploadScreenState();
}

class _WsUploadScreenState extends State<WsUploadScreen> {
  final TextEditingController _ipController = TextEditingController();
  final TextEditingController _portController = TextEditingController();
  static const String _ipHistoryKey = 'ws_ip_address';
  static const String _portHistoryKey = 'ws_port';
  bool _dragging = false;
  String? _lastErrorShown;
  String? _lastSuccessShown;

  @override
  void initState() {
    super.initState();
    _loadSavedSettings();
  }

  Future<void> _loadSavedSettings() async {
    await Preferences.initialize();
    if (!mounted) return;
    final ws = context.read<WsUploadState>();
    final savedIp = Preferences.getString(_ipHistoryKey);
    final savedPort = Preferences.getString(_portHistoryKey);

    if (savedIp != null) {
      _ipController.text = savedIp;
      ws.setIpAddress(savedIp);
    } else {
      _ipController.text = ws.ipAddress;
    }

    if (savedPort != null) {
      _portController.text = savedPort;
      ws.setPort(savedPort);
    } else {
      _portController.text = ws.port;
    }
  }

  @override
  void dispose() {
    _ipController.dispose();
    _portController.dispose();
    super.dispose();
  }

  Future<void> _openDialog<T>({required Widget Function(BuildContext context, WidgetFactory factory) builder, bool barrierDismissible = true}) async {
    if (!mounted) return;
    final factory = context.read<WidgetFactoryProvider>().factory;
    await factory.openDialog<T>(context: context, builder: (ctx, f) => builder(ctx, f), barrierDismissible: barrierDismissible);
  }

  Future<void> _pickBinFile(WsUploadState ws) async {
    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['pfr1'],
      dialogTitle: 'Select BIN file',
      initialDirectory: FilePickerHistory.initialDir('bin_file'),
    );
    if (result != null && result.files.single.path != null) {
      FilePickerHistory.rememberFile('bin_file', result.files.single.path!);
      await ws.selectBinFile(result.files.single.path);
    }
  }

  Widget _buildButton(
    BuildContext context, {
    required String label,
    required VoidCallback? onPressed,
    IconData? icon,
    PlatformButtonSize size = PlatformButtonSize.medium,
    PlatformButtonStyle style = PlatformButtonStyle.primary,
  }) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.button(label: label, onPressed: onPressed, icon: icon != null ? Icon(icon) : null, size: size, style: style);
  }

  Widget _buildCircularProgress(double? value, double? size) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.circularProgress(value: value, size: size);
  }

  Widget _buildLinearProgress(BuildContext context, double? value) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.progress(value: value);
  }

  Widget _buildGroupBox(BuildContext context, {required Widget child}) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.groupBox(child: child);
  }

  Widget _buildTextField(
    BuildContext context, {
    required TextEditingController controller,
    required String placeholder,
    required ValueChanged<String> onChanged,
    TextInputType keyboardType = TextInputType.text,
  }) {
    final factory = context.read<WidgetFactoryProvider>().factory;
    return factory.textField(controller: controller, placeholder: placeholder, keyboardType: keyboardType, onChanged: onChanged);
  }

  Widget _buildConfigRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(label, style: TextStyle(fontSize: 11, fontWeight: FontWeight.w500)),
          const SizedBox(width: 8),
          Flexible(
            child: Text(
              value,
              style: TextStyle(fontSize: 11, color: Colors.grey[600]),
              overflow: TextOverflow.ellipsis,
              maxLines: 1,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildDimensionComparison(WsUploadState ws) {
    final imageWidth = ws.binHeader!.getWidth();
    final imageHeight = ws.binHeader!.getHeight();
    final displayWidth = ws.boardConfig!.displayWidth;
    final displayHeight = ws.boardConfig!.displayHeight;

    final matches = (imageWidth == displayWidth && imageHeight == displayHeight) || (imageWidth == displayHeight && imageHeight == displayWidth);
    final statusColor = matches ? Colors.green : Colors.orange;
    final statusIcon = matches ? Icons.check_circle : Icons.warning;
    final statusText = matches
        ? 'Image dimensions match display'
        : 'Image dimensions ($imageWidth×$imageHeight) differ from display ($displayWidth×$displayHeight)';

    return Container(
      padding: const EdgeInsets.all(8),
      decoration: BoxDecoration(
        color: statusColor.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(4),
        border: Border.all(color: statusColor.withValues(alpha: 0.3)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(statusIcon, color: statusColor, size: 14),
          const SizedBox(width: 6),
          Flexible(
            child: Text(statusText, style: TextStyle(fontSize: 10, color: statusColor)),
          ),
        ],
      ),
    );
  }

  void _showUploadError(BuildContext context, String message) {
    _openDialog<void>(
      builder: (ctx, factory) {
        return factory.dialog(
          title: 'Error',
          icon: Icon(Icons.error_outline, color: Colors.red.shade500, size: 64),
          message: message,
          actions: [PlatformDialogAction(label: 'OK', onPressed: () => Navigator.of(ctx).pop(), style: PlatformDialogActionStyle.primary)],
        );
      },
    );
  }

  void _showUploadSuccess(BuildContext context, String message) {
    _openDialog<void>(
      builder: (ctx, factory) {
        return factory.dialog(
          icon: Icon(Icons.check, color: Colors.green.shade500, size: 64),
          title: 'Upload completed',
          message: message,
          actions: [PlatformDialogAction(label: 'OK', onPressed: () => Navigator.of(ctx).pop(), style: PlatformDialogActionStyle.primary)],
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<WsUploadState>(
      builder: (context, ws, _) {
        if (ws.uploading) {
          _lastSuccessShown = null;
        }
        // Show error alert if there's an error (either during or after upload)
        if (ws.error != null && ws.error != _lastErrorShown) {
          _lastErrorShown = ws.error;
          WidgetsBinding.instance.addPostFrameCallback((_) {
            if (!mounted) return;
            _showUploadError(context, ws.error!);
          });
        }
        final uploadSucceeded = !ws.uploading && ws.error == null && ws.progress >= 1.0 && ws.status.startsWith('Upload completed');
        if (uploadSucceeded && ws.status != _lastSuccessShown) {
          _lastSuccessShown = ws.status;
          WidgetsBinding.instance.addPostFrameCallback((_) {
            if (!mounted) return;
            _showUploadSuccess(context, ws.status);
          });
        }
        final fileLabel = ws.binPath != null ? ws.binPath!.split('/').last : 'No file selected';

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
                      child: Text('Upload to Device via WebSocket', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
                    ),
                    _buildGroupBox(
                      context,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('Connection.', style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                          const SizedBox(height: 4),
                          const Text(
                            'Enter the IP address and port of the device you want to connect to. Remember to connect to the device\'s own WiFi network first.',
                            style: TextStyle(fontSize: 14),
                          ),
                          const SizedBox(height: 12),

                          // IP Address Field
                          Row(
                            mainAxisAlignment: MainAxisAlignment.center,
                            crossAxisAlignment: CrossAxisAlignment.center,
                            mainAxisSize: MainAxisSize.max,
                            children: [
                              Flexible(
                                flex: 0,
                                child: Row(
                                  children: [
                                    Text('IP Address:', style: TextStyle(fontSize: 12)),
                                    const SizedBox(width: 8),
                                    SizedBox(
                                      width: 120,
                                      child: _buildTextField(
                                        context,
                                        controller: _ipController,
                                        placeholder: '192.168.4.1',
                                        keyboardType: TextInputType.number,
                                        onChanged: (value) {
                                          ws.setIpAddress(value);
                                          Preferences.setString(_ipHistoryKey, value);
                                        },
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              const SizedBox(width: 12),
                              // Port Field
                              Row(
                                children: [
                                  Text('Port:', style: TextStyle(fontSize: 12)),
                                  const SizedBox(width: 8),
                                  SizedBox(
                                    width: 80,
                                    child: _buildTextField(
                                      context,
                                      controller: _portController,
                                      placeholder: '81',
                                      keyboardType: TextInputType.number,
                                      onChanged: (value) {
                                        ws.setPort(value);
                                        Preferences.setString(_portHistoryKey, value);
                                      },
                                    ),
                                  ),
                                ],
                              ),
                              const SizedBox(width: 12),
                              // Connect Button
                              Row(
                                children: [
                                  _buildButton(
                                    context,
                                    label: ws.connected ? 'Disconnect' : 'Connect',
                                    onPressed: ws.connecting || ws.uploading
                                        ? null
                                        : () async {
                                            if (ws.connected) {
                                              await ws.disconnect();
                                            } else {
                                              await ws.connect();
                                            }
                                          },
                                    style: ws.connected ? PlatformButtonStyle.secondary : PlatformButtonStyle.primary,
                                  ),
                                  const SizedBox(width: 8),
                                  _buildButton(
                                    context,
                                    label: 'Shutdown',
                                    onPressed: ws.connected && !ws.uploading ? () async => await ws.shutdown() : null,
                                    style: PlatformButtonStyle.danger,
                                  ),
                                  if (ws.connecting) ...[
                                    const SizedBox(width: 8),
                                    SizedBox(width: 24, height: 24, child: _buildCircularProgress(null, 20)),
                                  ],
                                ],
                              ),
                            ],
                          ),

                          const SizedBox(height: 16),

                          if (ws.connected) ...[
                            Row(
                              children: [
                                const SizedBox(width: 8),
                                const Icon(Icons.check_circle, color: Colors.green, size: 16),
                                const SizedBox(width: 4),
                                Text('Connected', style: TextStyle(fontSize: 11, color: Colors.green)),
                              ],
                            ),
                          ],

                          // Status Message
                          if (ws.status.isNotEmpty || ws.error != null) ...[
                            const SizedBox(height: 12),

                            DecoratedBox(
                              decoration: BoxDecoration(
                                color: ws.error != null ? Colors.red.withValues(alpha: 0.1) : Colors.blue.withValues(alpha: 0.08),
                                borderRadius: BorderRadius.circular(4),
                              ),
                              child: Padding(
                                padding: const EdgeInsets.all(8),
                                child: Row(
                                  children: [
                                    if (ws.error != null) ...[const Icon(Icons.error_outline, color: Colors.red, size: 16)],
                                    const SizedBox(width: 8),
                                    Flexible(
                                      flex: 0,
                                      child: ws.error == null
                                          ? Text(
                                              ws.status,
                                              style: TextStyle(fontSize: 11, color: Colors.grey[600]),
                                              maxLines: 5,
                                              overflow: TextOverflow.ellipsis,
                                            )
                                          : Text(
                                              ws.error!,
                                              style: TextStyle(fontSize: 11, color: Colors.red),
                                              maxLines: 5,
                                              overflow: TextOverflow.ellipsis,
                                            ),
                                    ),
                                  ],
                                ),
                              ),
                            ),
                          ],
                        ],
                      ),
                    ),

                    // Board Configuration Info
                    const SizedBox(height: 16),

                    // File Selection Section
                    _buildGroupBox(
                      context,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        mainAxisAlignment: MainAxisAlignment.start,
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Row(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            mainAxisAlignment: MainAxisAlignment.start,
                            children: [
                              Flexible(
                                flex: 0,
                                child: Column(
                                  mainAxisSize: MainAxisSize.max,
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  mainAxisAlignment: MainAxisAlignment.start,
                                  children: [
                                    SizedBox(
                                      width: 150,
                                      child: const Text('File Selection', style: TextStyle(fontSize: 16, fontWeight: FontWeight.w500)),
                                    ),
                                    const SizedBox(height: 12, width: 12),
                                    Row(
                                      children: [
                                        _buildButton(
                                          context,
                                          label: 'Pick File',
                                          style: PlatformButtonStyle.primary,
                                          size: PlatformButtonSize.large,
                                          onPressed: ws.uploading ? null : () async => _pickBinFile(ws),
                                        ),
                                        const SizedBox(height: 12, width: 8),
                                        _buildButton(
                                          context,
                                          label: 'Rotate',
                                          style: PlatformButtonStyle.secondary,
                                          size: PlatformButtonSize.large,
                                          onPressed: !ws.uploading && ws.binHeader != null
                                              ? () {
                                                  ws.setRotation((ws.rotation + 1) % 4);
                                                }
                                              : null,
                                        ),
                                      ],
                                    ),
                                    if (ws.binHeader != null) ...[
                                      const SizedBox(width: 12, height: 12),
                                      SizedBox(
                                        width: 300,
                                        child: _buildGroupBox(
                                          context,
                                          child: Column(
                                            mainAxisSize: MainAxisSize.min,
                                            mainAxisAlignment: MainAxisAlignment.start,
                                            crossAxisAlignment: CrossAxisAlignment.start,
                                            children: [
                                              _buildConfigRow('Rotation', ws.binHeader!.orientation.toReadableString()),
                                              _buildConfigRow('Size', '${ws.binHeader!.getWidth()} x ${ws.binHeader!.getHeight()}'),
                                              _buildConfigRow('Color Mode', ws.binHeader!.colorMode.toJsonValue()),
                                              _buildConfigRow('Name', fileLabel),
                                            ],
                                          ),
                                        ),
                                      ),
                                    ],
                                    if (ws.boardConfig != null) ...[
                                      const SizedBox(height: 12),
                                      SizedBox(
                                        width: 300,
                                        child: _buildGroupBox(
                                          context,
                                          child: Column(
                                            crossAxisAlignment: CrossAxisAlignment.start,
                                            mainAxisSize: MainAxisSize.min,
                                            children: [
                                              const Text('Board Configuration', style: TextStyle(fontSize: 12, fontWeight: FontWeight.w500)),
                                              const SizedBox(height: 2),
                                              _buildConfigRow(
                                                'Display Size:',
                                                '${ws.boardConfig!.displayWidth} × ${ws.boardConfig!.displayHeight} px',
                                              ),
                                              if (ws.boardConfig!.flashSize > 0)
                                                _buildConfigRow('Flash Size:', '${(ws.boardConfig!.flashSize / 1024 / 1024).toStringAsFixed(1)} MB'),
                                              if (ws.boardConfig!.batteryVoltageMv != null && ws.boardConfig!.batteryVoltageMv! > 0)
                                                _buildConfigRow(
                                                  'Battery:',
                                                  '${ws.boardConfig!.batteryLevel}% (${ws.boardConfig!.batteryVoltageMv} mV)',
                                                ),
                                              if (ws.binHeader != null) ...[const SizedBox(height: 8), _buildDimensionComparison(ws)],
                                            ],
                                          ),
                                        ),
                                      ),
                                    ],
                                  ],
                                ),
                              ),
                              const SizedBox(width: 24),
                              Flexible(
                                flex: 1,
                                child: Row(
                                  mainAxisSize: MainAxisSize.max,
                                  children: [
                                    Expanded(
                                      child: DropTarget(
                                        enable: true,
                                        onDragEntered: (_) => setState(() => _dragging = true),
                                        onDragUpdated: (_) => setState(() => _dragging = true),
                                        onDragExited: (_) => setState(() => _dragging = false),
                                        onDragDone: (detail) async {
                                          setState(() => _dragging = false);
                                          if (detail.files.isNotEmpty) {
                                            await ws.selectBinFile(detail.files.first.path);
                                          }
                                        },
                                        child: GestureDetector(
                                          onTap: () async => _pickBinFile(ws),
                                          child: AnimatedContainer(
                                            height: 370,
                                            width: 300,
                                            duration: const Duration(milliseconds: 150),
                                            padding: const EdgeInsets.all(10),
                                            decoration: BoxDecoration(
                                              color: _dragging ? Colors.blue.withValues(alpha: 0.08) : Colors.grey.shade800.withAlpha(127),
                                              borderRadius: BorderRadius.circular(6),
                                              border: Border.all(color: _dragging ? Colors.blue : Colors.grey.shade600, width: _dragging ? 1.5 : 1),
                                            ),
                                            child: Center(
                                              child: ws.previewImage != null
                                                  ? RotatedBox(
                                                      quarterTurns: -ws.rotation,
                                                      child: SizedBox(
                                                        width: 300,
                                                        height: 300,
                                                        child: RawImage(image: ws.previewImage, fit: BoxFit.contain),
                                                      ),
                                                    )
                                                  : Column(
                                                      mainAxisAlignment: MainAxisAlignment.center,
                                                      children: [
                                                        Icon(Icons.upload_file, color: Colors.blue[600], size: 28),
                                                        const SizedBox(height: 8),
                                                        Text('Drop a .pfr1 file here', style: TextStyle(fontSize: 11, color: Colors.grey[700])),
                                                        const SizedBox(height: 4),
                                                        Text('or click to select', style: TextStyle(fontSize: 10, color: Colors.grey[500])),
                                                      ],
                                                    ),
                                            ),
                                          ),
                                        ),
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              // const Spacer(),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 8),

            // Bottom Action Bar
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                border: Border(top: BorderSide(color: Colors.grey[500]!.withValues(alpha: 0.3))),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  Expanded(child: ws.uploading ? _buildLinearProgress(context, ws.progress.clamp(0.0, 1.0)) : const SizedBox.shrink()),
                  const SizedBox(width: 12),
                  _buildButton(
                    context,
                    label: 'Upload',
                    onPressed: ws.canUpload ? () async => await ws.upload() : null,
                    style: PlatformButtonStyle.primary,
                    size: PlatformButtonSize.large,
                  ),
                ],
              ),
            ),
          ],
        );
      },
    );
  }
}
