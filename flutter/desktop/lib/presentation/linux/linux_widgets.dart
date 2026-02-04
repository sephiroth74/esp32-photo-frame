import 'package:flutter/material.dart';

import '../abstractions/widget_abstractions.dart';

class LinuxButton extends PlatformButton {
  const LinuxButton({
    required super.label,
    super.onPressed,
    super.size = PlatformButtonSize.medium,
    super.style = PlatformButtonStyle.primary,
    super.enabled = true,
    super.icon,
    super.isLoading = false,
    super.tooltip,
    super.key,
  });

  ButtonStyle _getButtonStyle() {
    final size = _getSize();
    return ElevatedButton.styleFrom(
      padding: size,
      elevation: style == PlatformButtonStyle.primary ? 2 : 0,
      backgroundColor: _getBackgroundColor(),
      foregroundColor: _getForegroundColor(),
      disabledBackgroundColor: Colors.grey[400],
      disabledForegroundColor: Colors.grey[600],
    );
  }

  EdgeInsetsGeometry _getSize() {
    switch (size) {
      case PlatformButtonSize.small:
        return const EdgeInsets.symmetric(horizontal: 12, vertical: 6);
      case PlatformButtonSize.medium:
        return const EdgeInsets.symmetric(horizontal: 20, vertical: 10);
      case PlatformButtonSize.large:
        return const EdgeInsets.symmetric(horizontal: 24, vertical: 14);
    }
  }

  Color _getBackgroundColor() {
    switch (style) {
      case PlatformButtonStyle.primary:
        return Colors.blue;
      case PlatformButtonStyle.secondary:
        return Colors.grey[300] ?? Colors.grey;
      case PlatformButtonStyle.danger:
        return Colors.red;
      case PlatformButtonStyle.outline:
        return Colors.transparent;
    }
  }

  Color _getForegroundColor() {
    switch (style) {
      case PlatformButtonStyle.primary:
        return Colors.white;
      case PlatformButtonStyle.secondary:
        return Colors.black87;
      case PlatformButtonStyle.danger:
        return Colors.white;
      case PlatformButtonStyle.outline:
        return Colors.blue;
    }
  }

  @override
  Widget build(BuildContext context) {
    final content = isLoading
        ? SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2, valueColor: AlwaysStoppedAnimation(_getForegroundColor())))
        : Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (icon != null) ...[icon!, const SizedBox(width: 8)],
              Text(label),
            ],
          );

    if (style == PlatformButtonStyle.outline) {
      return OutlinedButton(
        onPressed: enabled && !isLoading ? onPressed : null,
        style: OutlinedButton.styleFrom(
          padding: _getSize(),
          foregroundColor: _getForegroundColor(),
          disabledForegroundColor: Colors.grey[600],
          side: BorderSide(color: _getForegroundColor()),
        ),
        child: content,
      );
    }

    return ElevatedButton(onPressed: enabled && !isLoading ? onPressed : null, style: _getButtonStyle(), child: content);
  }
}

class LinuxTextField extends PlatformTextField {
  const LinuxTextField({
    super.label,
    super.placeholder,
    super.controller,
    super.onChanged,
    super.onSubmitted,
    super.borderStyle = PlatformTextFieldBorderStyle.rounded,
    super.maxLines = 1,
    super.minLines,
    super.obscureText = false,
    super.keyboardType = TextInputType.text,
    super.errorText,
    super.maxLength,
    super.enabled = true,
    super.key,
  });

  @override
  State<LinuxTextField> createState() => _LinuxTextFieldState();
}

class _LinuxTextFieldState extends State<LinuxTextField> {
  @override
  Widget build(BuildContext context) {
    return TextField(
      controller: widget.controller,
      onChanged: widget.onChanged,
      onSubmitted: (_) => widget.onSubmitted?.call(),
      maxLines: widget.obscureText ? 1 : widget.maxLines,
      minLines: widget.minLines,
      obscureText: widget.obscureText,
      keyboardType: widget.keyboardType,
      maxLength: widget.maxLength,
      enabled: widget.enabled,
      decoration: InputDecoration(
        labelText: widget.label,
        hintText: widget.placeholder,
        errorText: widget.errorText,
        border: _getBorder(),
        enabledBorder: _getBorder(),
        focusedBorder: _getFocusedBorder(),
        errorBorder: _getErrorBorder(),
        focusedErrorBorder: _getErrorBorder(),
        contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        suffixIcon: widget.errorText != null ? const Icon(Icons.error, color: Colors.red) : null,
      ),
    );
  }

  InputBorder _getBorder() {
    switch (widget.borderStyle) {
      case PlatformTextFieldBorderStyle.rounded:
        return OutlineInputBorder(borderRadius: BorderRadius.circular(8));
      case PlatformTextFieldBorderStyle.square:
        return const OutlineInputBorder();
      case PlatformTextFieldBorderStyle.none:
        return const UnderlineInputBorder();
    }
  }

  InputBorder _getFocusedBorder() {
    const color = Colors.blue;
    switch (widget.borderStyle) {
      case PlatformTextFieldBorderStyle.rounded:
        return OutlineInputBorder(
          borderRadius: BorderRadius.circular(8),
          borderSide: const BorderSide(color: color, width: 2),
        );
      case PlatformTextFieldBorderStyle.square:
        return const OutlineInputBorder(borderSide: BorderSide(color: color, width: 2));
      case PlatformTextFieldBorderStyle.none:
        return const UnderlineInputBorder(borderSide: BorderSide(color: color, width: 2));
    }
  }

  InputBorder _getErrorBorder() {
    const color = Colors.red;
    switch (widget.borderStyle) {
      case PlatformTextFieldBorderStyle.rounded:
        return OutlineInputBorder(
          borderRadius: BorderRadius.circular(8),
          borderSide: const BorderSide(color: color),
        );
      case PlatformTextFieldBorderStyle.square:
        return const OutlineInputBorder(borderSide: BorderSide(color: color));
      case PlatformTextFieldBorderStyle.none:
        return const UnderlineInputBorder(borderSide: BorderSide(color: color));
    }
  }
}

class LinuxGroupBox extends PlatformGroupBox {
  const LinuxGroupBox({
    super.title,
    required super.child,
    super.padding = const EdgeInsets.all(16.0),
    super.backgroundColor,
    super.borderColor,
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      color: backgroundColor,
      elevation: 2,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(8),
        side: BorderSide(color: borderColor ?? Colors.grey[300] ?? Colors.grey, width: 1),
      ),
      child: Padding(
        padding: padding,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            if (title != null) ...[Text(title!, style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 14)), const SizedBox(height: 12)],
            child,
          ],
        ),
      ),
    );
  }
}

class LinuxDialog extends PlatformDialog {
  const LinuxDialog({
    required super.title,
    super.message,
    super.content,
    super.actions = const [],
    super.barrierDismissible = true,
    super.constraints,
    super.onDismissed,
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      constraints: constraints,
      title: Text(title),
      content: content?.call(context),
      actions: [
        ...actions.map((action) {
          final isDefault = action.style == PlatformDialogActionStyle.primary;
          return ElevatedButton(
            onPressed: action.onPressed != null
                ? () {
                    action.onPressed?.call();
                    Navigator.of(context).pop();
                  }
                : null,
            style: ElevatedButton.styleFrom(
              backgroundColor: isDefault ? Colors.blue : Colors.grey[300],
              foregroundColor: isDefault ? Colors.white : Colors.black87,
            ),
            child: Text(action.label),
          );
        }),
      ],
    );
  }
}

class LinuxProgressIndicator extends PlatformProgressIndicator {
  const LinuxProgressIndicator({super.value, super.label, super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        if (label != null) ...[Text(label!, style: const TextStyle(fontSize: 12)), const SizedBox(height: 8)],
        LinearProgressIndicator(
          value: value,
          minHeight: 8,
          backgroundColor: Colors.grey[300],
          valueColor: const AlwaysStoppedAnimation<Color>(Colors.blue),
        ),
      ],
    );
  }
}

class LinuxCircularProgressIndicator extends PlatformCircularProgressIndicator {
  const LinuxCircularProgressIndicator({super.value, super.size, super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        SizedBox(
          width: size ?? 16,
          height: size ?? 16,
          child: CircularProgressIndicator(value: value),
        ),
      ],
    );
  }
}

class LinuxPopupMenuItem<T> extends PlatformPopupMenuItem<T> {
  const LinuxPopupMenuItem({required super.value, required super.label});
}

class LinuxPopupMenu<T> extends PlatformPopupMenu<T> {
  const LinuxPopupMenu({required super.items, super.label, super.selectedItem, super.onSelected, super.style, super.key});

  @override
  Widget build(BuildContext context) {
    return DropdownMenu<T>(
      onSelected: onSelected,
      label: label,
      dropdownMenuEntries: items.map((item) => DropdownMenuEntry<T>(value: item.value, label: item.label)).toList(),
      initialSelection: selectedItem,
    );
  }
}

class LinuxSwitch extends PlatformSwitch {
  const LinuxSwitch({required super.checked, required super.onChanged, super.key});

  @override
  Widget build(BuildContext context) {
    return Switch(value: checked, onChanged: onChanged);
  }
}

class LinuxSlider extends PlatformSlider {
  const LinuxSlider({required super.value, required super.min, required super.max, required super.onChanged, super.divisions, super.key});

  @override
  Widget build(BuildContext context) {
    return Slider(value: value, min: min, max: max, divisions: divisions, onChanged: onChanged);
  }
}

class LinuxCheckBox extends PlatformCheckbox {
  const LinuxCheckBox({required super.value, required super.onChanged, super.key});

  @override
  Widget build(BuildContext context) {
    return Checkbox.adaptive(value: value, onChanged: onChanged);
  }
}
