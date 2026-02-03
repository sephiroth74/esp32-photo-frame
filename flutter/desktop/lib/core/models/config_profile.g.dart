// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'config_profile.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

ConfigProfile _$ConfigProfileFromJson(Map<String, dynamic> json) =>
    ConfigProfile(
      name: json['name'] as String,
      filePath: json['filePath'] as String,
      lastModified: DateTime.parse(json['lastModified'] as String),
      config: ProcessingConfig.fromJson(json['config'] as Map<String, dynamic>),
    );

Map<String, dynamic> _$ConfigProfileToJson(ConfigProfile instance) =>
    <String, dynamic>{
      'name': instance.name,
      'filePath': instance.filePath,
      'lastModified': instance.lastModified.toIso8601String(),
      'config': instance.config,
    };

RecentFile _$RecentFileFromJson(Map<String, dynamic> json) => RecentFile(
  path: json['path'] as String,
  name: json['name'] as String,
  lastOpened: DateTime.parse(json['lastOpened'] as String),
);

Map<String, dynamic> _$RecentFileToJson(RecentFile instance) =>
    <String, dynamic>{
      'path': instance.path,
      'name': instance.name,
      'lastOpened': instance.lastOpened.toIso8601String(),
    };

AppPreferences _$AppPreferencesFromJson(Map<String, dynamic> json) =>
    AppPreferences(
      recentFiles:
          (json['recentFiles'] as List<dynamic>?)
              ?.map((e) => RecentFile.fromJson(e as Map<String, dynamic>))
              .toList() ??
          const [],
      lastOpenedProfile: json['lastOpenedProfile'] as String?,
      maxRecentFiles: (json['maxRecentFiles'] as num?)?.toInt() ?? 10,
    );

Map<String, dynamic> _$AppPreferencesToJson(AppPreferences instance) =>
    <String, dynamic>{
      'recentFiles': instance.recentFiles,
      'lastOpenedProfile': instance.lastOpenedProfile,
      'maxRecentFiles': instance.maxRecentFiles,
    };
