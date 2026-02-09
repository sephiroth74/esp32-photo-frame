/// Model for QR code data information
/// Format: photoframe://connect?ip=...&ssid=...&port=...&v=...&d=...&w=...&h=...
class QRCodeData {
  final String ip;
  final String ssid;
  final int port;
  final int? version;
  final int? displayType;
  final int? width;
  final int? height;
  final DateTime scannedAt;

  QRCodeData({
    required this.ip,
    required this.ssid,
    required this.port,
    this.version,
    this.displayType,
    this.width,
    this.height,
    required this.scannedAt,
  });

  /// Create from URI query parameters
  /// Expected format: photoframe://connect?ip=...&ssid=...&port=...&v=...&d=...&w=...&h=...
  factory QRCodeData.fromUri(Uri uri) {
    final params = uri.queryParameters;

    final ip = params['ip'];
    final ssid = params['ssid'];
    final port = params['port'];

    if (ip == null || ssid == null || port == null) {
      throw ArgumentError('Missing required parameters: ip, ssid, or port');
    }

    final portNum = int.tryParse(port);
    if (portNum == null) {
      throw ArgumentError('Invalid port number: $port');
    }

    return QRCodeData(
      ip: ip,
      ssid: ssid,
      port: portNum,
      version: int.tryParse(params['v'] ?? ''),
      displayType: int.tryParse(params['d'] ?? ''),
      width: int.tryParse(params['w'] ?? ''),
      height: int.tryParse(params['h'] ?? ''),
      scannedAt: DateTime.now(),
    );
  }

  /// Get WebSocket URL constructed from IP and port
  String get wsUrl => 'ws://$ip:$port/upload';

  /// Convert to JSON for storage
  Map<String, dynamic> toJson() => {
    'ip': ip,
    'ssid': ssid,
    'port': port,
    'version': version,
    'displayType': displayType,
    'width': width,
    'height': height,
    'scannedAt': scannedAt.toIso8601String(),
  };

  /// Create from JSON
  factory QRCodeData.fromJson(Map<String, dynamic> json) => QRCodeData(
    ip: json['ip'] as String,
    ssid: json['ssid'] as String,
    port: json['port'] as int,
    version: json['version'] as int?,
    displayType: json['displayType'] as int?,
    width: json['width'] as int?,
    height: json['height'] as int?,
    scannedAt: DateTime.parse(json['scannedAt'] as String),
  );

  @override
  String toString() => 'QRCodeData(ip: $ip, ssid: $ssid, port: $port, version: $version, displayType: $displayType, width: $width, height: $height)';
}
