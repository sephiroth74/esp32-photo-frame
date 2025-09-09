#!/bin/zsh

type="" # either bw or color
verbose=false
palette="" # palette file (optional)
input_file="" # input file
output_file="" # output file
colors=0 # number of colors (0 for bw, 6 for color), ignored if palette is set or type is bw

# usage: convert.sh -type <grey|color> [--palette <palette.gif>] [--colors <num_colors>] [-h|--help] <input_file> <output_file>

usage() {
    echo "Usage: $0 -type <grey|color> [--palette <palette.gif>] [--colors <num_colors>] <input_file> <output_file>"
    echo "  -t, --type <grey|color>       Specify the type of conversion (grey or color)."
    echo "  -p, --palette <palette.gif>  Optional palette file for color conversion."
    echo "  -c, --colors <num_colors>    Number of colors for color conversion (ignored if --palette is set)."
    echo "  -h, --help               Show this help message."
    echo "  <input_file>             Input image file."
    echo "  <output_file>            Output image file."
    exit 1
}

if [[ $# -eq 0 ]]; then
    usage
fi

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
    -t|--type)
        type="$2"
        shift 2
        ;;
    -p|--palette)
        palette="$2"
        shift 2
        ;;
    -c|--colors)
        colors="$2"
        shift 2
        ;;
    -v|--verbose)
        verbose=true
        shift
        ;;
    -h|--help)
        usage
        ;;
    -*)
        echo "Unknown option: $1"
        exit 1
        ;;
    *)
        # Positional arguments
        if [[ -z "$input_file" ]]; then
            input_file="$1"
        elif [[ -z "$output_file" ]]; then
            output_file="$1"
        fi
        shift
        ;;
    esac
done

# Check imagemagick is installed
if ! command -v magick &> /dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi

# Validate -type
if [[ -z "$type" ]]; then
    echo "Error: -type argument is required (grey or color)."
    exit 1
fi

if [[ "$type" != "grey" && "$type" != "color" ]]; then
    echo "Error: -type must be either 'grey' or 'color'."
    exit 1
fi

# Check if input file and output file are provided
if [[ -z "$input_file" || -z "$output_file" ]]; then
    echo "Error: Input file and output file are required."
    usage
fi

# Check if the input file exists
if [[ ! -f "$input_file" ]]; then
    echo "Error: Input file '$input_file' does not exist."
    exit 1
fi

# Check if the output directory exists, create it if it doesn't
output_dir=$(dirname "$output_file")
if [[ ! -d "$output_dir" ]]; then
    echo "Output directory '$output_dir' does not exist. Creating it..."
    mkdir -p "$output_dir"
    if [[ $? -ne 0 ]]; then
        echo "Error: Failed to create output directory '$output_dir'."
        exit 1
    fi
fi

# Check if the colors argument is a positive integer
if [[ "$type" == "color" && -n "$colors" && ! "$colors" =~ ^[0-9]+$ ]]; then
    echo "Error: --colors must be a positive integer."
    exit 1
fi

# Check if the palette file exists if provided
if [[ -n "$palette" && ! -f "$palette" ]]; then
    echo "Error: Palette file '$palette' does not exist."
    exit 1
fi  

if [ "$type" = "grey" ]; then
    command="magick $input_file -auto-level -dither FloydSteinberg"
    if [ -z "$palette" ]; then
        command="$command -remap pattern:gray50 -monochrome"
    else
        command="$command -remap $palette"
    fi
    command="$command -type Palette -depth 8 -alpha off -compress none BMP3:\"$output_file\""
else
    command="magick $input_file -auto-level -dither FloydSteinberg"
    # if colors is specified, and bigger than 0, then add it to the command
    if [[ "$colors" -gt 0 ]]; then
        command="$command -colors $colors"
    fi
    if [ -n "$palette" ]; then
        command="$command -remap $palette"
    fi
    command="$command -type Palette -depth 8 -alpha off -compress none BMP3:\"$output_file\""
fi

result=$(eval $command)
if [[ $? -ne 0 ]]; then
    echo "Error: Failed to convert image."
    exit 1
fi

exit 0