#!/bin/sh

SIZES="12 16 24 32 48 64 96 128 160 196"

# call svg_to_headers.sh for each size
for size in $SIZES
do
  echo "Generating icons for size ${size}x${size}..."
  ./svg_to_headers.sh $size
done

# now call final_generate_icons_h.py
echo "Generating final icons.h..."
python3 final_generate_icons_h.py
echo "Done generating icons.h."
