#!/usr/bin/env python3
"""
Generate 7-color test pattern include files for e-paper display testing.
Creates C array data that can be included directly into compiled firmware.
Format: RGB565 (16-bit per pixel)
"""

WIDTH = 800
HEIGHT = 480

# 7-color palette (RGB values)
COLORS_RGB = {
    'BLACK':  (0, 0, 0),
    'WHITE':  (255, 255, 255),
    'GREEN':  (0, 255, 0),
    'BLUE':   (0, 0, 255),
    'RED':    (255, 0, 0),
    'YELLOW': (255, 255, 0),
    'ORANGE': (255, 165, 0),
}

def rgb_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 (16-bit)"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    rgb565 = (r5 << 11) | (g6 << 5) | b5
    return rgb565

def write_c_array_16bit(data, output_path, values_per_line=8):
    """Write 16-bit array as C include file"""
    with open(output_path, 'w') as f:
        for i in range(0, len(data), values_per_line * 2):
            hex_values = []
            for j in range(0, values_per_line * 2, 2):
                if i + j + 1 < len(data):
                    # Little-endian: low byte, high byte
                    low = data[i + j]
                    high = data[i + j + 1]
                    hex_values.append(f'0x{low:02X}, 0x{high:02X}')
            f.write(f'    {", ".join(hex_values)},\n')
    print(f"Generated: {output_path} ({len(data)} bytes)")

def generate_7c_squares():
    """Generate grid of 7 colored squares"""
    print(f"Generating 7-color squares pattern: {WIDTH}x{HEIGHT}")

    data = bytearray(WIDTH * HEIGHT * 2)  # 2 bytes per pixel
    colors = ['RED', 'ORANGE', 'YELLOW', 'GREEN', 'BLUE', 'WHITE', 'BLACK']

    # Layout: 4 columns x 2 rows
    cols = 4
    rows = 2
    square_width = WIDTH // cols
    square_height = HEIGHT // rows

    for y in range(HEIGHT):
        for x in range(WIDTH):
            col_idx = min(x // square_width, cols - 1)
            row_idx = min(y // square_height, rows - 1)
            color_idx = row_idx * cols + col_idx

            if color_idx < len(colors):
                r, g, b = COLORS_RGB[colors[color_idx]]
            else:
                r, g, b = COLORS_RGB['WHITE']  # Fill remaining with white

            rgb565 = rgb_to_rgb565(r, g, b)

            pixel_offset = (y * WIDTH + x) * 2
            data[pixel_offset] = rgb565 & 0xFF      # Low byte
            data[pixel_offset + 1] = (rgb565 >> 8) & 0xFF  # High byte

    output = "../include/test_bitmaps_color_7c_squares.inc"
    write_c_array_16bit(data, output)

def generate_7c_vertical_bars():
    """Generate vertical color bars for all 7 colors"""
    print(f"Generating 7-color vertical bars: {WIDTH}x{HEIGHT}")

    data = bytearray(WIDTH * HEIGHT * 2)
    colors = ['RED', 'ORANGE', 'YELLOW', 'GREEN', 'BLUE', 'WHITE', 'BLACK']
    bar_width = WIDTH // len(colors)

    for y in range(HEIGHT):
        for x in range(WIDTH):
            bar_idx = min(x // bar_width, len(colors) - 1)
            r, g, b = COLORS_RGB[colors[bar_idx]]
            rgb565 = rgb_to_rgb565(r, g, b)

            pixel_offset = (y * WIDTH + x) * 2
            data[pixel_offset] = rgb565 & 0xFF
            data[pixel_offset + 1] = (rgb565 >> 8) & 0xFF

    output = "../include/test_bitmaps_color_7c_vbars.inc"
    write_c_array_16bit(data, output)

if __name__ == "__main__":
    print("=" * 60)
    print("7-Color Test Pattern Generator for E-Paper Display")
    print("=" * 60)
    print()

    generate_7c_squares()
    generate_7c_vertical_bars()

    print()
    print("All 7-color test patterns generated successfully!")
    print(f"Total data size per pattern: {WIDTH * HEIGHT * 2} bytes")
    print()
    print("Include in your project with:")
    print('  #include "test_bitmaps_color.h"')
