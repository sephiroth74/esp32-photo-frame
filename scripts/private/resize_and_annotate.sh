#!/bin/zsh

# Resize and annotate the image based on the EXIF data and the provided parameters.
# This script will resize the image to the target size, crop it if necessary, and annotate it with the EXIF date.

size="800x600"
target_width=0
target_height=0
pointsize=20
font='UbuntuMono-Nerd-Font-Bold'
annotate_background='#00000040'
input_file=""
output_file=""
temp_file="/tmp/tmp.jpg"
verbose=false

file_dir="$(dirname "$(realpath "$0")")"
find_subject_script="${file_dir}/find_subject.py"

fn delete_temp_file() {
    if [ -f "$temp_file" ]; then
        rm "$temp_file"
    fi
}

# Undefined  - 0
# Undefined  - [When no metadata]
# TopLeft  - 1
# TopRight  - 2
# BottomRight  - 3
# BottomLeft  - 4
# LeftTop  - 5
# RightTop  - 6
# RightBottom  - 7
# LeftBottom  - 8
# Unrecognized  - any value between 9-65535, since 
#                 there is no mapping from value 9-65535
#                 to some geometry like 'LeftBottom'
fn extract_image_info() {
    local file="$1"
    local result=$(identify -format "%[orientation]x%[fx:w]x%[fx:h]" "$file" 2>/dev/null)
    local orientation=$(echo "$result" | cut -d 'x' -f 1)
    local width=$(echo "$result" | cut -d 'x' -f 2)
    local height=$(echo "$result" | cut -d 'x' -f 3)
    local is_portrait=0

    # if orientation is 6 or 8 then the image is in portrait mode and we need to swap the width and height
    if [[ "$orientation" == "RightTop" || "$orientation" == "LeftBottom" ]]; then
        local temp="$width"
        width="$height"
        height="$temp"
        is_portrait=1
    fi

    echo "$orientation" "$is_portrait" "$width" "$height"
}

# read the size from arguments in the format: --size 800x600
# read the pointsize from arguments in the format: --pointsize 24
# read the font from arguments in the format: --font UbuntuMono-Nerd-Font-Bold
# read the annotate_background from arguments in the format: --annotate_background '#00000040'

function usage() {
    echo "Usage: resize_and_annotate.sh [--size <width>x<height>] [--pointsize <size>] [--font <font_name>] [--annotate_background <color>] -i <input_file> -o <output_file> [-v --verbose]"
    echo "Options:"
    echo "  -i, --input <file>          Input image file to resize and annotate"
    echo "  -o, --output <file>         Output image file after resizing and annotating"
    echo "  -s, --size <width>x<height> Target size for the image (default: $size)"
    echo "  --pointsize <size>         Point size for the annotation (default: $pointsize)"
    echo "  --font <font_name>         Font for the annotation (default: $font)"
    echo "  --annotate_background <color> Background color for the annotation (default: $annotate_background)"
    echo "  -v, --verbose               Enable verbose output"
    echo "  -h, --help                Show this help message"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    -s|--size)
        size="$2"
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
    -i|--input)
        input_file="$2"
        shift 2
        ;;
    -o|--output)
        output_file="$2"
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
        usage
        ;;
    *)
        echo "Unknown argument: $1"
        usage
        ;;
    esac
done

# 1. Validate input arguments
# ---------------------------

# Parse the size argument (using zsh regex)
if [[ "$size" =~ ^([0-9]+)x([0-9]+)$ ]]; then
    target_width="${match[1]}"
    target_height="${match[2]}"
else
    echo "Error: Size must be in the format <width>x<height> (e.g., 800x600)."
    usage
fi

# Validate target width and height
if ! [[ "$target_width" =~ ^[0-9]+$ ]] || ! [[ "$target_height" =~ ^[0-9]+$ ]]; then
    echo "Error: Width and height must be positive integers."
    usage
fi

# Check if input file is provided
if [ -z "$input_file" ]; then
    echo "Error: Input file is required."
    exit 1
fi

# Check if output file is provided
if [ -z "$output_file" ]; then
    echo "Error: Output file is required."
    exit 1
fi

# Check if ImageMagick is installed
if ! command -v magick &>/dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi

set -e

# 2. Process the image
# ---------------------------

# Extract orientation, width, and height from the input file

result=$(extract_image_info "$input_file")
if [[ $? -ne 0 ]]; then
    echo "Error: Could not extract image information from '$input_file'."
    exit 1
fi

orientation=$(echo "$result" | cut -d ' ' -f 1)
is_portrait=$(echo "$result" | cut -d ' ' -f 2)
source_width=$(echo "$result" | cut -d ' ' -f 3)
source_height=$(echo "$result" | cut -d ' ' -f 4)


# Print the orientation and dimensions
if [[ "$verbose" == true ]]; then
    echo "----------------------------------------------------"
    echo "Executing resize_and_annotate.sh with the following parameters:"
    echo "input file: $input_file"
    echo "output file: $output_file"
    echo "source orientation: $orientation"
    echo "source size: $source_width x $source_height"
    echo "target size: $target_width x $target_height"
fi

# Find the subject in the image
set +e
if ! command -v "$find_subject_script" &>/dev/null; then
    echo "`find_subject.py` could not be found. Please ensure it is in your PATH."
    exit 1
fi

result=$("$find_subject_script" --image "$input_file")
if [ $? -ne 0 ]; then
    echo "no subject(s) could not be found in the image!"
    exit 1
fi

# result will be in the format:
# imagesize: 1848,4000
# box: 335,541,1577,2312
# center: 956,1426
# offset: 32,-574

# Parse the result
imagesize=$(echo "$result" | grep "imagesize" | cut -d ':' -f 2 | tr -d ' ')
box=$(echo "$result" | grep "box" | cut -d ':' -f 2 | tr -d ' ')
center=$(echo "$result" | grep "center" | cut -d ':' -f 2 | tr -d ' ')
offset=$(echo "$result" | grep "offset" | cut -d ':' -f 2 | tr -d ' ')

# Extract the center offset from the offset variable
center_x=$(echo "$offset" | cut -d ',' -f 1)
center_y=$(echo "$offset" | cut -d ',' -f 2)

if [[ "$verbose" == true ]]; then
    echo "imagesize: $imagesize"
    echo "box: $box"
    echo "center: $center"
    echo "offset center: $center_x, $center_y"
fi

set -e

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
    # try to extract the datetime from the filename (eg: *20210722* = 2021-07-22) using zsh regular expression
    if [[ "$input_file" =~ ([0-9]{4})([0-9]{2})([0-9]{2}) ]]; then
        exif_date="${match[1]}-${match[2]}-${match[3]}"
    else
        exif_date=$(basename "$input_file" | sed 's/\.[^.]*$//') # remove extension
    fi
fi

# Resize the image to the closest target size

magick $input_file -auto-orient -resize ${target_width}x${target_height}^ $temp_file

result=$(identify -ping -format '%wx%h' $temp_file)

if [ $? -ne 0 ]; then
    echo "Error: Could not resize image."
    delete_temp_file 
    exit 1
fi

# Get the new dimensions
new_width=$(echo "$result" | cut -d 'x' -f 1)
new_height=$(echo "$result" | cut -d 'x' -f 2)


# Calculate the crop offsets
width_ratio=$(echo "scale=6; $new_width / $source_width" | bc)
height_ratio=$(echo "scale=6; $new_height / $source_height" | bc)

if [[ "$verbose" == true ]]; then
    echo "new size: $new_width x $new_height"
    echo "width ratio: $width_ratio"
    echo "height ratio: $height_ratio"
fi

# Calculate the crop offsets based on the center and the ratios and then make them integers
offset_x=$(echo "$center_x * $width_ratio" | bc)
offset_y=$(echo "$center_y * $height_ratio" | bc)
offset_x=$(printf "%.0f" "$offset_x")
offset_y=$(printf "%.0f" "$offset_y")

# Ensure offset_x and offset_y are valid integers (default to 0 if empty)
if [ -z "$offset_x" ] || ! [[ "$offset_x" =~ ^-?[0-9]+$ ]]; then
    offset_x=0
fi
if [ -z "$offset_y" ] || ! [[ "$offset_y" =~ ^-?[0-9]+$ ]]; then
    offset_y=0
fi


# If a 480x800 crop were perfectly centered on a 480x1039 image, the vertical space remaining would be 1039 - 800 = 239 pixels.

vertical_space=$(( (new_height - target_height) / 2 ))
horizontal_space=$(( (new_width - target_width) / 2 ))

if [[ "$verbose" == true ]]; then
    echo "vertical space: $vertical_space"
    echo "horizontal space: $horizontal_space"
    echo "crop offsets: $offset_x, $offset_y"
fi

# If the absolute value of offset_x or offset_y is greater than the horizontal or vertical space, we need to adjust the offsets (making their absolute values equal to the horizontal or vertical space)

if [ "$offset_x" -lt "-$horizontal_space" ]; then
    offset_x=$((-horizontal_space))
elif [ "$offset_x" -gt "$horizontal_space" ]; then
    offset_x=$horizontal_space
fi

if [ "$offset_y" -lt "-$vertical_space" ]; then
    offset_y=$((-vertical_space))
elif [ "$offset_y" -gt "$vertical_space" ]; then
    offset_y=$vertical_space
fi

if [[ "$verbose" == true ]]; then
    echo "adjusted crop offsets: $offset_x, $offset_y"
fi

magick $temp_file -gravity center -crop ${target_width}x${target_height}+$offset_x+$offset_y +repage -extent ${target_width}x${target_height} $output_file


# Annotate the image with the EXIF date
if [ -n "$exif_date" ]; then
    magick $output_file -undercolor $annotate_background -gravity SouthEast -fill white -font $font -pointsize $pointsize -annotate +0+0 $exif_date $output_file
fi

# finally delete the temporary file
delete_temp_file

if [[ "$verbose" == true ]]; then
    echo "image processed and saved to '$output_file'"
fi

exit 0