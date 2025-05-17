#!/bin/zsh

# Exit on error
set -e

# Check if ImageMagick is installed
if ! command -v magick &> /dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi

input_file=$1
output_directory=$2

# output_directory will be the same as input file directory
# Create output directory if it doesn't exist
mkdir -p "$output_directory"

# output_file will be the input file name with .bmp extension
output_file="${output_directory}/$(basename "$input_file" .jpg).bmp"


if [ -z "$input_file" ] || [ -z "$output_file" ]; then
    echo "Usage: $0 <input_file> <output_file>"
    exit 1
fi
if [ ! -f "$input_file" ]; then
    echo "Input file not found!"
    exit 1
fi

magick $input_file -resize 800x480^ -gravity Center -extent 800x480 -dither FloydSteinberg -remap pattern:gray50 -auto-orient .tmp.gif
magick .tmp.gif -alpha Off $output_file
rm .tmp.gif
if [ $? -eq 0 ]; then
    echo "Image processing completed successfully. Output file: $output_file"
else
    echo "Image processing failed."
fi

set +e

exit 0
# End of script