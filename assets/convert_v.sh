#!/bin/zsh

# Given a pair of images, this script will create a new image that contains the input images side by side horizontally.
# The output directory is the same as the input directory.
# The output image will be named input1 + "_" input2 + "combined.bmp".

#!/bin/zsh

# Exit on error
set -e

# Check if ImageMagick is installed
if ! command -v magick &> /dev/null; then
    echo "ImageMagick could not be found. Please install it to use this script."
    exit 1
fi

input_file1=$1
input_file2=$2
output_directory=$3

if [ -z "$1" ]; then
    echo "Usage: $0 <input_file1> <input_file2> <output_directory>"
    exit 1
fi

if [ -z "$2" ]; then
    echo "Usage: $0 <input_file1> <input_file2> <output_directory>"
    exit 1
fi

if [ -z "$3" ]; then
    echo "Usage: $0 <input_file1> <input_file2> <output_directory>"
    exit 1
fi

# Check if the output directory exists, if not create it
if [ ! -d "$output_directory" ]; then
    mkdir -p "$output_directory"
fi

# Check if input files exist

# output_file will be the name of the first input file + "_" + the name of the second input file + "combined.bmp"
filename_without_extension1="${input_file1##*/}"
filename_without_extension1="${filename_without_extension1%.*}"
filename_without_extension2="${input_file2##*/}"
filename_without_extension2="${filename_without_extension2%.*}"

output_file="${output_directory}/${filename_without_extension1}_${filename_without_extension2}_combined.bmp"

magick $input_file1 -auto-orient .tmp1.jpg
magick $input_file2 -auto-orient .tmp2.jpg

magick .tmp1.jpg -resize 400x480^ -gravity Center -extent 400x480 .tmp1.jpg
magick .tmp2.jpg -resize 400x480^ -gravity Center -extent 400x480 .tmp2.jpg
magick .tmp1.jpg .tmp2.jpg +append .tmp3.jpg
rm .tmp1.jpg
rm .tmp2.jpg

magick .tmp3.jpg -stroke white -strokewidth 4 -draw "line 398,0,398,480" .tmp3.jpg
magick .tmp3.jpg -dither FloydSteinberg -remap pattern:gray50 .tmp3.gif
rm .tmp3.jpg

magick .tmp3.gif -alpha Off $output_file
rm .tmp3.gif

if [ $? -eq 0 ]; then
    echo "Image processing completed successfully. Output file: $output_file"
else
    echo "Image processing failed."
fi

set +e

exit 0
# End of script
