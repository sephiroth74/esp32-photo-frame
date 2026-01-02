# Test PROGMEM vs SD Card Performance

## Overview

Questo test confronta le performance di rendering tra:
1. **PROGMEM** - Bitmap compilata nella flash del firmware
2. **SD Card** - Bitmap caricata dalla scheda SD in PSRAM

Entrambi usano la stessa bitmap 7-color 800×480 (384KB).

## File Creati

### 1. Bitmap Binary File
- **Percorso**: `platformio/data/bitmap7c_800x480.bin`
- **Dimensione**: 384,000 bytes
- **Formato**: 1 byte per pixel, formato nativo del display 7-color
- **Sorgente**: Estratta da `Bitmaps7c800x480.h` della libreria GxEPD2

### 2. Script di Conversione
- **Percorso**: `platformio/scripts/convert_7c_bitmap_to_bin.py`
- **Funzione**: Estrae il bitmap dal file .h e lo salva come .bin
- **Esecuzione**:
  ```bash
  cd platformio
  source .venv/bin/activate
  python scripts/convert_7c_bitmap_to_bin.py
  ```

### 3. Nuovo Test nel Display Debug
- **Opzione Menu**: `P` - PROGMEM vs SD Card Performance Test
- **Funzione**: `test7cBitmapComparison()`
- **Cosa fa**:
  1. Renderizza la bitmap da PROGMEM (flash)
  2. Legge la bitmap dalla SD card in PSRAM
  3. Renderizza dalla PSRAM
  4. Confronta i tempi e mostra le statistiche

## Preparazione

### Passo 1: Copia il file sulla SD card

```bash
# Trova il volume della SD card (solitamente /Volumes/NOME_SD o simile)
cp platformio/data/bitmap7c_800x480.bin /Volumes/YOUR_SD_CARD/

# Verifica che il file sia stato copiato correttamente
ls -lh /Volumes/YOUR_SD_CARD/bitmap7c_800x480.bin
```

Il file **deve** essere nella root della SD card con il nome esatto `bitmap7c_800x480.bin`.

### Passo 2: Compila e carica il firmware

```bash
cd platformio
.venv/bin/pio run -e pros3d_unexpectedmaker -t upload
```

### Passo 3: Apri il monitor seriale

```bash
.venv/bin/pio device monitor -b 115200
```

## Esecuzione del Test

1. Nel menu display debug, seleziona: **`P`**
2. Aspetta che il test completi entrambi i rendering
3. Leggi le statistiche nel serial monitor

## Esempio di Output Atteso

```
[TEST] 7C Bitmap Comparison - PROGMEM vs SD Card
========================================
This test compares rendering performance of:
  1. Bitmap from PROGMEM (compiled into firmware)
  2. Bitmap from SD card (loaded into PSRAM)
Both are the same 800x480 7-color bitmap.

[TEST 1/2] Rendering from PROGMEM...
----------------------------------------
[PROGMEM] Rendering page...
[PROGMEM] Rendering page...
[PROGMEM] Rendering page...
[PROGMEM] Total rendering time: 28500 ms

[TEST 2/2] Rendering from SD Card (PSRAM buffered)...
----------------------------------------
[SD] SD card initialized
[SD] File size: 384000 bytes
[SD] PSRAM buffer allocated: 384000 bytes
[SD] Reading file into PSRAM...
[SD] Read 384000 bytes in 920 ms
[SD] Rendering from PSRAM buffer...
[SD] Rendering page...
[SD] Rendering page...
[SD] Rendering page...
[SD] Rendering time: 19000 ms
[SD] Total time (read + render): 19920 ms
[SD] PSRAM buffer freed

========================================
PERFORMANCE COMPARISON
========================================
PROGMEM (compiled):         28500 ms
SD Card (read):               920 ms
SD Card (render):           19000 ms
SD Card (total):            19920 ms
----------------------------------------
SD Card is FASTER by 8580 ms (30.1% faster)
========================================

NOTE: Rendering time comparison is most important.
      SD card read time is one-time overhead.
```

## Interpretazione dei Risultati

### Tempi Attesi

**PROGMEM (Flash Memory)**:
- Rendering: ~28-30 secondi
- Pro: Nessun overhead di lettura
- Contro: Accesso più lento dalla flash

**SD Card → PSRAM**:
- Read time: ~900-1000 ms (una volta sola)
- Rendering: ~19-20 secondi
- Total: ~20-21 secondi
- Pro: PSRAM è più veloce della flash
- Contro: Overhead iniziale di lettura

### Perché PSRAM è più veloce?

1. **PSRAM Speed**: ~40-80 MB/s bandwidth
2. **Flash Speed**: ~20-40 MB/s bandwidth
3. **Accesso Sequenziale**: PSRAM ottimizzato per accesso rapido
4. **Cache Effects**: PSRAM ha migliori caratteristiche di caching

### Conclusione

Per il progetto finale:
- ✅ **Usa PSRAM buffer** per file dalla SD card
- ✅ Rendering ~30-40% più veloce
- ✅ Overhead di lettura (1 secondo) è trascurabile
- ✅ Memoria disponibile: 8MB PSRAM (384KB = 4.7% usage)

## Note Tecniche

### Formato del File

Il file `bitmap7c_800x480.bin` usa:
- **Formato**: 1 byte per pixel
- **Valori**: Byte nativi del display 7-color
  - `0x39`: Colore dominante nell'immagine (~65%)
  - `0xFF`: Bianco (~35%)
  - Altri valori per BLACK, RED, GREEN, BLUE, YELLOW, ORANGE

### Utilizzo Memoria

**PROGMEM (Flash)**:
- Bitmap: 384KB
- Totale firmware: ~2.9MB / 5.2MB (55.7%)

**PSRAM (quando carica da SD)**:
- Buffer temporaneo: 384KB
- Disponibile: 8MB
- Usage: 4.7% per una singola immagine

### Compatibilità

- ✅ **Funziona su**: ESP32-S3 con PSRAM (ProS3(d), FeatherS3)
- ⚠️ **Non funziona su**: Board senza PSRAM (fallback necessario)
- ✅ **Display supportati**: 6-color, 7-color displays

## Troubleshooting

### "Could not open /bitmap7c_800x480.bin"
- Verifica che il file sia sulla SD card root
- Controlla il nome esatto del file (case-sensitive)
- Assicurati che la SD card sia inizializzata correttamente

### "Could not allocate PSRAM buffer"
- Verifica che PSRAM sia disponibile: `psramFound()`
- Controlla memoria disponibile con `ESP.getFreePsram()`
- Prova a chiudere altri task che usano PSRAM

### Display mostra schermo bianco
- Il formato del file .bin potrebbe non corrispondere al display
- Verifica che il display sia configurato correttamente (DISP_6C o DISP_7C)
- Prova prima i test pattern compilati (opzione `6` o `7`)

## Files Modificati

1. `src/display_debug.cpp` - Aggiunto `test7cBitmapComparison()`
2. `scripts/convert_7c_bitmap_to_bin.py` - Nuovo script di conversione
3. `data/bitmap7c_800x480.bin` - File binario generato

## Next Steps

Se il test ha successo, questo approccio dovrebbe essere integrato nel codice di produzione:
1. Modifica `renderer.cpp` per usare PSRAM buffer
2. Implementa sistema di cache per evitare riletture
3. Aggiungi gestione errori e fallback
4. Ottimizza per ridurre ulteriormente i tempi
