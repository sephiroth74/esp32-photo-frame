# Hardware Testing Guide - Bluetooth Mode

Guida completa per testare il sistema Bluetooth Image Transfer su hardware reale.

## 📋 Prerequisiti

### Hardware
- ESP32 con e-paper display (800×480)
- SD card inserita e formattata
- Batteria carica (>20%)
- Pulsante connesso a GPIO1 (per wakeup e factory reset)
- Computer con Bluetooth LE

### Software
- Python 3.8 o superiore
- Librerie Python (vedi setup sotto)

## 🔧 Preparazione

### 0. Setup Python Tools

Prima di tutto, configura il virtualenv e installa le dipendenze Python:

```bash
# Naviga nella directory tools
cd platformio/tools

# Opzione 1: Setup automatico (raccomandato)
./setup.sh

# Opzione 2: Setup manuale
cd ..
python3 -m venv .venv
source .venv/bin/activate
pip install -r tools/requirements.txt
cd tools
```

Verifica l'installazione:
```bash
# Con virtualenv attivo
python bt_client.py --help

# Oppure senza attivare
../.venv/bin/python bt_client.py --help
```

### 1. Build e Upload Firmware

```bash
# Naviga nella directory platformio
cd platformio

# Abilita modalità Bluetooth in include/config.h
# Assicurati che sia definito:
# #define ENABLE_BT_IMAGE

# Build e upload
pio run -t upload

# Apri monitor seriale
pio device monitor -b 115200
```

### 2. Verifica Configurazione

Nel file `include/config/feathers3_unexpectedmaker.h` verifica:
- `WAKEUP_PIN` è GPIO1
- Display pins corretti per il tuo hardware
- SD card pins configurati

## 🧪 Test 1: Diagnostic Mode

Prima di testare la connessione BLE, esegui la suite diagnostica.

### Abilitare Diagnostic Mode

```cpp
// In include/config.h
#define ENABLE_BT_IMAGE
#define ENABLE_BT_DIAGNOSTIC
```

### Eseguire i Test

```bash
# Build e upload
pio run -t upload

# Monitor output
pio device monitor -b 115200
```

### Output Atteso

```
╔════════════════════════════════════════╗
║   BLUETOOTH MODE DIAGNOSTIC SUITE     ║
╚════════════════════════════════════════╝

TEST: BT Protocol Validation
✓ Valid config should pass
✓ Invalid magic should fail
✓ Invalid version should fail
...

========================================
TEST SUMMARY
========================================
Tests Passed: 50
Tests Failed: 0
Total Tests:  50
Success Rate: 100.0%
========================================
✓ ALL TESTS PASSED!
```

**Se i test falliscono**, controlla:
- SD card inserita e funzionante
- Display connesso correttamente
- PSRAM disponibile
- Battery reader funzionante

## 🧪 Test 2: BLE Connection Test

### Passo 1: Disabilita Diagnostic Mode

```cpp
// In include/config.h
#define ENABLE_BT_IMAGE
// #define ENABLE_BT_DIAGNOSTIC  // Commenta questa linea
```

### Passo 2: Upload Firmware

```bash
pio run -t upload
```

### Passo 3: Verifica First Boot

**Comportamento atteso al primo avvio:**

```
[BT] Entering Bluetooth mode setup
[BT] Wakeup reason: ESP_SLEEP_WAKEUP_UNDEFINED (0)
[BT] Is first boot: Yes
[BT] Checking for factory reset button...
[BT] No factory reset requested
[BT] Battery: XX.X%, XXXX mV
[BT] Starting image wait: First Boot (30 min)
[BT] First boot - showing waiting message
[BT] Display initialized and waiting message shown
[BT] BLE manager initialized, waiting for image (timeout: 1800000 ms)
```

**Display dovrebbe mostrare:**
```
┌────────────────────────────────┐
│                                │
│        Primo Avvio             │
│                                │
│  In attesa di una nuova        │
│  immagine...                   │
│                                │
│  Timeout: 30 minuti            │
│                                │
└────────────────────────────────┘
```

### Passo 4: Scan per Device BLE

Dal tuo computer:

```bash
cd platformio/tools
python bt_client.py scan
```

**Output atteso:**
```
Scanning for BLE devices...

Found X device(s):

1. PhotoFrame_XXXX
   Address: AA:BB:CC:DD:EE:FF
   RSSI: -45 dBm
   ⭐ Likely ESP32 Photo Frame!
```

**Nota:** Il nome esatto del device dipende dalla configurazione BLE nell'ESP32.

## 🧪 Test 3: Invio Test Pattern

Prima di inviare immagini reali, prova con un pattern di test.

```bash
python bt_client.py test --device AA:BB:CC:DD:EE:FF --rotation 0
```

**Output atteso:**
```
Connecting to AA:BB:CC:DD:EE:FF...
✓ Connected
✓ Subscribed to status notifications

Generating test pattern...
✓ Test pattern created: /tmp/test_pattern.bin

📷 Image: test_pattern.bin
   Size: 384,000 bytes
   Expected: 384,000 bytes
   Rotation: 0

📤 Step 1: Sending configuration...
   Config packet: 19 bytes
   Magic: 0xBEEF
   Version: 1
   Rotation: 0
   Dimensions: 800x480
   Image size: 384000
✓ Configuration sent

📤 Step 2: Sending image data...
   Progress: 13.0% (100/750 chunks) - 45.2 KB/s
   Progress: 26.1% (200/750 chunks) - 46.8 KB/s
   ...
   Progress: 100.0% (750/750 chunks) - 47.3 KB/s

✓ Image data sent in 8.1s
   Average speed: 47.3 KB/s

⏳ Waiting for ESP32 to process...

✓ Transfer complete!
✓ Disconnected
```

**Sul serial monitor ESP32:**
```
[BT] Config received: 19 bytes
[BT] ✓ Config validated successfully
[BT] Allocating image buffer: 384000 bytes
[BT] ✓ Buffer allocated
[BT] Image data chunk received: 512 bytes (chunk 1)
[BT] Image data chunk received: 512 bytes (chunk 2)
...
[BT] Image data chunk received: 384 bytes (chunk 750 - final)
[BT] ✓ Image transfer complete
[BT] Image received successfully, saving to SD card
[BT] ✓ Image saved to SD card successfully
[BT] Saved rotation to preferences: 0
[BT] ✓ SD card operations completed and closed
[BT] Initializing display...
[BT] Opening image file: /bt_images/last.bin
[BT] Loading image to buffer...
[BT] Applying rotation: 0
[BT] ✓ Image displayed successfully
[BT] Entering deep sleep (wake via GPIO1)
```

**Display dovrebbe mostrare** un gradiente orizzontale.

## 🧪 Test 4: Invio Immagine Reale

### Formato Immagini

Le immagini devono essere in formato binario raw:
- **Formato**: Raw binary, 1 byte per pixel
- **Dimensioni**: 800×480 pixels
- **Colorspace**: Grayscale (0-255)
- **Size**: Esattamente 384,000 bytes

### Preparare Immagine con ImageMagick

```bash
# Converti e ridimensiona
convert input.jpg -resize 800x480! -colorspace Gray -depth 8 output.gray

# Rinomina in .bin
mv output.gray output.bin
```

### Preparare Immagine con Python/PIL (script custom)

```python
from PIL import Image
import numpy as np

img = Image.open('input.jpg')
img = img.resize((800, 480), Image.LANCZOS)
img = img.convert('L')  # Grayscale
data = np.array(img)
data.tofile('output.bin')
```

### Inviare Immagine

```bash
python bt_client.py send output.bin --device AA:BB:CC:DD:EE:FF --rotation 0
```

**Note sulla rotazione:**
- `--rotation 0`: Landscape normale (800×480)
- `--rotation 1`: Portrait (480×800) - rotazione 90° oraria
- `--rotation 2`: Landscape invertito (800×480) - rotazione 180°
- `--rotation 3`: Portrait invertito (480×800) - rotazione 270° oraria

## 🧪 Test 5: Subsequent Boot

Dopo il primo invio riuscito, prova il comportamento di "subsequent boot".

### Passo 1: Risveglia il Device

Premi brevemente il pulsante GPIO1.

**Serial monitor:**
```
[BT] Wakeup reason: ESP_SLEEP_WAKEUP_EXT0 (2)
[BT] Is first boot: No
[BT] Checking for factory reset button...
[BT] Button not pressed, skipping factory reset check
[BT] Battery: XX.X%, XXXX mV
[BT] Starting image wait: Subsequent (5 min)
[BT] Subsequent boot - skipping display update (keeping last image)
[BT] BLE manager initialized, waiting for image (timeout: 300000 ms)
```

**Display:** Rimane invariato con l'ultima immagine mostrata.

### Passo 2: Invia Nuova Immagine

```bash
python bt_client.py send new_image.bin --device AA:BB:CC:DD:EE:FF
```

**Risultato:** Display si aggiorna con la nuova immagine.

### Passo 3: Test Timeout (Opzionale)

Risveglia il device e NON inviare nulla per 5 minuti.

**Comportamento atteso:**
- Dopo 5 minuti: timeout
- Display: **NON cambia** (rimane l'ultima immagine)
- Serial: `"Subsequent boot timeout - display left untouched"`
- Device va in deep sleep

## 🧪 Test 6: Factory Reset

### Passo 1: Trigger Reset

1. Risveglia o accendi il device
2. **Tieni premuto GPIO1 per 5 secondi** ininterrotti

**Serial monitor:**
```
[BT] Checking for factory reset button...
[BT] Button pressed, monitoring for 5000 ms long press...
[BT] Button held... 4000 ms remaining
[BT] Button held... 3000 ms remaining
[BT] Button held... 2000 ms remaining
[BT] Button held... 1000 ms remaining
[BT] Factory reset button held for 5000 ms - TRIGGERING RESET
[BT] ========================================
[BT] FACTORY RESET INITIATED
[BT] ========================================
[BT] Step 1: Clearing BT preferences...
[BT] ✓ All BT preferences cleared
[BT] Step 2: Deleting saved image file...
[BT] ✓ Deleted /bt_images/last.bin
[BT] Step 3: Showing confirmation message...
[BT] ✓ Confirmation displayed
[BT] Step 4: Restarting device...
```

**Display:**
```
┌────────────────────────────────┐
│                                │
│      Factory Reset             │
│                                │
│       Completato!              │
│                                │
│  Tutte le impostazioni sono    │
│     state ripristinate         │
│                                │
│       Riavvio...               │
│                                │
└────────────────────────────────┘
```

### Passo 2: Verifica Reset

Dopo il riavvio:
- Device torna a "first boot" state
- Timeout 30 minuti
- Rotazione resettata a 0
- Immagine salvata cancellata

## 🧪 Test 7: Gestione Errori

### Test 7.1: Immagine Dimensione Errata

```bash
# Crea file troppo piccolo
dd if=/dev/zero of=wrong_size.bin bs=1024 count=100

python bt_client.py send wrong_size.bin --device AA:BB:CC:DD:EE:FF
```

**Comportamento atteso:**
- Client mostra warning
- ESP32 rifiuta immagine
- Status notification con codice errore
- Display mostra messaggio d'errore

### Test 7.2: Batteria Bassa

Aspetta che la batteria scenda sotto il 20%.

**Comportamento atteso:**
- Display mostra warning batteria
- Continua operazione ma con LED ridotto
- Transfer potrebbe essere più lento

### Test 7.3: SD Card Mancante

Rimuovi la SD card e prova a inviare un'immagine.

**Comportamento atteso:**
- Transfer BLE completa normalmente
- Errore durante salvataggio su SD
- Display mostra errore SD card
- Device va in sleep

## 📊 Checklist Test Completa

- [ ] **Test 1: Diagnostics** - Tutti i test passano
- [ ] **Test 2: BLE Connection** - Device visibile e connettibile
- [ ] **Test 3: Test Pattern** - Pattern visualizzato correttamente
- [ ] **Test 4: Real Image** - Immagine reale visualizzata
- [ ] **Test 5: Subsequent Boot** - Display intatto al risveglio
- [ ] **Test 6: Factory Reset** - Reset funziona e device si resetta
- [ ] **Test 7.1: Wrong Size** - Errore gestito correttamente
- [ ] **Test 7.2: Low Battery** - Warning mostrato
- [ ] **Test 7.3: No SD Card** - Errore gestito correttamente
- [ ] **Rotation 0** - Landscape normale funziona
- [ ] **Rotation 1** - Portrait 90° funziona
- [ ] **Rotation 2** - Landscape 180° funziona
- [ ] **Rotation 3** - Portrait 270° funziona
- [ ] **First Boot Timeout** - Messaggio mostrato dopo 30 min
- [ ] **Subsequent Timeout** - Display intatto dopo 5 min
- [ ] **Multiple Transfers** - Invii multipli funzionano
- [ ] **Power Cycle** - Stato persistente dopo power off/on

## 🐛 Troubleshooting

### Device non trovato nello scan

**Possibili cause:**
- BLE non inizializzato (verifica serial monitor)
- Fuori range (avvicina device)
- Bluetooth disabled sul computer
- Device già connesso a altro client

**Soluzioni:**
- Riavvia ESP32
- Riavvia Bluetooth sul computer
- Verifica che ENABLE_BT_IMAGE sia definito

### Connection timeout

**Possibili cause:**
- Device in deep sleep
- Battery troppo bassa
- Interferenze BLE

**Soluzioni:**
- Risveglia device con GPIO1
- Carica batteria
- Allontana altri device BLE

### Transfer si interrompe

**Possibili cause:**
- Connessione BLE persa
- Buffer overflow su ESP32
- Battery critical

**Soluzioni:**
- Avvicina device
- Riduci CHUNK_SIZE nel client
- Carica batteria

### Display non si aggiorna

**Possibili cause:**
- Immagine non salvata su SD
- Display hardware issue
- Rotazione sbagliata

**Soluzioni:**
- Verifica serial monitor per errori
- Controlla connessioni display
- Prova rotation 0

### Factory reset non funziona

**Possibili cause:**
- Pulsante non premuto abbastanza
- GPIO1 non configurato correttamente
- Rilascio prematuro

**Soluzioni:**
- Tieni premuto per TUTTI i 5 secondi
- Verifica configurazione pin nel board config
- Guarda serial monitor per conferma

## 📈 Performance Attese

- **BLE Transfer Speed**: 40-50 KB/s
- **Total Transfer Time**: ~8-10 secondi per 384KB
- **SD Write Time**: 1-2 secondi
- **Display Update Time**: 2-5 secondi
- **First Boot Timeout**: 30 minuti
- **Subsequent Boot Timeout**: 5 minuti
- **Factory Reset Duration**: ~5 secondi

## ✅ Criteri di Successo

Il test hardware è considerato **PASSED** se:

1. ✅ Tutti i diagnostic tests passano
2. ✅ Device connettibile via BLE
3. ✅ Test pattern visualizzato correttamente
4. ✅ Immagini reali visualizzate correttamente
5. ✅ Tutte e 4 le rotazioni funzionano
6. ✅ Subsequent boot mantiene display intatto
7. ✅ Timeout gestiti correttamente (con/senza messaggio)
8. ✅ Factory reset funziona e resetta stato
9. ✅ Errori gestiti con messaggi appropriati
10. ✅ Stato persiste dopo power cycle

---

**Prossimi Passi:**
Una volta completati i test hardware con successo, si può procedere con:
1. Implementazione app Flutter per iOS/Android
2. Ottimizzazioni performance (se necessario)
3. Aggiunta features opzionali (multi-image queue, etc.)
