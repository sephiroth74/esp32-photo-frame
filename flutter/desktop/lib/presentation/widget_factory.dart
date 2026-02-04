import 'dart:io';

import 'package:flutter/material.dart';
import '../platform/platform_detector.dart';
import '../screens/home_screen_macos.dart';
import '../screens/home_screen_material.dart';
import 'abstractions/widget_abstractions.dart';
import 'macos/macos_widgets.dart';
import 'linux/linux_widgets.dart';

/// Factory for creating platform-specific widgets.
abstract class WidgetFactory {
  Widget createHomeScreen();

  PlatformSwitch switchWidget({required bool checked, required ValueChanged<bool> onChanged});

  PlatformSlider slider({
    required double value,
    required ValueChanged<double> onChanged,
    double min = 0.0,
    double max = 1.0,
    int? divisions,
    List<double>? stops,
  });

  /// Factory constructor to create platform-specific implementation
  PlatformCheckbox checkbox({required bool value, ValueChanged<bool?>? onChanged});

  PlatformButton button({
    required String label,
    VoidCallback? onPressed,
    PlatformButtonSize size = PlatformButtonSize.medium,
    PlatformButtonStyle style = PlatformButtonStyle.primary,
    bool enabled = true,
    Widget? icon,
    bool isLoading = false,
    String? tooltip,
  });

  PlatformTextField textField({
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
  });

  PlatformGroupBox groupBox({
    String? title,
    required Widget child,
    EdgeInsets padding = const EdgeInsets.all(16.0),
    Color? backgroundColor,
    Color? borderColor,
  });

  PlatformDialog dialog({
    required String title,
    String? message,
    Widget? content,
    List<PlatformDialogAction> actions = const [],
    bool barrierDismissible = true,
    VoidCallback? onDismissed,
  });

  PlatformProgressIndicator progress({double? value, String? label, bool visible = true});

  PlatformCircularProgressIndicator circularProgress({double? value, double? size});

  PlatformPopupMenuItem<T> popupMenuItem<T>({required T value, required String label});

  PlatformPopupMenu<T> popupMenu<T>({
    required List<PlatformPopupMenuItem<T>> items,
    T? selectedItem,
    Widget? label,
    ValueChanged<T?>? onSelected,
    PlatformPopupMenuStyle style = PlatformPopupMenuStyle.bevel,
  });
}

class MacOSWidgetFactory extends WidgetFactory {
  @override
  Widget createHomeScreen() {
    return const HomeScreenMacos();
  }

  @override
  PlatformCheckbox checkbox({required bool value, ValueChanged<bool?>? onChanged}) {
    return MacOSCheckbox(value: value, onChanged: onChanged);
  }

  @override
  PlatformSlider slider({
    required double value,
    required ValueChanged<double> onChanged,
    double min = 0.0,
    double max = 1.0,
    List<double>? stops,
    int? divisions,
  }) {
    return MacOSSlider(value: value, min: min, max: max, onChanged: onChanged, stops: stops);
  }

  @override
  PlatformPopupMenuItem<T> popupMenuItem<T>({required T value, required String label}) {
    return MacOSPopupMenuItem(value: value, label: label);
  }

  @override
  PlatformPopupMenu<T> popupMenu<T>({
    required List<PlatformPopupMenuItem<T>> items,
    T? selectedItem,
    Widget? label,
    ValueChanged<T?>? onSelected,
    PlatformPopupMenuStyle? style,
  }) {
    return MacOSPopupMenu(items: items, onSelected: onSelected);
  }

  @override
  PlatformButton button({
    required String label,
    VoidCallback? onPressed,
    PlatformButtonSize size = PlatformButtonSize.medium,
    PlatformButtonStyle style = PlatformButtonStyle.primary,
    bool enabled = true,
    Widget? icon,
    bool isLoading = false,
    String? tooltip,
  }) {
    return MacOSButton(
      label: label,
      onPressed: onPressed,
      size: size,
      style: style,
      enabled: enabled,
      icon: icon,
      isLoading: isLoading,
      tooltip: tooltip,
    );
  }

  @override
  PlatformTextField textField({
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
    return MacOSTextField(
      label: label,
      placeholder: placeholder,
      controller: controller,
      onChanged: onChanged,
      onSubmitted: onSubmitted,
      borderStyle: borderStyle,
      maxLines: maxLines,
      minLines: minLines,
      obscureText: obscureText,
      keyboardType: keyboardType,
      errorText: errorText,
      maxLength: maxLength,
      enabled: enabled,
    );
  }

  @override
  PlatformGroupBox groupBox({
    String? title,
    required Widget child,
    EdgeInsets padding = const EdgeInsets.all(16.0),
    Color? backgroundColor,
    Color? borderColor,
  }) {
    return MacOSGroupBox(title: title, padding: padding, backgroundColor: backgroundColor, borderColor: borderColor, child: child);
  }

  @override
  PlatformDialog dialog({
    required String title,
    String? message,
    Widget? content,
    List<PlatformDialogAction> actions = const [],
    bool barrierDismissible = true,
    VoidCallback? onDismissed,
  }) {
    return MacOSDialog(
      title: title,
      message: message,
      content: content,
      actions: actions,
      barrierDismissible: barrierDismissible,
      onDismissed: onDismissed,
    );
  }

  @override
  PlatformProgressIndicator progress({double? value, String? label, bool visible = true}) {
    return MacOSProgressIndicator(value: value, label: label, visible: visible);
  }

  @override
  PlatformCircularProgressIndicator circularProgress({double? value, double? size}) {
    return MacOSCircularProgressIndicator(value: value, size: size);
  }

  @override
  PlatformSwitch switchWidget({required bool checked, required ValueChanged<bool> onChanged}) {
    return MacOSSwitch(checked: checked, onChanged: onChanged);
  }
}

class WindowsWidgetFactory extends MacOSWidgetFactory {}

class LinuxWidgetFactory extends WidgetFactory {
  @override
  PlatformPopupMenuItem<T> popupMenuItem<T>({required T value, required String label}) {
    return LinuxPopupMenuItem(value: value, label: label);
  }

  @override
  PlatformCheckbox checkbox({required bool value, ValueChanged<bool?>? onChanged}) {
    return LinuxCheckBox(value: value, onChanged: onChanged);
  }

  @override
  PlatformSlider slider({
    required double value,
    double min = 0.0,
    double max = 1.0,
    required ValueChanged<double> onChanged,
    List<double>? stops,
    int? divisions,
  }) {
    return LinuxSlider(value: value, min: min, max: max, onChanged: onChanged, divisions: divisions);
  }

  @override
  PlatformPopupMenu<T> popupMenu<T>({
    required List<PlatformPopupMenuItem<T>> items,
    T? selectedItem,
    Widget? label,
    ValueChanged<T?>? onSelected,
    PlatformPopupMenuStyle? style,
  }) {
    return LinuxPopupMenu(items: items, onSelected: onSelected, label: label);
  }

  @override
  PlatformButton button({
    required String label,
    VoidCallback? onPressed,
    PlatformButtonSize size = PlatformButtonSize.medium,
    PlatformButtonStyle style = PlatformButtonStyle.primary,
    bool enabled = true,
    Widget? icon,
    bool isLoading = false,
    String? tooltip,
  }) {
    return LinuxButton(
      label: label,
      onPressed: onPressed,
      size: size,
      style: style,
      enabled: enabled,
      icon: icon,
      isLoading: isLoading,
      tooltip: tooltip,
    );
  }

  @override
  PlatformTextField textField({
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
    return LinuxTextField(
      label: label,
      placeholder: placeholder,
      controller: controller,
      onChanged: onChanged,
      onSubmitted: onSubmitted,
      borderStyle: borderStyle,
      maxLines: maxLines,
      minLines: minLines,
      obscureText: obscureText,
      keyboardType: keyboardType,
      errorText: errorText,
      maxLength: maxLength,
      enabled: enabled,
    );
  }

  @override
  PlatformGroupBox groupBox({
    String? title,
    required Widget child,
    EdgeInsets padding = const EdgeInsets.all(16.0),
    Color? backgroundColor,
    Color? borderColor,
  }) {
    return LinuxGroupBox(title: title, padding: padding, backgroundColor: backgroundColor, borderColor: borderColor, child: child);
  }

  @override
  PlatformDialog dialog({
    required String title,
    String? message,
    Widget? content,
    List<PlatformDialogAction> actions = const [],
    bool barrierDismissible = true,
    VoidCallback? onDismissed,
  }) {
    return LinuxDialog(
      title: title,
      message: message,
      content: content,
      actions: actions,
      barrierDismissible: barrierDismissible,
      onDismissed: onDismissed,
    );
  }

  @override
  PlatformProgressIndicator progress({double? value, String? label, bool visible = true}) {
    return LinuxProgressIndicator(value: value, label: label);
  }

  @override
  PlatformCircularProgressIndicator circularProgress({double? value, double? size}) {
    return LinuxCircularProgressIndicator(value: value, size: size);
  }

  @override
  PlatformSwitch switchWidget({required bool checked, required ValueChanged<bool> onChanged}) {
    return LinuxSwitch(checked: checked, onChanged: onChanged);
  }

  @override
  Widget createHomeScreen() {
    return const HomeScreenMaterial();
  }
}

class WidgetFactoryResolver {
  static WidgetFactory current() {
    switch (PlatformDetector.current) {
      case AppPlatform.macos:
        return MacOSWidgetFactory();
      case AppPlatform.windows:
        return WindowsWidgetFactory();
      case AppPlatform.linux:
        return LinuxWidgetFactory();
      case AppPlatform.unknown:
        return MacOSWidgetFactory();
    }
  }
}
