# Dithering Algorithms Implementation Guide

## Nuovi Algoritmi da Implementare

### 1. Atkinson Dithering
**Caratteristiche**:
- Creato da Bill Atkinson per MacPaint (1984)
- Diffonde solo 6/8 (75%) dell'errore
- Preserva la luminosità originale
- **Ottimo per foto** con toni chiari

**Matrice di diffusione**:
```
        *   1/8  1/8
    1/8 1/8  1/8
        1/8
```
Totale: 6/8 (25% errore scartato)

### 2. Stucki Dithering
**Caratteristiche**:
- Simile a Floyd-Steinberg ma più diffuso
- Usa matrice 5×3 (più ampia)
- **Ottimo per ridurre pattern visibili**

**Matrice di diffusione**:
```
            *   8/42  4/42
    2/42  4/42  8/42  4/42  2/42
    1/42  2/42  4/42  2/42  1/42
```
Totale: 42/42 (100% errore diffuso)

### 3. Jarvis-Judice-Ninke (JJN)
**Caratteristiche**:
- Matrice 5×3 molto diffusa
- **Migliore per fotografie** con molti dettagli
- Risultato molto smooth

**Matrice di diffusione**:
```
            *   7/48  5/48
    3/48  5/48  7/48  5/48  3/48
    1/48  3/48  5/48  3/48  1/48
```
Totale: 48/48 (100% errore diffuso)

---

## Confronto Algoritmi

| Algoritmo | Matrice | Errore Diffuso | Best For | Velocità |
|-----------|---------|----------------|----------|----------|
| Floyd-Steinberg | 2×2 | 100% | Gradients generici | ⚡⚡⚡ Veloce |
| Atkinson | 3×2 | 75% | Foto luminose | ⚡⚡ Medio |
| Stucki | 5×3 | 100% | Ridurre pattern | ⚡ Lento |
| Jarvis-Judice-Ninke | 5×3 | 100% | Fotografie dettagliate | ⚡ Lento |
| Ordered (Bayer) | - | - | Testo/linee | ⚡⚡⚡ Velocissimo |

---

## Implementazione

### Step 1: Aggiungere Enum `DitherMethod` ✅ FATTO
File: `rust/photoframe-processor/src/cli.rs`

```rust
#[derive(Debug, Clone, ValueEnum, PartialEq, Eq)]
pub enum DitherMethod {
    #[value(name = "floyd-steinberg")]
    FloydSteinberg,
    #[value(name = "atkinson")]
    Atkinson,
    #[value(name = "stucki")]
    Stucki,
    #[value(name = "jarvis")]
    JarvisJudiceNinke,
    #[value(name = "ordered")]
    Ordered,
}
```

### Step 2: Implementare Funzioni di Dithering
File: `rust/photoframe-processor/src/image_processing/convert_improved.rs`

Aggiungere alla fine del file (dopo riga 329):

```rust
/// Atkinson dithering - preserves brightness, good for photos
/// Diffuses only 75% of error, creating lighter results
pub fn apply_atkinson_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            let error_r = current_r - (palette_r as f32);
            let error_g = current_g - (palette_g as f32);
            let error_b = current_b - (palette_b as f32);

            // Atkinson error distribution pattern:
            //         *   1/8  1/8
            //     1/8 1/8  1/8
            //         1/8
            // Total: 6/8 (75% of error, 25% discarded)

            distribute_atkinson_error(
                &mut working_r,
                &mut working_g,
                &mut working_b,
                x,
                y,
                width,
                height,
                error_r,
                error_g,
                error_b,
            );
        }
    }

    Ok(output)
}

fn distribute_atkinson_error(
    working_r: &mut [Vec<f32>],
    working_g: &mut [Vec<f32>],
    working_b: &mut [Vec<f32>],
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    error_r: f32,
    error_g: f32,
    error_b: f32,
) {
    let factor = 1.0 / 8.0;
    let x_idx = x as usize;
    let y_idx = y as usize;

    // Right pixel (x+1, y)
    if x + 1 < width {
        working_r[y_idx][x_idx + 1] += error_r * factor;
        working_g[y_idx][x_idx + 1] += error_g * factor;
        working_b[y_idx][x_idx + 1] += error_b * factor;
    }
    // Right+1 pixel (x+2, y)
    if x + 2 < width {
        working_r[y_idx][x_idx + 2] += error_r * factor;
        working_g[y_idx][x_idx + 2] += error_g * factor;
        working_b[y_idx][x_idx + 2] += error_b * factor;
    }

    if y + 1 < height {
        // Left pixel (x-1, y+1)
        if x > 0 {
            working_r[y_idx + 1][x_idx - 1] += error_r * factor;
            working_g[y_idx + 1][x_idx - 1] += error_g * factor;
            working_b[y_idx + 1][x_idx - 1] += error_b * factor;
        }
        // Center pixel (x, y+1)
        working_r[y_idx + 1][x_idx] += error_r * factor;
        working_g[y_idx + 1][x_idx] += error_g * factor;
        working_b[y_idx + 1][x_idx] += error_b * factor;
        // Right pixel (x+1, y+1)
        if x + 1 < width {
            working_r[y_idx + 1][x_idx + 1] += error_r * factor;
            working_g[y_idx + 1][x_idx + 1] += error_g * factor;
            working_b[y_idx + 1][x_idx + 1] += error_b * factor;
        }
    }

    if y + 2 < height {
        // Center pixel (x, y+2)
        working_r[y_idx + 2][x_idx] += error_r * factor;
        working_g[y_idx + 2][x_idx] += error_g * factor;
        working_b[y_idx + 2][x_idx] += error_b * factor;
    }
}

/// Stucki dithering - reduces visible patterns with wide diffusion
pub fn apply_stucki_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            let error_r = current_r - (palette_r as f32);
            let error_g = current_g - (palette_g as f32);
            let error_b = current_b - (palette_b as f32);

            distribute_stucki_error(
                &mut working_r,
                &mut working_g,
                &mut working_b,
                x,
                y,
                width,
                height,
                error_r,
                error_g,
                error_b,
            );
        }
    }

    Ok(output)
}

fn distribute_stucki_error(
    working_r: &mut [Vec<f32>],
    working_g: &mut [Vec<f32>],
    working_b: &mut [Vec<f32>],
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    error_r: f32,
    error_g: f32,
    error_b: f32,
) {
    let x_idx = x as usize;
    let y_idx = y as usize;
    let divisor = 42.0;

    // Current row (y)
    if x + 1 < width {
        let factor = 8.0 / divisor;
        working_r[y_idx][x_idx + 1] += error_r * factor;
        working_g[y_idx][x_idx + 1] += error_g * factor;
        working_b[y_idx][x_idx + 1] += error_b * factor;
    }
    if x + 2 < width {
        let factor = 4.0 / divisor;
        working_r[y_idx][x_idx + 2] += error_r * factor;
        working_g[y_idx][x_idx + 2] += error_g * factor;
        working_b[y_idx][x_idx + 2] += error_b * factor;
    }

    // Next row (y+1)
    if y + 1 < height {
        if x >= 2 {
            let factor = 2.0 / divisor;
            working_r[y_idx + 1][x_idx - 2] += error_r * factor;
            working_g[y_idx + 1][x_idx - 2] += error_g * factor;
            working_b[y_idx + 1][x_idx - 2] += error_b * factor;
        }
        if x >= 1 {
            let factor = 4.0 / divisor;
            working_r[y_idx + 1][x_idx - 1] += error_r * factor;
            working_g[y_idx + 1][x_idx - 1] += error_g * factor;
            working_b[y_idx + 1][x_idx - 1] += error_b * factor;
        }
        let factor = 8.0 / divisor;
        working_r[y_idx + 1][x_idx] += error_r * factor;
        working_g[y_idx + 1][x_idx] += error_g * factor;
        working_b[y_idx + 1][x_idx] += error_b * factor;

        if x + 1 < width {
            let factor = 4.0 / divisor;
            working_r[y_idx + 1][x_idx + 1] += error_r * factor;
            working_g[y_idx + 1][x_idx + 1] += error_g * factor;
            working_b[y_idx + 1][x_idx + 1] += error_b * factor;
        }
        if x + 2 < width {
            let factor = 2.0 / divisor;
            working_r[y_idx + 1][x_idx + 2] += error_r * factor;
            working_g[y_idx + 1][x_idx + 2] += error_g * factor;
            working_b[y_idx + 1][x_idx + 2] += error_b * factor;
        }
    }

    // Row y+2
    if y + 2 < height {
        if x >= 2 {
            let factor = 1.0 / divisor;
            working_r[y_idx + 2][x_idx - 2] += error_r * factor;
            working_g[y_idx + 2][x_idx - 2] += error_g * factor;
            working_b[y_idx + 2][x_idx - 2] += error_b * factor;
        }
        if x >= 1 {
            let factor = 2.0 / divisor;
            working_r[y_idx + 2][x_idx - 1] += error_r * factor;
            working_g[y_idx + 2][x_idx - 1] += error_g * factor;
            working_b[y_idx + 2][x_idx - 1] += error_b * factor;
        }
        let factor = 4.0 / divisor;
        working_r[y_idx + 2][x_idx] += error_r * factor;
        working_g[y_idx + 2][x_idx] += error_g * factor;
        working_b[y_idx + 2][x_idx] += error_b * factor;

        if x + 1 < width {
            let factor = 2.0 / divisor;
            working_r[y_idx + 2][x_idx + 1] += error_r * factor;
            working_g[y_idx + 2][x_idx + 1] += error_g * factor;
            working_b[y_idx + 2][x_idx + 1] += error_b * factor;
        }
        if x + 2 < width {
            let factor = 1.0 / divisor;
            working_r[y_idx + 2][x_idx + 2] += error_r * factor;
            working_g[y_idx + 2][x_idx + 2] += error_g * factor;
            working_b[y_idx + 2][x_idx + 2] += error_b * factor;
        }
    }
}

/// Jarvis-Judice-Ninke dithering - best for detailed photographs
pub fn apply_jarvis_judice_ninke_dithering(
    img: &RgbImage,
    palette: &[(u8, u8, u8)],
) -> Result<RgbImage> {
    let (width, height) = img.dimensions();
    let mut output = RgbImage::new(width, height);
    let (mut working_r, mut working_g, mut working_b) = create_working_buffers(img);

    for y in 0..height {
        for x in 0..width {
            let y_idx = y as usize;
            let x_idx = x as usize;

            let current_r = working_r[y_idx][x_idx].clamp(0.0, 255.0);
            let current_g = working_g[y_idx][x_idx].clamp(0.0, 255.0);
            let current_b = working_b[y_idx][x_idx].clamp(0.0, 255.0);

            let (palette_r, palette_g, palette_b) = find_closest_color_weighted(
                current_r as u8,
                current_g as u8,
                current_b as u8,
                palette,
            );

            output.put_pixel(x, y, Rgb([palette_r, palette_g, palette_b]));

            let error_r = current_r - (palette_r as f32);
            let error_g = current_g - (palette_g as f32);
            let error_b = current_b - (palette_b as f32);

            distribute_jjn_error(
                &mut working_r,
                &mut working_g,
                &mut working_b,
                x,
                y,
                width,
                height,
                error_r,
                error_g,
                error_b,
            );
        }
    }

    Ok(output)
}

fn distribute_jjn_error(
    working_r: &mut [Vec<f32>],
    working_g: &mut [Vec<f32>],
    working_b: &mut [Vec<f32>],
    x: u32,
    y: u32,
    width: u32,
    height: u32,
    error_r: f32,
    error_g: f32,
    error_b: f32,
) {
    let x_idx = x as usize;
    let y_idx = y as usize;
    let divisor = 48.0;

    // Current row (y)
    if x + 1 < width {
        let factor = 7.0 / divisor;
        working_r[y_idx][x_idx + 1] += error_r * factor;
        working_g[y_idx][x_idx + 1] += error_g * factor;
        working_b[y_idx][x_idx + 1] += error_b * factor;
    }
    if x + 2 < width {
        let factor = 5.0 / divisor;
        working_r[y_idx][x_idx + 2] += error_r * factor;
        working_g[y_idx][x_idx + 2] += error_g * factor;
        working_b[y_idx][x_idx + 2] += error_b * factor;
    }

    // Next row (y+1)
    if y + 1 < height {
        if x >= 2 {
            let factor = 3.0 / divisor;
            working_r[y_idx + 1][x_idx - 2] += error_r * factor;
            working_g[y_idx + 1][x_idx - 2] += error_g * factor;
            working_b[y_idx + 1][x_idx - 2] += error_b * factor;
        }
        if x >= 1 {
            let factor = 5.0 / divisor;
            working_r[y_idx + 1][x_idx - 1] += error_r * factor;
            working_g[y_idx + 1][x_idx - 1] += error_g * factor;
            working_b[y_idx + 1][x_idx - 1] += error_b * factor;
        }
        let factor = 7.0 / divisor;
        working_r[y_idx + 1][x_idx] += error_r * factor;
        working_g[y_idx + 1][x_idx] += error_g * factor;
        working_b[y_idx + 1][x_idx] += error_b * factor;

        if x + 1 < width {
            let factor = 5.0 / divisor;
            working_r[y_idx + 1][x_idx + 1] += error_r * factor;
            working_g[y_idx + 1][x_idx + 1] += error_g * factor;
            working_b[y_idx + 1][x_idx + 1] += error_b * factor;
        }
        if x + 2 < width {
            let factor = 3.0 / divisor;
            working_r[y_idx + 1][x_idx + 2] += error_r * factor;
            working_g[y_idx + 1][x_idx + 2] += error_g * factor;
            working_b[y_idx + 1][x_idx + 2] += error_b * factor;
        }
    }

    // Row y+2
    if y + 2 < height {
        if x >= 2 {
            let factor = 1.0 / divisor;
            working_r[y_idx + 2][x_idx - 2] += error_r * factor;
            working_g[y_idx + 2][x_idx - 2] += error_g * factor;
            working_b[y_idx + 2][x_idx - 2] += error_b * factor;
        }
        if x >= 1 {
            let factor = 3.0 / divisor;
            working_r[y_idx + 2][x_idx - 1] += error_r * factor;
            working_g[y_idx + 2][x_idx - 1] += error_g * factor;
            working_b[y_idx + 2][x_idx - 1] += error_b * factor;
        }
        let factor = 5.0 / divisor;
        working_r[y_idx + 2][x_idx] += error_r * factor;
        working_g[y_idx + 2][x_idx] += error_g * factor;
        working_b[y_idx + 2][x_idx] += error_b * factor;

        if x + 1 < width {
            let factor = 3.0 / divisor;
            working_r[y_idx + 2][x_idx + 1] += error_r * factor;
            working_g[y_idx + 2][x_idx + 1] += error_g * factor;
            working_b[y_idx + 2][x_idx + 1] += error_b * factor;
        }
        if x + 2 < width {
            let factor = 1.0 / divisor;
            working_r[y_idx + 2][x_idx + 2] += error_r * factor;
            working_g[y_idx + 2][x_idx + 2] += error_g * factor;
            working_b[y_idx + 2][x_idx + 2] += error_b * factor;
        }
    }
}
```

### Step 3: Aggiornare `convert.rs` per usare i nuovi algoritmi

File: `rust/photoframe-processor/src/image_processing/convert.rs`

Modificare le funzioni `apply_6c_processing()` e `apply_7c_processing()`:

```rust
use crate::cli::DitherMethod;

fn apply_6c_processing(img: &RgbImage, dithering_method: &DitherMethod) -> Result<RgbImage> {
    match dithering_method {
        DitherMethod::FloydSteinberg => {
            super::convert_improved::apply_enhanced_floyd_steinberg_dithering(img, &SIX_COLOR_PALETTE)
        }
        DitherMethod::Atkinson => {
            super::convert_improved::apply_atkinson_dithering(img, &SIX_COLOR_PALETTE)
        }
        DitherMethod::Stucki => {
            super::convert_improved::apply_stucki_dithering(img, &SIX_COLOR_PALETTE)
        }
        DitherMethod::JarvisJudiceNinke => {
            super::convert_improved::apply_jarvis_judice_ninke_dithering(img, &SIX_COLOR_PALETTE)
        }
        DitherMethod::Ordered => {
            super::convert_improved::apply_ordered_dithering(img, &SIX_COLOR_PALETTE)
        }
    }
}
```

---

## Esempi d'Uso

```bash
# Floyd-Steinberg (default - best for general use)
./photoframe-processor -i ~/photos -o ~/outputs --dithering floyd-steinberg

# Atkinson (preserves brightness, good for bright photos)
./photoframe-processor -i ~/photos -o ~/outputs --dithering atkinson

# Stucki (reduces patterns, slower)
./photoframe-processor -i ~/photos -o ~/outputs --dithering stucki

# Jarvis-Judice-Ninke (best for detailed photos, slowest)
./photoframe-processor -i ~/photos -o ~/outputs --dithering jarvis

# Ordered/Bayer (best for text and line art, fastest)
./photoframe-processor -i ~/photos -o ~/outputs --dithering ordered
```

---

**Status**: Ready for implementation
**Estimated Time**: 2-3 hours
**Priority**: Medium (enhancement, not critical)
