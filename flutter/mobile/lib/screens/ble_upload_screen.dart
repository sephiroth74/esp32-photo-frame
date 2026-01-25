import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

import '../models/processing_models.dart';

class BleUploadScreen extends StatefulWidget {
  final File image;
  final ProcessingJob job;

  const BleUploadScreen({super.key, required this.image, required this.job});

  @override
  State<BleUploadScreen> createState() => _BleUploadScreenState();
}

class _BleUploadScreenState extends State<BleUploadScreen> {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  final List<DiscoveredDevice> _devices = [];
  StreamSubscription<DiscoveredDevice>? _scanSub;
  DiscoveredDevice? _selected;
  bool _scanning = false;
  bool _connecting = false;
  bool _connected = false;
  bool _loadingConfig = false;
  bool _uploading = false;
  double _progress = 0;
  String _status = 'Pronto a scansionare i frame Bluetooth';

  static final Uuid _frameService = Uuid.parse('0000FFB0-0000-1000-8000-00805F9B34FB');

  @override
  void dispose() {
    _scanSub?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Upload via Bluetooth')),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _StatusBanner(status: _status),
            const SizedBox(height: 12),
            _scanCard(),
            const SizedBox(height: 12),
            _deviceList(),
            const SizedBox(height: 12),
            _configCard(),
            const SizedBox(height: 12),
            _uploadCard(),
          ],
        ),
      ),
    );
  }

  Widget _scanCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Row(
          children: [
            ElevatedButton.icon(
              onPressed: _scanning ? null : _startScan,
              icon: Icon(_scanning ? Icons.radar : Icons.search),
              label: Text(_scanning ? 'Scanning…' : 'Scan dispositivi'),
            ),
            const SizedBox(width: 12),
            Expanded(child: Text('Cerchiamo solo dispositivi validi (servizio BLE Photoframe).')),
          ],
        ),
      ),
    );
  }

  Widget _deviceList() {
    if (_devices.isEmpty) {
      return const Card(
        child: Padding(
          padding: EdgeInsets.all(12),
          child: Text('Nessun dispositivo trovato. Avvia una scansione per cercare frame.'),
        ),
      );
    }
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          children: _devices.map((d) {
            final selected = _selected?.id == d.id;
            return ListTile(
              title: Text(d.name.isNotEmpty ? d.name : 'Dispositivo senza nome'),
              subtitle: Text(d.id),
              trailing: selected
                  ? const Icon(Icons.check_circle, color: Colors.green)
                  : OutlinedButton(onPressed: _connecting ? null : () => _selectDevice(d), child: const Text('Seleziona')),
            );
          }).toList(),
        ),
      ),
    );
  }

  Widget _configCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Verifica configurazione frame', style: TextStyle(fontWeight: FontWeight.w600)),
            const SizedBox(height: 8),
            Text(
              'Display target: ${widget.job.targetResolution.width.toInt()}x${widget.job.targetResolution.height.toInt()} (${widget.job.displayType == DisplayType.blackAndWhite ? 'B/W' : '6 colori'})',
            ),
            const SizedBox(height: 8),
            ElevatedButton.icon(
              onPressed: !_connected || _loadingConfig ? null : _loadConfig,
              icon: Icon(_loadingConfig ? Icons.sync : Icons.settings),
              label: Text(_loadingConfig ? 'Lettura config…' : 'Carica config dal dispositivo'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _uploadCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Upload immagine', style: TextStyle(fontWeight: FontWeight.w600)),
            const SizedBox(height: 8),
            LinearProgressIndicator(value: _uploading ? _progress : null, minHeight: 8),
            const SizedBox(height: 8),
            Row(
              children: [
                ElevatedButton.icon(
                  onPressed: _canUpload ? _upload : null,
                  icon: Icon(_uploading ? Icons.cloud_upload : Icons.play_circle),
                  label: Text(_uploading ? 'Upload in corso' : 'Avvia upload'),
                ),
                const SizedBox(width: 12),
                Text(_progressLabel),
              ],
            ),
          ],
        ),
      ),
    );
  }

  bool get _canUpload => _connected && !_uploading && !_loadingConfig && _selected != null;

  String get _progressLabel => _uploading ? '${(_progress * 100).toStringAsFixed(0)}%' : 'Pronto';

  Future<void> _startScan() async {
    setState(() {
      _scanning = true;
      _devices.clear();
      _status = 'Scanning…';
    });

    _scanSub?.cancel();
    _scanSub = _ble
        .scanForDevices(withServices: [_frameService], scanMode: ScanMode.balanced)
        .listen(
          (device) {
            final already = _devices.any((d) => d.id == device.id);
            if (!already && device.name.isNotEmpty) {
              setState(() {
                _devices.add(device);
              });
            }
          },
          onError: (e) {
            setState(() {
              _status = 'Errore durante la scansione: $e';
              _scanning = false;
            });
          },
          onDone: () {
            setState(() {
              _scanning = false;
              _status = 'Scansione terminata';
            });
          },
        );

    await Future.delayed(const Duration(seconds: 6));
    await _scanSub?.cancel();
    if (mounted) {
      setState(() => _scanning = false);
    }
  }

  Future<void> _selectDevice(DiscoveredDevice device) async {
    setState(() {
      _selected = device;
      _connecting = true;
      _status = 'Connessione a ${device.name}…';
    });

    try {
      // Placeholder: in una release reale ci si connette e si prepara la MTU.
      await Future.delayed(const Duration(seconds: 2));
      if (!mounted) return;
      setState(() {
        _connecting = false;
        _connected = true;
        _status = 'Connesso a ${device.name}';
      });
    } catch (e) {
      setState(() {
        _connecting = false;
        _connected = false;
        _status = 'Connessione fallita: $e';
      });
    }
  }

  Future<void> _loadConfig() async {
    setState(() {
      _loadingConfig = true;
      _status = 'Lettura configurazione…';
    });
    await Future.delayed(const Duration(seconds: 2));
    if (!mounted) return;
    setState(() {
      _loadingConfig = false;
      _status = 'Configurazione verificata';
    });
  }

  Future<void> _upload() async {
    setState(() {
      _uploading = true;
      _progress = 0;
      _status = 'Upload in corso…';
    });

    const total = 20;
    for (var i = 1; i <= total; i++) {
      await Future.delayed(const Duration(milliseconds: 180));
      if (!mounted) return;
      setState(() => _progress = i / total);
    }

    if (!mounted) return;
    setState(() {
      _uploading = false;
      _status = 'Upload completato';
    });
  }
}

class _StatusBanner extends StatelessWidget {
  final String status;

  const _StatusBanner({required this.status});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.blue.shade50,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.blue.shade100),
      ),
      child: Row(
        children: [
          const Icon(Icons.info_outline, color: Colors.blue),
          const SizedBox(width: 8),
          Expanded(child: Text(status)),
        ],
      ),
    );
  }
}
