#!/usr/bin/env python3
"""
Convert 6-color binary file to PROGMEM header file.
Takes a .bin file from the photo processor and creates a C header
for inclusion in the firmware.
"""

import os
import sys

def read_binary_file(bin_path):
    """Read binary file"""
    print(f"Reading binary file: {bin_path}")

    with open(bin_path, 'rb') as f:
        data = f.read()

    print(f"File size: {len(data)} bytes")
    return bytearray(data)

def analyze_bitmap(data):
    """Analyze bitmap content"""
    print("\nBitmap Analysis:")
    print(f"  Total size: {len(data)} bytes")
    print(f"  Expected size for 800x480: {800 * 480} bytes")

    if len(data) != 800 * 480:
        print(f"  WARNING: Size mismatch!")

    # Show first 256 bytes
    print("\n  First 256 bytes:")
    for i in range(min(256, len(data))):
        if i % 16 == 0:
            print(f"\n  {i:04X}: ", end='')
        print(f"{data[i]:02X} ", end='')
    print()

    # Count unique values
    unique_values = set(data)
    print(f"\n  Unique byte values: {len(unique_values)}")

    # 6-color palette
    color_names = {
        0xFF: 'WHITE',
        0x00: 'BLACK',
        0xE0: 'RED',
        0x1C: 'GREEN',
        0x03: 'BLUE',
        0xFC: 'YELLOW',
    }

    # Count occurrences
    from collections import Counter
    counter = Counter(data)
    print("\n  Color distribution:")
    for value in sorted(unique_values):
        count = counter[value]
        percentage = (count / len(data)) * 100
        color_name = color_names.get(value, 'UNKNOWN')
        print(f"    0x{value:02X} ({color_name:7s}): {count:7d} ({percentage:5.2f}%)")

def write_header_file(data, output_path, array_name):
    """Write data as C header file with PROGMEM"""
    print(f"\nWriting header file: {output_path}")

    with open(output_path, 'w') as f:
        # Header guard
        guard_name = f"__{array_name.upper()}_H__"
        f.write(f"#ifndef {guard_name}\n")
        f.write(f"#define {guard_name}\n\n")

        # Include PROGMEM
        f.write("#if defined(ESP8266) || defined(ESP32)\n")
        f.write("#include <pgmspace.h>\n")
        f.write("#else\n")
        f.write("#include <avr/pgmspace.h>\n")
        f.write("#endif\n\n")

        # Comment
        f.write(f"// 6-Color bitmap for 800x480 display\n")
        f.write(f"// Size: {len(data)} bytes\n")
        f.write(f"// Format: 1 byte per pixel (6-color palette)\n")
        f.write(f"// Colors: WHITE=0xFF, BLACK=0x00, RED=0xE0, GREEN=0x1C, BLUE=0x03, YELLOW=0xFC\n\n")

        # Array declaration
        f.write(f"const unsigned char {array_name}[{len(data)}] PROGMEM = {{\n")

        # Write data in hex format, 16 bytes per line
        bytes_per_line = 16
        for i in range(0, len(data), bytes_per_line):
            chunk = data[i:i+bytes_per_line]
            hex_bytes = ', '.join(f'0x{b:02X}' for b in chunk)

            # Add comment with offset
            if i % (bytes_per_line * 16) == 0:
                f.write(f"  // Offset 0x{i:04X}\n")

            f.write(f"  {hex_bytes},\n")

        f.write("};\n\n")
        f.write(f"#endif // {guard_name}\n")

    print(f"Written {len(data)} bytes")

if __name__ == "__main__":
    # Input and output paths
    input_file = "/Users/alessandro/Library/CloudStorage/GoogleDrive-alessandro.crugnola@gmail.com/My Drive/Arduino/photo-frame/images/colored/bin/6c_MjAyMzA0MDJfMTcwODAz.bin"

    script_dir = os.path.dirname(os.path.abspath(__file__))
    platformio_dir = os.path.dirname(script_dir)

    # Output files
    output_header = os.path.join(platformio_dir, "include", "test_photo_6c.h")
    output_bin = os.path.join(platformio_dir, "data", "test_photo_6c.bin")

    print("=" * 60)
    print("6-Color Binary to PROGMEM Converter")
    print("=" * 60)
    print()

    # Read binary file
    if not os.path.exists(input_file):
        print(f"ERROR: Input file not found: {input_file}")
        sys.exit(1)

    bitmap_data = read_binary_file(input_file)

    # Analyze
    analyze_bitmap(bitmap_data)

    # Write header file
    print()
    write_header_file(bitmap_data, output_header, "test_photo_6c")

    # Also copy the .bin file to data folder for SD card test
    print(f"\nCopying to data folder: {output_bin}")
    with open(output_bin, 'wb') as f:
        f.write(bitmap_data)
    print(f"Copied {len(bitmap_data)} bytes")

    print()
    print("=" * 60)
    print("Conversion complete!")
    print("=" * 60)
    print()
    print("Files created:")
    print(f"  1. PROGMEM header: {output_header}")
    print(f"  2. Binary file:    {output_bin}")
    print()
    print("Next steps:")
    print("  1. Copy .bin to SD card:")
    print(f"     cp {output_bin} /Volumes/YOUR_SD_CARD/")
    print("  2. Include header in display_debug.cpp:")
    print('     #include "test_photo_6c.h"')
    print("  3. Run test 'P' to compare PROGMEM vs SD")
