# Native Format Integration - TODO

## Status

✅ **Completed**:
1. Native format conversion functions added to `binary.rs`
   - `rgb_to_6c_native()` - Convert RGB pixel to native nibble
   - `convert_to_6c_native_binary()` - Convert full image to native format
   - `color_distance()` - Helper for color matching

✅ **Tested on ESP32**:
- Native format works correctly with `writeNative()`
- Colors display properly
- File size reduced by 50% (192KB vs 384KB)
- PSRAM usage reduced by 50%
- SD card read time reduced by 50%

⏳ **Remaining Work**:
Need to integrate the native format into the photoframe-processor workflow.

## Changes Needed

### 1. Add Native Format Option to CLI

**File**: `src/cli.rs`

Add a new output type or flag for native format:

```rust
// Option 1: Add as new OutputType
pub enum OutputType {
    Bmp,
    Png,
    Jpg,
    Bin,
    Native,  // ← ADD THIS
}

// Option 2: Add as boolean flag
#[derive(Parser)]
pub struct Args {
    // ... existing fields

    /// Use native format for 6-color binaries (50% smaller, optimal for GDEP073E01)
    #[arg(long)]
    pub native_format: bool,
}
```

**Recommendation**: Use Option 1 (new OutputType) for clarity.

### 2. Modify Binary Output Logic

**File**: `src/image_processing/mod.rs`

Modify the 3 locations where binary files are written:

**Location 1: Single Image Processing** (around line 1109-1114):
```rust
crate::cli::OutputType::Bin => {
    let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
    std::fs::write(&output_path, binary_data).with_context(|| {
        format!("Failed to save binary: {}", output_path.display())
    })?;
}
```

**Change to**:
```rust
crate::cli::OutputType::Bin => {
    // Use native format for 6-color, standard format otherwise
    let binary_data = if matches!(self.processing_type, ProcessingType::SixColor) {
        binary::convert_to_6c_native_binary(&processed_img)?
    } else {
        binary::convert_to_esp32_binary(&processed_img)?
    };
    std::fs::write(&output_path, binary_data).with_context(|| {
        format!("Failed to save binary: {}", output_path.display())
    })?;
}
```

**Location 2: Portrait Processing** (around line 1258-1263):
```rust
crate::cli::OutputType::Bin => {
    let binary_data = binary::convert_to_esp32_binary(&processed_img)?;
    std::fs::write(&output_path, binary_data).with_context(|| {
        format!("Failed to save portrait binary: {}", output_path.display())
    })?;
}
```

**Change to** (same as Location 1):
```rust
crate::cli::OutputType::Bin => {
    let binary_data = if matches!(self.processing_type, ProcessingType::SixColor) {
        binary::convert_to_6c_native_binary(&processed_img)?
    } else {
        binary::convert_to_esp32_binary(&processed_img)?
    };
    std::fs::write(&output_path, binary_data).with_context(|| {
        format!("Failed to save portrait binary: {}", output_path.display())
    })?;
}
```

**Location 3: Combined Portrait Processing** (around line 1765-1770):
```rust
crate::cli::OutputType::Bin => {
    let binary_data = binary::convert_to_esp32_binary(&combined_img)?;
    std::fs::write(&output_path, &binary_data).with_context(|| {
        format!("Failed to save combined binary: {}", output_path.display())
    })?;
}
```

**Change to** (same as above):
```rust
crate::cli::OutputType::Bin => {
    let binary_data = if matches!(self.processing_type, ProcessingType::SixColor) {
        binary::convert_to_6c_native_binary(&combined_img)?
    } else {
        binary::convert_to_esp32_binary(&combined_img)?
    };
    std::fs::write(&output_path, &binary_data).with_context(|| {
        format!("Failed to save combined binary: {}", output_path.display())
    })?;
}
```

### 3. Access to ProcessingType

**Issue**: Need to access `self.processing_type` in the binary saving code.

**Solution**: The `processing_type` field should already be available in the `ImageProcessor` struct. Verify with:

```rust
// In src/image_processing/mod.rs
pub struct ImageProcessor {
    pub config: Config,
    pub processing_type: ProcessingType,  // ← This field
    // ... other fields
}
```

If not present, add it during initialization.

### 4. Add Tests

**File**: `src/image_processing/binary.rs`

Add test for the new native format conversion:

```rust
#[test]
fn test_convert_to_6c_native_binary() {
    // Create a small test image with known colors
    let mut img: RgbImage = ImageBuffer::new(4, 2);

    // Set pixels to 6-color palette colors
    img.put_pixel(0, 0, Rgb([0, 0, 0]));           // BLACK
    img.put_pixel(1, 0, Rgb([255, 255, 255]));    // WHITE
    img.put_pixel(2, 0, Rgb([252, 0, 0]));        // RED
    img.put_pixel(3, 0, Rgb([0, 252, 0]));        // GREEN
    img.put_pixel(0, 1, Rgb([0, 0, 255]));        // BLUE
    img.put_pixel(1, 1, Rgb([252, 252, 0]));      // YELLOW
    img.put_pixel(2, 1, Rgb([0, 0, 0]));          // BLACK
    img.put_pixel(3, 1, Rgb([255, 255, 255]));    // WHITE

    let binary_data = convert_to_6c_native_binary(&img).unwrap();

    // Should be (4 * 2) / 2 = 4 bytes
    assert_eq!(binary_data.len(), 4);

    // Check packed values
    // Row 0: BLACK,WHITE = 0x01, RED,GREEN = 0x42
    // Row 1: BLUE,YELLOW = 0x35, BLACK,WHITE = 0x01
    assert_eq!(binary_data[0], 0x01); // BLACK + WHITE
    assert_eq!(binary_data[1], 0x42); // RED + GREEN
    assert_eq!(binary_data[2], 0x35); // BLUE + YELLOW
    assert_eq!(binary_data[3], 0x01); // BLACK + WHITE
}

#[test]
fn test_rgb_to_6c_native() {
    // Test exact palette matches
    assert_eq!(rgb_to_6c_native(&Rgb([0, 0, 0])), 0x0);           // BLACK
    assert_eq!(rgb_to_6c_native(&Rgb([255, 255, 255])), 0x1);    // WHITE
    assert_eq!(rgb_to_6c_native(&Rgb([252, 0, 0])), 0x4);        // RED
    assert_eq!(rgb_to_6c_native(&Rgb([0, 252, 0])), 0x2);        // GREEN
    assert_eq!(rgb_to_6c_native(&Rgb([0, 0, 255])), 0x3);        // BLUE
    assert_eq!(rgb_to_6c_native(&Rgb([252, 252, 0])), 0x5);      // YELLOW
}
```

## Usage Examples

Once integrated, users can process images with:

```bash
# Standard format (1 byte per pixel, 384KB)
./photoframe-processor -i photos/ -o output/ --size 800x480 --output-format bin --auto

# Native format (2 pixels per byte, 192KB) - AUTOMATIC for 6-color!
./photoframe-processor -i photos/ -o output/ --size 800x480 --output-format bin --auto

# Multiple formats including native
./photoframe-processor -i photos/ -o output/ --size 800x480 --output-format bin,bmp,jpg --auto
```

**Note**: Native format will be automatically used for 6-color processing (`--auto` detects color display).

## Benefits

Once integrated:
- ✅ **50% smaller file sizes** - 192KB instead of 384KB
- ✅ **50% less PSRAM usage** - Important for memory-constrained systems
- ✅ **Faster SD card operations** - Half the data to read/write
- ✅ **Backward compatible** - ESP32 firmware runtime detection works with both formats
- ✅ **No performance impact** - Display refresh time dominated by e-paper hardware

## Testing Plan

1. **Unit Tests**: Run `cargo test` in photoframe-processor
2. **Integration Test**: Process a real photo with 6-color mode
3. **ESP32 Test**: Copy generated .bin to SD card and test with option `8`
4. **Verification**: Check file sizes are ~192KB (not 384KB)
5. **Visual Test**: Verify colors display correctly on e-paper

## Documentation Updates Needed

After integration, update:

1. **CLAUDE.md**: Add native format information
2. **README.md**: Update examples to show native format
3. **rust/photoframe-processor/README.md**: Document native format benefits

## Estimated Time

- Code changes: ~30 minutes
- Testing: ~15 minutes
- Documentation: ~15 minutes
- **Total**: ~1 hour

## Notes

- The native format is **automatically applied** for 6-color processing
- Black/white (bw) and 7-color (7c) continue using standard format
- The ESP32 firmware already supports both formats via runtime detection
- This is a pure optimization - no functionality changes needed on ESP32 side
