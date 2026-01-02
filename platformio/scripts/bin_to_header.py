#!/usr/bin/env python3
"""
Convert binary image file to C++ header with PROGMEM array.
"""

import sys
import os

def bin_to_header(input_file, output_file, array_name="test_image"):
    """Convert binary file to C++ header with PROGMEM array."""

    # Read binary file
    with open(input_file, 'rb') as f:
        data = f.read()

    file_size = len(data)
    print(f"Input file: {input_file}")
    print(f"File size: {file_size} bytes")

    # Generate header file
    with open(output_file, 'w') as f:
        # Write header guard
        guard_name = os.path.basename(output_file).replace('.', '_').upper()
        f.write(f"#ifndef {guard_name}\n")
        f.write(f"#define {guard_name}\n\n")

        # Write includes
        f.write("#include <Arduino.h>\n\n")

        # Write array declaration
        f.write(f"// Binary image data: {file_size} bytes\n")
        f.write(f"// Native 6-color format: 2 pixels per byte\n")
        f.write(f"// Image size: 800x480\n")
        f.write(f"const uint8_t {array_name}[{file_size}] PROGMEM = {{\n")

        # Write data in rows of 16 bytes
        for i in range(0, file_size, 16):
            chunk = data[i:i+16]
            hex_values = ', '.join(f'0x{b:02X}' for b in chunk)
            f.write(f"    {hex_values}")
            if i + 16 < file_size:
                f.write(",\n")
            else:
                f.write("\n")

        f.write("};\n\n")

        # Write size constant
        f.write(f"const size_t {array_name}_size = {file_size};\n\n")

        # Write header guard end
        f.write(f"#endif // {guard_name}\n")

    print(f"Output file: {output_file}")
    print(f"Array name: {array_name}")
    print(f"Done!")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: bin_to_header.py <input.bin> <output.h> [array_name]")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    array_name = sys.argv[3] if len(sys.argv) > 3 else "test_image"

    if not os.path.exists(input_file):
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)

    bin_to_header(input_file, output_file, array_name)
