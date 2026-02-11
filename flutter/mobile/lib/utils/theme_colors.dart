import 'package:flutter/material.dart';

/// Helper class for theme-aware color selection across the app
class ThemeColors {
  ThemeColors(this.context);

  final BuildContext context;

  /// Get color scheme from theme
  ColorScheme get _scheme => Theme.of(context).colorScheme;

  // Primary brand colors - use system palette
  Color get primary => _scheme.primary;
  Color get primaryContainer => _scheme.primaryContainer;
  Color get secondary => _scheme.secondary;
  Color get tertiary => _scheme.tertiary;

  // Semantic colors
  Color get surface => _scheme.surface;
  Color get onSurface => _scheme.onSurface;
  Color get error => _scheme.error;
  Color get onError => _scheme.onError;

  // Status colors - with theme awareness
  Color get success => const Color(0xFF4CAF50); // Material green
  Color get warning => const Color(0xFFFFA500); // Orange (fallback, can use tertiary)
  Color get info => _scheme.secondary;

  // Text colors
  Color get textPrimary => _scheme.onSurface;
  Color get textSecondary => _scheme.onSurfaceVariant;
  Color get textHint => _scheme.outline;

  // Background colors - respect theme brightness
  Color get background => _scheme.surfaceContainerLowest;
  Color get surfaceLight => _scheme.surfaceContainer;

  // Overlay colors - can use fixed values for contrast
  Color get overlayDark => Colors.black.withValues(alpha: 0.55);
  Color get overlayLight => Colors.white.withValues(alpha: 0.8);

  // AppBar colors - follow Material 3 guidelines
  Color get appBarBackground => _scheme.surface;
  Color get appBarForeground => _scheme.onSurface;

  // Border colors
  Color get borderLight => _scheme.outlineVariant;
  Color get borderMedium => _scheme.outline;

  // Disabled/inactive colors
  Color get disabled => _scheme.surfaceContainerHighest;
  Color get disabledText => _scheme.onSurfaceVariant;
}
