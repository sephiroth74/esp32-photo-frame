# Photoframe Dithering Integration

## Struttura

Libreria Rust condivisa per il dithering:
- **Rust processor** → usa come dipendenza normale (rlib)
- **Flutter app** → usa via FFI (cdylib)

## Setup Completato

✅ Creata libreria `photoframe-dithering/` con:
- Core dithering algorithms (Floyd-Steinberg, Atkinson, Stucki, JJN, Ordered)
- API FFI per Flutter
- Perceptual color matching
- Gamma correction

✅ Flutter configurato con:
- Bindings FFI in `photoframe_dithering_ffi.dart`
- `DitheringProcessor` aggiornato per usare FFI
- Dipendenza `ffi: ^2.1.0`

✅ Build scripts:
- `scripts/build_rust_macos.sh` - per sviluppo macOS
- `scripts/build_rust_android.sh` - per Android release

✅ Processor Rust aggiornato per usare la libreria condivisa

## Prossimi Passi

1. **Compilare per macOS (testing):**
   ```bash
   cd flutter/mobile
   ./scripts/build_rust_macos.sh
   flutter pub get
   flutter run -d macos
   ```

2. **Testare che il dithering funzioni identicamente**

3. **Per Android:**
   ```bash
   export ANDROID_NDK_HOME=/path/to/ndk
   ./scripts/build_rust_android.sh
   flutter build apk
   ```

4. **Opzionalmente rimuovere** il vecchio codice Dart di dithering in `dithering_processor.dart` (ora è solo un wrapper FFI)

## Vantaggi

- ✅ Dithering **identico** tra Rust e Flutter (stesso codice)
- ✅ **Performance native** anche in Flutter
- ✅ **Un solo codice** da mantenere
- ✅ Facilità di debug (se c'è un problema, lo fissi in un posto solo)
