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
./target/debug/processor -i /path/to/images -o /path/to/output
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
