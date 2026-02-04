import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:flutter/material.dart';

import '../abstractions/widget_abstractions.dart';

class MacOSButton extends PlatformButton {
  const MacOSButton({
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

  AppKitButtonType _mapStyle() {
    switch (style) {
      case PlatformButtonStyle.primary:
        return AppKitButtonType.primary;
      case PlatformButtonStyle.secondary:
        return AppKitButtonType.secondary;
      case PlatformButtonStyle.danger:
        return AppKitButtonType.primary;
      case PlatformButtonStyle.outline:
        return AppKitButtonType.secondary;
    }
  }

  AppKitControlSize _mapSize() {
    switch (size) {
      case PlatformButtonSize.small:
        return AppKitControlSize.small;
      case PlatformButtonSize.medium:
        return AppKitControlSize.regular;
      case PlatformButtonSize.large:
        return AppKitControlSize.large;
    }
  }

  @override
  Widget build(BuildContext context) {
    final content = isLoading
        ? Row(mainAxisSize: MainAxisSize.min, children: const [AppKitProgressCircle(size: 14), SizedBox(width: 8), Text('Loading...')])
        : Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (icon != null) ...[icon!, const SizedBox(width: 6)],
              Text(label),
            ],
          );

    final button = AppKitButton(size: _mapSize(), type: _mapStyle(), onTap: enabled ? onPressed : null, child: content);

    if (tooltip != null && tooltip!.isNotEmpty) {
      return Tooltip(message: tooltip!, child: button);
    }

    return button;
  }
}

class MacOSTextField extends PlatformTextField {
  const MacOSTextField({
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
  State<MacOSTextField> createState() => _MacOSTextFieldState();
}

class _MacOSTextFieldState extends State<MacOSTextField> {
  late final TextEditingController _internalController;
  TextEditingController get _controller => widget.controller ?? _internalController;

  @override
  void initState() {
    super.initState();
    _internalController = TextEditingController();
  }

  AppKitTextFieldBorderStyle _mapBorder() {
    switch (widget.borderStyle) {
      case PlatformTextFieldBorderStyle.rounded:
        return AppKitTextFieldBorderStyle.rounded;
      case PlatformTextFieldBorderStyle.square:
        return AppKitTextFieldBorderStyle.line;
      case PlatformTextFieldBorderStyle.none:
        return AppKitTextFieldBorderStyle.none;
    }
  }

  @override
  void dispose() {
    if (widget.controller == null) {
      _internalController.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (widget.label != null) ...[
          Text(widget.label!, style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600)),
          const SizedBox(height: 6),
        ],
        AppKitTextField(
          borderStyle: _mapBorder(),
          controller: _controller,
          maxLines: widget.maxLines,
          placeholder: widget.placeholder,
          onChanged: widget.onChanged,
          onSubmitted: (value) => widget.onSubmitted?.call(),
          enabled: widget.enabled,
        ),
        if (widget.errorText != null) ...[
          const SizedBox(height: 4),
          Text(widget.errorText!, style: const TextStyle(color: Colors.red, fontSize: 11)),
        ],
      ],
    );
  }
}

class MacOSGroupBox extends PlatformGroupBox {
  const MacOSGroupBox({
    super.title,
    required super.child,
    super.padding = const EdgeInsets.all(16.0),
    super.backgroundColor,
    super.borderColor,
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    return AppKitGroupBox(
      style: AppKitGroupBoxStyle.roundedScrollBox,
      child: Container(padding: padding, color: backgroundColor, child: child),
    );
  }
}

class MacOSDialog extends PlatformDialog {
  const MacOSDialog({
    required super.title,
    super.message,
    super.content,
    super.actions = const [],
    super.barrierDismissible = true,
    super.onDismissed,
    super.constraints,
    super.key,
  });

  AppKitButton _mapAction(BuildContext context, PlatformDialogAction action) {
    final type = action.style == PlatformDialogActionStyle.primary ? AppKitButtonType.primary : AppKitButtonType.secondary;
    return AppKitButton(
      size: AppKitControlSize.regular,
      type: type,
      onTap: action.onPressed != null ? () => {Navigator.of(context).pop(), action.onPressed!()} : null,
      child: Text(action.label),
    );
  }

  @override
  Widget build(BuildContext context) {
    final primary = actions.isNotEmpty
        ? _mapAction(context, actions.first)
        : _mapAction(context, PlatformDialogAction(label: 'OK', onPressed: () => Navigator.of(context).pop()));
    final secondary = actions.length > 1 ? _mapAction(context, actions[1]) : null;

    return AppKitDialog(
      constraints: const BoxConstraints(minWidth: 450, maxWidth: 450),
      title: Text(title),
      message: content!,
      primaryButton: primary,
      secondaryButton: secondary,
    );
  }
}

class MacOSProgressIndicator extends PlatformProgressIndicator {
  const MacOSProgressIndicator({super.value, super.label, super.visible = true, super.key});

  @override
  Widget build(BuildContext context) {
    if (!visible) return const SizedBox.shrink();

    if (value == null) {
      return Row(
        children: [
          const AppKitProgressCircle(size: 14),
          if (label != null) ...[const SizedBox(width: 8), Text(label!)],
        ],
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        AppKitProgressBar(value: value!),
        if (label != null) ...[const SizedBox(height: 6), Text(label!)],
      ],
    );
  }
}

class MacOSCircularProgressIndicator extends PlatformCircularProgressIndicator {
  const MacOSCircularProgressIndicator({super.value, super.size, super.key});

  @override
  Widget build(BuildContext context) {
    return AppKitProgressCircle(value: value, size: size);
  }
}

class MacOSPopupMenuItem<T> extends PlatformPopupMenuItem<T> {
  const MacOSPopupMenuItem({required super.value, required super.label});
}

class MacOSPopupMenu<T> extends PlatformPopupMenu<T> {
  const MacOSPopupMenu({required super.items, required super.selectedItem, required super.onSelected, super.style, super.key});

  @override
  Widget build(BuildContext context) {
    final style = switch (this.style) {
      PlatformPopupMenuStyle.plain => AppKitPopupButtonStyle.plain,
      PlatformPopupMenuStyle.bevel => AppKitPopupButtonStyle.bevel,
      PlatformPopupMenuStyle.push => AppKitPopupButtonStyle.push,
      _ => AppKitPopupButtonStyle.bevel,
    };

    return AppKitPopupButton<T>(
      selectedItem: selectedItem,
      style: style,
      items: items.map((item) => AppKitContextMenuItem<T>(value: item.value, child: Text(item.label))).toList(),
      onItemSelected: onSelected != null ? (value) => onSelected!(value) : null,
    );
  }
}

class MacOSSwitch extends PlatformSwitch {
  const MacOSSwitch({required super.checked, required super.onChanged, super.key});

  @override
  Widget build(BuildContext context) {
    return AppKitSwitch(checked: checked, onChanged: onChanged);
  }
}

class MacOSSlider extends PlatformSlider {
  const MacOSSlider({required super.value, required super.min, required super.max, required super.onChanged, super.stops, super.key});

  @override
  Widget build(BuildContext context) {
    return AppKitSlider(value: value, min: min, max: max, stops: stops ?? [], onChanged: onChanged);
  }
}

class MacOSCheckbox extends PlatformCheckbox {
  const MacOSCheckbox({required super.value, required super.onChanged, super.key});

  @override
  Widget build(BuildContext context) {
    return AppKitCheckbox(value: value, onChanged: onChanged);
  }
}
