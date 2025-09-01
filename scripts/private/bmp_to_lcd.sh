#!/bin/zsh

# Convert all files inside an input directory to either .h or .bin using "./bmp2cpp"

file_dir="$(dirname "$(realpath "$0")")"

bmp2cpp="${file_dir}/bmp2cpp"

output_dir=""
input_dir=""
output_format="c-array" # or "binary"

set -e

usage="Usage: $0 -input <input_directory> -output <output_directory> [-format <c-array|binary>]"

while [[ $# -gt 0 ]]; do
    case "$1" in
    -input)
        input_dir="$2"
        shift 2
        ;;
    -output)
        output_dir="$2"
        shift 2
        ;;
    -format)
        output_format="$2"
        shift 2
        ;;
    -*)
        echo "Unknown option: $1"
        echo "$usage"
        exit 1
        ;;
    *)
        echo "Unknown argument: $1"
        echo "$usage"
        exit 1
        ;;
    esac
done

# Validate input directory
if [[ -z "$input_dir" || ! -d "$input_dir" ]]; then
    echo "Error: Input directory '$input_dir' does not exist or is not a directory."
    echo "$usage"
    exit 1
fi

# Validate output directory
if [[ -z "$output_dir" ]]; then
    echo "Error: Output directory is required."
    echo "$usage"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$output_dir"

# Process each BMP file in the input directory
for bmp_file in "$input_dir"/*.bmp; do
    if [[ ! -f "$bmp_file" ]]; then
        echo "No BMP files found in '$input_dir'."
        continue
    fi
    # Get the base name of the file without the extension
    base_name=$(basename "$bmp_file" .bmp)
    # Image name without special characters
    image_name=$(echo "$base_name" | tr -cd '[:alnum:]_') # Keep alphanumeric characters and underscores

    # image name must start with a letter
    if [[ ! "$image_name" =~ ^[a-zA-Z] ]]; then
        image_name="img_$image_name" # Prefix with 'img_' if it doesn't start with a letter
    fi

    # Determine the output file path based on the selected format
    if [[ "$output_format" == "c-array" ]]; then
        output_file="$output_dir/${image_name}.h"
    else
        output_file="$output_dir/${image_name}.bin"
    fi

    # Call the bmp2cpp tool to perform the conversion

    command="$bmp2cpp --output-format $output_format"

    # if output_format is c-array, add the -c option

    if [[ "$output_format" == "c-array" ]]; then
        command="$command --array-name \"$image_name\""
    fi

    command="$command \"$bmp_file\" \"$output_file\""
    result=$(eval "$command")
    if [[ $? -ne 0 ]]; then
        echo "Error processing '$bmp_file': $result"
        exit 1
    fi
done

set +e

exit 0
