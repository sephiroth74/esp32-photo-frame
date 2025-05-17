import os
import sys
import pathlib
import re

INPUT_FILE = "warning_icon_196x196.h"
OUTPUT_FILE = "warning_icon_196x196_reformatted.h"
ITEMS_PER_LINE = 25

# move to the directory where the script is located
script_path = pathlib.Path(__file__).resolve()
script_dir = script_path.parent
os.chdir(script_dir)

def main():
    with open(INPUT_FILE, "r") as f:
        content = f.read()

    # Find the array initializer
    match = re.search(r'(\{)([\s\S]*)(\})', content)
    if not match:
        print("Array initializer not found.")
        return

    before = content[:match.start(1)+1]
    array_content = match.group(2)
    after = content[match.end(3)-1:]

    # Remove comments and newlines, split by comma
    items = [item.strip() for item in array_content.replace('\n', '').split(',') if item.strip()]

    # Reformat with 25 items per line
    lines = []
    for i in range(0, len(items), ITEMS_PER_LINE):
        lines.append('  ' + ', '.join(items[i:i+ITEMS_PER_LINE]))

    # Write output
    with open(OUTPUT_FILE, "w") as f:
        f.write(before + '\n')
        f.write(',\n'.join(lines))
        f.write('\n' + after)

    print(f"Reformatted array written to {OUTPUT_FILE}")

if __name__ == "__main__":
    main()