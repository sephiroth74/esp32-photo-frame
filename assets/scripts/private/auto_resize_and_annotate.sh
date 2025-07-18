#!/bin/zsh

# Given the input arguments, then the script will call 'resize_and_annotate.sh' with the appropriate parameters.
# It will adjust the size argument passed to 'resize_and_annotate.sh' to match the image orientation (eg. swapping the input size if it doesn't match the image orientation, if 'auto' is enabled).

pointsize=20
font='UbuntuMono-Nerd-Font-Bold'
annotate_background='#00000040'
verbose=false
input_file=""
output_file=""
target_size=""
target_is_portrait=0
auto_process=false

file_dir="$(dirname "$(realpath "$0")")"

# include 'utils.sh' script
source "${file_dir}/utils.sh"

resize_and_annotate_script="${file_dir}/resize_and_annotate.sh"

usage() {
    echo "Usage: $0 [options] -i <input_file> -o <output_file> -s|--size <target_size> [--pointsize <pointsize>] [--font <font>] [--annotate_background <annotate_background>] [--auto]"
    echo "Options:"
    echo "  -v, --verbose       Enable verbose output"
    echo "  -i, --input <file>  Input image file to resize and annotate"
    echo "  -o, --output <file> Output image file after resizing and annotating"
    echo "  -s, --size <size>   Target size for the image (format: WIDTHxHEIGHT)"
    echo "  --pointsize <size>  Point size for the annotation (default: $pointsize)"
    echo "  --font <font>        Font for the annotation (default: $font)"
    echo "  --annotate_background <color> Background color for the annotation (default: $annotate_background)"
    echo "  --auto                Enable automatic processing (swaps width and height if necessary)"
    echo "  -h, --help          Show this help message"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--input)
            input_file="$2"
            shift 2
            ;;
        -o|--output)
            output_file="$2"
            shift 2
            ;;
        -s|--size)
            target_size="$2"
            shift 2
            ;;
        --pointsize)
            pointsize="$2"
            shift 2
            ;;
        --font)
            font="$2"
            shift 2
            ;;
        --annotate_background)
            annotate_background="$2"
            shift 2
            ;;
        --auto)
            auto_process=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        --help)
            usage
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        -*)
            echo "Unknown option: $1"
            usage
            ;;
        *)
            echo "Unexpected argument: $1"
            usage
            ;;
    esac
done

# Print the input parameters if verbose mode is enabled
if [[ "$verbose" == true ]]; then
    echo "----------------------------------------------------"
    echo "Executing auto_resize_and_annotate.sh with the following parameters:"
    echo "Input file: $input_file"
    echo "Output file: $output_file"
    echo "Target size: $target_size"
    echo "Point size: $pointsize"
    echo "Font: $font"
    echo "Annotate background color: $annotate_background"
    echo "Auto process: $auto_process"
fi

# Check if input file and output file are provided
if [[ -z "$input_file" || -z "$output_file" || -z "$target_size" ]]; then
    echo "Error: Input file, output file, and target size are required."
    usage
fi

# Check if the input file exists
if [[ ! -f "$input_file" ]]; then
    echo "Error: Input file '$input_file' does not exist."
    exit 1
fi

# Check if the target size is in the correct format (WIDTHxHEIGHT)
if [[ ! "$target_size" =~ ^[0-9]+x[0-9]+$ ]]; then
    echo "Error: Target size must be in the format WIDTHxHEIGHT (e.g., 800x600)."
    exit 1
fi

# Extract width and height from the target size
IFS='x' read -r target_width target_height <<< "$target_size"

# Check if width and height are positive integers
if ! [[ "$target_width" =~ ^[0-9]+$ ]] || ! [[ "$target_height" =~ ^[0-9]+$ ]]; then
    echo "Error: Width and height must be positive integers."
    exit 1
fi

# Check if ImageMagick is installed
if ! command -v magick &>/dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi

# Check if the resize_and_annotate.sh script exists
if [[ ! -f "$resize_and_annotate_script" ]]; then
    echo "Error: resize_and_annotate.sh script not found!"
    exit 1
fi

# Determine the orientation of the target size
if (( target_width > target_height )); then
    target_is_portrait=0  # Landscape
else
    target_is_portrait=1  # Portrait
fi

# Detemine the orientation of the input image
result=$(extract_image_info "$input_file")
if [[ $? -ne 0 ]]; then
    echo "Error: Could not extract image information from '$input_file'."
    exit 1
fi

source_orientation=$(echo "$result" | cut -d ' ' -f 1)
source_is_portrait=$(echo "$result" | cut -d ' ' -f 2)
source_width=$(echo "$result" | cut -d ' ' -f 3)
source_height=$(echo "$result" | cut -d ' ' -f 4)


# If the source orientation is 6 or 8, it means the image is in portrait mode, then adjust the target_width and target_height accordingly
if [[ "$source_is_portrait" == "1" ]]; then
    is_portrait=1
    if [[ "$verbose" == true ]]; then
        echo "Image is in portrait mode."
    fi
    
    # Swap target width and height, if target_is_portrait is 0 (landscape)
    if [[ "$target_is_portrait" -eq 0 && "$auto_process" == true ]]; then
        temp=$target_width
        target_width=$target_height
        target_height=$temp
    fi
    
else
    is_portrait=0
    if [[ "$verbose" == true ]]; then
        echo "Image is in landscape mode."
    fi

    # Swap target width and height, if target_is_portrait is 1 (portrait)
    if [[ "$target_is_portrait" -eq 1 && "$auto_process" == true ]]; then
        temp=$target_width
        target_width=$target_height
        target_height=$temp
    fi
fi

if [[ "$verbose" == true ]]; then
    echo "Source image orientation: $source_orientation (width: $source_width, height: $source_height)"
    echo "Target size: ${target_width}x${target_height}"
fi

# Now call the resize_and_annotate.sh script with the adjusted parameters
command="$resize_and_annotate_script -i \"$input_file\" -o \"$output_file\" -s \"${target_width}x${target_height}\" --pointsize \"$pointsize\" --font \"$font\" --annotate_background \"$annotate_background\""

if [[ "$verbose" == true ]]; then
    command="$command --verbose"
fi


if [[ "$verbose" == true ]]; then
    echo "Executing command: $command"
fi

result=$(eval "$command")

# Check if the command was successful
if [[ $? -ne 0 ]]; then
    echo "Error: Failed to process the image with the command:"
    echo "$command"
    echo "Output: $result"
    exit 1
else 
    if [[ "$verbose" == true ]]; then
        echo "$result"
        echo "image processed and saved to '$output_file'"
    fi
fi

exit 0