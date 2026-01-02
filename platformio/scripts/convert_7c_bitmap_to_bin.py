#!/usr/bin/env python3
"""
Convert 7-color bitmap from Bitmaps7c800x480.h to binary file for SD card.
Extracts the byte array from the C header and saves it as a .bin file.
"""

import re
import sys

def extract_bitmap_from_header(header_path):
    """Extract bitmap data from C header file"""
    print(f"Reading header file: {header_path}")

    with open(header_path, 'r') as f:
        content = f.read()

    # Find the array declaration
    # Pattern: const unsigned char Bitmap7c800x480[384000] PROGMEM = { ... };
    match = re.search(r'const unsigned char Bitmap7c800x480\[(\d+)\] PROGMEM = \{[^}]*/\* 0X[^*]+\*/\s*([^}]+)\};', content, re.DOTALL)

    if not match:
        print("ERROR: Could not find bitmap array in header file")
        return None, 0

    size = int(match.group(1))
    data_str = match.group(2)

    print(f"Array size declared: {size} bytes")

    # Extract all hex values (0X39, 0X3A, etc.)
    hex_values = re.findall(r'0X([0-9A-Fa-f]{2})', data_str)

    print(f"Hex values found: {len(hex_values)}")

    if len(hex_values) != size:
        print(f"WARNING: Found {len(hex_values)} bytes, expected {size}")

    # Convert to bytes
    bitmap_data = bytearray([int(h, 16) for h in hex_values])

    return bitmap_data, size

def save_binary_file(data, output_path):
    """Save bitmap data as binary file"""
    print(f"Writing binary file: {output_path}")

    with open(output_path, 'wb') as f:
        f.write(data)

    print(f"Written {len(data)} bytes")

def analyze_bitmap(data):
    """Analyze bitmap content"""
    print("\nBitmap Analysis:")
    print(f"  Total size: {len(data)} bytes")
    print(f"  Expected size for 800x480: {800 * 480} bytes")

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
    print(f"  Values: {sorted([f'0x{v:02X}' for v in unique_values])}")

    # Count occurrences of most common values
    from collections import Counter
    counter = Counter(data)
    print("\n  Most common values:")
    for value, count in counter.most_common(10):
        percentage = (count / len(data)) * 100
        print(f"    0x{value:02X}: {count:7d} ({percentage:5.2f}%)")

if __name__ == "__main__":
    import os

    header_file = "/Users/alessandro/Documents/Arduino/libraries/GxEPD2/src/bitmaps/Bitmaps7c800x480.h"
    script_dir = os.path.dirname(os.path.abspath(__file__))
    platformio_dir = os.path.dirname(script_dir)
    output_file = os.path.join(platformio_dir, "data", "bitmap7c_800x480.bin")

    print("=" * 60)
    print("7-Color Bitmap to Binary Converter")
    print("=" * 60)
    print()

    # Extract bitmap from header
    bitmap_data, declared_size = extract_bitmap_from_header(header_file)

    if bitmap_data is None:
        print("ERROR: Failed to extract bitmap data")
        sys.exit(1)

    # Analyze the data
    analyze_bitmap(bitmap_data)

    # Save to binary file
    print()
    save_binary_file(bitmap_data, output_file)

    print()
    print("=" * 60)
    print("Conversion complete!")
    print("=" * 60)
    print()
    print("Next steps:")
    print("  1. Copy the .bin file to SD card root")
    print(f"     cp {output_file} /Volumes/YOUR_SD_CARD/")
    print("  2. Use test 'F' in display_debug to load and render from SD")
    print("  3. Compare with test '7' for PROGMEM rendering")
