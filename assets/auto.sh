#!/bin/zsh

set -e

type="bw" # Options: bw, 6c
palette=""
use_palette=0
colors=6

palette_dir="/Users/alessandro/Desktop/arduino/photos"

input="/Users/alessandro/Desktop/arduino/photos/photos"
output="/Users/alessandro/Desktop/arduino/photos/outputs/bmp"
output_bin="/Users/alessandro/Desktop/arduino/photos/outputs/bin"

if [ "$type" = "bw" ]; then
    output="$output-bw"
    output_bin="$output_bin-bw"
    if [ $use_palette -eq 1 ]; then
        palette="$palette_dir/colortable-grey4.gif"
    else
        palette=""
        colors=4
    fi
elif [ "$type" = "6c" ]; then
    output="$output-6c"
    output_bin="$output_bin-6c"
    if [ $use_palette -eq 1 ]; then
        palette="$palette_dir/colortable-6.gif"
    else
        palette=""
        colors=6
    fi
else
    echo "Unknown type: $type"
    exit 1
fi

# now if type if 6c convert it back to 'color'
if [ "$type" = "6c" ]; then
    type="color"
elif [ "$type" = "bw" ]; then
    type="grey"
fi

echo "-------------------------------"
echo "Input: $input"
echo "Output: $output"
echo "Output Binary: $output_bin"
echo "Type: $type"
echo "Colors: $colors"
echo "Palette: $palette"
echo "-------------------------------"

if [ -z "$palette" ]; then
    ./convert_all.sh $input -output $output -type $type -colors $colors -y
else
    ./convert_all.sh $input -output $output -type $type -palette $palette -y
fi

./bmp_to_lcd.sh -input $output -output $output_bin -format binary

# ./bmp_to_lcd.sh -input ~/Desktop/arduino/photos/outputs/bmp -output ~/Desktop/arduino/photos/outputs/carray -format c-array

set +e
