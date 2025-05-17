#!/bin/zsh

# This script list all .jpg images in the given directory
# and check if the image width is greater or equal to its height.
# If so, it will call the `convert_h.sh` command with the image path as an argument.
# Otherwise collect all the images that are not in landscape mode into an array.
# for every pair in the array it will call the `convert_v.sh` command with the images pair as arguments.

portrait_images=()
landscape_images=()

# Check if the script is run with zsh
if [ -z "$ZSH_VERSION" ]; then
    echo "This script requires zsh to run."
    exit 1
fi


# Usage: ./script.sh <input_directory> <output_directory>
# Example: ./script.sh /path/to/directory /path/to/output
# Check if the input and output path is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <input_directory> <output_directory>"
    exit 1
fi

if [ ! -d "$1" ]; then
    echo "Directory not found!"
    exit 1
fi

# Check if the output path is provided
if [ -z "$2" ]; then
    echo "Usage: $0 <input_directory> <output_directory>"
    exit 1
fi



# Check if the directory exists
if [ ! -d "$1" ]; then
    echo "Directory not found!"
    exit 1
fi

# Check if the output path is provided
if [ -z "$2" ]; then
    echo "Usage: $0 <input_directory> <output_directory>"
    exit 1
fi

output_dir="$2"

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
for file in "$1"/*.jpg; do
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
    ./convert_h.sh "$image" "$output_dir"
    index=$((index + 1))
done
echo "Done."

echo "Processing portrait images..."
# Loop through the portrait_images array and call the convert_v.sh script with pairs of images
index=1
for ((i=0; i<${#portrait_images[@]}; i+=2)); do
    # Check if there is a pair of images
    if [ $((i+1)) -lt ${#portrait_images[@]} ]; then
        # Call the convert_v.sh script with the image paths as arguments
        echo "[$index / ${#portrait_images[@]}] Processing portrait pair: ${portrait_images[i]} and ${portrait_images[i+1]}"
        ./convert_v.sh "${portrait_images[i+1]}" "${portrait_images[i+2]}" "$output_dir"
        index=$((index + 2))
    else
        echo "Unpaired image found: ${portrait_images[i]}"
    fi
done
echo "Done."

echo "All images processed."