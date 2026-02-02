# Photoframe-Processor2 CLI Guide

## Binary: `processor`

This is the main CLI for image processing. The binary prints parameters in a readable, structured format.

## Build

```bash
cd rust/processor
cargo build
```

The compiled binary will be at: `target/debug/processor`.

## Basic usage

### Print parameters (no processing)

```bash
./target/debug/processor -i /path/to/images -o /path/to/output
```

### Full parameters

Run with `--help` to list all parameters:

```bash
./target/debug/processor --help
```

### JSON Progress (for GUI)

When using `--json-progress`, output is JSON lines only. Each message includes `type`, and progress messages include `phase`.

**Phases (`phase`)**:
- `startup`
- `discovery`
- `inspection`
- `processing`
- `saving`
- `complete`

**Message types (`type`)**:
- `progress`
- `filecompleted`
- `filefailed`
- `error`
- `complete`

## Examples

### Example 1: Simple BW conversion

```bash
./target/debug/processor -i /tmp/images -o /tmp/output \
  --color bw \
  --format pfr1 \
  --jobs 4
```

### Example 2: 6‑color with portrait pairing

```bash
./target/debug/processor -i /tmp/images -o /tmp/output \
  --color 6c \
  --target-orientation portrait \
  --format pfr1 jpg \
  --detect-people \
  --annotate \
  --auto-color-correct \
  --divider-width 5 \
  --divider-color CCCCCC \
  --jobs 8
```

### Example 3: With optimizations

```bash
./target/debug/processor -i /tmp/images -o /tmp/output \
  --color 6c \
  --format pfr1 \
  --auto-optimize \
  --brightness 10 \
  --contrast 15 \
  --saturation 110 \
  --dithering ordered \
  --verbose
```

### Example 4: Validate a PFR1 file

```bash
./target/debug/processor --validate /path/to/file.pfr1
```

### Example 5: JSON Progress

```bash
./target/debug/processor -i /tmp/images -o /tmp/output --json-progress --report json
```

## Main parameters

### I/O

- `-i PATH` - Input path (file or directory)
- `-o, --output DIR` - Output directory (required)

### Display

- `-c, --color [bw|6c]` - Color type (default: 6c)
- `--target-orientation [landscape|portrait]` - Target orientation (default: landscape)

### Output

- `--output-format [bmp|pfr1|jpg|png]` - Comma separated list of output formats (default: pfr1)

### Image adjustments

- `--brightness [-100..100]` - Brightness (default: 0)
- `--contrast [-100..100]` - Contrast (default: 0)
- `--saturation [0..200]` - Saturation (default: 100)
- `--auto-color-correct` - Automatic color correction
- `--auto-optimize` - Automatic optimization

### Dithering

- `--dithering [none|floyd_steinberg|ordered|error_diffusion]` - Dithering method (default: floyd_steinberg)
- `--dither-strength [0.0..1.0]` - Dither strength (default: 1.0)

### Portrait pairing

- `--divider-width PIXELS` - Divider width (default: 10)
- `--divider-color RRGGBB` - Divider color hex (default: 000000)

### AI features

- `--detect-people` - Enable people detection
- `--confidence-threshold [0.0..1.0]` - Confidence threshold (default: 0.5)

### Annotation

- `--annotate` - Enable date annotation
- `--font FONT` - Annotation font
  - Format: font name ("Arial"), file ("Arial.ttf"), or full path
  - Default: "Arial"
- `--font-size SIZE` - Font size in pixels (default: 22)
- `--annotation_background COLOR` - Background color (default: #40000000)
  - Hex with alpha (#AARRGGBB or #RRGGBB)

### Processing

- `-j, --jobs NUM` - Parallel jobs (0 = auto CPU cores)
- `--extensions EXT...` - Input extensions

### Config & mode

- `--config FILE` - Config file
- `-v, --verbose` - Verbose output
- `--debug` - Debug mode
- `--json-progress` - JSON progress (JSON output only)
- `--report [plain|full|json]` - Generate report
- `--validate FILE` - Validate a PFR1 file

## Output

The CLI prints structured sections. Paired files are shown on two rows, with the output only on the last row:

```
═══════════════════════════════════════════════════════════════
📋 PHOTOFRAME-PROCESSOR CLI PARAMETERS
═══════════════════════════════════════════════════════════════

📁 INPUT/OUTPUT:
  Input paths:        [...]
  Output directory:   [...]

🖼️  DISPLAY CONFIGURATION:
  Width:              800 px
  Height:             480 px
  Color type:         SixColor
  Target orientation: Landscape

💾 OUTPUT FORMATS:
  Formats:            [...]

⚙️  PROCESSING OPTIONS:
  Parallel jobs:      4 (0 = auto)
  File extensions:    [...]

🎨 IMAGE ADJUSTMENTS:
  Brightness:         +10
  Contrast:           -5
  Saturation:         1.2 x
  Auto color correct: false
  Auto optimize:      false

🔲 DITHERING:
  Method:             FloydSteinberg
  Strength:           1.0

👥 PORTRAIT PAIRING:
  Divider width:      10 px
  Divider color:      #000000

🤖 AI FEATURES:
  People detection:   true
  Confidence threshold: 0.50

✍️  ANNOTATION:
  Enabled:            true
  Font name:          Arial
  Font size:          22 px
  Background color:   #40000000

⚡ CONFIGURATION & MODE:
  Config file:        None
  Verbose:            false
  Debug mode:         false
  JSON progress:      false
  Generate report:    false
  Validate PFR1:      None

═══════════════════════════════════════════════════════════════

✅ PARAMETER VALIDATION:
  ✓ Display dimensions valid (800 x 480)
  ✓ Confidence threshold valid
  ✓ Dither strength valid
  ✓ Saturation valid
  ✓ Brightness valid
  ✓ Contrast valid
  ✓ Input paths specified
  ✓ Output directory specified

✨ All parameters valid! Ready for processing.
```

## Validation

The CLI validates parameters automatically:

- Width and height > 0
- Confidence threshold 0.0–1.0
- Dither strength 0.0–1.0
- Saturation >= 0.0
- Brightness -100 to 100
- Contrast -100 to 100
- Input path required
- Output directory required# Photoframe-Processor2 CLI Guide

## Binary: `processor`

Questo è il CLI principale per il processing delle immagini. Il binary stampa tutti i parametri ricevuti in modo leggibile e strutturato.

## Compilazione

```bash
cd rust/photoframe-processor2
cargo build --bin processor
```

Il binary compilato si troverà in: `target/debug/processor`

## Utilizzo Base

### Stampa parametri (senza processing)

```bash
./target/debug/processor /path/to/images -o /path/to/output
```

### Parametri Completi

Lanciare con `--help` per vedere tutti i parametri:

```bash
./target/debug/processor --help
```

### JSON Progress (per GUI)

Quando si usa `--json-progress`, l’output è composto solo da righe JSON. Ogni messaggio include il campo `type` e, per i progress, anche `phase`.

**Fasi (`phase`)**:
- `startup`
- `discovery`
- `inspection`
- `processing`
- `saving`
- `complete`

**Tipi di messaggi (`type`)**:
- `progress`
- `filecompleted`
- `filefailed`
- `error`
- `complete`

## Esempi

### Esempio 1: Conversione BW semplice

```bash
./target/debug/processor /tmp/images -o /tmp/output \
  --width 800 \
  --height 480 \
  --color bw \
  --format pfr1 \
  --jobs 4
```

### Esempio 2: 6-color con portrait pairing

```bash
./target/debug/processor /tmp/images -o /tmp/output \
  --width 1200 \
  --height 825 \
  --color 6c \
  --target-orientation portrait \
  --format pfr1 jpg \
  --detect-people \
  --annotate \
  --auto-color-correct \
  --divider-width 15 \
  --divider-color CCCCCC \
  --jobs 8
```

### Esempio 3: Con ottimizzazioni

```bash
./target/debug/processor /tmp/images -o /tmp/output \
  --width 800 \
  --height 480 \
  --color 6c \
  --format pfr1 \
  --auto-optimize \
  --brightness 10 \
  --contrast 5 \
  --saturation 1.1 \
  --dithering ordered \
  --verbose
```

### Esempio 4: Validazione file PFR1

```bash
./target/debug/processor --validate /path/to/file.pfr1
```

### Esempio 5: JSON Progress

```bash
./target/debug/processor /tmp/images -o /tmp/output --json-progress --report json
```

## Parametri Principali

### I/O

- `input PATH` - Path di input (file o directory)
- `-o, --output DIR` - Directory di output (obbligatorio)

### Display

- `-w, --width PIXELS` - Larghezza display (default: 800)
- `-h, --height PIXELS` - Altezza display (default: 480)
- `-c, --color [bw|6c]` - Tipo colore (default: 6c)
- `--target-orientation [landscape|portrait]` - Orientamento target (default: landscape)

### Output

- `--format [bmp|pfr1|jpg|png]` - Formati output (default: pfr1)

### Regolazioni Immagine

- `--brightness [-100..100]` - Luminosità (default: 0)
- `--contrast [-100..100]` - Contrasto (default: 0)
- `--saturation [0.0..2.0]` - Saturazione (default: 1.0)
- `--auto-color-correct` - Correzione automatica colore
- `--auto-optimize` - Ottimizzazione automatica

### Dithering

- `--dithering [none|floyd_steinberg|ordered|error_diffusion]` - Metodo dithering (default: floyd_steinberg)
- `--dither-strength [0.0..1.0]` - Forza dithering (default: 1.0)

### Portrait Pairing

- `--divider-width PIXELS` - Larghezza divisore (default: 10)
- `--divider-color RRGGBB` - Colore divisore hex (default: 000000)

### AI Features

- `--detect-people` - Abilita rilevamento persone
- `--confidence-threshold [0.0..1.0]` - Soglia confidenza (default: 0.5)

### Annotation

- `--annotate` - Abilita annotazione della data di creazione
- `--font FONT` - Specifica font per l'annotazione
  - Formato: nome font ("Arial"), file ("Arial.ttf"), oppure path completo
  - Default: "Arial"
- `--font-size SIZE` - Dimensione font in pixel (default: 22)
- `--annotation_background COLOR` - Colore background annotazione (default: #40000000)
  - Formato: esadecimale con alpha (#AARRGGBB o #RRGGBB)

### Processing

- `-j, --jobs NUM` - Job paralleli (0 = auto CPU cores)
- `--extensions EXT...` - Estensioni file da processare

### Config & Mode

- `--config FILE` - File di configurazione
- `-v, --verbose` - Output verboso
- `--debug` - Modalità debug
- `--json-progress` - Progresso in JSON (solo output JSON)
- `--report [plain|full|json]` - Genera report
- `--validate FILE` - Valida file PFR1

## Output

Il CLI stampa una tabella organizzata con sezioni. I file accoppiati sono mostrati in due righe, con l’output solo nell’ultima riga:

```
═══════════════════════════════════════════════════════════════
📋 PHOTOFRAME-PROCESSOR CLI PARAMETERS
═══════════════════════════════════════════════════════════════

📁 INPUT/OUTPUT:
  Input paths:        [...]
  Output directory:   [...]

🖼️  DISPLAY CONFIGURATION:
  Width:              800 px
  Height:             480 px
  Color type:         SixColor
  Target orientation: Landscape

💾 OUTPUT FORMATS:
  Formats:            [...]

⚙️  PROCESSING OPTIONS:
  Parallel jobs:      4 (0 = auto)
  File extensions:    [...]

🎨 IMAGE ADJUSTMENTS:
  Brightness:         +10
  Contrast:           -5
  Saturation:         1.2 x
  Auto color correct: false
  Auto optimize:      false

🔲 DITHERING:
  Method:             FloydSteinberg
  Strength:           1.0

👥 PORTRAIT PAIRING:
  Divider width:      10 px
  Divider color:      #000000

🤖 AI FEATURES:
  People detection:   true
  Confidence threshold: 0.50

✍️  ANNOTATION:
  Enabled:            true
  Font name:          Arial
  Font size:          22 px
  Background color:   #40000000

⚡ CONFIGURATION & MODE:
  Config file:        None
  Verbose:            false
  Debug mode:         false
  JSON progress:      false
  Generate report:    false
  Validate PFR1:      None

═══════════════════════════════════════════════════════════════

✅ PARAMETER VALIDATION:
  ✓ Display dimensions valid (800 x 480)
  ✓ Confidence threshold valid
  ✓ Dither strength valid
  ✓ Saturation valid
  ✓ Brightness valid
  ✓ Contrast valid
  ✓ Input paths specified
  ✓ Output directory specified

✨ All parameters valid! Ready for processing.
```

## Validazione

Il CLI valida automaticamente tutti i parametri:

- Width e height > 0
- Confidence threshold 0.0-1.0
- Dither strength 0.0-1.0
- Saturation >= 0.0
- Brightness -100 to 100
- Contrast -100 to 100
- Input path obbligatorio
- Output directory obbligatorio

Se qualche parametro non è valido, il programma esce con codice 1.

## Next Step

Dopo aver validato il CLI e i parametri, il prossimo step sarà implementare la logica di processing vera e propria seguendo l'architettura analizzata nel `FLOW_ANALYSIS.md`.
