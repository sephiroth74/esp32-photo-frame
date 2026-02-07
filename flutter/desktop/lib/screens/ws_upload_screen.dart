import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:desktop_drop/desktop_drop.dart';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:photoframe_flutter/core/services/bin_parser.dart';
import 'package:photoframe_flutter/core/services/preferences.dart';
import 'package:provider/provider.dart';

import '../core/providers/ws_provider.dart';
import '../core/providers/widget_factory_provider.dart';
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

  @override
  void initState() {
    super.initState();
    _loadSavedSettings();
  }

  Future<void> _loadSavedSettings() async {
    await Preferences.initialize();

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
    return AppKitTextField(controller: controller, placeholder: placeholder, keyboardType: keyboardType, onChanged: onChanged);
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<WsUploadState>(
      builder: (context, ws, _) {
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
                          const Text('WebSocket Connection.', style: TextStyle(fontSize: 12, fontWeight: FontWeight.w600)),
                          const Text('Enter the IP address and port of the device you want to connect to.', style: TextStyle(fontSize: 12)),
                          const SizedBox(height: 8),
                          Row(
                            children: [
                              const Text('Note: ', style: TextStyle(fontSize: 12, fontWeight: FontWeight.w600)),
                              const Text('Please ensure to connect first to the device own WIFI', style: TextStyle(fontSize: 12)),
                            ],
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
                                    label: ws.connected ? 'Disconnect' : (ws.connecting ? 'Connecting...' : 'Connect'),
                                    onPressed: ws.connecting
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
                                  if (ws.connected) ...[
                                    const SizedBox(width: 8),
                                    const Icon(Icons.check_circle, color: Colors.green, size: 16),
                                    const SizedBox(width: 4),
                                    Text('Connected to ${ws.ipAddress}:${ws.port}', style: TextStyle(fontSize: 11, color: Colors.green)),
                                  ],
                                ],
                              ),
                            ],
                          ),

                          const SizedBox(height: 16),

                          // Status Message
                          if (ws.status.isNotEmpty) ...[
                            const SizedBox(height: 12),
                            Text(ws.status, style: TextStyle(fontSize: 11, color: Colors.grey[600])),
                          ],

                          // Error Message
                          if (ws.error != null) ...[
                            const SizedBox(height: 12),
                            Container(
                              padding: const EdgeInsets.all(8),
                              decoration: BoxDecoration(color: Colors.red.withValues(alpha: 0.1), borderRadius: BorderRadius.circular(4)),
                              child: Row(
                                children: [
                                  const Icon(Icons.error_outline, color: Colors.red, size: 16),
                                  const SizedBox(width: 8),
                                  Expanded(
                                    child: Text(ws.error!, style: TextStyle(fontSize: 11, color: Colors.red)),
                                  ),
                                ],
                              ),
                            ),
                          ],
                          const SizedBox(height: 8),
                        ],
                      ),
                    ),

                    const SizedBox(height: 16),

                    // File Selection Section
                    _buildGroupBox(
                      context,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const SizedBox(height: 8),
                          const Text('File Selection', style: TextStyle(fontSize: 12, fontWeight: FontWeight.w500)),
                          const SizedBox(height: 12),
                          Row(
                            children: [
                              _buildButton(context, label: 'Select BIN File', onPressed: () async => _pickBinFile(ws)),
                              const SizedBox(width: 12),
                              Expanded(
                                child: Text(fileLabel, style: TextStyle(fontSize: 11, color: Colors.grey[600])),
                              ),
                            ],
                          ),
                          const SizedBox(height: 12),
                          DropTarget(
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
                                duration: const Duration(milliseconds: 150),
                                height: 240,
                                padding: const EdgeInsets.all(10),
                                decoration: BoxDecoration(
                                  color: _dragging ? Colors.blue.withValues(alpha: 0.08) : Colors.grey[100],
                                  borderRadius: BorderRadius.circular(6),
                                  border: Border.all(color: _dragging ? Colors.blue : Colors.grey[300]!, width: _dragging ? 1.5 : 1),
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
                                            Icon(Icons.upload_file, color: Colors.grey[600], size: 28),
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
                          const SizedBox(height: 8),
                          if (ws.binHeader != null) ...[
                            Row(
                              children: [
                                _buildGroupBox(
                                  context,
                                  child: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    mainAxisAlignment: MainAxisAlignment.start,
                                    crossAxisAlignment: CrossAxisAlignment.start,
                                    children: [
                                      Row(
                                        mainAxisSize: MainAxisSize.min,
                                        children: [
                                          Text('Rotation:', style: TextStyle(fontSize: 12)),
                                          SizedBox(width: 8, height: 12),
                                          Text(ws.binHeader!.rotation.toString(), style: TextStyle(fontSize: 12, color: Colors.grey[600])),
                                        ],
                                      ),
                                      Row(
                                        mainAxisSize: MainAxisSize.min,
                                        children: [
                                          Text('Size:', style: TextStyle(fontSize: 12)),
                                          SizedBox(width: 8, height: 12),
                                          Text(
                                            '${ws.binHeader!.getWidth()} x ${ws.binHeader!.getHeight()}',
                                            style: TextStyle(fontSize: 12, color: Colors.grey[600]),
                                          ),
                                        ],
                                      ),
                                      Row(
                                        mainAxisSize: MainAxisSize.min,
                                        children: [
                                          Text('Color Mode:', style: TextStyle(fontSize: 12)),
                                          SizedBox(width: 8, height: 12),
                                          Text(ws.binHeader!.colorMode.toReadableString(), style: TextStyle(fontSize: 12, color: Colors.grey[600])),
                                        ],
                                      ),
                                    ],
                                  ),
                                ),
                                const Spacer(flex: 1),
                                const SizedBox(width: 12),
                                _buildButton(
                                  context,
                                  label: 'Rotate',
                                  icon: Icons.rotate_right,
                                  size: PlatformButtonSize.large,
                                  onPressed: () {
                                    ws.setRotation((ws.rotation + 1) % 4);
                                  },
                                ),
                              ],
                            ),
                          ],
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),

            // Bottom Action Bar
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                border: Border(top: BorderSide(color: Colors.grey[300]!)),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  _buildButton(
                    context,
                    label: 'Upload',
                    onPressed: ws.canUpload ? () async => await ws.upload() : null,
                    style: PlatformButtonStyle.primary,
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
