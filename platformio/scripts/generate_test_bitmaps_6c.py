#!/usr/bin/env python3
"""
Generate 6-color test pattern include files for e-paper display testing.
Creates C array data that can be included directly into compiled firmware.
"""

WIDTH = 800
HEIGHT = 480

# 6-color palette matching color2Epd() function
COLORS = {
    'WHITE':  0xFF,
    'BLACK':  0x00,
    'RED':    0xE0,
    'GREEN':  0x1C,
    'BLUE':   0x03,
    'YELLOW': 0xFC,
}

def write_c_array(data, output_path, bytes_per_line=16):
    """Write byte array as C include file"""
    with open(output_path, 'w') as f:
        for i in range(0, len(data), bytes_per_line):
            chunk = data[i:i+bytes_per_line]
            hex_bytes = ', '.join(f'0x{b:02X}' for b in chunk)
            f.write(f'    {hex_bytes},\n')
    print(f"Generated: {output_path} ({len(data)} bytes)")

def generate_6c_squares():
    """Generate 2x3 grid of colored squares"""
    print(f"Generating 6-color squares pattern: {WIDTH}x{HEIGHT}")

    data = bytearray(WIDTH * HEIGHT)
    cols = 3
    rows = 2
    square_width = WIDTH // cols
    square_height = HEIGHT // rows

    color_order = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'WHITE', 'BLACK']

    for y in range(HEIGHT):
        for x in range(WIDTH):
            col_idx = min(x // square_width, cols - 1)
            row_idx = min(y // square_height, rows - 1)
            color_idx = row_idx * cols + col_idx
            color_value = COLORS[color_order[color_idx]]
            data[y * WIDTH + x] = color_value

    output = "../include/test_bitmaps_color_6c_squares.inc"
    write_c_array(data, output)

def generate_6c_vertical_bars():
    """Generate vertical color bars: RED|GREEN|BLUE|YELLOW|WHITE|BLACK"""
    print(f"Generating 6-color vertical bars: {WIDTH}x{HEIGHT}")

    data = bytearray(WIDTH * HEIGHT)
    colors = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'WHITE', 'BLACK']
    bar_width = WIDTH // len(colors)

    for y in range(HEIGHT):
        for x in range(WIDTH):
            bar_idx = min(x // bar_width, len(colors) - 1)
            color_value = COLORS[colors[bar_idx]]
            data[y * WIDTH + x] = color_value

    output = "../include/test_bitmaps_color_6c_vbars.inc"
    write_c_array(data, output)

def generate_6c_horizontal_bands():
    """Generate horizontal color bands"""
    print(f"Generating 6-color horizontal bands: {WIDTH}x{HEIGHT}")

    data = bytearray(WIDTH * HEIGHT)
    colors = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'WHITE', 'BLACK']
    band_height = HEIGHT // len(colors)

    for y in range(HEIGHT):
        band_idx = min(y // band_height, len(colors) - 1)
        color_value = COLORS[colors[band_idx]]
        for x in range(WIDTH):
            data[y * WIDTH + x] = color_value

    output = "../include/test_bitmaps_color_6c_hbands.inc"
    write_c_array(data, output)

def generate_6c_checkerboard():
    """Generate checkerboard pattern alternating all 6 colors"""
    print(f"Generating 6-color checkerboard: {WIDTH}x{HEIGHT}")

    data = bytearray(WIDTH * HEIGHT)
    colors = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'WHITE', 'BLACK']
    square_size = 80  # 80x80 pixel squares

    for y in range(HEIGHT):
        for x in range(WIDTH):
            # Calculate which square we're in
            square_x = x // square_size
            square_y = y // square_size
            # Use checkerboard pattern to select color
            color_idx = ((square_x + square_y) * 3) % len(colors)
            color_value = COLORS[colors[color_idx]]
            data[y * WIDTH + x] = color_value

    output = "../include/test_bitmaps_color_6c_checker.inc"
    write_c_array(data, output)

if __name__ == "__main__":
    print("=" * 60)
    print("6-Color Test Pattern Generator for E-Paper Display")
    print("=" * 60)
    print()

    generate_6c_squares()
    generate_6c_vertical_bars()
    generate_6c_horizontal_bands()
    generate_6c_checkerboard()

    print()
    print("All 6-color test patterns generated successfully!")
    print(f"Total data size: {WIDTH * HEIGHT * 4} bytes")
    print()
    print("Include in your project with:")
    print('  #include "test_bitmaps_color.h"')
