/// Abstract widget interfaces for platform-agnostic UI components.
/// Each platform (macOS, Windows, Linux) will provide concrete implementations.
library;

import 'package:flutter/material.dart';

// ============================================================================
// ENUMS
// ============================================================================

/// Button size variants
enum PlatformButtonSize { small, medium, large }

/// Button style variants
enum PlatformButtonStyle { primary, secondary, danger, outline }

/// Text field border style
enum PlatformTextFieldBorderStyle { rounded, square, none }

/// Dialog action button style
enum PlatformDialogActionStyle { primary, secondary, cancel, destructive }

/// Popup menu style variants
enum PlatformPopupMenuStyle { bevel, inline, push, plain }

// ============================================================================
// ABSTRACT CLASSES
// ============================================================================

/// Platform-agnostic button widget
abstract class PlatformButton extends StatelessWidget {
  final String label;
  final VoidCallback? onPressed;
  final PlatformButtonSize size;
  final PlatformButtonStyle style;
  final bool enabled;
  final Widget? icon;
  final bool isLoading;
  final String? tooltip;

  const PlatformButton({
    required this.label,
    this.onPressed,
    this.size = PlatformButtonSize.medium,
    this.style = PlatformButtonStyle.primary,
    this.enabled = true,
    this.icon,
    this.isLoading = false,
    this.tooltip,
    super.key,
  });

  /// Factory constructor to create platform-specific implementation
  factory PlatformButton.create({
    required String label,
    VoidCallback? onPressed,
    PlatformButtonSize size = PlatformButtonSize.medium,
    PlatformButtonStyle style = PlatformButtonStyle.primary,
    bool enabled = true,
    Widget? icon,
    bool isLoading = false,
    String? tooltip,
  }) {
    throw UnimplementedError('Use platform-specific factory');
  }
}

/// Platform-agnostic text input field
abstract class PlatformTextField extends StatefulWidget {
  final String? label;
  final String? placeholder;
  final TextEditingController? controller;
  final ValueChanged<String>? onChanged;
  final VoidCallback? onSubmitted;
  final PlatformTextFieldBorderStyle borderStyle;
  final int? maxLines;
  final int? minLines;
  final bool obscureText;
  final TextInputType keyboardType;
  final String? errorText;
  final int? maxLength;
  final bool enabled;

  const PlatformTextField({
    this.label,
    this.placeholder,
    this.controller,
    this.onChanged,
    this.onSubmitted,
    this.borderStyle = PlatformTextFieldBorderStyle.rounded,
    this.maxLines = 1,
    this.minLines,
    this.obscureText = false,
    this.keyboardType = TextInputType.text,
    this.errorText,
    this.maxLength,
    this.enabled = true,
    super.key,
  });

  /// Factory constructor to create platform-specific implementation
  factory PlatformTextField.create({
    String? label,
    String? placeholder,
    TextEditingController? controller,
    ValueChanged<String>? onChanged,
    VoidCallback? onSubmitted,
    PlatformTextFieldBorderStyle borderStyle = PlatformTextFieldBorderStyle.rounded,
    int? maxLines = 1,
    int? minLines,
    bool obscureText = false,
    TextInputType keyboardType = TextInputType.text,
    String? errorText,
    int? maxLength,
    bool enabled = true,
  }) {
    throw UnimplementedError('Use platform-specific factory');
  }
}

/// Platform-agnostic group box (panel with border and title)
abstract class PlatformGroupBox extends StatelessWidget {
  final String? title;
  final Widget child;
  final EdgeInsets padding;
  final Color? backgroundColor;
  final Color? borderColor;

  const PlatformGroupBox({
    this.title,
    required this.child,
    this.padding = const EdgeInsets.all(16.0),
    this.backgroundColor,
    this.borderColor,
    super.key,
  });

  /// Factory constructor to create platform-specific implementation
  factory PlatformGroupBox.create({
    String? title,
    required Widget child,
    EdgeInsets padding = const EdgeInsets.all(16.0),
    Color? backgroundColor,
    Color? borderColor,
  }) {
    throw UnimplementedError('Use platform-specific factory');
  }
}

/// Platform-agnostic checkbox
abstract class PlatformCheckbox extends StatelessWidget {
  final bool value;
  final ValueChanged<bool?>? onChanged;

  const PlatformCheckbox({required this.value, this.onChanged, super.key});
}

/// Platform-agnostic dropdown/select widget
abstract class PlatformDropdown<T> extends StatefulWidget {
  final T? value;
  final List<T> items;
  final String Function(T) itemLabel;
  final ValueChanged<T?>? onChanged;
  final String? label;
  final bool enabled;

  const PlatformDropdown({required this.items, required this.itemLabel, this.value, this.onChanged, this.label, this.enabled = true, super.key});

  /// Factory constructor to create platform-specific implementation
  factory PlatformDropdown.create({
    required List<T> items,
    required String Function(T) itemLabel,
    T? value,
    ValueChanged<T?>? onChanged,
    String? label,
    bool enabled = true,
  }) {
    throw UnimplementedError('Use platform-specific factory');
  }
}

/// Platform-agnostic slider widget
abstract class PlatformSlider extends StatelessWidget {
  final double value;
  final double min;
  final double max;
  final List<double>? stops;
  final int? divisions;
  final ValueChanged<double>? onChanged;

  const PlatformSlider({required this.value, required this.min, required this.max, this.stops, this.divisions, this.onChanged, super.key});
}

/// Platform-agnostic dialog action button
class PlatformDialogAction {
  final String label;
  final VoidCallback? onPressed;
  final PlatformDialogActionStyle style;
  final bool isDestructive;

  PlatformDialogAction({required this.label, this.onPressed, this.style = PlatformDialogActionStyle.secondary, this.isDestructive = false});
}

/// Platform-agnostic dialog/alert
abstract class PlatformDialog extends StatelessWidget {
  final String title;
  final String? message;
  final Widget? content;
  final List<PlatformDialogAction> actions;
  final bool barrierDismissible;
  final VoidCallback? onDismissed;

  const PlatformDialog({
    required this.title,
    this.message,
    this.content,
    this.actions = const [],
    this.barrierDismissible = true,
    this.onDismissed,
    super.key,
  });

  /// Factory constructor to create platform-specific implementation
  factory PlatformDialog.create({
    required String title,
    String? message,
    Widget? content,
    List<PlatformDialogAction> actions = const [],
    bool barrierDismissible = true,
    VoidCallback? onDismissed,
  }) {
    throw UnimplementedError('Use platform-specific factory');
  }
}

/// Platform-agnostic menu item
class PlatformMenuItem {
  final String label;
  final VoidCallback? onPressed;
  final String? shortcut;
  final bool enabled;
  final List<PlatformMenuItem>? submenu;

  PlatformMenuItem({required this.label, this.onPressed, this.shortcut, this.enabled = true, this.submenu});
}

/// Platform-agnostic application menu bar
abstract class PlatformMenuBar extends StatelessWidget {
  final List<PlatformMenuItem> items;

  const PlatformMenuBar({required this.items, super.key});

  /// Factory constructor to create platform-specific implementation
  factory PlatformMenuBar.create({required List<PlatformMenuItem> items}) {
    throw UnimplementedError('Use platform-specific factory');
  }
}

/// Platform-agnostic toast/snackbar notification
abstract class PlatformNotification {
  final String message;
  final Duration duration;
  final VoidCallback? onDismissed;

  const PlatformNotification({required this.message, this.duration = const Duration(seconds: 3), this.onDismissed});

  /// Show the notification
  void show(BuildContext context);

  /// Dismiss the notification
  void dismiss();
}

/// Platform-agnostic progress indicator
abstract class PlatformProgressIndicator extends StatelessWidget {
  final double? value; // null = indeterminate
  final String? label;
  final bool visible;

  const PlatformProgressIndicator({this.value, this.label, this.visible = true, super.key});
}

/// Platform-agnostic circular progress indicator
abstract class PlatformCircularProgressIndicator extends StatelessWidget {
  final double? value; // null = indeterminate (0.0-1.0 for determinate)
  final double? size;

  const PlatformCircularProgressIndicator({this.value, this.size, super.key});
}

/// Platform-agnostic file picker
abstract class PlatformFilePicker {
  /// Pick a single file
  Future<String?> pickFile({String? initialDirectory, List<String>? allowedExtensions, String dialogTitle = 'Pick a file'});

  /// Pick multiple files
  Future<List<String>?> pickFiles({String? initialDirectory, List<String>? allowedExtensions, String dialogTitle = 'Pick files'});

  /// Pick a directory
  Future<String?> pickDirectory({String? initialDirectory, String dialogTitle = 'Pick a directory'});

  /// Save file dialog
  Future<String?> saveFile({String? initialDirectory, String? suggestedName, List<String>? allowedExtensions, String dialogTitle = 'Save file'});
}

/// Platform-agnostic keyboard shortcuts
abstract class PlatformKeyboardShortcuts {
  /// Register a keyboard shortcut
  void register(String shortcut, VoidCallback action, {bool isRepeatable = false});

  /// Unregister a keyboard shortcut
  void unregister(String shortcut);

  /// Check if a shortcut is registered
  bool isRegistered(String shortcut);

  /// Clear all shortcuts
  void clearAll();
}

abstract class PlatformPopupMenuItem<T> {
  final T value;
  final String label;

  const PlatformPopupMenuItem({required this.value, required this.label});
}

/// Platform-agnostic popup menu
abstract class PlatformPopupMenu<T> extends StatelessWidget {
  final List<PlatformPopupMenuItem<T>> items;
  final T? selectedItem;
  final ValueChanged<T?>? onSelected;
  final PlatformPopupMenuStyle? style;
  final Widget? label;

  const PlatformPopupMenu({
    required this.items,
    this.label,
    this.selectedItem,
    this.onSelected,
    this.style = PlatformPopupMenuStyle.bevel,
    super.key,
  });
}

/// Platform-agnostic switch widget
abstract class PlatformSwitch extends StatelessWidget {
  final bool checked;
  final ValueChanged<bool>? onChanged;

  const PlatformSwitch({required this.checked, this.onChanged, super.key});
}
