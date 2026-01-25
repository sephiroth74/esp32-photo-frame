# Guida all'Implementazione - Photo Frame Processor Flutter

Questa guida fornisce i dettagli tecnici per completare l'implementazione dell'app Flutter usando correttamente l'API di `appkit_ui_elements` v0.2.1.

## Correzioni API Necessarie

### 1. AppKitButton

**API Corretta**:
```dart
AppKitButton(
  size: AppKitControlSize.small,  // NOT buttonSize: ButtonSize.small
  onTap: () { },                   // NOT onPressed: () { }
  child: const Text('Button'),
)
```

**Enums disponibili**:
- `AppKitControlSize.mini`
- `AppKitControlSize.small`
- `AppKitControlSize.regular`
- `AppKitControlSize.large`

### 2. AppKitGroupBox

**API Corretta** (già implementata correttamente):
```dart
AppKitGroupBox(
  title: 'Section Title',
  style: AppKitGroupBoxStyle.standardScrollBox,
  child: Column(children: [...]),
)
```

### 3. AppKitPopupButton

Controllare il file in `.pub-cache`:
```bash
cat ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/lib/src/controls/appkit_popup_button.dart
```

**Struttura tipica**:
```dart
AppKitPopupButton<T>(
  initialValue: value,
  items: [
    AppKitPopupMenuItem(value: T.value1, label: 'Label 1'),
    AppKitPopupMenuItem(value: T.value2, label: 'Label 2'),
  ],
  onValueChanged: (value) { },
)
```

### 4. AppKitCheckbox

**API da verificare**:
```dart
AppKitCheckbox(
  value: bool,
  onChanged: (value) { },
  label: 'Checkbox label',  // O usa child: Text('...')
)
```

### 5. AppKitSlider

**API da verificare**:
```dart
AppKitSlider(
  value: double,
  min: 0.0,
  max: 1.0,
  onChanged: (value) { },
  // Potrebbe non supportare divisions
)
```

### 6. AppKitProgressBar

**API da verificare**:
```dart
AppKitProgressBar(
  value: double,  // 0.0 - 1.0
  // Potrebbe non avere parametri aggiuntivi
)
```

## File da Correggere

### 1. lib/screens/home_screen.dart

**Linee con errori da correggere**:

#### AppKitButton (linee 78-92, 111-125)
```dart
// ERRATO:
AppKitButton(
  buttonSize: ButtonSize.small,  // ❌
  onPressed: () async { },       // ❌
  child: const Text('Browse...'),
)

// CORRETTO:
AppKitButton(
  size: AppKitControlSize.small,  // ✅
  onTap: () async { },            // ✅
  child: const Text('Browse...'),
)
```

#### AppKitGroupBox - Rimozione parametro 'title' se non supportato (linee 54, 137)
Se AppKitGroupBox non accetta `title` come parametro nominale, usare:
```dart
// Opzione 1: Usa AppKitLabel separato
Column(
  crossAxisAlignment: CrossAxisAlignment.start,
  children: [
    AppKitLabel(text: const Text('Section Title')),
    const SizedBox(height: 8),
    AppKitGroupBox(
      style: AppKitGroupBoxStyle.standardScrollBox,
      child: ...,
    ),
  ],
)

// Opzione 2: Controlla se esiste AppKitGroupContainer
AppKitGroupContainer(  // Se esiste questo widget
  title: 'Section Title',
  child: ...,
)
```

#### AppKitPopupButton (linee 153-177, 188-208)
```dart
// Verifica l'API corretta controllando il file della libreria
AppKitPopupButton<DisplayType>(
  initialValue: config.displayType,  // Potrebbe essere 'value' o 'initialValue'
  items: [
    AppKitPopupMenuItem(
      value: DisplayType.blackWhite,
      label: 'Black & White',  // Potrebbe essere 'child' invece di 'label'
    ),
  ],
  onValueChanged: (value) { },  // Potrebbe essere 'onChanged'
)
```

#### AppKitCheckbox (linee 241-248, 361-407)
```dart
// Verifica se usa 'label' o 'child'
AppKitCheckbox(
  value: config.autoOptimize,
  onChanged: (value) {
    provider.updateConfig(config.copyWith(autoOptimize: value));
  },
  label: const Text('Auto-optimize'),  // O child: const Text('...')
)
```

#### AppKitSlider (linee 286-310)
```dart
// Rimuovi 'divisions' se non supportato
AppKitSlider(
  value: config.ditherStrength,
  min: 0.5,
  max: 1.5,
  // divisions: 100,  // ❌ Rimuovi se non supportato
  onChanged: (value) {
    provider.updateConfig(config.copyWith(ditherStrength: value));
  },
)
```

#### AppKitButton Process Button (linee 410-422)
```dart
AppKitButton(
  size: AppKitControlSize.large,  // NOT buttonSize
  onTap: provider.isProcessing ? null : () {  // NOT onPressed
    provider.startProcessing();
  },
  child: Text(
    provider.isProcessing ? 'Processing...' : 'Process Images',
  ),
)
```

### 2. lib/providers/processing_provider.dart

**Rimuovere import inutilizzato**:
```dart
// Linea 5: Rimuovi questa linea
// import 'package:process_run/process_run.dart';  // ❌ Non usato
```

## Comandi per Verificare API

### 1. Controllare tutti i widget disponibili
```bash
cd ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/lib
find . -name "*.dart" | xargs grep -l "class AppKit" | head -20
```

### 2. Controllare API specifica di un widget
```bash
# AppKitPopupButton
cat ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/lib/src/controls/appkit_popup_button.dart | head -100

# AppKitCheckbox
cat ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/lib/src/controls/appkit_checkbox.dart | head -100

# AppKitSlider
cat ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/lib/src/controls/appkit_slider.dart | head -100

# AppKitProgressBar
cat ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/lib/src/controls/appkit_progress_bar.dart | head -100
```

### 3. Cercare esempi nella libreria
```bash
cd ~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1
find . -name "example*.dart" -o -name "*_example.dart"
```

### 4. Controllare il repository GitHub
```bash
# Clona il repository per vedere esempi
git clone https://github.com/sephiroth74/appkit_ui_elements.git /tmp/appkit_ui_elements
cd /tmp/appkit_ui_elements
find . -name "example" -type d
```

## Strategia di Correzione

### Passo 1: Correggere AppKitButton
1. Sostituisci `buttonSize:` con `size:`
2. Sostituisci `ButtonSize.` con `AppKitControlSize.`
3. Sostituisci `onPressed:` con `onTap:`

### Passo 2: Verificare e correggere AppKitGroupBox
1. Se `title:` non funziona, usa `AppKitLabel` separato
2. O cerca `AppKitGroupContainer` come alternativa

### Passo 3: Correggere AppKitPopupButton
1. Leggi il file sorgente per capire i parametri corretti
2. Verifica se usa `label` o `child` per gli items
3. Verifica se usa `onChanged` o `onValueChanged`

### Passo 4: Correggere AppKitCheckbox
1. Verifica se usa `label` o `child`
2. Verifica se `onChanged` è corretto

### Passo 5: Correggere AppKitSlider
1. Rimuovi `divisions` se non supportato
2. Aggiungi label manuale sopra lo slider se necessario

### Passo 6: Testare
```bash
flutter analyze
flutter run -d macos
```

## Alternative se AppKit Components Mancano

Se alcuni componenti non sono disponibili in `appkit_ui_elements`, considera:

### 1. Usare macos_ui come fallback
```yaml
# In pubspec.yaml
dependencies:
  macos_ui: ^2.0.0
```

### 2. Creare wrapper custom
```dart
class CustomPopupButton<T> extends StatelessWidget {
  // Wrapper che usa Material DropdownButton con stile macOS
}
```

### 3. Contribuire a appkit_ui_elements
Se mancano componenti essenziali, considera di contribuire al repository.

## Testing Checklist

Dopo le correzioni, verifica:

- [ ] `flutter analyze` non mostra errori
- [ ] `flutter run -d macos` compila senza errori
- [ ] File picker funziona
- [ ] Config viene salvata/caricata
- [ ] Progress bar si aggiorna
- [ ] Light/dark mode funziona
- [ ] Tutti i controlli sono interattivi
- [ ] Il binary Rust viene trovato ed eseguito

## Risorse

- **Repository**: https://github.com/sephiroth74/appkit_ui_elements
- **Pub.dev**: https://pub.dev/packages/appkit_ui_elements
- **Issues**: https://github.com/sephiroth74/appkit_ui_elements/issues
- **Local Cache**: `~/.pub-cache/hosted/pub.dev/appkit_ui_elements-0.2.1/`

## Note Finali

L'implementazione corrente fornisce una solida base. Le correzioni principali riguardano:
1. Parametri dei widget (size vs buttonSize, onTap vs onPressed)
2. Enums corretti (AppKitControlSize vs ButtonSize)
3. Possibili componenti mancanti da sostituire

Una volta corrette le API, l'app dovrebbe compilare e funzionare correttamente come GUI nativa macOS per il processor Rust.
