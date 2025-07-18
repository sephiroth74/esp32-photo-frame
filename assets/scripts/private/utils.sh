
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
extract_image_info() {
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
    else
        if (( height > width )); then
            is_portrait=1
        else
            is_portrait=0
        fi
    fi

    echo "$orientation" "$is_portrait" "$width" "$height"
}

