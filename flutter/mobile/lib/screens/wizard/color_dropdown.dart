import 'package:flutter/material.dart';

class ColorDropdown extends StatelessWidget {
  final Color value;
  final ValueChanged<Color> onChanged;

  const ColorDropdown({super.key, required this.value, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    final colorMap = {
      Colors.white: 'White',
      Colors.black: 'Black',
      Colors.red: 'Red',
      Colors.green: 'Green',
      Colors.blue: 'Blue',
      Colors.yellow: 'Yellow',
    };

    return DropdownButton<Color>(
      value: value,
      isExpanded: true,
      items: colorMap.entries
          .map(
            (entry) => DropdownMenuItem(
              value: entry.key,
              child: Row(
                children: [
                  Container(
                    width: 20,
                    height: 20,
                    decoration: BoxDecoration(
                      color: entry.key,
                      border: Border.all(color: Colors.grey),
                      borderRadius: BorderRadius.circular(4),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Text(entry.value),
                ],
              ),
            ),
          )
          .toList(),
      onChanged: (color) {
        if (color != null) {
          onChanged(color);
        }
      },
    );
  }
}
