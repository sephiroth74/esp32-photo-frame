/// Model for QR code data information
class QRCodeData {
  final String wsUrl;
  final String token;
  final DateTime scannedAt;

  QRCodeData({required this.wsUrl, required this.token, required this.scannedAt});

  /// Create from URI query parameters
  factory QRCodeData.fromUri(Uri uri) {
    final params = uri.queryParameters;
    final wsUrl = params['wsUrl'];
    final token = params['token'];

    if (wsUrl == null || token == null) {
      throw ArgumentError('Missing wsUrl or token in deep link');
    }

    return QRCodeData(wsUrl: wsUrl, token: token, scannedAt: DateTime.now());
  }

  /// Convert to JSON for storage
  Map<String, dynamic> toJson() => {'wsUrl': wsUrl, 'token': token, 'scannedAt': scannedAt.toIso8601String()};

  /// Create from JSON
  factory QRCodeData.fromJson(Map<String, dynamic> json) =>
      QRCodeData(wsUrl: json['wsUrl'] as String, token: json['token'] as String, scannedAt: DateTime.parse(json['scannedAt'] as String));

  @override
  String toString() => 'QRCodeData(wsUrl: $wsUrl, token: $token)';
}
