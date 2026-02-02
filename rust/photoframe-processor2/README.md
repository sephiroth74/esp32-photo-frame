# photoframe-processor2

High‑performance CLI for converting images into formats compatible with ESP32 e‑paper displays. Supports parallel processing, automatic pairing, optional AI face detection, and detailed reports.

## Key features

- Multiple outputs: bmp, jpg, png, pfr1
- Automatic portrait/landscape pairing
- Optional AI face detection (feature `ai`)
- Reports: `plain`, `full`, and `json`
- JSON progress for GUI integration

## Build

```
cd rust/photoframe-processor2
cargo build --bin processor
```

## Run

```
./target/debug/processor /path/to/images -o /path/to/output
```

## JSON Progress

With `--json-progress`, output is JSON lines only. Progress messages include a `phase` field.

Phases: `startup`, `discovery`, `inspection`, `processing`, `saving`, `complete`.

Types: `progress`, `filecompleted`, `filefailed`, `error`, `complete`.

## Reports

- `--report plain` for text output
- `--report full` for detailed tables
- `--report json` for JSON reports

## Documentation

- CLI: [CLI_GUIDE.md](CLI_GUIDE.md)
- Flow analysis: [FLOW_ANALYSIS.md](FLOW_ANALYSIS.md)# photoframe-processor2

CLI ad alte prestazioni per convertire immagini in formati compatibili con ESP32 e display e‑paper. Supporta processing in parallelo, pairing automatico, AI face detection (opzionale) e report dettagliati.

## Caratteristiche principali

- Output multipli: bmp, jpg, png, pfr1
- Pairing automatico di immagini portrait/landscape
- Face detection (feature `ai`)
- Report in modalità `plain`, `full` e `json`
- JSON progress per integrazione GUI

## Build

```
cd rust/photoframe-processor2
cargo build --bin processor
```

## Esecuzione

```
./target/debug/processor /path/to/images -o /path/to/output
```

## JSON Progress

Con `--json-progress` l’output è composto solo da righe JSON. I progress includono anche `phase`.

Fasi: `startup`, `discovery`, `inspection`, `processing`, `saving`, `complete`.

Tipi: `progress`, `filecompleted`, `filefailed`, `error`, `complete`.

## Report

- `--report plain` per output testuale
- `--report full` per tabelle dettagliate
- `--report json` per report in JSON

## Documentazione

- CLI: [CLI_GUIDE.md](CLI_GUIDE.md)
- Flow analysis: [FLOW_ANALYSIS.md](FLOW_ANALYSIS.md)
