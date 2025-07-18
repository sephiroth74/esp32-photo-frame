#!/bin/zsh

# expected arguments:
# -i <input_directory> (can be multiple)
# -o <output_directory>
# -t type (either bw or 6c) [optional, default: bw]
# -p <palette_file> [optional, default: none]
# -c <colors> (only if palette is not specified) [optional, default: 0]
# -s|--size <target_size> (target size in pixels, either width or height, depending on the image orientation)
# --pointsize <pointsize> [optional, default: 24]
# --font <font> [optional, default: 'UbuntuMono-Nerd-Font-Bold']
# --annotate_background <annotate_background> [optional, default: '#00000040']
# --auto (enable automatic processing, which swaps width and height if necessary) [optional, default: false]
# -v|--verbose (enable verbose output) [optional, default: false]

# This script will call 'auto_resize_and_annotate.sh' with the appropriate parameters for each image in the input directories.

input_dirs=()
output_dir=""
type="bw"
palette=""
colors=0
pointsize=24
font='UbuntuMono-Nerd-Font-Bold'
annotate_background='#00000040'
auto_process=false
verbose=false
extensions=()
target_size=""
target_width=0
target_height=0
default_extensions=("jpg" "jpeg" "png")

tmp_dir="/tmp/auto"
intermediate_dir="${tmp_dir}/intermediate"

file_dir="$(dirname "$(realpath "$0")")"

auto_resize_and_annotate_script="${file_dir}/private/auto_resize_and_annotate.sh"
combine_images_script="${file_dir}/private/combine_images.sh"
convert_script="${file_dir}/private/convert.sh"
bmp_to_lcd_script="${file_dir}/private/bmp_to_lcd.sh"

source "${file_dir}/private/utils.sh"

t1=$(date +%s%N)

usage() {
    echo "Usage: $0 [options] -i <input_directory>... -o <output_directory> [-t <type>] [-p <palette_file>] -s|--size <target_size> [--pointsize <pointsize>] [--font <font>] [--annotate_background <annotate_background>] [--auto] --extensions <extensions> [-v|--verbose]"
    echo "Options:"
    echo "  -i, --input <directory>  Input directory containing images (can be specified multiple times)"
    echo "  -o, --output <directory> Output directory for resized and annotated images"
    echo "  -t, --type <type>        Type of image processing (bw or 6c, default: bw)"
    echo "  -p, --palette <file>     Palette file for color processing (optional)"
    echo "  -s, --size <target_size> Target size in pixels (width or height depending on orientation)"
    echo "  --pointsize <size>       Point size for the annotation (default: $pointsize)"
    echo "  --font <font>            Font for the annotation (default: $font)"
    echo "  --annotate_background <color> Background color for the annotation (default: $annotate_background)"
    echo "  --auto                   Enable automatic processing (swaps width and height if necessary, default: false)"
    echo "  --extensions <ext1,ext2,...> Comma-separated list of image extensions to process (default: jpg,jpeg,png,heic)"

    echo "  -v, --verbose            Enable verbose output"
    echo "  -h, --help               Show this help message"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--input)
            input_dirs+=("$2")
            shift 2
            ;;
        -o|--output)
            output_dir="$2"
            shift 2
            ;;
        -t|--type)
            type="$2"
            shift 2
            ;;
        -p|--palette)
            palette="$2"
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
        --extensions)
            IFS=',' read -r extensions <<< "$2"
            shift 2
            ;;
        --auto)
            auto_process=true
            shift
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
    esac
done

# Validate required arguments
if [[ ${#input_dirs[@]} -eq 0 ]]; then
    echo "Error: At least one input directory must be specified."
    usage
fi

# Validate output directory
if [[ -z "$output_dir" ]]; then
    echo "Error: Output directory must be specified."
    usage
fi

# Validate target size
if [[ -z "$target_size" ]]; then
    echo "Error: Target size must be specified."
    usage
fi

# Parse target size (using zsh pattern matching)
if [[ "$target_size" =~ ^([0-9]+)x([0-9]+)$ ]]; then
    target_width="${match[1]}"
    target_height="${match[2]}"
else
    echo "Error: Invalid target size format. Use <width>x<height> (e.g., 800x480)."
    usage
fi

# Validate type
if [[ -z "$type" ]]; then
    echo "Error: Type must be specified (bw or 6c)."
    usage
fi

if [[ "$type" != "bw" && "$type" != "6c" ]]; then
    echo "Error: Type must be either 'bw' or '6c'."
    usage
fi

# Create output directory if it doesn't exist
mkdir -p "$output_dir"  

# if type if 6c and palette is not specified, then set colors to 6
if [[ "$type" == "6c" && -z "$palette" ]]; then
    colors=6
fi

# if type is bw and palette is not specified, then set colors to 0
if [[ "$type" == "bw" && -z "$palette" ]]; then
    colors=0
fi

# if extensions is empty, then set it to default_extensions
if [[ ${#extensions[@]} -eq 0 ]]; then
    extensions=("${default_extensions[@]}")
fi

    
# Check if the auto_resize_and_annotate.sh script exists
if [[ ! -f "$auto_resize_and_annotate_script" ]]; then
    echo "Error: $auto_resize_and_annotate_script script not found."
    exit 1
fi

# Check if the combine_images.sh script exists
if [[ ! -f "$combine_images_script" ]]; then
    echo "Error: $combine_images_script script not found."
    exit 1
fi

# Check if the convert.sh script exists
if [[ ! -f "$convert_script" ]]; then
    echo "Error: $convert_script script not found."
    exit 1
fi

# Check if the bmp_to_lcd.sh script exists
if [[ ! -f "$bmp_to_lcd_script" ]]; then
    echo "Error: $bmp_to_lcd_script script not found."
    exit 1
fi

# create tmp_dir if it doesn't exist
if [[ ! -d "$tmp_dir" ]]; then
    mkdir -p "$tmp_dir"
fi

# create intermediate_dir if it doesn't exist
if [[ ! -d "$intermediate_dir" ]]; then
    mkdir -p "$intermediate_dir"
fi

# clean up the tmp_dir and intermediate_dir
if [[ -d "$intermediate_dir" ]]; then
    set +e
    rm -rf "$intermediate_dir"/*
    set -e
fi  

set -e

start_time=$(date +%s%N)
count=0

portrait_images=()
landscape_images=()

tmp_portrait_images=()
tmp_landscape_images=()

echo "Starting auto processing of images..."

# Process each input directory
for input_dir in "${input_dirs[@]}"; do
    if [[ ! -d "$input_dir" ]]; then
        echo "Error: Input directory '$input_dir' does not exist."
        continue
    fi

    # For each image in the input directory, construct the command, using glob to match image files
    # find all the image files in the input directory with the specified extensions

    if [[ "$verbose" == true ]]; then
        echo "----------------------------------------------------------------"
        echo "Processing directory: $input_dir"
    fi

    find_command="find \"$input_dir\" -type f \\( "
    for ext in "${extensions[@]}"; do
        find_command+=" -iname \"*.${ext}\" -o"
    done
    find_command="${find_command% -o} \\)"

    eval "$find_command" | while read -r input_file; do
        if [[ ! -f "$input_file" ]]; then
            echo "Skipping non-file: $input_file"
            continue
        fi

        echo -n "[$((count + 1))] checking $input_file.. "

        # if the file extension is not in default_extensions (case insensitive), then convert it to jpg
        file_extension="${input_file##*.}"

        # convert to lowercase for comparison
        file_extension_lower=$(echo "$file_extension" | tr '[:upper:]' '[:lower:]')

        if [[ ! " ${default_extensions[@]} " =~ " ${file_extension_lower} " ]] && [[ ! " ${default_extensions[@]} " =~ " ${file_extension_lower} " ]]; then
            if [[ "$verbose" == true ]]; then
                echo -n "converting"
            fi
            output_file="${tmp_dir}/$(basename "$input_file" .${input_file##*.})".jpg
            magick "$input_file" "$output_file"
            input_file="$output_file"
        fi

        image_info=$(extract_image_info "$input_file")

        if [[ $? -ne 0 ]]; then
            echo "error"
            exit 1
        fi

        orientation=$(echo "$image_info" | cut -d ' ' -f 1)
        is_portrait=$(echo "$image_info" | cut -d ' ' -f 2)
        source_width=$(echo "$image_info" | cut -d ' ' -f 3)
        source_height=$(echo "$image_info" | cut -d ' ' -f 4)

        if [[ "$is_portrait" == "1" ]]; then
            portrait_images+=("$input_file")
        else
            landscape_images+=("$input_file")
        fi

        count=$((count + 1))

        echo "ok"
    done
done

elapsed_time=$(( ( $(date +%s%N) - start_time ) / 1000000 )) # convert nanoseconds to milliseconds
elapsed_time_string=""
if [[ $elapsed_time -ge 1000 ]]; then
    elapsed_time_string="$((elapsed_time / 1000)) seconds"
else
    elapsed_time_string="${elapsed_time} ms"
fi
echo "checking $count files done in $elapsed_time_string"
echo ""

# now print all portrait and landscape images
if [[ "$verbose" == true ]]; then
    echo "----------------------------------------------------------------"
    echo "Portrait and Landscape Images Summary:"
    echo "Portrait Images: ${#portrait_images[@]}"
    echo "Landscape Images: ${#landscape_images[@]}"
fi

# Shuffle the portrait images array
shuffled_portrait_images=()
for image in "${portrait_images[@]}"; do
    # Generate a random index
    random_index=$((RANDOM % (${#shuffled_portrait_images[@]} + 1)))
    # Insert the image at the random index
    shuffled_portrait_images=("${shuffled_portrait_images[@]:0:$random_index}" "$image" "${shuffled_portrait_images[@]:$random_index}")
done

# Reinitialize the portrait_images array with the shuffled images
portrait_images=("${shuffled_portrait_images[@]}")

auto_resize_and_annotate_command="${auto_resize_and_annotate_script} --pointsize \"$pointsize\" --font \"$font\" --annotate_background \"$annotate_background\""

if [[ "$verbose" == true ]]; then
    auto_resize_and_annotate_command+=" -v"
fi

if [[ "$auto_process" == true ]]; then
    auto_resize_and_annotate_command+=" --auto"
fi


# Now process all the images in the landscape_images and portrait_images array using the auto_resize_and_annotate.sh script
# store the output into tmp_landscape_images and tmp_portrait_images arrays

echo "Processing landscape images..."

count=0
start_time=$(date +%s%N)

total_landscape_images=${#landscape_images[@]}
total_portrait_images=${#portrait_images[@]}

for image in "${landscape_images[@]}"; do
    output_file="${intermediate_dir}/$(basename "$image")"
    command_to_run="$auto_resize_and_annotate_command -i \"$image\" -o \"$output_file\" --size \"$target_size\""

    s1=$(date +%s%N)
    echo -n "[$((count + 1))/$total_landscape_images] Processing $image.. "

    result=$(eval "$command_to_run")
    if [[ $? -ne 0 ]]; then
        echo "error"
        exit 1
    fi

    s2=$(date +%s%N)
    elapsed_time=$(( (s2 - s1) / 1000000 )) # convert nanoseconds to milliseconds
    echo " done in $elapsed_time ms"
    
    tmp_landscape_images+=("$output_file")
    count=$((count + 1))
done

elapsed_time=$(( ( $(date +%s%N) - start_time ) / 1000000 )) # convert nanoseconds to milliseconds
elapsed_time_string=""
if [[ $elapsed_time -ge 1000 ]]; then
    elapsed_time_string="$((elapsed_time / 1000)) seconds"
else
    elapsed_time_string="${elapsed_time} ms"
fi
echo "Processed $count landscape images in $elapsed_time_string"
echo ""


count=0
start_time=$(date +%s%N)
s1=$(date +%s%N)

# target size for portrait images is half of the target_width and target_height
tmp_size="$((target_width / 2))x$((target_height))"

echo "Processing portrait images..."
for image in "${portrait_images[@]}"; do
    output_file="${intermediate_dir}/$(basename "$image")"
    command_to_run="$auto_resize_and_annotate_command -i \"$image\" -o \"$output_file\" --size \"$tmp_size\""

    s1=$(date +%s%N)
    echo -n "[$((count + 1))/$total_portrait_images] Processing $image.. "
    result=$(eval "$command_to_run")
    if [[ $? -ne 0 ]]; then
        echo "error"
        exit 1
    fi

    elapsed_time=$(( ( $(date +%s%N) - s1 ) / 1000000 )) # convert nanoseconds to milliseconds
    echo " done in $elapsed_time ms"
    
    tmp_portrait_images+=("$output_file")
    count=$((count + 1))
done

end_time=$(date +%s%N)
elapsed_time=$(( ( $(date +%s%N) - start_time ) / 1000000 )) # convert nanoseconds to milliseconds

elapsed_time_string=""
if [[ $elapsed_time -ge 1000 ]]; then
    elapsed_time_string="$((elapsed_time / 1000)) seconds"
else
    elapsed_time_string="${elapsed_time} ms"
fi

echo "Processed $count portrait images in $elapsed_time_string"
echo "Total portrait images: ${#tmp_portrait_images[@]}"
echo ""

# now combine all the portrait images. each couple of images will be combined into one image and then the output will be stored into the landscape_images array
echo "Combining portrait images into landscape images..."

count=0
start_time=$(date +%s%N)

if [[ ${#tmp_portrait_images[@]} -eq 0 ]]; then
    # nothing here..
else
    for ((i = 1; i < ${#tmp_portrait_images[@]}; i+=2)); do
        image1="${tmp_portrait_images[i]}"
        image2="${tmp_portrait_images[i + 1]}"

        # Check if both images exist
        if [[ ! -f "$image1" ]]; then
            echo "Error: first image does not exist ($image1)"
            continue
        fi
        if [[ ! -f "$image2" ]]; then
            image2=$image1 # if the second image does not exist, use the first image
        fi

        s1=$(date +%s%N)

        output_file="${intermediate_dir}/combined_portrait_$((i / 2 + 1)).jpg"
        command_to_run="$combine_images_script --size \"$target_size\" \"$image1\" \"${image2}\" \"$output_file\""

        basename1=$(basename "$image1")
        basename2=$(basename "$image2")

        echo "command: $command_to_run"

        echo -n "[$((count + 1))] processing $basename1 and $basename2 into $output_file.. "

        eval "$command_to_run"
        if [[ $? -ne 0 ]]; then
            echo "error"
            exit 1
        fi
        tmp_landscape_images+=("$output_file")

        # delete the input images
        rm -f "$image1" "$image2"

        s2=$(date +%s%N)
        elapsed_time=$(( (s2 - s1) / 1000000 )) # convert nanoseconds to milliseconds
        echo " done in $elapsed_time ms"
        
        count=$((count + 1))
        
    done
fi

elapsed_time=$(( ( $(date +%s%N) - start_time ) / 1000000 )) # convert nanoseconds to milliseconds
echo "Combined $((count * 2)) portrait images into landscape images in $elapsed_time ms"
echo ""


# now convert all the images using the convert.sh script

echo "Converting images to the specified type ($type)..."

count=0
start_time=$(date +%s%N)
command_to_run="$convert_script"

if [[ "$type" == "bw" ]]; then
    command_to_run+=" --type grey"
elif [[ "$type" == "6c" ]]; then
    command_to_run+=" --type color"
else
    echo "Error: Invalid type specified. Must be either 'bw' or '6c'."
    exit 1
fi

# if palette is specified, then add it to the command
if [[ -n "$palette" ]]; then
    if [[ ! -f "$palette" ]]; then
        echo "Error: Palette file '$palette' does not exist."
        exit 1
    fi
    command_to_run+=" --palette \"$palette\""
else
    # if colors is specified, then add it to the command
    if [[ "$colors" -gt 0 ]]; then
        command_to_run+=" --colors \"$colors\""
    fi
fi

for image in "${tmp_landscape_images[@]}"; do
    # output file is /output_dir/filename.bmp
    output_file="${output_dir}/$(basename "$image" .jpg).bmp"
    command="$command_to_run \"$image\" \"$output_file\""

    s1=$(date +%s%N)

    echo -n "[$((count + 1))] Converting $image to $output_file.. "

    result=$(eval "$command")

    if [[ $? -ne 0 ]]; then
        echo "error"
        exit 1
    fi

    s2=$(date +%s%N)
    elapsed_time=$(( (s2 - s1) / 1000000 )) # convert nanoseconds to milliseconds
    echo " done in $elapsed_time ms"

    count=$((count + 1))
done

t2=$(date +%s%N)
elapsed_time=$(( ( t2 - start_time ) / 1000000 )) # convert nanoseconds to milliseconds
total_time=$(( (t2 - t1) / 1000000 )) # convert nanoseconds to milliseconds

total_time_string=""
if [[ $total_time -ge 1000 ]]; then
    total_time_string="$((total_time / 1000)) seconds"
else
    total_time_string="${total_time} ms"
fi

echo "Processed $count images in $elapsed_time ms"
echo ""

# No converting all the output images to binary format
output_bin_dir="${output_dir}/bin"
mkdir -p "$output_bin_dir"

echo "Converting images to binary format..."
t1=$(date +%s%N)
result=$(eval "$bmp_to_lcd_script -input \"$output_dir\" -output \"$output_bin_dir\" -format binary")
if [[ $? -ne 0 ]]; then
    echo "Error: Failed to convert images to binary format."
    exit 1
fi
t2=$(date +%s%N)
elapsed_time=$(( (t2 - t1) / 1000000 )) # convert nanoseconds to milliseconds
elapsed_time_string=""
if [[ $elapsed_time -ge 1000 ]]; then
    elapsed_time_string="$((elapsed_time / 1000)) seconds"
else
    elapsed_time_string="${elapsed_time} ms"
fi
echo "Converted images to binary format in $elapsed_time_string"
echo ""

echo ""
echo "All images processed and saved to $output_dir in $total_time_string"
echo ""

# clean up the tmp_dir
if [[ -d "$tmp_dir" ]]; then
    if [[ "$verbose" == true ]]; then
        echo "Cleaning up temporary directory.."
    fi
    set +e
    rm -rf "$tmp_dir"
    set -e
fi

set +e

echo "done"
exit 0
