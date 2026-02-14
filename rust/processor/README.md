# PhotoFrame Processor

High‑performance CLI for converting images into formats compatible with ESP32 e‑paper displays. 

The main purpose is to prepare images for the ESP32 photoframe arduino board. It will convert images into the
required **pfr1** format, as well as other common formats like **bmp**, **jpg**, and **png**.

Supports parallel processing, automatic pairing, optional AI face detection, and detailed reports, and more...

**[ImageMagick](https://imagemagick.org/)** is required for some operations, like auto-colors or text annotations. It must be available in the env **$PATH** variable

## Key features

- Multiple outputs: bmp, jpg, png, pfr1
- Automatic portrait/landscape pairing
- Optional AI face detection (feature `ai`)
- Reports: `plain`, `full`, and `json`
- JSON progress for GUI integration

## Build

```
cd rust/processor
cargo build 
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

See the full documentation for the cli arguments:

```
./target/debug/processor --help
```
