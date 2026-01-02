#!/usr/bin/env python3
"""
Generate a test pattern BIN file for 6-color e-paper display testing.
Creates an 800x480 image with 6 colored squares matching the display palette.
"""

# Display dimensions
WIDTH = 800
HEIGHT = 480

# 6-color palette matching color2Epd() function
# From renderer.cpp lines 1734-1751
COLORS = {
    'WHITE':  0xFF,  # (255,255,255)
    'BLACK':  0x00,  # (0,0,0)
    'RED':    0xE0,  # (255,0,0) → 224
    'GREEN':  0x1C,  # (0,255,0) → 28
    'BLUE':   0x03,  # (0,0,255) → 3
    'YELLOW': 0xFC,  # (255,255,0) → 252
}

# Create output file
output_path = "/Users/alessandro/Documents/git/sephiroth74/arduino/esp32-photo-frame/platformio/test_pattern_6c.bin"

# Calculate grid: 2 rows x 3 columns
cols = 3
rows = 2
square_width = WIDTH // cols
square_height = HEIGHT // rows

# Color order for squares (top-left to bottom-right)
color_order = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'WHITE', 'BLACK']

print(f"Generating test pattern: {WIDTH}x{HEIGHT}")
print(f"Grid: {rows} rows x {cols} columns")
print(f"Square size: {square_width}x{square_height}")
print(f"Output: {output_path}")

# Generate the binary data
data = bytearray(WIDTH * HEIGHT)

for y in range(HEIGHT):
    for x in range(WIDTH):
        # Determine which square this pixel belongs to
        col_idx = x // square_width
        row_idx = y // square_height

        # Clamp to valid indices
        col_idx = min(col_idx, cols - 1)
        row_idx = min(row_idx, rows - 1)

        # Get color index (0-5)
        color_idx = row_idx * cols + col_idx
        color_name = color_order[color_idx]
        color_value = COLORS[color_name]

        # Write pixel
        pixel_idx = y * WIDTH + x
        data[pixel_idx] = color_value

# Write to file
with open(output_path, 'wb') as f:
    f.write(data)

print(f"\nTest pattern generated successfully!")
print(f"File size: {len(data)} bytes")
print(f"\nColor layout (800x480):")
print(f"┌─────────────┬─────────────┬─────────────┐")
print(f"│  RED (0xE0) │ GREEN (0x1C)│ BLUE (0x03) │ 240px")
print(f"├─────────────┼─────────────┼─────────────┤")
print(f"│YELLOW(0xFC) │ WHITE (0xFF)│ BLACK(0x00) │ 240px")
print(f"└─────────────┴─────────────┴─────────────┘")
print(f"  266px each     266px each    268px each")
print(f"\nCopy this file to SD card: /test_pattern_6c.bin")
