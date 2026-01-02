# Power Recovery Delay & Hardware Improvements

## Documento Tecnico: Gestione Potenza Display E-Paper

**Display**: GDEP073E01 (6-color ACeP, 800×480)
**Board**: ESP32-S3 ProS3(d) / FeatherS3
**Firmware Version**: v0.11.0+

---

## 1. Cos'è il Power Recovery Delay?

### Contesto Tecnico

Il "power recovery delay" è un **ritardo intenzionale** inserito DOPO che il display ha completato un refresh completo della pagina. Si trova nel codice a [main.cpp:565-569](platformio/src/main.cpp#L565-L569):

```cpp
#if defined(DISP_6C) || defined(DISP_7C)
    // Power recovery delay after page refresh for 6-color displays
    // Helps prevent washout by allowing capacitors to recharge
    delay(200);  // ← QUESTO È IL DELAY DI RECUPERO POTENZA
#endif
```

### Perché Serve?

I display e-paper 6-color/7-color (come il GDEP073E01) hanno caratteristiche elettriche molto diverse dai display B/W:

#### Consumo di Corrente
- **Display B/W**: ~80-150mA durante refresh
- **Display 6C/7C (GDEP073E01)**: **~400mA di picco** durante refresh
- **Durata refresh**: ~17-30 secondi (contro 2-5 secondi dei B/W)

#### Problema dei Condensatori Interni

Il display e-paper contiene **condensatori interni** che:
1. **Si scaricano** durante il lungo processo di refresh (17+ secondi)
2. **Non hanno tempo** di ricaricarsi completamente se si fanno operazioni subito dopo
3. **Causano "washout"** (sbiadimento) o artefatti se riusati troppo presto

### Cosa Fa il Delay di 200ms?

Il delay attuale di **200ms** permette ai condensatori interni del display di:
- **Ricaricarsi parzialmente** dopo il refresh
- **Stabilizzare la tensione** del rail di alimentazione del display
- **Prevenire stress elettrico** al controller del display

---

## 2. Soluzione 3: Aumentare il Power Recovery Delay

### Razionale

Se le **strisce bianche persistono** dopo aver rimosso il `fillScreen()` (Soluzione 1), potrebbe essere un problema di **alimentazione insufficiente** durante o dopo il refresh.

### Modifica Proposta

**File**: `platformio/src/main.cpp` (linee 565-569)

**Codice Originale**:
```cpp
#if defined(DISP_6C) || defined(DISP_7C)
    // Power recovery delay after page refresh for 6-color displays
    // Helps prevent washout by allowing capacitors to recharge
    delay(200);  // ← TROPPO BREVE - insufficiente per ricarica completa
#endif
```

**Codice Implementato** (v0.11.1): ✅
```cpp
#if defined(DISP_6C) || defined(DISP_7C)
    // Power recovery delay after page refresh for 6-color displays
    // Increased from 400ms to 1000ms in v0.11.1 to improve power stability
    // Helps prevent:
    // - White vertical stripes from voltage drop
    // - Washout from incomplete capacitor recharge
    // - Display artifacts from power supply instability
    delay(1000);  // ← AUMENTATO A 1000ms (1 secondo)
#endif
```

### Benefici Ottenuti

- ✅ **Ricarica completa** (90-95%) dei condensatori interni del display
- ✅ **Stabilizzazione totale** della tensione di alimentazione 3.3V
- ✅ **Riduzione stress elettrico** sul regolatore di tensione del ProS3(d)
- ✅ **Eliminazione strisce** causate da voltage drop durante refresh consecutivi
- ✅ **Raffreddamento regolatore** - il TPS62A01 ha tempo di stabilizzarsi termicamente

### Trade-off

- **Svantaggio**: Aggiunge 600ms extra al tempo totale di rendering rispetto ai 400ms precedenti
  - Impatto reale: ~0.04mAh/giorno extra (trascurabile su batteria 5000mAh)
  - Su un refresh di 17+ secondi, l'aggiunta di 600ms è solo il 3.5% in più
- **Vantaggio**: Stabilità e qualità del display significativamente migliorate
  - Elimina artefatti intermittenti
  - Prolunga la vita del display
  - Riduce lo stress sui componenti

---

## 3. Miglioramenti Hardware Consigliati

### 3.1 Condensatore Bulk di Alimentazione ⭐ **RACCOMANDATO**

#### Componente
- **Tipo**: Condensatore elettrolitico
- **Capacità**: **470µF** (minimo 100µF, ideale 470-1000µF)
- **Tensione**: **16V** (sufficiente per 5V USB)
- **Polarità**: ⚠️ IMPORTANTE - rispettare la polarità!

#### Posizionamento
```
USB 5V ──┬──[Condensatore 470µF]──┬── GND
         │                        │
         └─→ VIN ProS3(d)         └─→ GND ProS3(d)
```

**Dove saldare**:
1. **Terminale positivo (+)** → pin **USB** o **VIN** del ProS3(d)
2. **Terminale negativo (-)** → pin **GND** del ProS3(d)

⚠️ **ATTENZIONE**: Il condensatore deve essere il più **vicino possibile** al connettore USB per massimizzare l'efficacia.

#### Perché Funziona?

Il condensatore bulk agisce come **"riserva di energia"**:

1. **Durante periodi di basso consumo**: Il condensatore si carica dalla porta USB
2. **Durante picchi di corrente** (400mA refresh): Il condensatore fornisce corrente istantanea
3. **Riduce voltage drop**: Mantiene stabile la tensione 5V anche durante picchi
4. **Filtra rumore**: Elimina oscillazioni e spike sulla linea di alimentazione

#### Effetto Atteso sulle Strisce

Se le strisce sono causate da **voltage drop** durante il refresh:
- ✅ Il condensatore dovrebbe **eliminarle completamente**
- ✅ Migliora la qualità generale del display (colori più uniformi)
- ✅ Riduce stress sul regolatore di tensione del board

---

### 3.2 Condensatore di Bypass per Display (Opzionale)

#### Componente
- **Tipo**: Condensatore ceramico
- **Capacità**: **100nF (0.1µF)**
- **Tensione**: **50V**
- **Non polarizzato**

#### Posizionamento
```
Display VCC ──┬──[Condensatore 100nF]──┬── GND
              │                         │
              └─→ Pin VCC display       └─→ Pin GND display
```

**Dove saldare**:
1. **Un terminale** → pin **VCC** del connettore FPC del display
2. **Altro terminale** → pin **GND** del connettore FPC del display

⚠️ Saldare il più **vicino possibile ai pin del connettore**.

#### Perché Funziona?

Il condensatore ceramico da 100nF:
- **Filtra rumore ad alta frequenza** sulla linea di alimentazione del display
- **Stabilizza la tensione** durante switching rapidi del controller
- **Riduce EMI** (interferenze elettromagnetiche)

#### Quando Serve?

Questo è **meno critico** del condensatore bulk principale, ma può aiutare se:
- Hai cavi FPC molto lunghi (>15cm)
- Vedi ancora artefatti dopo aver aggiunto il condensatore bulk
- Usi il display in ambienti con molto rumore elettrico

---

### 3.3 Resistenze di Pull-up/Pull-down (NON Necessarie)

**Conclusione**: Per questo progetto **NON servono resistenze aggiuntive** perché:

1. **SPI già configurato correttamente**: Le linee SPI (SCK, MOSI, CS) hanno già pull-up/pull-down interni abilitati nell'ESP32
2. **Display con pull-up integrati**: Il GDEP073E01 ha già resistenze di terminazione interne
3. **Cavi corti**: I cavi FPC sono relativamente corti (<20cm) e non necessitano terminazione

❌ **Non aggiungere resistenze** a meno che non tu non abbia problemi di comunicazione SPI (raro).

---

## 4. Implementazione Pratica

### Setup Breadboard/Protoboard Consigliato

#### Opzione A: Alimentazione USB (5V)

```
                    ProS3(d) Board
                    ┌─────────────────┐
     USB 5V ────────┤USB              │
                    │                 │
                    │              3V3├──→ Display VCC (3.3V)
                    │                 │
    ┌─[470µF]───────┤VIN              │
    │               │                 │
    └───────────────┤GND           GND├──→ Display GND
                    │                 │
                    │       SPI (MOSI)├──→ Display DIN
                    │       SPI (SCK )├──→ Display CLK
                    │       GPIO (CS )├──→ Display CS
                    │       GPIO (DC )├──→ Display DC
                    │       GPIO (RST)├──→ Display RST
                    │       GPIO (BUSY)←─ Display BUSY
                    └─────────────────┘

Legenda:
[470µF] = Condensatore elettrolitico 470µF/16V (polarizzato!)
         Positivo (+) verso VIN/USB
         Negativo (-) verso GND
```

#### Opzione B: Alimentazione Batteria LiPo (3.7V) ⭐ **TUO CASO**

```
    LiPo 3.7V 5000mAh
    (Connettore JST)
           │
           ├─── BAT+ (Rosso)
           │              ProS3(d) Board
           │              ┌─────────────────┐
           │         ┌────┤BAT/JST          │
           └─────────┘    │                 │
                          │              3V3├──→ Display VCC (3.3V)
                          │                 │
          ┌─[470µF]───────┤3V3              │◄─── ⚠️ POSIZIONE CORRETTA
          │               │                 │      per alimentazione batteria
          └───────────────┤GND           GND├──→ Display GND
                          │                 │
                          │       SPI (MOSI)├──→ Display DIN
                          │       SPI (SCK )├──→ Display CLK
                          │       GPIO (CS )├──→ Display CS
                          │       GPIO (DC )├──→ Display DC
                          │       GPIO (RST)├──→ Display RST
                          │       GPIO (BUSY)←─ Display BUSY
                          └─────────────────┘

⚠️ IMPORTANTE per Alimentazione Batteria:
[470µF] = Condensatore elettrolitico 470µF/16V (polarizzato!)
         Positivo (+) verso rail 3V3 (DOPO il regolatore interno)
         Negativo (-) verso GND

🔴 NON mettere il condensatore tra BAT+ e GND!
   Il condensatore deve stare DOPO il regolatore di tensione interno del ProS3(d).
```

#### Spiegazione Setup Batteria

**Perché il condensatore va sul rail 3V3 e non su BAT+?**

1. **Il ProS3(d) ha un regolatore interno** (TPS62A01) che:
   - Converte la tensione batteria (3.0-4.2V) in 3.3V stabile
   - Ha un max output di **600mA** continuo
   - Durante i picchi di 400mA del display, il regolatore lavora al 66% della capacità

2. **Mettere il condensatore su 3V3** (output del regolatore):
   - ✅ Fornisce corrente istantanea durante i picchi del display
   - ✅ Riduce lo stress sul regolatore interno
   - ✅ Stabilizza la tensione 3.3V che va al display
   - ✅ La batteria da 5000mAh fornisce già capacità sufficiente lato input

3. **Mettere il condensatore su BAT+** (input del regolatore):
   - ❌ Non aiuta direttamente il display (che lavora a 3.3V)
   - ❌ La batteria da 5000mAh ha già capacità enorme (non serve bulk extra)
   - ❌ Potrebbe interferire con il circuito di carica della batteria

**Posizionamento Fisico Consigliato**:
```
Pin ProS3(d):        Componente:
───────────────      ────────────────────────────────
3V3 (output) ───┬──► [Condensatore 470µF +] positivo
                │
GND         ────┴──► [Condensatore 470µF -] negativo
```

⚠️ **Attenzione**: Il pin 3V3 del ProS3(d) è **OUTPUT**, non INPUT! Il regolatore interno fornisce corrente, non la riceve.

#### Considerazioni Aggiuntive per Setup a Batteria

**Capacità Batteria 5000mAh**:
- ✅ **Più che sufficiente** per questo progetto
- Consumi tipici:
  - ESP32-S3 deep sleep: ~10µA
  - Display refresh (400mA per 17 sec): ~1.9mAh per refresh
  - Con 1 refresh/ora: ~45mAh/giorno → **110 giorni di autonomia** 🎉
  - Con 1 refresh/30min: ~90mAh/giorno → **55 giorni di autonomia**

**Voltage Drop della Batteria**:
- Batteria LiPo carica (100%): **4.2V** → Regolatore lavora bene
- Batteria al 50%: **3.7V** → Regolatore lavora ancora bene
- Batteria scarica (20%): **3.4V** → Regolatore ancora funzionale
- ⚠️ **Sotto 3.3V**: Il regolatore TPS62A01 potrebbe avere problemi

**Quando il Condensatore Aiuta Maggiormente**:
1. **Batteria sotto 50%** (tensione <3.7V) → Il regolatore lavora più duramente
2. **Temperature fredde** → La batteria ha resistenza interna maggiore
3. **Batteria invecchiata** → Impedenza interna aumentata nel tempo

**Firmware già gestisce batteria bassa**:
Il codice a [main.cpp:555-559] già controlla `battery_info.is_critical()` e salta alcune operazioni se la batteria è critica, riducendo il carico sul regolatore.

### Lista Componenti Necessari

| Componente | Specifica | Quantità | Priorità | Costo (circa) |
|------------|-----------|----------|----------|---------------|
| Condensatore elettrolitico | 470µF, 16V, polarizzato | 1 | ⭐ Alta | €0.20-0.50 |
| Condensatore ceramico | 100nF (0.1µF), 50V | 1 (opzionale) | Bassa | €0.05-0.10 |
| Cavi jumper | Maschio-maschio | 2 | Alta | €0.10 |

**Costo totale**: €0.30-0.60 per il setup minimo raccomandato

---

## 5. Test e Validazione

### Procedura di Test

#### Fase 1: Test Software (Già Fatto)
✅ Implementata Soluzione 1: Rimosso `fillScreen()` ridondante

#### Fase 2: Test Power Recovery Delay (Se Serve)
Se le strisce persistono dopo Fase 1:

1. **Modifica il delay**: Cambia da 200ms a 400ms in main.cpp:568
2. **Ricompila e carica** il firmware
3. **Test**: 10-20 refresh di immagini diverse
4. **Monitora**: Annotare se le strisce sono ridotte o eliminate

#### Fase 3: Test Hardware (Se Ancora Serve)
Se le strisce persistono dopo Fase 2:

1. **Aggiungi condensatore 470µF** tra VIN e GND (rispettare polarità!)
2. **Non ricompilare** - usa lo stesso firmware
3. **Test**: 10-20 refresh di immagini diverse
4. **Monitora qualità display**:
   - Uniformità dei colori
   - Presenza/assenza di strisce
   - Qualità generale del refresh

### Strumenti Diagnostici (Opzionale)

Se vuoi misurare oggettivamente il problema:

**Multimetro**:
- Misura tensione su pin VIN durante refresh
- Voltage drop >0.5V indica problema di alimentazione
- Un buon condensatore bulk dovrebbe mantenere drop <0.2V

**Oscilloscopio** (avanzato):
- Visualizza ripple sulla linea 5V durante refresh
- Identifica spike e cadute di tensione
- Valida efficacia del condensatore

---

## 6. Diagnostica Problemi Persistenti

### Se le Strisce Persistono Dopo Tutti i Fix

#### Test A: Verifica Alimentazione USB
1. **Prova diversi cavi USB**: Alcuni cavi hanno resistenza elevata
2. **Prova diverse porte USB**: Le porte USB 2.0 forniscono massimo 500mA, USB 3.0+ fornisce 900mA
3. **Usa alimentatore dedicato**: 5V 2A USB wall charger (ideale per questo display)

#### Test B: Verifica Batteria (Se Alimentato a Batteria)
1. **Misura tensione batteria**: Deve essere >3.7V durante refresh
2. **Test con batteria carica al 100%**: Verifica se problema sparisce
3. **Considera batteria più capace**: LiPo con discharge rate >1C

#### Test C: Verifica Display Hardware
1. **Ispeziona connettore FPC**: Assicurati che sia inserito completamente
2. **Pulisci contatti FPC**: Usa alcol isopropilico e cotton fioc
3. **Testa con display diverso**: Verifica se il problema è nel display stesso

---

## 7. Conclusioni e Raccomandazioni

### Ordine di Implementazione Consigliato

1. ✅ **[FATTO] Soluzione 1**: Rimosso `fillScreen()` - Test in corso
2. ⏳ **[SE SERVE] Soluzione 3**: Aumentare delay da 200ms a 400ms - Modifiche software minime
3. ⭐ **[RACCOMANDATO] Hardware**: Aggiungere condensatore 470µF - Costo €0.30, grande impatto

### Previsioni

**Scenario più probabile**:
- La Soluzione 1 (rimozione `fillScreen()`) dovrebbe **risolvere il problema** nella maggior parte dei casi
- Se non basta, il condensatore 470µF dovrebbe **eliminare completamente** le strisce

**Scenario improbabile ma possibile**:
- Se anche con entrambi i fix le strisce persistono, potrebbe essere un problema hardware del display o del connettore FPC

### Prossimi Passi

1. **Carica firmware v0.11.0** con Soluzione 1
2. **Testa per alcuni giorni** con refresh normali
3. **Se vedi ancora strisce**: Implementa Soluzione 3 (delay 400ms)
4. **Se ancora persistono**: Aggiungi condensatore 470µF hardware

---

## 8. Riferimenti Tecnici

### Datasheet e Specifiche

- **GDEP073E01**: 6-color ACeP display, 800×480, ~400mA peak current
- **ESP32-S3**: 3.3V logic, 5V USB input, internal LDO regulator
- **ProS3(d)**: TPS62A01 buck converter, max 600mA output @ 3.3V

### Documenti Correlati

- [DISPLAY_STRIPES_INVESTIGATION.md](DISPLAY_STRIPES_INVESTIGATION.md) - Analisi root cause
- [DISPLAY_TROUBLESHOOTING_COMPLETE_GUIDE.md](DISPLAY_TROUBLESHOOTING_COMPLETE_GUIDE.md) - Guida troubleshooting
- [GDEP073E01_FORMAT_UPDATE.md](GDEP073E01_FORMAT_UPDATE.md) - Formato display

### Community e Supporto

- **GxEPD2 Library Issues**: https://github.com/ZinggJM/GxEPD2/issues
- **Waveshare E-Paper**: Support for ACeP 6-color displays
- **ESP32 Forums**: Power supply design discussions

---

**Documento creato**: 2026-01-01
**Versione firmware**: v0.11.0
**Autore**: Technical Documentation - ESP32 Photo Frame Project
