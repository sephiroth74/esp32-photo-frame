# Refactoring Main.cpp - Architettura Multi-Entry-Point

## Obiettivo Generale
Trasformare il `main.cpp` attualmente monolitico e condizionato con `#ifdef` in un'architettura modulare dove ogni **configurazione di build** ha il suo **entry point dedicato**, mantenendo il `main.cpp` come **router** minimalista.

---

## Problemi Attuali

### 1. **main.cpp Monolitico**
- File di 1747 linee con logica fortemente condizionata da macro preprocessore
- `#ifdef ENABLE_BT_IMAGE` divide il codice in due percorsi incompatibili
- `#ifdef ENABLE_DISPLAY_DIAGNOSTIC` e `#ifdef ENABLE_BT_DIAGNOSTIC` aggiungono ulteriore complessità
- Difficile da mantenere: ogni modifica richiede di testare tutti gli `#ifdef`

### 2. **Duplication e Logica Sparse**
- Funzioni comuni come `initialize_hardware()`, `setup_battery_and_power()` sono duplicazioni nascoste
- La logica di setup è sparsa tra le varie diramazioni condizionali
- Difficult tracciare quale codice verrà realmente compilato per quale configurazione

### 3. **Mancanza di Separation of Concerns**
- Tutto è in un unico file, rendendo difficile:
  - Testare singoli path
  - Aggiungere nuove configurazioni
  - Refactoring delle funzioni comuni

---

## Architettura Proposta

```
src/
├── main.cpp                    # ✨ NUOVO: Entry point minimalista (router)
├── default_main.cpp            # NUOVO: Normal mode (Google Drive + SD Card)
├── main_bt.cpp                 # NUOVO: Bluetooth image mode
├── display_diagnostic_main.cpp # NUOVO: Display diagnostic mode
├── bt_diagnostic_main.cpp      # NUOVO: Bluetooth diagnostic mode
├── common_main.h               # NUOVO: Header con dichiarazioni comuni
├── common_main.cpp             # NUOVO: Implementazioni comuni
└── [altri file esistenti]
```

### Struttura dei File

#### **1. `main.cpp` (Entry Point Router)**
```cpp
// main.cpp - Minimalista: ~50 linee
#if defined(ENABLE_BT_DIAGNOSTIC)
    #include "bt_diagnostic_main.h"
    // Extern setup/loop da bt_diagnostic_main.cpp
#elif defined(ENABLE_DISPLAY_DIAGNOSTIC)
    #include "display_diagnostic_main.h"
    // Extern setup/loop da display_diagnostic_main.cpp
#elif defined(ENABLE_BT_IMAGE)
    #include "bt_main.h"
    // Extern setup/loop da main_bt.cpp
#else
    #include "default_main.h"
    // Extern setup/loop da default_main.cpp
#endif

// Questi setup/loop vengono linkati in fase di compilazione
extern void setup();
extern void loop();
```

#### **2. `common_main.h/cpp` - Funzioni Comuni**
Contiene tutte le funzioni **condivise** tra i vari main:

**Funzioni comuni:**
- `initialize_hardware()` ✓
- `setup_battery_and_power()` ✓
- `setup_time_and_connectivity()` (parzialmente condivisa)
- `calculate_wakeup_delay()` ✓
- Strutture e costanti condivise
- Helper per display, battery, board utils

**Funzioni specifiche per mode:**
- `init_image_buffer()` → vai in common
- `init_display_hardware()` → vai in common
- `cleanup_image_buffer()` → vai in common
- Utilità di rendering → DisplayManager (già esiste)

#### **3. `default_main.cpp` - Normal Mode**
```cpp
// default_main.cpp - ~800 linee
#include "default_main.h"
#include "common_main.h"

photo_frame::GoogleDrive drive;
photo_frame::SdCard sdCard;
photo_frame::WifiManager wifiManager;
// ... globals specifiche per questo mode

void setup() {
    // Initialization stage 1-2
    // Phase 1: Buffer init
    // Phase 2: Hardware init
    // Google Drive operations
    // SD Card operations
    // Image rendering
    // Sleep
}

void loop() {
    delay(1000);
}
```

#### **4. `main_bt.cpp` - Bluetooth Mode**
```cpp
// main_bt.cpp - ~400 linee
#include "main_bt.h"
#include "main_common.h"

photo_frame::BluetoothImageManager bt_manager;
// ... globals specifiche

void setup() {
    // Initialization stage 1-2
    // Factory reset check
    // Battery check
    // Display init
    // BLE wait for image
    // Image rendering
    // Sleep
}

void loop() {
    delay(1000);
}
```

#### **5. `display_diagnostic_main.cpp`**
```cpp
// display_diagnostic_main.cpp - Piccolo
#include "display_diagnostic_main.h"
#include "common_main.h"

void setup() {
    // Diagnostic display setup
}

void loop() {
    // Diagnostic loop
}
```

#### **6. `bt_diagnostic_main.cpp`**
```cpp
// bt_diagnostic_main.cpp - Piccolo
#include "bt_diagnostic_main.h"
#include "common_main.h"

void setup() {
    // BT diagnostic setup
}

void loop() {
    // BT diagnostic loop
}
```

---

## Funzioni da Estrarre in `common_main.h/cpp`

### Dichiarate in `common_main.h`:
```cpp
// Hardware initialization
bool initialize_hardware();

// Power management
photo_frame::photo_frame_error_t setup_battery_and_power(
    photo_frame::battery_info_t& battery_info,
    esp_sleep_wakeup_cause_t wakeup_reason);

// Time & connectivity (versione base)
photo_frame::photo_frame_error_t setup_time_and_connectivity_base(
    const photo_frame::battery_info_t& battery_info,
    bool is_reset,
    DateTime& now);

// Display management
bool init_image_buffer();
bool init_display_hardware();
void cleanup_image_buffer();

// Sleep & wakeup
struct refresh_delay_t;
refresh_delay_t calculate_wakeup_delay(
    photo_frame::battery_info_t& battery_info,
    DateTime& now);

void finalize_and_enter_sleep(
    photo_frame::battery_info_t& battery_info,
    DateTime& now,
    esp_sleep_wakeup_cause_t wakeup_reason,
    const refresh_delay_t& refresh_delay);

// Rendering
photo_frame::photo_frame_error_t render_image(
    fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    photo_frame::GoogleDrive& drive,
    const photo_frame::battery_info_t& battery_info);

// Global objects (extern in .cpp, extern declaration in .h)
extern photo_frame::DisplayManager g_display;
extern bool portrait_mode;
extern unsigned long startupTime;
```

---

## Benefici del Refactoring

### 1. **Clarity & Maintainability**
- Ogni configurazione ha il suo file pulito e leggibile
- No `#ifdef` annidati confusi
- Facile capire quale codice si esegue in quale modalità

### 2. **Testability**
- Ogni main può essere testato indipendentemente
- Funzioni comuni concentrate in un posto

### 3. **Scalability**
- Aggiungere nuova configurazione: crea un nuovo `xxx_main.cpp`
- No impatto su altri file
- Struttura auto-documentante

### 4. **Build Time**
- Compilatore vede meno codice per configurazione
- Preprocessore ha meno `#ifdef` da elaborare
- Potenziale miglioramento tempi di build

### 5. **Git History & Conflicts**
- Meno conflitti: modifiche separate per ogni main
- Più facile cherry-pick di bugfix
- Blame/history più chiaro

---

## Piano d'Azione Dettagliato

### **Fase 1: Preparazione**
1. Creare la nuova struttura di directory (se necessaria)
2. Creare header files per dichiarazioni comuni
3. Creare file stub vuoti per tutti i nuovi main

### **Fase 2: Estrazione Comune**
1. Identificare tutte le funzioni condivise
2. Implementare in `common_main.cpp`
3. Dichiarare in `common_main.h`
4. Aggiungere extern declarations dove necessario

### **Fase 3: Implementazione dei Main**
1. **default_main.cpp**: Copiare dalla versione "normal" del main.cpp originale
2. **main_bt.cpp**: Copiare dalla sezione ENABLE_BT_IMAGE
3. **display_diagnostic_main.cpp**: Copiare da display_debug.h
4. **bt_diagnostic_main.cpp**: Copiare da bt_diagnostic.h

### **Fase 4: Ridisegno main.cpp**
1. Svuotare il file
2. Aggiungere solo le inclusioni condizionali
3. Esporre setup/loop come extern dal file appropriato

### **Fase 5: Testing**
1. Build per `pros3d_unexpectedmaker` (ENABLE_BT_IMAGE)
2. Build per `feathers3_unexpectedmaker` (ENABLE_DISPLAY_DIAGNOSTIC)
3. Verificare assenza errori di linking
4. Controllare comportamento runtime

### **Fase 6: Cleanup**
1. Rimuovere `#ifdef` dal main.cpp originale
2. Verificare che nessun altro file usi main.cpp direttamente
3. Aggiornare CMakeLists.json/platformio.ini se necessario

---

## Variabili Globali da Gestire

| Variabile        | Scope             | Note                                             |
| ---------------- | ----------------- | ------------------------------------------------ |
| `g_display`      | Globale           | Deve rimanere globale (usato da display_manager) |
| `portrait_mode`  | Globale           | Usato da display                                 |
| `startupTime`    | Globale           | Tracciamento tempo                               |
| `drive`          | default_main      | Locale a default_main.cpp                        |
| `sdCard`         | default_main + bt | Locale, duplicare se necessario                  |
| `wifiManager`    | default_main      | Locale                                           |
| `systemConfig`   | default_main      | Locale                                           |
| `battery_reader` | common            | Potrebbe essere in common                        |

---

## Macro Preprocessore da Tenere

Mantenerle nel `main.cpp` router per scegliere quale main includere:
- `ENABLE_BT_IMAGE`
- `ENABLE_DISPLAY_DIAGNOSTIC`
- `ENABLE_BT_DIAGNOSTIC`

Rimuoverle dai singoli main file dopo l'estrazione.

---

## Checklist di Completamento

- [ ] Creare `common_main.h` con dichiarazioni
- [ ] Implementare `common_main.cpp` con funzioni comuni
- [ ] Creare `default_main.h` e `default_main.cpp`
- [ ] Creare `main_bt.h` e `main_bt.cpp`
- [ ] Creare `display_diagnostic_main.h` e `.cpp`
- [ ] Creare `bt_diagnostic_main.h` e `.cpp`
- [ ] Ridisegnare `main.cpp` come router puro
- [ ] Compilare per `pros3d_unexpectedmaker`
- [ ] Compilare per `feathers3_unexpectedmaker`
- [ ] Testare runtime di tutte le configurazioni
- [ ] Verificare assenza di undefined references
- [ ] Cleanup e documentazione finale

---

## Note Importanti

### 1. **Extern Declarations**
- I main file espongono `void setup()` e `void loop()`
- Il Arduino framework li cerca automaticamente
- No namespace: rimangono a scope globale (Arduino requirement)

### 2. **Inclusioni**
- `common_main.h` deve essere lightweight (no mega-includes)
- Include solo ciò che serve
- Usare forward declarations dove possibile

### 3. **Compilazione Condizionale**
- Ogni `xxx_main.cpp` è condizionato dal relativo `#ifdef`
- Solo un main file sarà compilato per configurazione
- No code size bloat da includes non usati

### 4. **Global State**
- `g_display` rimane globale (già è così, non cambiare)
- Altri global state localizzato nel suo main file
- `common_main.cpp` ha solo funzioni pure (no state globale se possibile)

---

## Estimated Size Impact

| File                        | Linee Originali | Linee Stimate | Tipo                                       |
| --------------------------- | --------------- | ------------- | ------------------------------------------ |
| main.cpp                    | 1747            | ~50           | Diminuzione                                |
| default_main.cpp            | -               | ~800          | NUOVO                                      |
| main_bt.cpp                 | -               | ~400          | NUOVO                                      |
| common_main.cpp             | -               | ~700          | NUOVO (extract)                            |
| display_diagnostic_main.cpp | -               | ~100          | NUOVO (link)                               |
| bt_diagnostic_main.cpp      | -               | ~50           | NUOVO (link)                               |
| **TOTAL**                   | **1747**        | **~2100**     | Link-time optimization: stesso size finale |

Lo size finale di compilazione rimane uguale perché solo un main è compilato per volta.

---

## Dependencies & Includes

### Common Dependencies (tutte i main):
```cpp
#include <Arduino.h>
#include "esp32/spiram.h"
#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "display_manager.h"
#include "errors.h"
#include "io_utils.h"
#include "preferences_helper.h"
#include "renderer.h"
#include "rgb_status.h"
#include "littlefs_manager.h"
#include "sd_card.h"
#include "string_utils.h"
#include "unified_config.h"
#include "wifi_manager.h"
#include <assets/icons/icons.h>
```

### default_main specific:
```cpp
#include "google_drive.h"
#include "google_drive_client.h"
```

### bt_main specific:
```cpp
#ifdef ENABLE_BT_IMAGE
#include "bluetooth_image_manager.h"
#include "bt_protocol.h"
#include "bt_utils.h"
#endif
```

---

## Timeline Stimato

1. **Preparazione**: 30 min
2. **Common extraction**: 2 ore
3. **default_main**: 2 ore
4. **bt_main**: 1,5 ore
5. **Diagnostic mains**: 1 ora
6. **main.cpp router**: 30 min
7. **Testing & debug**: 2 ore
8. **Cleanup**: 30 min

**TOTALE**: ~10 ore

