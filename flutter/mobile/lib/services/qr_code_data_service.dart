import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:photoframe/utils/app_logger.dart';
import '../models/qr_code_data.dart';

/// Service to manage QR code data persistence
class QRCodeDataService {
  static const String _storageKey = 'qr_code_data';
  static final QRCodeDataService _instance = QRCodeDataService._internal();

  late SharedPreferences _prefs;
  QRCodeData? _currentQRData;

  QRCodeDataService._internal();

  factory QRCodeDataService() {
    return _instance;
  }

  /// Initialize the service
  /// Must be called once before using any other methods
  Future<void> initialize() async {
    try {
      _prefs = await SharedPreferences.getInstance();
      await _loadCachedData();
      logger.info('QRCodeDataService initialized');
    } catch (e) {
      logger.severe('Failed to initialize QRCodeDataService: $e');
      rethrow;
    }
  }

  /// Get the current QR code data
  QRCodeData? getCurrentQRData() {
    return _currentQRData;
  }

  /// Save new QR code data
  Future<void> saveQRData(QRCodeData data) async {
    try {
      final jsonString = jsonEncode(data.toJson());
      await _prefs.setString(_storageKey, jsonString);
      _currentQRData = data;
      logger.info('QR code data saved: $data');
    } catch (e) {
      logger.severe('Failed to save QR code data: $e');
      rethrow;
    }
  }

  /// Load cached QR code data from storage
  Future<QRCodeData?> _loadCachedData() async {
    try {
      final jsonString = _prefs.getString(_storageKey);
      if (jsonString == null) {
        logger.info('No cached QR code data found');
        return null;
      }

      final json = jsonDecode(jsonString) as Map<String, dynamic>;
      _currentQRData = QRCodeData.fromJson(json);
      logger.info('Loaded cached QR code data: $_currentQRData');
      return _currentQRData;
    } catch (e) {
      logger.warning('Failed to load cached QR code data: $e');
      return null;
    }
  }

  /// Clear stored QR code data
  Future<void> clearQRData() async {
    try {
      await _prefs.remove(_storageKey);
      _currentQRData = null;
      logger.info('QR code data cleared');
    } catch (e) {
      logger.severe('Failed to clear QR code data: $e');
      rethrow;
    }
  }

  /// Check if QR data is available and not expired
  bool hasValidQRData({Duration maxAge = const Duration(hours: 1)}) {
    if (_currentQRData == null) return false;

    final age = DateTime.now().difference(_currentQRData!.scannedAt);
    return age < maxAge;
  }
}
