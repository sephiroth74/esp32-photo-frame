#!/bin/zsh

# This script converts a single image to BMP format with optional color palette and type.
#     echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>"
#     echo "Error: -palette argument is required when -type is 'color'."
# Usage: convert_h.sh -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>

# Exit on error
set -e

# check if pointsize is set as environment variable
if [ -z "$pointsize" ]; then
    pointsize=20
fi
# check if font is set as environment variable
if [ -z "$font" ]; then
    font='UbuntuMono-Nerd-Font-Bold'
fi
# check if annotate_background is set as environment variable
if [ -z "$annotate_background" ]; then
    annotate_background='#00000080'
fi

# Check if ImageMagick is installed
if ! command -v magick &>/dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi

# Default values
type=""
palette=""
input_file=""
output_dir=""

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
    -type)
        type="$2"
        shift 2
        ;;
    -palette)
        palette="$2"
        shift 2
        ;;
    -*)
        echo "Unknown option: $1"
        exit 1
        ;;
    *)
        # Positional arguments
        if [[ -z "$input_file" ]]; then
            input_file="$1"
        elif [[ -z "$output_dir" ]]; then
            output_dir="$1"
        else
            echo "Unexpected argument: $1"
            exit 1
        fi
        shift
        ;;
    esac
done

# Validate -type
if [[ -z "$type" ]]; then
    echo "Error: -type argument is required (grey or color)."
    exit 1
fi

if [[ "$type" != "grey" && "$type" != "color" ]]; then
    echo "Error: -type must be either 'grey' or 'color'."
    exit 1
fi

# If type is color, palette must be provided
if [[ "$type" == "color" && -z "$palette" ]]; then
    echo "Error: -palette argument is required when -type is 'color'."
    exit 1
fi

# echo "type: $type"
# echo "palette: $palette"
# echo "input_file: $input_file"
# echo "output_dir: $output_dir"

# Validate input and output directories
if [[ -z "$input_file" || -z "$output_dir" ]]; then
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>"
    exit 1
fi

# output_directory will be the same as input file directory
# Create output directory if it doesn't exist
mkdir -p "$output_dir"

# output_file will be the input file name with .bmp extension
output_file="${output_dir}/$(basename "$input_file" .jpg).bmp"

if [ -z "$input_file" ] || [ -z "$output_file" ]; then
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input> <output_directory>"
    exit 1
fi
if [ ! -f "$input_file" ]; then
    echo "Input file not found!"
    exit 1
fi

# extract date time from the input file name
exif_date=$(identify -format "%[EXIF:DateTimeOriginal]" "$input_file" 2>/dev/null)
# Check if the exif_date extraction was successful
if [ $? -eq 0 ] && [ -n "$exif_date" ]; then
    # Convert the date time to a format suitable for annotation
    exif_date=$(echo "$exif_date" | sed 's/:/\//g' | sed 's/ /T/' | sed 's/T/ /')
    # now extract the date part only
    exif_date=$(echo "$exif_date" | cut -d ' ' -f 1)
else
    exif_date=""
fi

# if exif_date is empty, use the file name (excluding the full path)
if [ -z "$exif_date" ]; then
    exif_date=$(basename "$input_file" | sed 's/\.[^.]*$//') # remove extension
fi

if [ "$type" = "grey" ]; then
    magick $input_file -resize 800x480^ -gravity Center -extent 800x480 -dither FloydSteinberg -remap pattern:gray50 -auto-orient .tmp.gif #grey
    if [ -n "$exif_date" ]; then
        magick .tmp.gif -undercolor $annotate_background -gravity SouthEast -fill white -font $font -pointsize $pointsize -annotate +0+0 $exif_date .tmp.gif
    fi

    magick .tmp.gif -depth 8 -alpha off -compress none BMP3:$output_file
else
    magick $input_file -resize 800x480^ -gravity Center -extent 800x480 -dither FloydSteinberg -remap $palette -auto-orient .tmp.gif #colored
    magick .tmp.gif -depth 8 -colors 256 -compress none BMP3:$output_file                                                            # 8-bit color
fi

rm .tmp.gif
if [ $? -eq 0 ]; then
    echo "Done. Output file: $output_file"
else
    echo "Failed."
fi

set +e

exit 0
# End of script
