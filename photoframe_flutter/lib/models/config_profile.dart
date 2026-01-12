import 'package:json_annotation/json_annotation.dart';
import 'processing_config.dart';

part 'config_profile.g.dart';

@JsonSerializable()
class ConfigProfile {
  final String name;
  final String filePath;
  final DateTime lastModified;
  final ProcessingConfig config;

  const ConfigProfile({
    required this.name,
    required this.filePath,
    required this.lastModified,
    required this.config,
  });

  factory ConfigProfile.fromJson(Map<String, dynamic> json) =>
      _$ConfigProfileFromJson(json);

  Map<String, dynamic> toJson() => _$ConfigProfileToJson(this);

  ConfigProfile copyWith({
    String? name,
    String? filePath,
    DateTime? lastModified,
    ProcessingConfig? config,
  }) {
    return ConfigProfile(
      name: name ?? this.name,
      filePath: filePath ?? this.filePath,
      lastModified: lastModified ?? this.lastModified,
      config: config ?? this.config,
    );
  }
}

@JsonSerializable()
class RecentFile {
  final String path;
  final String name;
  final DateTime lastOpened;

  const RecentFile({
    required this.path,
    required this.name,
    required this.lastOpened,
  });

  factory RecentFile.fromJson(Map<String, dynamic> json) =>
      _$RecentFileFromJson(json);

  Map<String, dynamic> toJson() => _$RecentFileToJson(this);
}

@JsonSerializable()
class AppPreferences {
  final List<RecentFile> recentFiles;
  final String? lastOpenedProfile;
  final int maxRecentFiles;

  const AppPreferences({
    this.recentFiles = const [],
    this.lastOpenedProfile,
    this.maxRecentFiles = 10,
  });

  factory AppPreferences.fromJson(Map<String, dynamic> json) =>
      _$AppPreferencesFromJson(json);

  Map<String, dynamic> toJson() => _$AppPreferencesToJson(this);

  AppPreferences copyWith({
    List<RecentFile>? recentFiles,
    String? lastOpenedProfile,
    int? maxRecentFiles,
  }) {
    return AppPreferences(
      recentFiles: recentFiles ?? this.recentFiles,
      lastOpenedProfile: lastOpenedProfile ?? this.lastOpenedProfile,
      maxRecentFiles: maxRecentFiles ?? this.maxRecentFiles,
    );
  }
}