#!/bin/zsh

# This script list all .jpg images in the given directory
# and check if the image width is greater or equal to its height.
# If so, it will call the `convert_h.sh` command with the image path as an argument.
# Otherwise collect all the images that are not in landscape mode into an array.
# for every pair in the array it will call the `convert_v.sh` command with the images pair as arguments.

# arguments:
# -type: the type of images to process, either "grey" or "color"
# if type is not provided, it will default to "grey"
# if type is color, then provide also the path to the color table gif file (using -palette argument)
# $1: input directory
# $2: output directory

# Check if the script is run with zsh
if [ -z "$ZSH_VERSION" ]; then
    echo "This script requires zsh to run."
    exit 1
fi


# Default values
type=""
palette=""
input_dir=""
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
            if [[ -z "$input_dir" ]]; then
                input_dir="$1"
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

# Validate input and output directories
if [[ -z "$input_dir" || -z "$output_dir" ]]; then
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_directory> <output_directory>"
    exit 1
fi


echo "type: $type"
echo "palette: $palette"
echo "Input directory: $input_dir"
echo "Output directory: $output_dir"
echo "Processing images in directory: $input_dir"

portrait_images=()
landscape_images=()

mkdir -p "$output_dir"

# Check if ImageMagick is installed
if ! command -v magick &> /dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi
# Check if the convert_h.sh script exists
if [ ! -f "convert_h.sh" ]; then
    echo "convert_h.sh script not found!"
    exit 1
fi

# Check if the convert_v.sh script exists
if [ ! -f "convert_v.sh" ]; then
    echo "convert_v.sh script not found!"
    exit 1
fi

# Check if the convert_h.sh script is executable
if [ ! -x "convert_h.sh" ]; then
    echo "convert_h.sh script is not executable!"
    exit 1
fi

# Check if the convert_v.sh script is executable
if [ ! -x "convert_v.sh" ]; then
    echo "convert_v.sh script is not executable!"
    exit 1
fi

# Loop through all .jpg files in the given directory
for file in "$input_dir"/*.jpg; do
    # Check if the file exists
    if [ ! -f "$file" ]; then
        echo "File not found: $file"
        continue
    fi

    # Get the image dimensions, ignore errors
    # print errors (stderr) to /dev/null

    dimensions=$(magick identify -format "%[EXIF:Orientation]x%wx%h" "$file" 2>/dev/null)
    orientation=0
    set -e

    # if the script is not able to get the dimensions, retry without the orientation
    if [ -z "$dimensions" ]; then
        dimensions=$(magick identify -format "1%wx%h" "$file") # 800x480
    fi

    orientation=$(echo "$dimensions" | cut -d'x' -f1)
    width=$(echo "$dimensions" | cut -d'x' -f2)
    height=$(echo "$dimensions" | cut -d'x' -f3)

    # Check if the image is rotated
    # if the orientation is 1 or 3, the image is not rotated
    if [ "$orientation" -eq 1 ] || [ "$orientation" -eq 3 ] || [ "$orientation" -eq 0 ]; then
        # The image is not rotated, so we can use the width and height directly
        width=$(echo "$dimensions" | cut -d'x' -f2)
        height=$(echo "$dimensions" | cut -d'x' -f3)
    else
        # The image is rotated, so we need to swap the width and height
        temp=$width
        width=$height
        height=$temp
    fi

    # Check if the width is greater than or equal to the height
    if [ "$width" -ge "$height" ]; then
        echo "Collecting landscape: $file"
        landscape_images+=("$file")
    else
        echo "Collecting portrait: $file"
        portrait_images+=("$file")
    fi
done


# Print the total number of landscape and portrait images collected
echo "----------------------------------------------------------------"
echo "Total landscape images: ${#landscape_images[@]}"
echo "Total portrait images: ${#portrait_images[@]}"
echo "----------------------------------------------------------------"
echo " "

# Ask the user if they want to continue
read -q "answer?Do you want convert all the images? (y/n): "
if [[ "$answer" != "y" && "$answer" != "Y" ]]; then
    echo "Exiting..."
    exit 0
fi


echo "Processing landscape images..."
# Loop through the landscape_images array and call the convert_h.sh script with each image
index=1
for image in "${landscape_images[@]}"; do
    # Call the convert_h.sh script with the image path as an argument
    echo "[$index / ${#landscape_images[@]}] Processing landscape image: $image"

    # Check if palette is provided
    # If palette is not provided, call convert_h.sh without -palette option
    # If palette is provided, call convert_h.sh with -palette option
    if [ -z "$palette" ]; then
        ./convert_h.sh -type $type "$image" "$output_dir"
    else
        ./convert_h.sh -type $type -palette $palette "$image" "$output_dir"
    fi
    index=$((index + 1))
done
echo "Done."

echo "Processing portrait images..."

# randomize the portrait_images array (using zsh which does not have shuf)
shuffled_portrait_images=()
for image in "${portrait_images[@]}"; do
    # Generate a random index
    random_index=$((RANDOM % (${#shuffled_portrait_images[@]} + 1)))
    # Insert the image at the random index
    shuffled_portrait_images=("${shuffled_portrait_images[@]:0:$random_index}" "$image" "${shuffled_portrait_images[@]:$random_index}")
done


# Reinitialize the portrait_images array with the shuffled images
portrait_images=("${shuffled_portrait_images[@]}")

# Loop through the portrait_images array and call the convert_v.sh script with pairs of images
# echo all the portrait images
echo "Portrait images to process: ${#portrait_images[@]}"

# for every pair of images in the portrait_images array
for ((i=1; i<${#portrait_images[@]}; i+=2)); do
    image1="${portrait_images[i]}"
    image2="${portrait_images[i+1]}"
    # Check if there is a pair of images
    if [ -z "$image2" ]; then
        echo "Unpaired image found: $image1"
        continue
    fi

    echo "[$i / ${#portrait_images[@]}] Processing portrait pair: ${portrait_images[i]} and ${portrait_images[i+1]}"
        if [ -z "$palette" ]; then
            ./convert_v.sh -type $type "$image1" "$image2" "$output_dir"
        else
            ./convert_v.sh -type $type -palette $palette "$image1" "$image2" "$output_dir"
        fi
done


echo "Done."
echo "All images processed."