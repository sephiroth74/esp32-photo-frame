#!/usr/bin/env python3
"""
Convert 6-color binary file (1 byte/pixel) to native display format (2 pixels/byte).
Takes a .bin file and creates a native format file for direct writeNative() usage.
"""

import os
import sys

def convert_to_native_6c(input_file, output_file):
    """
    Convert 6-color format to native display format.

    Input format: 1 byte per pixel
        0xFF = WHITE
        0x00 = BLACK
        0xE0 = RED
        0x1C = GREEN
        0x03 = BLUE
        0xFC = YELLOW

    Native format: 2 pixels per byte (3 bits per pixel, packed in nibbles)
    Input format for writeNative() before _convert_to_native():
        0x0 = BLACK
        0x1 = WHITE
        0x2 = GREEN
        0x3 = BLUE
        0x4 = RED
        0x5 = YELLOW

    The library's _convert_to_native() then converts to controller format.
    """

    print(f"Reading input file: {input_file}")

    with open(input_file, 'rb') as f:
        data = f.read()

    print(f"Input size: {len(data)} bytes")

    if len(data) != 800 * 480:
        print(f"WARNING: Expected 384000 bytes for 800x480, got {len(data)}")

    # Color mapping from file format to writeNative() input format
    # These values will be converted by _convert_to_native() in the library
    color_map = {
        0xFF: 0x1,  # WHITE
        0x00: 0x0,  # BLACK
        0xE0: 0x4,  # RED
        0x1C: 0x2,  # GREEN
        0x03: 0x3,  # BLUE
        0xFC: 0x5,  # YELLOW
    }

    # Convert to native format (2 pixels per byte)
    native_data = bytearray()

    for i in range(0, len(data), 2):
        # Get two pixels
        pixel1 = data[i]
        pixel2 = data[i + 1] if i + 1 < len(data) else 0xFF

        # Convert to native nibbles
        nibble1 = color_map.get(pixel1, 0x1)  # Default to WHITE if unknown
        nibble2 = color_map.get(pixel2, 0x1)

        # Pack into one byte (high nibble = pixel1, low nibble = pixel2)
        packed = (nibble1 << 4) | nibble2
        native_data.append(packed)

    print(f"Native size: {len(native_data)} bytes ({len(data) / len(native_data):.1f}x compression)")

    # Analyze distribution
    from collections import Counter
    counter = Counter(native_data)

    # Reverse color map for display
    color_names = {
        0x0: 'BLACK',
        0x1: 'WHITE',
        0x2: 'GREEN',
        0x3: 'BLUE',
        0x4: 'RED',
        0x5: 'YELLOW',
    }

    print("\nMost common packed bytes:")
    for value, count in counter.most_common(10):
        nibble1 = (value >> 4) & 0xF
        nibble2 = value & 0xF
        percentage = (count / len(native_data)) * 100
        color1 = color_names.get(nibble1, 'UNKNOWN')
        color2 = color_names.get(nibble2, 'UNKNOWN')
        print(f"  0x{value:02X} ({color1:6s}, {color2:6s}): {count:7d} ({percentage:5.2f}%)")

    # Write output
    print(f"\nWriting native format: {output_file}")
    with open(output_file, 'wb') as f:
        f.write(native_data)

    print(f"Written {len(native_data)} bytes")

    # Show first 64 bytes for verification
    print("\nFirst 64 bytes (hex):")
    for i in range(min(64, len(native_data))):
        if i % 16 == 0:
            print(f"\n  {i:04X}: ", end='')
        print(f"{native_data[i]:02X} ", end='')
    print("\n")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    platformio_dir = os.path.dirname(script_dir)

    # Input and output files
    input_file = os.path.join(platformio_dir, "data", "test_photo_6c.bin")
    output_file = os.path.join(platformio_dir, "data", "test_photo_6c_native.bin")

    print("=" * 70)
    print("6-Color to Native Format Converter")
    print("=" * 70)
    print()

    if not os.path.exists(input_file):
        print(f"ERROR: Input file not found: {input_file}")
        sys.exit(1)

    convert_to_native_6c(input_file, output_file)

    print()
    print("=" * 70)
    print("Conversion complete!")
    print("=" * 70)
    print()
    print("Files:")
    print(f"  Input:  {input_file}")
    print(f"  Output: {output_file}")
    print()
    print("Next steps:")
    print("  1. Copy to SD card:")
    print(f"     cp {output_file} /Volumes/YOUR_SD_CARD/")
    print("  2. Run test in display_debug (option 9)")
    print("  3. Compare performance with old method")
