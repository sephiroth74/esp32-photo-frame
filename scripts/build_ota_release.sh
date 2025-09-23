#!/bin/bash

# ESP32 Photo Frame - OTA Release Builder
# Builds firmware for all supported boards and creates OTA manifest

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORMIO_DIR="$PROJECT_ROOT/platformio"
SCRIPTS_DIR="$PROJECT_ROOT/scripts"
OTA_DIR="$PLATFORMIO_DIR/ota"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Extract version from config.h
get_firmware_version() {
    local config_file="$PLATFORMIO_DIR/include/config.h"

    if [[ ! -f "$config_file" ]]; then
        error "Config file not found: $config_file"
    fi

    local major minor patch
    major=$(grep -E "#define\s+FIRMWARE_VERSION_MAJOR\s+" "$config_file" | awk '{print $3}' | head -1)
    minor=$(grep -E "#define\s+FIRMWARE_VERSION_MINOR\s+" "$config_file" | awk '{print $3}' | head -1)
    patch=$(grep -E "#define\s+FIRMWARE_VERSION_PATCH\s+" "$config_file" | awk '{print $3}' | head -1)

    if [[ -z "$major" || -z "$minor" || -z "$patch" ]]; then
        error "Could not extract version from config.h. Found: major=$major, minor=$minor, patch=$patch"
    fi

    echo "v$major.$minor.$patch"
}

# Extract minimum supported version from config.h
get_min_supported_version() {
    local config_file="$PLATFORMIO_DIR/include/config.h"

    if [[ ! -f "$config_file" ]]; then
        error "Config file not found: $config_file"
    fi

    local major minor patch
    major=$(grep -E "#define\s+OTA_MIN_SUPPORTED_VERSION_MAJOR\s+" "$config_file" | awk '{print $3}' | head -1)
    minor=$(grep -E "#define\s+OTA_MIN_SUPPORTED_VERSION_MINOR\s+" "$config_file" | awk '{print $3}' | head -1)
    patch=$(grep -E "#define\s+OTA_MIN_SUPPORTED_VERSION_PATCH\s+" "$config_file" | awk '{print $3}' | head -1)

    if [[ -z "$major" || -z "$minor" || -z "$patch" ]]; then
        error "Could not extract min supported version from config.h. Found: major=$major, minor=$minor, patch=$patch"
    fi

    echo "$major|$minor|$patch"
}

# Set release version from config.h or command line
RELEASE_VERSION="${1:-$(get_firmware_version)}"

log() {
    echo -e "${BLUE}[OTA Builder]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
    exit 1
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check dependencies
check_dependencies() {
    log "Checking dependencies..."

    command -v pio >/dev/null 2>&1 || error "PlatformIO CLI not found. Please install PlatformIO."
    command -v jq >/dev/null 2>&1 || error "jq not found. Please install jq for JSON processing."
    command -v sha256sum >/dev/null 2>&1 || command -v shasum >/dev/null 2>&1 || error "sha256sum or shasum not found."

    if [[ ! -f "$PLATFORMIO_DIR/platformio.ini" ]]; then
        error "PlatformIO project not found at $PLATFORMIO_DIR"
    fi

    success "All dependencies found"
}

# Get SHA256 checksum (cross-platform)
get_checksum() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | cut -d' ' -f1
    else
        error "No checksum utility available"
    fi
}

# Clean and prepare OTA directory
prepare_ota_dir() {
    log "Preparing OTA directory..."
    mkdir -p "$OTA_DIR/firmware"

    # Clean existing firmware builds
    rm -f "$OTA_DIR/firmware"/*.bin

    # Ensure manifest directory exists
    touch "$OTA_DIR/ota_manifest.json"
}

# Build firmware for a specific board
build_firmware() {
    local board="$1"
    local output_name="firmware-${board}.bin"
    local output_path="$OTA_DIR/firmware/$output_name"

    # Build in current directory context to avoid contaminating callers output
    (
        cd "$PLATFORMIO_DIR"

        # Clean previous build (suppress all output)
        pio run -e "$board" -t clean >/dev/null 2>&1 || true

        # Build firmware (suppress all output)
        if ! pio run -e "$board" >/dev/null 2>&1; then
            echo "ERROR: Failed to build firmware for $board" >&2
            exit 1
        fi

        # Find the built firmware
        local fw_path
        fw_path=$(find ".pio/build/$board" -name "*.bin" -type f | grep -E "(firmware|main)" | head -1)

        if [[ -z "$fw_path" || ! -f "$fw_path" ]]; then
            echo "ERROR: Built firmware not found for $board" >&2
            exit 1
        fi

        # Copy firmware to build directory
        cp "$fw_path" "$output_path"
    )

    # Check if build succeeded
    if [[ $? -ne 0 ]]; then
        error "Failed to build firmware for $board"
    fi

    if [[ ! -f "$output_path" ]]; then
        error "Built firmware not found: $output_path"
    fi

    local size
    size=$(stat -f%z "$output_path" 2>/dev/null || stat -c%s "$output_path" 2>/dev/null)

    local checksum
    checksum=$(get_checksum "$output_path")

    # Log success to stderr to avoid contaminating the return value
    echo -e "${GREEN}[SUCCESS]${NC} Built $board firmware: $size bytes, SHA256: ${checksum:0:16}..." >&2

    # Return values for manifest generation (output only the result, no logs)
    echo "$output_name|$size|sha256:$checksum"
}

# Get board-specific configuration
get_board_config() {
    local board="$1"

    case "$board" in
        "feathers3_unexpectedmaker")
            echo "partitions/photo_frame_16mb_ota.csv|16MB"
            ;;
        *)
            echo "partitions/photo_frame_16mb_ota.csv|16MB"
            ;;
    esac
}

# Generate OTA manifest
generate_manifest() {
    local manifest_path="$OTA_DIR/ota_manifest.json"
    local base_url="${OTA_BASE_URL:-https://github.com/sephiroth74/esp32-photo-frame}"

    log "Generating OTA manifest..."

    # Only support feathers3_unexpectedmaker (removed deprecated adafruit_huzzah32_feather_v2)
    local board="feathers3_unexpectedmaker"

    log "Building firmware for $board..."
    local build_result
    build_result=$(build_firmware "$board")

    local filename size checksum
    IFS='|' read -r filename size checksum <<< "$build_result"

    local config
    config=$(get_board_config "$board")
    local partition_table flash_size
    IFS='|' read -r partition_table flash_size <<< "$config"

    # Extract version components for structured version object
    local version_clean="${RELEASE_VERSION#v}" # Remove 'v' prefix if present
    local version_major minor patch
    IFS='.' read -r version_major minor patch <<< "$version_clean"

    # Extract minimum supported version components
    local min_version_info
    min_version_info=$(get_min_supported_version)
    local min_major min_minor min_patch
    IFS='|' read -r min_major min_minor min_patch <<< "$min_version_info"

    # Generate complete manifest JSON
    cat > "$manifest_path" << EOF
{
  "manifest_version": "1.0",
  "project": "esp32-photo-frame",
  "releases": [
    {
      "version": {
        "major": $version_major,
        "minor": $minor,
        "patch": $patch,
        "string": "$RELEASE_VERSION"
      },
      "release_date": "$(date -u +%Y-%m-%d)",
      "release_notes": "Automated OTA release build",
      "compatibility": [
        {
            "board": "$board",
            "min_version": {
                "major": $min_major,
                "minor": $min_minor,
                "patch": $min_patch,
                "string": "v$min_major.$min_minor.$min_patch"
            },
            "breaking_changes": false,
            "migration_required": false
        }
      ]
    }
  ]
}
EOF

    # Validate JSON
    if ! jq empty "$manifest_path" >/dev/null 2>&1; then
        error "Generated manifest is not valid JSON"
    fi

    success "OTA manifest generated: $manifest_path"
}

# Create release summary
create_summary() {
    local summary_path="$OTA_DIR/RELEASE_SUMMARY.md"

    log "Creating release summary..."

    cat > "$summary_path" << EOF
# ESP32 Photo Frame - OTA Release $RELEASE_VERSION

**Release Date:** $(date -u +"%Y-%m-%d %H:%M:%S UTC")

## Firmware Builds

EOF

    for fw in "$OTA_DIR/firmware"/*.bin; do
        if [[ -f "$fw" ]]; then
            local basename
            basename=$(basename "$fw")
            local size
            size=$(stat -f%z "$fw" 2>/dev/null || stat -c%s "$fw" 2>/dev/null)
            local checksum
            checksum=$(get_checksum "$fw")

            # Format size in human-readable format (cross-platform)
            local human_size
            if (( size >= 1048576 )); then
                human_size="$(( size / 1048576 ))MB"
            elif (( size >= 1024 )); then
                human_size="$(( size / 1024 ))KB"
            else
                human_size="${size}B"
            fi

            cat >> "$summary_path" << EOF
### $basename
- **Size:** $human_size ($size bytes)
- **SHA256:** \`$checksum\`

EOF
        fi
    done

    cat >> "$summary_path" << EOF
## Installation

1. Upload firmware files to GitHub releases
2. Update OTA server with manifest
3. Devices will check for updates automatically

## Verification

\`\`\`bash
# Verify firmware checksums
EOF

    for fw in "$OTA_DIR/firmware"/*.bin; do
        if [[ -f "$fw" ]]; then
            local basename checksum
            basename=$(basename "$fw")
            checksum=$(get_checksum "$fw")
            if command -v sha256sum >/dev/null 2>&1; then
                echo "echo '$checksum  $basename' | sha256sum -c" >> "$summary_path"
            elif command -v shasum >/dev/null 2>&1; then
                echo "echo '$checksum  $basename' | shasum -a 256 -c" >> "$summary_path"
            fi
        fi
    done

    echo '```' >> "$summary_path"

    success "Release summary created: $summary_path"
}

# Main execution
main() {
    log "Starting OTA release build for version $RELEASE_VERSION"

    check_dependencies
    prepare_ota_dir
    generate_manifest
    create_summary

    success "OTA release build completed!"
    log "Build artifacts:"
    log "  - Firmware: $OTA_DIR/firmware/"
    log "  - Manifest: $OTA_DIR/ota_manifest.json"
    log "  - Summary: $OTA_DIR/RELEASE_SUMMARY.md"

    if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
        echo "ota_build_dir=$OTA_DIR" >> "$GITHUB_OUTPUT"
        echo "ota_manifest=$OTA_DIR/ota_manifest.json" >> "$GITHUB_OUTPUT"
    fi
}

# Run if called directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi