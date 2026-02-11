# Source - https://stackoverflow.com/a/36046965
# Posted by Antimony, modified by community. See post 'Timeline' for change history
# Retrieved 2026-02-11, License - CC BY-SA 4.0

#!/bin/zsh

current_dir=$(dirname "$0")

# format all the files inside the include and src directories
find "$current_dir/include" -iname '*.h' | xargs clang-format -i
find "$current_dir/src" -iname '*.cpp' | xargs clang-format -i

