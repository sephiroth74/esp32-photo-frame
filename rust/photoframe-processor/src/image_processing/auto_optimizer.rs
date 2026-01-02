/// Automatic parameter optimization for e-paper display processing
///
/// This module analyzes image characteristics and determines optimal processing
/// parameters (dithering method, strength, contrast) specifically tuned for:
/// - GDEP073E01 6-color ACeP display (Black, White, Red, Yellow, Green, Blue)
/// - Portrait photography with preference for pastel/soft tones
/// - People-focused photography (solo or group)

use anyhow::Result;
use image::RgbImage;

use crate::cli::DitherMethod;
use crate::image_processing::subject_detection::SubjectDetectionResult;

#[derive(Debug, Clone)]
pub struct ImageAnalysis {
    pub has_people: bool,
    pub people_count: usize,
    pub avg_brightness: f32,        // 0.0-1.0 (0=dark, 1=bright)
    pub contrast_ratio: f32,        // 0.0-1.0 (0=flat, 1=high contrast)
    pub color_saturation: f32,      // 0.0-1.0 (0=grayscale, 1=vibrant)
    pub detail_complexity: f32,     // 0.0-1.0 (0=simple, 1=complex)
    pub dominant_hue: String,       // "red", "blue", "green", "neutral"
    pub is_low_light: bool,
    pub is_high_contrast: bool,
    pub is_monochrome_like: bool,
    pub is_pastel_toned: bool,      // True if colors are soft/desaturated
}

#[derive(Debug, Clone)]
pub struct OptimizationResult {
    pub dither_method: DitherMethod,
    pub dither_strength: f32,
    pub contrast_adjustment: f32,
    pub auto_color_correct: bool,
    pub reasoning: Vec<String>,
}

/// Main entry point: analyze image and determine optimal parameters
pub fn analyze_and_optimize(
    img: &RgbImage,
    people_detection: Option<&SubjectDetectionResult>,
) -> Result<OptimizationResult> {
    // Step 1: Analyze image characteristics
    let analysis = analyze_image_characteristics(img, people_detection)?;

    // Step 2: Determine optimal parameters for 6-color display with pastel preference
    let optimization = determine_optimal_parameters(&analysis)?;

    Ok(optimization)
}

/// Analyze image characteristics for optimization decisions
pub fn analyze_image_characteristics(
    img: &RgbImage,
    people_detection: Option<&SubjectDetectionResult>,
) -> Result<ImageAnalysis> {
    let (avg_brightness, contrast_ratio, _min_brightness) = calculate_brightness_stats(img);
    let color_saturation = calculate_color_saturation(img);
    let detail_complexity = calculate_detail_complexity(img);
    let dominant_hue = determine_dominant_hue(img);

    // Determine if image has people
    let has_people = people_detection.map_or(false, |d| d.person_count > 0);
    let people_count = people_detection.map_or(0, |d| d.person_count);

    // Classification flags
    let is_low_light = avg_brightness < 0.35;
    let is_high_contrast = contrast_ratio > 0.65;
    let is_monochrome_like = color_saturation < 0.15;
    let is_pastel_toned = color_saturation < 0.4 && avg_brightness > 0.45;

    Ok(ImageAnalysis {
        has_people,
        people_count,
        avg_brightness,
        contrast_ratio,
        color_saturation,
        detail_complexity,
        dominant_hue,
        is_low_light,
        is_high_contrast,
        is_monochrome_like,
        is_pastel_toned,
    })
}

/// Calculate brightness and contrast statistics
fn calculate_brightness_stats(img: &RgbImage) -> (f32, f32, f32) {
    let mut sum = 0u64;
    let mut min_brightness = 255u8;
    let mut max_brightness = 0u8;
    let pixel_count = (img.width() * img.height()) as u64;

    for pixel in img.pixels() {
        // Perceptual brightness (ITU-R BT.709)
        let brightness = (
            (pixel[0] as f32 * 0.2126) +
            (pixel[1] as f32 * 0.7152) +
            (pixel[2] as f32 * 0.0722)
        ) as u8;

        sum += brightness as u64;
        min_brightness = min_brightness.min(brightness);
        max_brightness = max_brightness.max(brightness);
    }

    let avg_brightness = (sum as f32) / (pixel_count as f32) / 255.0;
    let contrast_ratio = (max_brightness - min_brightness) as f32 / 255.0;
    let min_norm = min_brightness as f32 / 255.0;

    (avg_brightness, contrast_ratio, min_norm)
}

/// Calculate color saturation (how vibrant vs desaturated)
fn calculate_color_saturation(img: &RgbImage) -> f32 {
    let mut saturation_sum = 0.0;
    let pixel_count = (img.width() * img.height()) as f32;

    for pixel in img.pixels() {
        let r = pixel[0] as f32 / 255.0;
        let g = pixel[1] as f32 / 255.0;
        let b = pixel[2] as f32 / 255.0;

        let max_channel = r.max(g).max(b);
        let min_channel = r.min(g).min(b);

        // HSV saturation calculation
        let saturation = if max_channel > 0.0 {
            (max_channel - min_channel) / max_channel
        } else {
            0.0
        };

        saturation_sum += saturation;
    }

    saturation_sum / pixel_count
}

/// Calculate detail complexity using edge density
fn calculate_detail_complexity(img: &RgbImage) -> f32 {
    let gray = image::imageops::grayscale(img);

    // Simple Sobel-like edge detection
    let mut edge_count = 0;
    let width = gray.width();
    let height = gray.height();

    for y in 1..(height - 1) {
        for x in 1..(width - 1) {
            let center = gray.get_pixel(x, y)[0] as i32;
            let right = gray.get_pixel(x + 1, y)[0] as i32;
            let bottom = gray.get_pixel(x, y + 1)[0] as i32;

            let gradient_x = (right - center).abs();
            let gradient_y = (bottom - center).abs();

            if gradient_x > 30 || gradient_y > 30 {
                edge_count += 1;
            }
        }
    }

    let total_pixels = ((width - 2) * (height - 2)) as f32;
    edge_count as f32 / total_pixels
}

/// Determine dominant hue (for display-specific optimizations)
fn determine_dominant_hue(img: &RgbImage) -> String {
    let mut r_sum = 0u64;
    let mut g_sum = 0u64;
    let mut b_sum = 0u64;
    let pixel_count = (img.width() * img.height()) as u64;

    for pixel in img.pixels() {
        r_sum += pixel[0] as u64;
        g_sum += pixel[1] as u64;
        b_sum += pixel[2] as u64;
    }

    let r_avg = r_sum as f32 / pixel_count as f32;
    let g_avg = g_sum as f32 / pixel_count as f32;
    let b_avg = b_sum as f32 / pixel_count as f32;

    // Determine dominant hue based on which channel is strongest
    if r_avg > g_avg && r_avg > b_avg && r_avg > 140.0 {
        "red".to_string()
    } else if g_avg > r_avg && g_avg > b_avg && g_avg > 140.0 {
        "green".to_string()
    } else if b_avg > r_avg && b_avg > g_avg && b_avg > 140.0 {
        "blue".to_string()
    } else {
        "neutral".to_string()
    }
}

/// Determine optimal processing parameters for 6-color display with pastel preference
fn determine_optimal_parameters(analysis: &ImageAnalysis) -> Result<OptimizationResult> {
    let mut reasoning = Vec::new();

    // ========================================
    // 1. DITHERING METHOD SELECTION
    // ========================================

    let dither_method = if analysis.has_people {
        reasoning.push(format!("📸 Portrait photo with {} people detected", analysis.people_count));

        if analysis.is_pastel_toned {
            // PASTEL TONES: Use Atkinson for brightness preservation
            reasoning.push("🎨 Pastel tones detected → Atkinson dithering preserves soft colors".to_string());
            DitherMethod::Atkinson
        } else if analysis.detail_complexity > 0.25 {
            // HIGH DETAIL: Use Jarvis for smooth skin tones (ridotto da 0.4 a 0.25)
            reasoning.push("🔍 High detail complexity → Jarvis-Judice-Ninke for smooth skin".to_string());
            DitherMethod::JarvisJudiceNinke
        } else if analysis.is_low_light {
            // LOW LIGHT: Use Stucki for better shadow detail (changed from Floyd)
            reasoning.push("💡 Low light conditions → Stucki for better shadow detail".to_string());
            DitherMethod::Stucki
        } else {
            // DEFAULT PORTRAIT: Floyd-Steinberg for balanced results
            reasoning.push("👤 Standard portrait → Floyd-Steinberg for balanced rendering".to_string());
            DitherMethod::FloydSteinberg
        }
    } else {
        reasoning.push("🏞️ Landscape/scene (no people detected)".to_string());

        if analysis.is_monochrome_like {
            // B/W SCENES: Use ordered dithering for clean patterns
            reasoning.push("⚫⚪ Near-monochrome → Ordered dithering for classic halftone look".to_string());
            DitherMethod::Ordered
        } else if analysis.detail_complexity > 0.35 {
            // COMPLEX SCENES: Use Stucki for wide diffusion (ridotto da 0.5 a 0.35)
            reasoning.push("🌲 Complex scene → Stucki for wide error diffusion".to_string());
            DitherMethod::Stucki
        } else if analysis.is_pastel_toned {
            // PASTEL LANDSCAPES: Use Atkinson
            reasoning.push("🌸 Pastel landscape → Atkinson preserves soft colors".to_string());
            DitherMethod::Atkinson
        } else if analysis.color_saturation > 0.4 {
            // VIBRANT LANDSCAPES: Use Jarvis for rich color rendering (nuovo)
            reasoning.push("🎨 Vibrant scene → Jarvis-Judice-Ninke for rich colors".to_string());
            DitherMethod::JarvisJudiceNinke
        } else {
            // DEFAULT LANDSCAPE: Floyd-Steinberg
            reasoning.push("🏔️ Standard landscape → Floyd-Steinberg for balanced results".to_string());
            DitherMethod::FloydSteinberg
        }
    };

    // ========================================
    // 2. DITHER STRENGTH SELECTION
    // ========================================

    let dither_strength = if analysis.is_pastel_toned {
        // PASTEL PREFERENCE: Gentle dithering to preserve soft tones
        reasoning.push("✨ Pastel optimization: Reduced dither strength (0.8) to preserve soft gradients".to_string());
        0.8
    } else if analysis.has_people {
        if analysis.is_low_light {
            // DARK PORTRAITS: Low dithering to reduce noise
            reasoning.push("🌙 Dark portrait → Low dither (0.7) to minimize noise on faces".to_string());
            0.7
        } else if analysis.avg_brightness > 0.7 {
            // BRIGHT PORTRAITS: Standard dithering
            reasoning.push("☀️ Bright portrait → Standard dither (1.0) for detail preservation".to_string());
            1.0
        } else {
            // NORMAL PORTRAITS: Gentle dithering for smooth skin
            reasoning.push("👥 Normal lighting → Gentle dither (0.9) for smooth skin tones".to_string());
            0.9
        }
    } else {
        // LANDSCAPES: More aggressive dithering acceptable
        if analysis.detail_complexity > 0.5 {
            reasoning.push("🌿 High-detail scene → Strong dither (1.3) for texture rendering".to_string());
            1.3
        } else if analysis.is_monochrome_like {
            reasoning.push("📷 Monochrome scene → Moderate dither (1.1) for tonal transitions".to_string());
            1.1
        } else {
            reasoning.push("🖼️ Standard scene → Balanced dither (1.0)".to_string());
            1.0
        }
    };

    // ========================================
    // 3. CONTRAST ADJUSTMENT SELECTION
    // ========================================

    let contrast_adjustment = if analysis.is_pastel_toned {
        // PASTEL PRESERVATION: Avoid increasing contrast (would destroy soft look)
        reasoning.push("🎀 Pastel preservation: No contrast boost to maintain soft aesthetic".to_string());
        0.0
    } else if analysis.contrast_ratio < 0.2 {
        // EXTREMELY FLAT IMAGE: Minimal boost needed (riduciamo l'aggressività)
        if analysis.has_people {
            reasoning.push("📉 Very low contrast portrait → Gentle boost (+0.10) for subtle features".to_string());
            0.10
        } else {
            reasoning.push("📉 Very flat scene → Light boost (+0.15) for depth".to_string());
            0.15
        }
    } else if analysis.contrast_ratio < 0.35 {
        // LOW CONTRAST: Minimal boost (riduciamo drasticamente per preferire toni pastello)
        if analysis.has_people {
            reasoning.push("📊 Low contrast portrait → Minimal boost (+0.05) for definition".to_string());
            0.05
        } else {
            reasoning.push("📊 Low contrast scene → Light boost (+0.10) for clarity".to_string());
            0.10
        }
    } else if analysis.is_high_contrast {
        // HIGH CONTRAST: Riduciamo per un look più pastello
        if analysis.has_people {
            reasoning.push("⚡ High contrast portrait → Strong reduction (-0.20) for softer, pastel-like shadows".to_string());
            -0.20
        } else {
            reasoning.push("⚡ High contrast scene → Moderate reduction (-0.15) for softer look".to_string());
            -0.15
        }
    } else {
        // NORMAL CONTRAST: Preferenza per toni pastello - riduzione più marcata
        reasoning.push("🌸 Normal contrast → Moderate reduction (-0.10) for lighter, pastel preference".to_string());
        -0.10
    };

    // ========================================
    // 4. AUTO COLOR CORRECTION
    // ========================================

    let auto_color_correct = if analysis.is_pastel_toned {
        // PASTEL IMAGES: Skip color correction (would oversaturate)
        reasoning.push("🚫 Skip auto-color: Preserving natural pastel tones".to_string());
        false
    } else if analysis.is_low_light && !analysis.is_monochrome_like {
        // LOW LIGHT COLOR IMAGES: Apply correction
        reasoning.push("💡 Low light color photo → Enable color correction for vibrancy".to_string());
        true
    } else if analysis.color_saturation < 0.2 && !analysis.is_monochrome_like {
        // DESATURATED (BUT NOT B/W): Apply gentle correction
        reasoning.push("🎨 Desaturated colors → Enable gentle color enhancement".to_string());
        true
    } else {
        // DEFAULT: Skip (preserve original color balance for pastel look)
        reasoning.push("✓ Natural color balance → Skip correction for authentic look".to_string());
        false
    };

    // ========================================
    // 5. 6-COLOR DISPLAY SPECIFIC ADJUSTMENTS
    // ========================================

    let (dither_method, dither_strength, mut reasoning) = adjust_for_6color_display(
        dither_method,
        dither_strength,
        analysis,
        reasoning,
    );

    // Add final summary
    reasoning.push(format!(
        "📺 Final params: {} | Strength: {:.1} | Contrast: {:+.2} | Auto-color: {}",
        format!("{:?}", dither_method),
        dither_strength,
        contrast_adjustment,
        if auto_color_correct { "Yes" } else { "No" }
    ));

    Ok(OptimizationResult {
        dither_method,
        dither_strength,
        contrast_adjustment,
        auto_color_correct,
        reasoning,
    })
}

/// Adjust parameters specifically for 6-color ACeP display characteristics
fn adjust_for_6color_display(
    dither_method: DitherMethod,
    mut dither_strength: f32,
    analysis: &ImageAnalysis,
    mut reasoning: Vec<String>,
) -> (DitherMethod, f32, Vec<String>) {
    reasoning.push("🖥️ 6-color ACeP display optimizations:".to_string());

    // Monochrome images need more dithering (no native grays)
    if analysis.is_monochrome_like {
        dither_strength = dither_strength.max(1.1);
        reasoning.push("  ├─ B/W image: Increased dither for gray simulation".to_string());
    }

    // Pastel images should use gentle dithering (preserve soft gradients)
    if analysis.is_pastel_toned {
        dither_strength = dither_strength.min(0.9);
        reasoning.push("  ├─ Pastel tones: Capped dither to preserve softness".to_string());
    }

    // Native display colors (red, yellow) can use less dithering
    if analysis.dominant_hue == "red" || analysis.dominant_hue == "green" {
        dither_strength *= 0.9;
        reasoning.push(format!("  ├─ Dominant {} hue: Reduced dither (native color)", analysis.dominant_hue));
    }

    // Blue-heavy images need more dithering (blue is less saturated on 6-color)
    if analysis.dominant_hue == "blue" {
        dither_strength *= 1.1;
        reasoning.push("  └─ Blue-dominant: Increased dither for better rendering".to_string());
    } else {
        reasoning.push("  └─ Color balance suitable for display".to_string());
    }

    (dither_method, dither_strength, reasoning)
}
