#!/bin/zsh

# This script list all .jpg images in the given directory
# and check if the image width is greater or equal to its height.
# If so, it will call the `convert_landscape.sh` command with the image path as an argument.
# Otherwise collect all the images that are not in landscape mode into an array.
# for every pair in the array it will call the `convert_portrait.sh` command with the images pair as arguments.

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

usage="Usage: $0 -type <grey|color> [-palette <palette.gif>] [-colors <num_colors>] [-landscape-only] [-portrait-only] -output <output_directory> <input_directory>..."

export pointsize=24
export font='UbuntuMono-Nerd-Font-Bold'
export annotate_background='#00000040'

# Default values
type=""
palette=""
colors=0
output_dir=""
input_dirs=()
landscape_only=false
portrait_only=false
auto_process=false


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
    -colors)
        colors="$2"
        shift 2
        ;;
    -output)
        output_dir="$2"
        shift 2
        ;;
    -landscape-only)
        landscape_only=true
        shift
        ;;
    -portrait-only)
        portrait_only=true
        shift
        ;;
    -y| --yes)
        auto_process=true
        shift
        ;;
    -*)
        echo "Unknown option: $1"
        exit 1
        ;;
    *)
        # Positional arguments
        # All remaining arguments are considered input directories
        # then add all of them to the input_dirs array
        input_dirs+=("$1")
        shift
        ;;
    esac
done

# Validate input directories
if [[ ${#input_dirs[@]} -eq 0 ]]; then
    echo "$usage"
    exit 1
fi

# print all input directories
for input_dir in "${input_dirs[@]}"; do
    if [[ ! -d "$input_dir" ]]; then
        echo "Error: Input directory '$input_dir' does not exist or is not a directory."
        echo "$usage"
        exit 1
    fi
done

# Validate -type
if [[ -z "$type" ]]; then
    echo "Error: -type argument is required (grey or color)."
    echo "$usage"
    exit 1
fi

if [[ "$type" != "grey" && "$type" != "color" ]]; then
    echo "Error: -type must be either 'grey' or 'color'."
    echo "$usage"
    exit 1
fi

# If type is color, palette or colors must be provided, but not both (colors is ignored if palette is provided)
if [[ "$type" == "color" && -z "$palette" && ( -z "$colors" || "$colors" -le 0 ) ]]; then
    echo "Error: -palette or -colors argument is required when -type is 'color'."
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>"
    exit 1
elif [[ "$type" == "color" && -n "$palette" && ( -n "$colors" && "$colors" -gt 0 ) ]]; then
    echo "Error: -palette and -colors cannot be used together. Please use one of them."
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>"
    exit 1
fi

# Validate input and output directories
if [[ -z "$output_dir" ]]; then
    echo "Error: -output argument is required."
    echo "$usage"
    exit 1
fi

echo "type: $type"
echo "colors: $colors"
echo "auto process: $auto_process"

if [[ -z "$palette" ]]; then
    echo "palette: not provided"
else
    echo "palette: $palette"
fi
echo "Input directories: ${input_dirs[*]}"
echo "Output directory: $output_dir"

portrait_images=()
landscape_images=()

mkdir -p "$output_dir"

# Check if ImageMagick is installed
if ! command -v magick &>/dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi
# Check if the convert_landscape.sh script exists
if [ ! -f "convert_landscape.sh" ]; then
    echo "convert_landscape.sh script not found!"
    exit 1
fi

# Check if the convert_portrait.sh script exists
if [ ! -f "convert_portrait.sh" ]; then
    echo "convert_portrait.sh script not found!"
    exit 1
fi

# Check if the convert_landscape.sh script is executable
if [ ! -x "convert_landscape.sh" ]; then
    echo "convert_landscape.sh script is not executable!"
    exit 1
fi

# Check if the convert_portrait.sh script is executable
if [ ! -x "convert_portrait.sh" ]; then
    echo "convert_portrait.sh script is not executable!"
    exit 1
fi

for input_dir in "${input_dirs[@]}"; do
    echo ""
    echo "Processing directory: $input_dir"
    echo "----------------------------------------------------------------"
    # Loop through all .jpg files in the given directory
    # for file in "$input_dir"/*.jpg; do
    # for file in $input_dir/**/*.[jJ][pP]([eE]|)[gG]; do
    for file in $input_dir/**/*.(jpg|JPG|jpeg|JPEG|HEIC|heic); do
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
            if $portrait_only; then
                echo "Skipping landscape image in portrait-only mode: $file"
                continue
            fi
            echo "Collecting landscape: $file"
            landscape_images+=("$file")
        else
            if $landscape_only; then
                echo "Skipping portrait image in landscape-only mode: $file"
                continue
            fi
            echo "Collecting portrait: $file"
            portrait_images+=("$file")
        fi
    done
done

# Print the total number of landscape and portrait images collected
echo "----------------------------------------------------------------"
echo "Total landscape images: ${#landscape_images[@]}"
echo "Total portrait images: ${#portrait_images[@]}"
echo "Total images: $((${#landscape_images[@]} + ${#portrait_images[@]}))"
echo "----------------------------------------------------------------"
echo " "

# Ask the user if they want to continue (if auto_process is false)
if ! $auto_process; then
    read -q "answer?Do you want convert all the images? (y/n): "
    if [[ "$answer" != "y" && "$answer" != "Y" ]]; then
        echo "Exiting..."
        exit 0
    fi
fi

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

echo ""
echo "Processing landscape images..."
# Loop through the landscape_images array and call the convert_landscape.sh script with each image

if [ -z "$palette" ]; then
    command="./convert_landscape.sh -type $type -colors $colors"
else
    command="./convert_landscape.sh -type $type -palette $palette"
fi

index=1
for image in "${landscape_images[@]}"; do
    # Call the convert_landscape.sh script with the image path as an argument
    echo "[$index / ${#landscape_images[@]}] Processing landscape image: $image"
    final_command="$command \"$image\" \"$output_dir\""
    eval ${final_command}
    index=$((index + 1))
done
echo "done"

echo ""
echo "Processing portrait images..."

if [ -z "$palette" ]; then
    command="./convert_portrait.sh -type $type -colors $colors"
else
    command="./convert_portrait.sh -type $type -palette $palette"
fi

# for every pair of images in the portrait_images array
for ((i = 1; i < ${#portrait_images[@]}; i += 2)); do
    image1="${portrait_images[i]}"
    image2="${portrait_images[i + 1]}"
    # Check if there is a pair of images

    if [ -z "$image1" ]; then
        echo "Unpaired image found at index $i: $image1"
        continue
    fi

    if [ -z "$image2" ]; then
        echo "Unpaired image found at index $i: $image2"
        continue
    fi

    echo "[$i / ${#portrait_images[@]}] Processing portrait pair: $image1 and ${image2}"

    # call command with the two images and output directory
    final_command="$command \"$image1\" \"$image2\" \"$output_dir\""
    eval ${final_command}
done

echo "done"
echo "All images processed."
