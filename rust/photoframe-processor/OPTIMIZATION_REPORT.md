# Optimization Report - Guida

## 🎯 Cos'è l'Optimization Report?

L'optimization report è una tabella riassuntiva che mostra i parametri utilizzati per ogni immagine processata quando si usa `--auto-optimize`.

## 📊 Come Usarlo

```bash
cd /Users/alessandro/Documents/git/sephiroth74/arduino/esp32-photo-frame/rust/photoframe-processor

# Processa immagini con auto-optimize e genera il report
./target/release/photoframe-processor \
  -i ~/photos \
  -o ~/outputs \
  --size 800x480 \
  --output-format bin \
  --auto-optimize \
  --optimization-report \
  --force
```

## 📋 Esempio di Output

Alla fine del processing, vedrai una tabella come questa:

```
╔══════════════════════════════════════════════════════════════════════════════════════════════════╗
║                              📊 OPTIMIZATION REPORT - LANDSCAPE IMAGES                            ║
╚══════════════════════════════════════════════════════════════════════════════════════════════════╝

┌────────────────────────────┬───────────────────┬─────────┬──────────┬────────────┬────────────┐
│ Image                      │ Dithering         │ Strength│ Contrast │ Auto-Color │ People     │
├────────────────────────────┼───────────────────┼─────────┼──────────┼────────────┼────────────┤
│ IMG_001.jpg                │ FloydSteinberg    │ 1.0     │ -0.15    │ ✓          │ 2 people   │
│ IMG_002.jpg                │ Atkinson          │ 0.8     │ 0.00     │ ✗          │ No people  │
│ sunset.jpg                 │ Stucki            │ 1.2     │ -0.20    │ ✓          │ No people  │
│ portrait.jpg               │ JJN               │ 1.0     │ -0.10    │ ✓          │ 1 person   │
└────────────────────────────┴───────────────────┴─────────┴──────────┴────────────┴────────────┘

╔══════════════════════════════════════════════════════════════════════════════════════════════════╗
║                              📊 OPTIMIZATION REPORT - PORTRAIT IMAGES                             ║
╚══════════════════════════════════════════════════════════════════════════════════════════════════╝

┌────────────────────────────┬───────────────────┬─────────┬──────────┬────────────┬────────────┐
│ Image                      │ Dithering         │ Strength│ Contrast │ Auto-Color │ People     │
├────────────────────────────┼───────────────────┼─────────┼──────────┼────────────┼────────────┤
│ portrait_01.jpg            │ FloydSteinberg    │ 1.0     │ -0.15    │ ✓          │ 1 person   │
│ portrait_02.jpg            │ Atkinson          │ 0.8     │ 0.00     │ ✓          │ 2 people   │
└────────────────────────────┴───────────────────┴─────────┴──────────┴────────────┴────────────┘

╔══════════════════════════════════════════════════════════════════════════════════════════════════╗
║                              📊 OPTIMIZATION REPORT - COMBINED PORTRAITS                          ║
╚══════════════════════════════════════════════════════════════════════════════════════════════════╝

┌────────────────────────────┬───────────────────┬─────────┬──────────┬────────────┬────────────┐
│ Combined Output            │ Left Dithering    │ Strength│ Contrast │ Auto-Color │ People     │
├────────────────────────────┼───────────────────┼─────────┼──────────┼────────────┼────────────┤
│ combined_bw_aaa_bbb.bin    │ FloydSteinberg    │ 1.0     │ -0.15    │ ✓          │ 1 person   │
│   Left: portrait_01.jpg    │                   │         │          │            │            │
│   Right: portrait_02.jpg   │ Atkinson          │ 0.8     │ 0.00     │ ✓          │ 1 person   │
└────────────────────────────┴───────────────────┴─────────┴──────────┴────────────┴────────────┘

Summary:
  Total images processed: 6
  Landscapes: 4 images
  Portraits: 2 images
  Combined portraits: 2 images (1 pair)
```

## 📊 Colonne della Tabella

| Colonna | Descrizione |
|---------|-------------|
| **Image** | Nome del file originale |
| **Dithering** | Metodo di dithering usato (FloydSteinberg, Atkinson, Stucki, JJN, Ordered) |
| **Strength** | Intensità del dithering (0.5-1.5, default 1.0) |
| **Contrast** | Aggiustamento contrasto (-0.5 a +0.3) |
| **Auto-Color** | Se la correzione colore automatica è stata applicata (✓/✗) |
| **People** | Numero di persone rilevate nell'immagine |

## 🎨 Interpretazione dei Parametri

### Dithering Methods

- **FloydSteinberg**: Generale, buon bilanciamento
- **Atkinson**: Preserva luminosità, ottimo per foto chiare
- **Stucki**: Riduce pattern, ottimo per paesaggi
- **JJN (Jarvis-Judice-Ninke)**: Ottimizzato per foto, alta qualità
- **Ordered**: Ottimizzato per testo/grafiche

### Contrast Adjustment

- **Negativo** (es. -0.15): Riduce contrasto → immagine più chiara
- **Zero** (0.00): Nessun aggiustamento
- **Positivo** (es. +0.10): Aumenta contrasto → immagine più scura

### Auto-Color

- **✓ (Sì)**: ImageMagick/custom color correction applicata
- **✗ (No)**: Colori originali preservati (es. toni pastello)

## 💡 Utilità del Report

1. **Debugging**: Capire quali parametri sono stati usati per ogni immagine
2. **Confronto**: Vedere come diverse immagini sono state ottimizzate
3. **Pattern Recognition**: Identificare tendenze nei parametri scelti
4. **Quality Control**: Verificare che l'auto-optimizer stia funzionando correttamente

## 📝 Note

- Il report viene mostrato **solo** se usi `--auto-optimize`
- Senza `--optimization-report`, l'auto-optimize funziona comunque ma non mostra la tabella finale
- Il report è organizzato in 3 sezioni: Landscape, Portrait, Combined Portraits
- Per combined portraits, vengono mostrati i parametri di entrambe le immagini left/right

---

**Versione**: v1.0
**Data**: 2026-01-02
