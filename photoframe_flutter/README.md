# Photo Frame Processor - Flutter macOS

GUI nativa macOS per il processore di immagini ESP32 Photo Frame, costruita con Flutter e [appkit_ui_elements](https://github.com/sephiroth74/appkit_ui_elements).

## ✨ Caratteristiche

- **Interfaccia nativa macOS**: Usa AppKit UI Elements per un'esperienza utente nativa
- **Theme System**: Supporto completo per light/dark mode automatico
- **Progress Real-time**: Monitoraggio del progresso tramite JSON output
- **Persistenza Configurazione**: Salvataggio automatico delle impostazioni
- **Integrazione con Rust**: Esecuzione diretta del binario `photoframe-processor`

## 📋 Requisiti

- Flutter SDK 3.10.3 o superiore
- macOS 11.0 (Big Sur) o superiore
- Xcode 13 o superiore
- Rust photoframe-processor compilato (v1.1.0+)

## 🚀 Installazione

```bash
# Clona il repository (se non l'hai già fatto)
cd /path/to/esp32-photo-frame

# Installa le dipendenze Flutter
cd photoframe_flutter
flutter pub get

# Genera il codice JSON serialization
flutter pub run build_runner build
```

## 🛠️ Compilazione

### Development
```bash
flutter run -d macos
```

### Release
```bash
flutter build macos --release
```

Il binario compilato sarà disponibile in:
```
build/macos/Build/Products/Release/photoframe_flutter.app
```

## 📦 Struttura del Progetto

```
photoframe_flutter/
├── lib/
│   ├── main.dart                      # Entry point con AppKitApp
│   ├── models/
│   │   └── processing_config.dart     # Model della configurazione
│   ├── providers/
│   │   └── processing_provider.dart   # State management con Provider
│   └── screens/
│       └── home_screen.dart           # Schermata principale
├── pubspec.yaml                       # Dipendenze Flutter
└── README.md                          # Questo file
```

## 🎨 Interfaccia Utente

L'interfaccia usa i seguenti componenti AppKit:

- **AppKitScaffold**: Struttura principale con toolbar
- **AppKitGroupContainer**: Gruppi di controlli con titolo
- **AppKitPushButton**: Pulsanti nativi macOS
- **AppKitPopupButton**: Menu a tendina
- **AppKitCheckbox**: Checkbox nativi
- **AppKitSlider**: Slider nativi
- **AppKitProgressBar**: Barra di progresso

### Sezioni dell'interfaccia

1. **File Selection**
   - Input e Output directory con file picker nativo

2. **Display Settings**
   - Display Type (Black & White, 6-Color, 7-Color)
   - Target Orientation (Landscape, Portrait)

3. **Dithering Settings**
   - Auto-optimize checkbox
   - Dither method popup
   - Strength e Contrast sliders (quando auto-optimize è off)

4. **Output Formats**
   - Checkbox multiple per BMP, BIN, JPEG, PNG

5. **Process Button & Progress**
   - Pulsante grande "Process Images"
   - Progress bar e conteggio durante il processing

6. **Progress Section**
   - File corrente in elaborazione
   - Messaggi di completamento/errore

## 🔧 Configurazione

### Persistenza Configurazione

La configurazione viene salvata automaticamente in:
```
~/Library/Application Support/it.sephiroth.photoframeFlutter/config.json
```

Include tutti i parametri:
- Percorsi input/output
- Display type e orientation
- Dithering settings
- Output formats
- Advanced options

### Integrazione Rust Binary

Il provider cerca il binario `photoframe-processor` in queste posizioni:
1. `../rust/photoframe-processor/target/release/photoframe-processor` (relativo)
2. Path assoluto nel progetto
3. PATH di sistema

Per configurare un path personalizzato, modifica `_findProcessorBinary()` in `processing_provider.dart`.

## 📝 Dipendenze Principali

```yaml
dependencies:
  appkit_ui_elements: ^0.2.1      # Componenti UI nativi macOS
  provider: ^6.1.2                # State management
  file_picker: ^8.1.6             # File/directory picker nativo
  path_provider: ^2.1.5           # Percorsi di sistema
  json_annotation: ^4.9.0         # Serializzazione JSON
  process_run: ^1.2.1             # Esecuzione processi
```

## 🚧 Funzionalità Future

- [ ] Sezione Processing Options completa (AI detection)
- [ ] Sezione Annotation Settings
- [ ] Sezione Divider Settings
- [ ] Advanced Options (force, dry-run, debug, etc.)
- [ ] Visualizzazione log real-time dal processo
- [ ] Anteprima immagini prima/dopo
- [ ] Presets configurabili
- [ ] Export/import configurazioni
- [ ] Notifiche sistema al completamento

## 🐛 Troubleshooting

### Build fallisce
Assicurati di aver eseguito il code generation:
```bash
flutter pub run build_runner build --delete-conflicting-outputs
```

### Binary non trovato
Verifica che il binario Rust sia compilato:
```bash
cd ../rust/photoframe-processor
cargo build --release
```

### AppKit UI non appare correttamente
Verifica che la versione di appkit_ui_elements sia corretta:
```bash
flutter pub outdated
flutter pub upgrade appkit_ui_elements
```

## 📚 Riferimenti

- [Flutter Documentation](https://docs.flutter.dev/)
- [appkit_ui_elements](https://github.com/sephiroth74/appkit_ui_elements)
- [Provider Package](https://pub.dev/packages/provider)
- [ESP32 Photo Frame Processor](https://github.com/sephiroth74/arduino/tree/main/esp32-photo-frame)

## 📄 Licenza

Questo progetto è parte del progetto ESP32 Photo Frame ed è rilasciato sotto licenza MIT.

## 👤 Autore

Alessandro Crugnola

## 🤝 Contributi

Contributi, issues e feature requests sono benvenuti!

---

**Nota**: Questa è una versione iniziale dell'app Flutter. Molte funzionalità della GUI Rust (come AI detection, annotation settings, etc.) devono ancora essere implementate.
