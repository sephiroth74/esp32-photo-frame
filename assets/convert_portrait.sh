#!/bin/zsh

# Given a pair of images, this script will create a new image that contains the input images side by side horizontally.
# The output directory is the same as the input directory.
# The output image will be named input1 + "_" input2 + "combined.bmp".

#!/bin/zsh

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

# Default values
type=""
palette=""
input_file1=""
input_file2=""
output_dir=""
colors=0

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
    -*)
        echo "Unknown option: $1"
        exit 1
        ;;
    *)
        # Positional arguments
        if [[ -z "$input_file1" ]]; then
            input_file1="$1"
        elif [[ -z "$input_file2" ]]; then
            input_file2="$1"
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

# If type is color, palette or colors must be provided, but not both (colors is ignored if palette is provided)
if [[ "$type" == "color" && -z "$palette" && (-z "$colors" || "$colors" -le 0) ]]; then
    echo "Error: -palette or -colors argument is required when -type is 'color'."
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>"
    exit 1
elif [[ "$type" == "color" && -n "$palette" && (-n "$colors" && "$colors" -gt 0) ]]; then
    echo "Error: -palette and -colors cannot be used together. Please use one of them."
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input_file> <output_directory>"
    exit 1
fi

# Validate input and output directories
if [[ -z "$input_file1" || -z "$input_file2" || -z "$output_dir" ]]; then
    echo "Usage: $0 -type <grey|color> [-palette <palette.gif>] <input1> <input2> <output_directory>"
    exit 1
fi

mkdir -p "$output_dir"

# extract date time from the input file name
exif_date1=$(identify -format "%[EXIF:DateTimeOriginal]" "$input_file1" 2>/dev/null)
# Check if the exif_date extraction was successful
if [ $? -eq 0 ] && [ -n "$exif_date1" ]; then
    # Convert the date time to a format suitable for annotation
    exif_date1=$(echo "$exif_date1" | sed 's/:/\//g' | sed 's/ /T/' | sed 's/T/ /')
    # now extract the date part only
    exif_date1=$(echo "$exif_date1" | cut -d ' ' -f 1)
else
    exif_date1=""
fi

# extract date time from the input file name
exif_date2=$(identify -format "%[EXIF:DateTimeOriginal]" "$input_file2" 2>/dev/null)
# Check if the exif_date extraction was successful
if [ $? -eq 0 ] && [ -n "$exif_date2" ]; then
    # Convert the date time to a format suitable for annotation
    exif_date2=$(echo "$exif_date2" | sed 's/:/\//g' | sed 's/ /T/' | sed 's/T/ /')
    # now extract the date part only
    exif_date2=$(echo "$exif_date2" | cut -d ' ' -f 1)
else
    exif_date2=""
fi

# if exif_date is empty, use the file name (excluding the full path)
if [ -z "$exif_date1" ]; then
    exif_date1=$(basename "$input_file1" | sed 's/\.[^.]*$//') # remove extension
fi

# if exif_date is empty, use the file name (excluding the full path)
if [ -z "$exif_date2" ]; then
    exif_date2=$(basename "$input_file2" | sed 's/\.[^.]*$//') # remove extension
fi

# output_file will be the name of the first input file + "_" + the name of the second input file + "combined.bmp"
filename_without_extension1="${input_file1##*/}"
filename_without_extension1="${filename_without_extension1%.*}"
filename_without_extension2="${input_file2##*/}"
filename_without_extension2="${filename_without_extension2%.*}"

output_file="${output_dir}/${filename_without_extension1}_${filename_without_extension2}_combined.bmp"

magick $input_file1 -auto-orient .tmp1.jpg
magick $input_file2 -auto-orient .tmp2.jpg

magick .tmp1.jpg -resize 400x480^ -gravity Center -extent 400x480 .tmp1.jpg
magick .tmp2.jpg -resize 400x480^ -gravity Center -extent 400x480 .tmp2.jpg

if [ -n "$exif_date1" ]; then
    magick .tmp1.jpg -undercolor $annotate_background -gravity SouthEast -fill white -font $font -pointsize $pointsize -annotate +10+1 $exif_date1 .tmp1.jpg
fi

if [ -n "$exif_date2" ]; then
    magick .tmp2.jpg -undercolor $annotate_background -gravity SouthEast -fill white -font $font -pointsize $pointsize -annotate +10+1 $exif_date2 .tmp2.jpg
fi

magick .tmp1.jpg .tmp2.jpg +append .tmp3.jpg
rm .tmp1.jpg
rm .tmp2.jpg

magick .tmp3.jpg -stroke white -strokewidth 4 -draw "line 398,0,398,480" .tmp3.jpg

command=""

if [ "$type" = "grey" ]; then
    command="magick .tmp3.jpg -auto-level -adaptive-sharpen 1 -dither FloydSteinberg"

    if [ -z "$palette" ]; then
        command="$command -remap pattern:gray50 -monochrome"
    else
        command="$command -remap $palette"
    fi

    command="$command -type Palette -depth 8 -alpha off -compress none BMP3:$output_file"
else
    command="magick .tmp3.jpg -auto-level -dither FloydSteinberg"
    if [ -z "$palette" ]; then
        command="$command -colors $colors"
    else
        command="$command -remap $palette"
    fi
    command="$command -type Palette -depth 8 -alpha off -compress none BMP3:$output_file"
fi

eval $command

if [ -f .tmp3.jpg ]; then
    rm .tmp3.jpg
fi

if [ -f .tmp3.gif ]; then
    rm .tmp3.gif
fi

if [ $? -eq 0 ]; then
    echo "Done. Output file: $output_file"
else
    echo "Failed."
fi

set +e

exit 0
# End of script
