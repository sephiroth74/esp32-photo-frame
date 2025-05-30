#!/bin/zsh

# Given a pair of images, this script will create a new image that contains the input images side by side horizontally.
# The output directory is the same as the input directory.
# The output image will be named input1 + "_" input2 + "combined.bmp".

#!/bin/zsh

# Exit on error
set -e

pointsize=24
font='UbuntuMono-Nerd-Font-Bold'


# Default values
type=""
palette=""
input_file1=""
input_file2=""
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

# If type is color, palette must be provided
if [[ "$type" == "color" && -z "$palette" ]]; then
    echo "Error: -palette argument is required when -type is 'color'."
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
    magick .tmp1.jpg -gravity SouthEast -fill white -font $font -pointsize $pointsize -annotate +10+1 $exif_date1 .tmp1.jpg
fi

if [ -n "$exif_date2" ]; then
    magick .tmp2.jpg -gravity SouthEast -fill white -font $font -pointsize $pointsize -annotate +10+1 $exif_date2 .tmp2.jpg
fi

magick .tmp1.jpg .tmp2.jpg +append .tmp3.jpg
rm .tmp1.jpg
rm .tmp2.jpg

magick .tmp3.jpg -stroke white -strokewidth 4 -draw "line 398,0,398,480" .tmp3.jpg

if [ "$type" = "grey" ]; then
    magick .tmp3.jpg -dither FloydSteinberg -remap pattern:gray50 .tmp3.gif
    magick .tmp3.gif -monochrome .tmp3.gif #monochrome
    magick .tmp3.gif -depth 8 -alpha off -compress none BMP3:$output_file
    # magick .tmp3.gif -alpha Off $output_file # grey
else
    # Convert to color using the provided palette
    magick .tmp3.jpg -dither FloydSteinberg -remap $palette .tmp3.gif
    # magick .tmp3.gif -alpha Off -colors 256 $output_file # 8-bit color
    magick .tmp.gif -alpha Off -depth 8 -colors 256 -compress none BMP3:$output_file # 8-bit color
    # magick .tmp3.gif -alpha Off -type truecolor $output_file # 24-bit color

    # now extract the bmp image data into an array of bytes and save to a .cpp file
    
fi


rm .tmp3.jpg


rm .tmp3.gif

if [ $? -eq 0 ]; then
    echo "Done. Output file: $output_file"
else
    echo "Failed."
fi

set +e

exit 0
# End of script
