import 'package:flutter/material.dart';

class AdjustmentSlider extends StatelessWidget {
  final String label;
  final double value;
  final double min;
  final double max;
  final ValueChanged<double> onChanged;
  final ValueChanged<double>? onChangeEnd;

  const AdjustmentSlider({
    super.key,
    required this.label,
    required this.value,
    required this.min,
    required this.max,
    required this.onChanged,
    this.onChangeEnd,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(label),
            Text(value.toStringAsFixed(2), style: Theme.of(context).textTheme.bodySmall),
          ],
        ),
        Slider(value: value, min: min, max: max, onChanged: onChanged, onChangeEnd: onChangeEnd),
        const SizedBox(height: 16),
      ],
    );
  }
}
