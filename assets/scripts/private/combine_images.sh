#!/bin/zsh

# file directory, relative to the script location
file_dir="$(dirname "$(realpath "$0")")"
verbose=false

# include 'utils.sh' script
source "${file_dir}/utils.sh"

# Combine 2 images horizontally into one image
# Usage: ./combine_images.sh image1.jpg image2.jpg output.jpg

stroke_color="#ffffff"
stroke_width=3
size="800x480"

# Usage:
# ./combine_images.sh --stroke_color "#ff0000" --stroke_width 4  -s|--size <size> image1.jpg image2.jpg output.jpg

usage() {
    echo "Usage: $0 [--stroke_color color] [--stroke_width width] [-v|--verbose] image1.jpg image2.jpg output.jpg"
    echo "Options:"
    echo "  --stroke_color color   Set the stroke color (default: #ffffff)"
    echo "  --stroke_width width   Set the stroke width (default: 2)"
    echo "  -v, --verbose          Enable verbose output"
    echo "  -s, --size             Set the size of the output image (default: 480x800)"
    echo "  image1.jpg            The first input image"
    echo "  image2.jpg            The second input image"
    echo "  output.jpg            The output image"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --stroke_color)
            stroke_color="$2"
            shift 2
            ;;
        --stroke_width)
            stroke_width="$2"
            shift 2
            ;;
        -s|--size)
            size="$2"
            shift 2
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        *)
            if [[ -z "$image1" ]]; then
                image1="$1"
            elif [[ -z "$image2" ]]; then
                image2="$1"
            elif [[ -z "$output" ]]; then
                output="$1"
            else
                usage
            fi
            shift
            ;;
    esac
done

# Check if all required arguments are provided
if [[ -z "$image1" || -z "$image2" || -z "$output" ]]; then
    echo "Error: Missing required arguments."
    usage
fi

# Check if input images exist
if [[ ! -f "$image1" ]]; then
    echo "Error: Input image '$image1' does not exist."
    exit 1
fi

if [[ ! -f "$image2" ]]; then
    echo "Error: Input image '$image2' does not exist."
    exit 1
fi

# Check ImageMagick is installed
if ! command -v magick &> /dev/null; then
    echo "Error: ImageMagick is not installed. Please install it to use this script."
    exit 1
fi

if [[ "$verbose" == true ]]; then
    echo "----------------------------------------------------"
    echo "Executing combine_images.sh with the following parameters:"
    echo "Image 1: $image1"
    echo "Image 2: $image2"
    echo "Output: $output"
    echo "Stroke Color: $stroke_color"
    echo "Stroke Width: $stroke_width"
fi

if [[ "$verbose" == true ]]; then
    echo "Extracting image information from '$image1'"
fi  

result1=$(extract_image_info "$image1")
if [[ $? -ne 0 ]]; then
    echo "Error: Could not extract image information from '$image1'."
    exit 1
fi

if [[ "$verbose" == true ]]; then
    echo "Extracting image information from '$image2'"
fi

result2=$(extract_image_info "$image2")
if [[ $? -ne 0 ]]; then
    echo "Error: Could not extract image information from '$image2'."
    exit 1
fi

# Get image information
IFS=' ' read -r orientation1 is_portrait1 width1 height1 <<< "$result1"
IFS=' ' read -r orientation2 is_portrait2 width2 height2 <<< "$result2"

if [[ "$verbose" == true ]]; then
    echo "Image 1: $image1"
    echo "  Orientation: $orientation1"
    echo "  Is Portrait: $is_portrait1"
    echo "  Width: $width1"
    echo "  Height: $height1"
    echo "----------------"
    echo "Image 2: $image2"
    echo "  Orientation: $orientation2"
    echo "  Is Portrait: $is_portrait2"
    echo "  Width: $width2"
    echo "  Height: $height2"
fi


# The 2 images must have the same height
if [[ "$height1" -ne "$height2" ]]; then
    echo "Error: The images must have the same height to combine them horizontally."
    exit 1
fi

center_x=$(( (width1 + width2) / 2 ))

# Combine images horizontally and resize, drawing an horizontal line between them (in the middle)
magick "$image1" "$image2" +append -stroke "$stroke_color" -strokewidth "$stroke_width" -draw "line $center_x,0,$center_x,$height1" "$output"

if [[ "$verbose" == true ]]; then
    echo "done"
fi