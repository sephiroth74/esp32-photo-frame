pub mod bin_format;
mod core;
pub mod dithering;
pub mod types;

use image::ImageEncoder;
use std::slice;

// Re-export core public API for Rust users (include color adjustments)
pub use core::{
    apply_color_adjustments, convert_bw_to_demo_bitmap_mode1, convert_image_from_bytes,
    convert_to_demo_bitmap_mode1, convert_to_esp32_binary, process_image_with_display_type,
};

// Re-export the dithering dispatcher from the dithering module
pub use dithering::apply_dithering;

pub use bin_format::*;
pub use types::*;

/// FFI-safe result returned to callers across the C ABI.
///
/// - `success`: whether the operation completed successfully.
/// - `width`, `height`: dimensions of the resulting image/payload.
/// - `data_ptr`/`data_len`: pointer and length of a heap-allocated buffer owned by Rust.
///   Callers MUST free this buffer by calling `photoframe_dithering_free` when done.
#[repr(C)]
pub struct DitheringResult {
    pub success: bool,
    pub width: u32,
    pub height: u32,
    pub data_ptr: *mut u8,
    pub data_len: usize,
}

/// Free memory allocated by Rust and returned across the FFI boundary.
///
/// Safety: `ptr` must be a pointer previously returned by this crate and `len` must
/// match the original allocation length.
#[no_mangle]
pub extern "C" fn photoframe_dithering_free(ptr: *mut u8, len: usize) {
    if !ptr.is_null() {
        unsafe {
            let _ = Vec::from_raw_parts(ptr, len, len);
        }
    }
}

/// Apply dithering to image bytes (PNG/JPEG input) -> PNG output.
///
/// Safety and parameter mapping (FFI):
/// - `image_data` must point to a valid buffer of length `image_len` containing a
///   JPEG/PNG/etc image in a format understood by the `image` crate.
/// - `method`: mapping to `DitheringMethod`:
///     - 0 => FloydSteinberg
///     - 1 => Atkinson
///     - 2 => Stucki
///     - 3 => JarvisJudiceNinke
///     - 4 => Ordered
/// - `display_type`: mapping:
///     - 0 => SixColors
///     - 1 => BlackAndWhite
/// - `dither_strength`: recommended range 0.0..=2.0 (1.0 default). Values outside
///   that range are accepted but may produce extreme visual results.
/// - `saturation`, `contrast`, `brightness`: same semantics and recommended ranges
///   as in `apply_color_adjustments` (see core module).
///
/// The function returns a `DitheringResult` with `data_ptr` pointing to a heap
/// allocated PNG buffer; callers MUST call `photoframe_dithering_free(ptr, len)`
/// to avoid memory leaks.
#[no_mangle]
pub unsafe extern "C" fn photoframe_dithering_apply(
    image_data: *const u8,
    image_len: usize,
    method: u8,
    display_type: u8,
    dither_strength: f32,
    saturation: f32,
    contrast: f32,
    brightness: f32,
) -> DitheringResult {
    // Convert input to slice
    let input_slice = unsafe { slice::from_raw_parts(image_data, image_len) };

    // Decode image
    let img = match image::load_from_memory(input_slice) {
        Ok(img) => img.to_rgb8(),
        Err(_) => {
            return DitheringResult {
                success: false,
                width: 0,
                height: 0,
                data_ptr: std::ptr::null_mut(),
                data_len: 0,
            }
        }
    };

    // Apply color adjustments (function now lives in core module)
    let adjusted = crate::core::apply_color_adjustments(&img, saturation, contrast, brightness);

    // Parse method and display type
    let dither_method = match method {
        0 => DitheringMethod::FloydSteinberg,
        1 => DitheringMethod::Atkinson,
        2 => DitheringMethod::Stucki,
        3 => DitheringMethod::JarvisJudiceNinke,
        4 => DitheringMethod::Ordered,
        _ => DitheringMethod::FloydSteinberg,
    };

    let display = match display_type {
        0 => DisplayType::SixColors,
        1 => DisplayType::BlackAndWhite,
        _ => DisplayType::SixColors,
    };

    // Apply dithering
    let result =
        crate::dithering::apply_dithering(&adjusted, dither_method, display, dither_strength);
    if result.is_err() {
        return DitheringResult {
            success: false,
            width: 0,
            height: 0,
            data_ptr: std::ptr::null_mut(),
            data_len: 0,
        };
    }

    let result = result.unwrap();

    // Encode to PNG using simple encoder
    let mut output_bytes = Vec::new();
    let encoder = image::codecs::png::PngEncoder::new(&mut output_bytes);
    match encoder.write_image(
        result.as_raw(),
        result.width(),
        result.height(),
        image::ExtendedColorType::Rgb8,
    ) {
        Ok(_) => {
            let len = output_bytes.len();
            let ptr = output_bytes.as_mut_ptr();
            std::mem::forget(output_bytes); // Don't drop, let caller free

            DitheringResult {
                success: true,
                width: result.width(),
                height: result.height(),
                data_ptr: ptr,
                data_len: len,
            }
        }
        Err(_) => DitheringResult {
            success: false,
            width: 0,
            height: 0,
            data_ptr: std::ptr::null_mut(),
            data_len: 0,
        },
    }
}

/// Convert image bytes using a DisplayType and return raw payload (FFI entry).
///
/// `display_type` mapping:
/// - 0 => BlackAndWhite
/// - 1 => SixColors
///
/// Returns a DitheringResult with data_ptr/len pointing to a heap-allocated buffer
/// which MUST be freed by calling `photoframe_dithering_free`.
///
/// # Parameters
/// - image_data: Raw image bytes (JPEG, PNG, etc.)
/// - image_len: Length of image data
/// - processing_type: 0 = BlackAndWhite, 1 = SixColors
/// - rotation: Display rotation (0-3)
#[no_mangle]
pub unsafe extern "C" fn photoframe_convert_with_processing(
    image_data: *const u8,
    image_len: usize,
    processing_type: u8,
    rotation: u8,
) -> DitheringResult {
    // Convert input to slice
    let input_slice = unsafe { slice::from_raw_parts(image_data, image_len) };

    let ptype = match processing_type {
        0 => DisplayType::BlackAndWhite,
        1 => DisplayType::SixColors,
        _ => DisplayType::SixColors,
    };

    match convert_image_from_bytes(input_slice, ptype) {
        Some((payload, w, h)) => {
            // Infer color mode: 0 if all bytes are 0x00 or 0xFF (BW), else 1 (6C)
            let color_mode = if payload.iter().all(|&b| b == 0x00 || b == 0xFF) {
                0
            } else {
                1
            };
            // Build .bin file with PFR1 header
            let mut buf = build_bin_file(
                &payload, w as u16, h as u16, rotation, color_mode, 1u8,
            );
            let len = buf.len();
            let ptr = buf.as_mut_ptr();
            std::mem::forget(buf);
            DitheringResult {
                success: true,
                width: w,
                height: h,
                data_ptr: ptr,
                data_len: len,
            }
        }
        None => DitheringResult {
            success: false,
            width: 0,
            height: 0,
            data_ptr: std::ptr::null_mut(),
            data_len: 0,
        },
    }
}

// Re-export for Rust usage
pub use core::*;
