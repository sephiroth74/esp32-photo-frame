#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Magic 'PFR1' little-endian (0x50465231)
 */
#define BIN_MAGIC 1346785841

/**
 * Header size: magic(4) + ver(1) + hlen(2) + w(2) + h(2) + rot(1) + color(1) + payload_len(4) + header_crc32(4)
 */
#define BIN_HEADER_SIZE 21

/**
 * FFI-safe result returned to callers across the C ABI.
 *
 * - `success`: whether the operation completed successfully.
 * - `width`, `height`: dimensions of the resulting image/payload.
 * - `data_ptr`/`data_len`: pointer and length of a heap-allocated buffer owned by Rust.
 *   Callers MUST free this buffer by calling `photoframe_dithering_free` when done.
 */
typedef struct DitheringResult {
  bool success;
  uint32_t width;
  uint32_t height;
  uint8_t *data_ptr;
  uintptr_t data_len;
} DitheringResult;

/**
 * FFI-safe validation result for PFR1 .pfr1 files.
 */
typedef struct BinValidationResult {
  bool success;
  uint8_t version;
  uint16_t header_len;
  uint16_t width;
  uint16_t height;
  uint8_t rotation;
  uint8_t color_mode;
  uint32_t payload_len;
  uint32_t payload_crc32;
} BinValidationResult;

/**
 * Dummy FFI function for linker/debugging purposes.
 */
uint32_t photoframe_dummy_function(void);

/**
 * Free memory allocated by Rust and returned across the FFI boundary.
 *
 * Safety: `ptr` must be a pointer previously returned by this crate and `len` must
 * match the original allocation length.
 */
void photoframe_dithering_free(uint8_t *ptr, uintptr_t len);

/**
 * Apply dithering to image bytes (PNG/JPEG input) -> PNG output.
 *
 * Safety and parameter mapping (FFI):
 * - `image_data` must point to a valid buffer of length `image_len` containing a
 *   JPEG/PNG/etc image in a format understood by the `image` crate.
 * - `method`: mapping to `DitheringMethod` (see `DitheringMethod` enum)
 * - `color_mode`: mapping (see `ColorMode`)
 * - `dither_strength`: recommended range 0.0..=2.0 (1.0 default). Values outside
 *   that range are accepted but may produce extreme visual results.
 * - `saturation`, `contrast`, `brightness`: same semantics and recommended ranges
 *   as in `apply_color_adjustments` (see core module).
 *
 * The function returns a `DitheringResult` with `data_ptr` pointing to a heap
 * allocated PNG buffer; callers MUST call `photoframe_dithering_free(ptr, len)`
 * to avoid memory leaks.
 */
struct DitheringResult photoframe_dithering_apply(const uint8_t *image_data,
                                                  uintptr_t image_len,
                                                  const char *dither_method,
                                                  uint8_t color_mode,
                                                  float dither_strength,
                                                  float saturation,
                                                  float contrast,
                                                  float brightness);

/**
 * Convert image bytes using a DisplayType and return raw payload (FFI entry).
 *
 * `display_type` mapping:
 * - 0 => BlackAndWhite
 * - 1 => SixColors
 *
 * Returns a DitheringResult with data_ptr/len pointing to a heap-allocated buffer
 * which MUST be freed by calling `photoframe_dithering_free`.
 *
 * # Parameters
 * - image_data: Raw image bytes (JPEG, PNG, etc.)
 * - image_len: Length of image data
 * - processing_type: 0 = BlackAndWhite, 1 = SixColors
 * - rotation: Display rotation (0-3)
 */
struct DitheringResult photoframe_convert_with_processing(const uint8_t *image_data,
                                                          uintptr_t image_len,
                                                          uint8_t rotation,
                                                          uint8_t mode);

/**
 * Validate a PFR1 .pfr1 file (C ABI).
 * Returns metadata on success; on failure, success=false and other fields are zeroed.
 */
struct BinValidationResult photoframe_validate_bin(const uint8_t *data_ptr, uintptr_t data_len);
