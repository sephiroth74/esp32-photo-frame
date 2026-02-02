use image::ImageReader;
use image::RgbImage;
use photoframe_lib::{
    ColorMode, DisplayType, DitheringMethod, SIX_COLOR_PALETTE, apply_color_adjustments,
    apply_dithering, convert_bw_to_demo_bitmap_mode1, convert_image_from_bytes,
    convert_to_demo_bitmap_mode1, convert_to_esp32_binary, photoframe_convert_with_processing,
    photoframe_dithering_free,
};
use std::fs;

// Helper: ensure output directory exists and return its path
fn ensure_output_dir() -> std::path::PathBuf {
    let out = std::path::Path::new("tests/output").to_path_buf();
    fs::create_dir_all(&out).expect("create tests/output");
    out
}

fn load_test_image() -> RgbImage {
    let path = std::path::Path::new("tests/data/default_portrait.jpg");
    let img = ImageReader::open(path)
        .expect("open test image")
        .decode()
        .expect("decode");
    img.to_rgb8()
}

fn save(img: &RgbImage, name: &str) {
    let out_dir = ensure_output_dir();
    let save_path = out_dir.join(name);
    img.save(&save_path).expect("failed to save image");
}

// Helper: check that every pixel in `img` is one of the colors in `palette`.
fn pixels_only_in_palette(img: &RgbImage, palette: &[(u8, u8, u8)]) -> bool {
    for p in img.pixels() {
        let mut found = false;
        for &(pr, pg, pb) in palette {
            if p[0] == pr && p[1] == pg && p[2] == pb {
                found = true;
                break;
            }
        }
        if !found {
            return false;
        }
    }
    true
}

// Helper: check BW palette membership fast
fn pixels_only_bw(img: &RgbImage) -> bool {
    for p in img.pixels() {
        if !((p[0] == 0 && p[1] == 0 && p[2] == 0) || (p[0] == 255 && p[1] == 255 && p[2] == 255)) {
            return false;
        }
    }
    true
}

// --- Brightness tests ---
#[test]
fn test_brightness_near_average() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 1.0, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    assert!(out.pixels().any(|p| p[0] != 0 || p[1] != 0 || p[2] != 0));
    save(&out, "brightness_near.png");
}

#[test]
fn test_brightness_medium_far() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 1.0, 1.5);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "brightness_medium.png");
}

#[test]
fn test_brightness_limit() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 1.0, 3.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "brightness_limit.png");
}

// --- Contrast tests ---
#[test]
fn test_contrast_near_average() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 1.0, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "contrast_near.png");
}

#[test]
fn test_contrast_medium_far() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 1.5, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "contrast_medium.png");
}

#[test]
fn test_contrast_limit() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 3.0, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "contrast_limit.png");
}

// --- Saturation tests ---
#[test]
fn test_saturation_near_average() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.0, 1.0, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "saturation_near.png");
}

#[test]
fn test_saturation_medium_far() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 1.5, 1.0, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "saturation_medium.png");
}

#[test]
fn test_saturation_limit() {
    let img = load_test_image();
    let out = apply_color_adjustments(&img, 3.0, 1.0, 1.0);
    assert_eq!(out.dimensions(), img.dimensions());
    save(&out, "saturation_limit.png");
}

// --- Dithering method tests (three strength scenarios) ---
#[test]
fn test_dithering_methods_near() {
    let img = load_test_image();
    let methods = [
        DitheringMethod::FloydSteinberg,
        DitheringMethod::Atkinson,
        DitheringMethod::Stucki,
        DitheringMethod::JarvisJudiceNinke,
        DitheringMethod::Ordered,
    ];
    for method in methods.iter() {
        let out = apply_dithering(&img, *method, DisplayType::SixColors, 1.0).expect("dithering");

        assert_eq!(out.dimensions(), img.dimensions());
        save(&out, &format!("dither_{:?}_near.png", method));
    }
}

#[test]
fn test_dithering_methods_medium_far() {
    let img = load_test_image();
    let methods = [
        DitheringMethod::FloydSteinberg,
        DitheringMethod::Atkinson,
        DitheringMethod::Stucki,
        DitheringMethod::JarvisJudiceNinke,
        DitheringMethod::Ordered,
    ];
    for method in methods.iter() {
        let out = apply_dithering(&img, *method, DisplayType::SixColors, 1.5).expect("dithering");
        assert_eq!(out.dimensions(), img.dimensions());
        save(&out, &format!("dither_{:?}_medium.png", method));
    }
}

#[test]
fn test_dithering_methods_limit() {
    let img = load_test_image();
    let methods = [
        DitheringMethod::FloydSteinberg,
        DitheringMethod::Atkinson,
        DitheringMethod::Stucki,
        DitheringMethod::JarvisJudiceNinke,
        DitheringMethod::Ordered,
    ];
    for method in methods.iter() {
        let out = apply_dithering(&img, *method, DisplayType::SixColors, 2.0).expect("dithering");
        assert_eq!(out.dimensions(), img.dimensions());
        save(&out, &format!("dither_{:?}_limit.png", method));
    }
}

// --- Dither strength focused tests (varying strength for a single method) ---
#[test]
fn test_dither_strength_cases() {
    let img = load_test_image();
    let method = DitheringMethod::FloydSteinberg;
    let cases = [("near", 1.0), ("medium", 0.5), ("limit", 2.0)];
    for (label, strength) in cases.iter() {
        let out =
            apply_dithering(&img, method, DisplayType::SixColors, *strength).expect("dithering");
        assert_eq!(out.dimensions(), img.dimensions());
        save(&out, &format!("dither_strength_{}_{}.png", label, strength));
    }
}

// --- Conversion tests after dithering ---
#[test]
fn test_convert_after_dithering_sixcolors_and_bw() {
    let img = load_test_image();
    let (w, h) = img.dimensions();

    let methods = [
        DitheringMethod::FloydSteinberg,
        DitheringMethod::Atkinson,
        DitheringMethod::Stucki,
        DitheringMethod::JarvisJudiceNinke,
        DitheringMethod::Ordered,
    ];

    let out_dir = ensure_output_dir();

    // Allowed demo mode bytes for SixColors
    let demo_allowed: [u8; 6] = [0x00, 0xFF, 0xFC, 0xE0, 0x03, 0x1C];

    for method in methods.iter() {
        // SixColors path
        let d6 = apply_dithering(&img, *method, DisplayType::SixColors, 1.0).expect("dithering");
        save(&d6, &format!("post_dither_{:?}_6c.png", method));
        // Verify only six-color palette colors are present
        assert!(
            pixels_only_in_palette(&d6, &SIX_COLOR_PALETTE),
            "Image contains colors outside SIX_COLOR_PALETTE for {:?}",
            method
        );

        // convert_to_esp32_binary now returns Result<Vec<u8>> (one byte per pixel RRRGGGBB)
        let bin6 = convert_to_esp32_binary(&d6).expect("convert_to_esp32_binary failed");
        assert_eq!(
            bin6.len(),
            (w * h) as usize,
            "sixcolors payload len mismatch for {:?}",
            method
        );

        // convert_to_demo_bitmap_mode1 returns one byte per pixel (mode1 mapping)
        let demo6 = convert_to_demo_bitmap_mode1(&d6).expect("convert_to_demo_bitmap_mode1 failed");
        assert_eq!(
            demo6.len(),
            (w * h) as usize,
            "demo wrapper len mismatch for {:?}",
            method
        );
        // Check demo bytes are only from allowed set
        for &b in demo6.iter() {
            assert!(
                demo_allowed.contains(&b),
                "demo payload contains unexpected byte 0x{:02X}",
                b
            );
        }

        std::fs::write(
            out_dir.join(format!("post_dither_{:?}_6c.pfr1", method)),
            &bin6,
        )
        .expect("write bin6");
        std::fs::write(
            out_dir.join(format!("post_dither_{:?}_6c.demo", method)),
            &demo6,
        )
        .expect("write demo6");

        // BlackAndWhite path
        let dbw =
            apply_dithering(&img, *method, DisplayType::BlackAndWhite, 1.0).expect("dithering");
        save(&dbw, &format!("post_dither_{:?}_bw.png", method));
        // Verify only BW palette colors are present
        assert!(
            pixels_only_bw(&dbw),
            "Image contains colors outside BW_PALETTE for {:?}",
            method
        );

        // For BW, convert_to_esp32_binary returns one byte per pixel; black->0x00, white->0xFF
        let binbw = convert_to_esp32_binary(&dbw).expect("convert_to_esp32_binary failed for BW");
        assert_eq!(
            binbw.len(),
            (w * h) as usize,
            "bw payload len mismatch for {:?}",
            method
        );
        for &b in binbw.iter() {
            assert!(
                b == 0x00 || b == 0xFF,
                "bw payload contains non-bw byte 0x{:02X}",
                b
            );
        }

        let demobw =
            convert_to_demo_bitmap_mode1(&dbw).expect("convert_to_demo_bitmap_mode1 failed for BW");
        assert_eq!(
            demobw.len(),
            (w * h) as usize,
            "demo bw wrapper len mismatch for {:?}",
            method
        );
        for &b in demobw.iter() {
            assert!(
                b == 0x00 || b == 0xFF,
                "demo bw contains non-bw byte 0x{:02X}",
                b
            );
        }

        std::fs::write(
            out_dir.join(format!("post_dither_{:?}_bw.pfr1", method)),
            &binbw,
        )
        .expect("write binbw");
        std::fs::write(
            out_dir.join(format!("post_dither_{:?}_bw.demo", method)),
            &demobw,
        )
        .expect("write demobw");
    }
}

fn make_test_pattern_8x8_bw() -> RgbImage {
    // Create an 8x8 checkerboard-like pattern where top-left is white
    let mut img = RgbImage::new(8, 8);
    for y in 0..8 {
        for x in 0..8 {
            let white = (x + y) % 2 == 0;
            if white {
                img.put_pixel(x as u32, y as u32, image::Rgb([255, 255, 255]));
            } else {
                img.put_pixel(x as u32, y as u32, image::Rgb([0, 0, 0]));
            }
        }
    }
    img
}

fn make_test_pattern_8x8_6c() -> RgbImage {
    // Create an 8x8 image cycling through the SIX_COLOR_PALETTE colors
    let mut img = RgbImage::new(8, 8);
    let palette = [
        (0u8, 0u8, 0u8),
        (255u8, 255u8, 255u8),
        (255u8, 0u8, 0u8),
        (255u8, 255u8, 0u8),
        (0u8, 255u8, 0u8),
        (0u8, 0u8, 255u8),
    ];
    for y in 0..8 {
        for x in 0..8 {
            let idx = ((y * 8 + x) % palette.len()) as usize;
            let (r, g, b) = palette[idx];
            img.put_pixel(x as u32, y as u32, image::Rgb([r, g, b]));
        }
    }
    img
}

#[test]
fn test_deterministic_8x8_bw_conversion() {
    let img = make_test_pattern_8x8_bw();
    // Convert to compressed ESP32 one-byte-per-pixel
    let bin = convert_to_esp32_binary(&img).expect("convert failed");
    assert_eq!(bin.len(), 64);
    for y in 0..8 {
        for x in 0..8 {
            let idx = (y * 8 + x) as usize;
            let expected = if (x + y) % 2 == 0 { 0xFFu8 } else { 0x00u8 };
            assert_eq!(bin[idx], expected, "pixel {} {} mismatch", x, y);
        }
    }

    // Convert using demo bitmap BW path to ensure mode1 byte mapping
    let demo = convert_bw_to_demo_bitmap_mode1(&img).expect("bw demo convert failed");
    assert_eq!(demo.len(), 64);
    for y in 0..8 {
        for x in 0..8 {
            let idx = (y * 8 + x) as usize;
            let expected = if (x + y) % 2 == 0 { 0xFFu8 } else { 0x00u8 };
            assert_eq!(demo[idx], expected, "demo pixel {} {} mismatch", x, y);
        }
    }
}

#[test]
fn test_deterministic_8x8_6c_conversion() {
    let img = make_test_pattern_8x8_6c();
    // Apply ordered dithering (deterministic) to SixColors
    let d = apply_dithering(&img, DitheringMethod::Ordered, DisplayType::SixColors, 1.0)
        .expect("dithering");
    let bin = convert_to_esp32_binary(&d).expect("convert failed");
    assert_eq!(bin.len(), 64);
    // compute expected compressed values for palette
    let palette = [
        (0u8, 0u8, 0u8),
        (255u8, 255u8, 255u8),
        (255u8, 0u8, 0u8),
        (255u8, 255u8, 0u8),
        (0u8, 255u8, 0u8),
        (0u8, 0u8, 255u8),
    ];
    let mut expected_vals: Vec<u8> = Vec::new();
    for &(r, g, b) in palette.iter() {
        let val = ((r / 32) << 5) + ((g / 32) << 2) + (b / 64);
        expected_vals.push(val);
    }
    for i in 0..12 {
        let expected = expected_vals[i % 6];
        assert_eq!(bin[i], expected, "index mismatch at {}", i);
    }
}

// --- FFI and payload size tests ---

#[test]
fn test_ffi_convert_and_free() {
    // Read raw bytes of the test JPEG
    let path = std::path::Path::new("tests/data/default_portrait.jpg");
    let bytes = std::fs::read(path).expect("read test image bytes");

    // Call FFI wrapper for BlackAndWhite (0)
    unsafe {
        let res = photoframe_convert_with_processing(
            bytes.as_ptr(),
            bytes.len(),
            0u8,
            ColorMode::BlackAndWhite,
        );
        assert!(res.success, "ffi convert BW failed");
        assert!(res.width > 0 && res.height > 0, "invalid dims");
        assert!(
            !res.data_ptr.is_null() && res.data_len > 0,
            "no data returned"
        );
        // free
        photoframe_dithering_free(res.data_ptr, res.data_len);
    }

    // Call FFI wrapper for SixColors (1)
    unsafe {
        let res = photoframe_convert_with_processing(
            bytes.as_ptr(),
            bytes.len(),
            0u8,
            ColorMode::SixColors,
        );
        assert!(res.success, "ffi convert 6c failed");
        assert!(res.width > 0 && res.height > 0, "invalid dims");
        assert!(
            !res.data_ptr.is_null() && res.data_len > 0,
            "no data returned"
        );
        // free
        photoframe_dithering_free(res.data_ptr, res.data_len);
    }
}

#[test]
fn test_payload_size_for_800x480_image() {
    let path = std::path::Path::new("tests/data/default_portrait.jpg");
    let bytes = std::fs::read(path).expect("read test image bytes");

    // Sanity: decode dims using image crate
    let img = ImageReader::open(path)
        .expect("open")
        .decode()
        .expect("decode");
    let (w, h) = img.to_rgb8().dimensions();
    let expected = (w as usize) * (h as usize);
    assert_eq!(
        expected, 384000,
        "expected test image to be 384000 pixels total"
    );

    // SixColors
    if let Some((payload6, pw, ph)) = convert_image_from_bytes(&bytes, DisplayType::SixColors) {
        assert_eq!(pw, w);
        assert_eq!(ph, h);
        assert_eq!(payload6.len(), expected, "SixColors payload size mismatch");
    } else {
        panic!("convert_image_from_bytes returned None for SixColors");
    }

    // BlackAndWhite
    if let Some((payload_bw, bw_w, bw_h)) =
        convert_image_from_bytes(&bytes, DisplayType::BlackAndWhite)
    {
        assert_eq!(bw_w, w);
        assert_eq!(bw_h, h);
        assert_eq!(
            payload_bw.len(),
            expected,
            "BlackAndWhite payload size mismatch"
        );
    } else {
        panic!("convert_image_from_bytes returned None for BlackAndWhite");
    }
}
