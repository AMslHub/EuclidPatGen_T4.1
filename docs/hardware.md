# Hardware-Dokumentation EuclidPatGen T4.1

3-Kanal Euklidischer Pattern-Generator als Eurorack-Modul auf Basis des Teensy 4.1.

---

## Pin-Belegung Teensy 4.1

### SPI0 — Touch + DAC (MOSI=11, MISO=12, SCK=13)

| Pin | Konstante | Funktion |
|---|---|---|
| 11 | — | SPI0 MOSI |
| 12 | — | SPI0 MISO |
| 13 | — | SPI0 SCK |
| 2 | TOUCH_IRQ_PIN | XPT2046 Interrupt |
| 10 | TOUCH_CS_PIN | XPT2046 Chip Select |
| 9 | MCP4822_CS_VALUE_PIN | MCP4822 DAC1 CS (Out1, Out2) |
| 8 | MCP4822_CS_PITCH_PIN | MCP4822 DAC2 CS (Out3, reserviert) |

MCP4822 LDAC: fest auf **GND** verdrahtet (kein Teensy-Pin belegt).

**MCP4822 Kanalzuordnung:**

| DAC | CS-Pin | Kanal A | Kanal B |
| --- | --- | --- | --- |
| DAC1 | 9 | Value Out 1 | Value Out 2 |
| DAC2 | 8 | Value Out 3 | reserviert (0V) |

Hinweis: `MCP4822_CS_PITCH_PIN` ist ein historischer Name aus der Planung — aktuell als dritter Value-Ausgang genutzt.

### SPI1 — TFT ILI9341 (MOSI=26, MISO=1, SCK=27)

| Pin | Konstante | Funktion |
|---|---|---|
| 1 | TFT_MISO_PIN | MISO |
| 20 | TFT_DC_PIN | Data/Command |
| 21 | TFT_CS_PIN | Chip Select |
| 26 | TFT_MOSI_PIN | MOSI |
| 27 | TFT_SCLK_PIN | SCK |

TFT_RST: fest auf **3.3V** verdrahtet (TFT_RST_PIN = 255 = nicht belegt).

### Gate-Ausgänge

| Pin | Konstante | Funktion |
|---|---|---|
| 3 | GATE_OUT1_PIN | Gate Kanal 1 |
| 4 | GATE_OUT2_PIN | Gate Kanal 2 |
| 5 | GATE_OUT3_PIN | Gate Kanal 3 |

### CV-Eingänge

| Pin | Konstante | Funktion |
|---|---|---|
| 38 (A14) | CV_IN_1_PIN | CV-Eingang Kanal 1 |
| 39 (A15) | CV_IN_2_PIN | CV-Eingang Kanal 2 |
| 40 (A16) | CV_IN_3_PIN | CV-Eingang Kanal 3 |

### Encoder

| Pin | Konstante | Funktion |
|---|---|---|
| 15 | ENC1_CLK_PIN | Encoder 1 CLK |
| 16 | ENC1_DT_PIN | Encoder 1 DT |
| 24 | ENC1_SW_PIN | Encoder 1 Taster |
| 17 | ENC2_CLK_PIN | Encoder 2 CLK |
| 18 | ENC2_DT_PIN | Encoder 2 DT |
| 25 | ENC2_SW_PIN | Encoder 2 Taster |
| 22 | ENC3_CLK_PIN | Encoder 3 CLK |
| 23 | ENC3_DT_PIN | Encoder 3 DT |
| 32 | ENC3_SW_PIN | Encoder 3 Taster |

### Externe Eingänge

| Pin | Konstante | Funktion |
|---|---|---|
| 6 | CLOCK_IN_PIN | Externer Clock-Eingang |
| 7 | RESET_IN_PIN | Externer Reset-Eingang |

---

## Eurorack-Stromversorgung

Anschluss über Standard **16-Pin Eurorack Busboard-Connector**:

| Schiene | Verwendung |
| --- | --- |
| +12V | TL074 (Value-Ausgänge), −12V-Bias für MCP6002 (über R_bias) |
| −12V | MCP6002 Bias-Widerstand R_bias = 170 kΩ |
| +5V | 74HCT14 (Gate-Ausgänge) |
| GND | Gemeinsame Masse |

+3.3V wird vom Teensy 4.1 intern erzeugt (On-Board-Regler, gespeist aus USB oder +5V).

---

## Betriebsspannungen — Übersicht

| Bauteil | Funktion | VDD | VSS |
|---|---|---|---|
| MCP6002 | CV-Eingänge | +3.3V | GND |
| MCP4822 | DAC (Value-Ausgänge) | +3.3V | GND |
| TL074 | Value-Ausgänge Verstärker | +12V | −12V |
| 74HCT14 | Gate-Ausgänge | +5V | GND |
| MMBT3904 | Clock-In / Reset-In | +3.3V Pull-up | GND |

---

## CV-Eingänge (3×)

**Zweck:** Externe CV-Modulation von Pattern-Parametern (Mapping noch offen).

**IC:** MCP6002 (Dual Op-Amp, Single Supply)
**Betriebsspannung:** +3.3V / GND

**Topologie:** Invertierender Summierverstärker mit −12V-Bias

```
CV IN ── R1=100k ──┬── (−) MCP6002 ── Vout ── Teensy ADC
                   │        │
              R2=47k (Feedback)
                   │
             R_bias=170k ── −12V (Eurorack-Rail)
                   │
                  (+) ── GND
```

| Bauteil | Wert | Funktion |
|---|---|---|
| R1 | 100 kΩ | Eingangswiderstand, schützt Op-Amp via virtuelle Masse |
| R2 | 47 kΩ | Feedback |
| R_bias | 170 kΩ | An −12V, erzeugt Offset am Ausgang |

**Kennwerte:**

| Vin | Vout |
|---|---|
| 0V | +3.3V |
| 3.3V | +1.75V |
| 6.6V | ~+0.2V |

- Eingangsspannungsbereich: **0–6.6V**
- Signal ist invertiert → Software-Kompensation: `cvRaw = 4095 - analogRead(pin)`
- ADC-Auflösung: 12-bit (`analogReadResolution(12)`)
- Pins: A14 (38), A15 (39), A16 (40)

---

## Gate-Ausgänge (3×)

**IC:** 74HCT14 (Hex Schmitt-Trigger-Inverter)
**Betriebsspannung:** +5V (Eurorack-Busboard)

**Topologie:**
```
Teensy Pin ── 74HCT14 ── 1 kΩ ── GATE OUT (Buchse)
(3.3V Logik)  (5V VDD)
```

**Logik (invertierend):**

| Teensy Pin | 74HCT14 Ausgang | Bedeutung |
|---|---|---|
| HIGH | LOW | Kein Gate (Idle) |
| LOW | HIGH (+5V) | Gate aktiv |

- 3 von 6 Invertern genutzt, 3 bleiben frei
- Schutzwiderstand: **1 kΩ** in Serie am Ausgang (Eurorack-Standard)
- Pins: 3, 4, 5

---

## Value-Ausgänge / CV-Ausgänge (3×)

### DAC

**IC:** MCP4822 (12-bit Dual-DAC, SPI)
**Betriebsspannung:** +3.3V / GND (kein SPI-Pegelwandler zum Teensy nötig)
**Gain-Modus:** 1× (Bit13=1, `0x2000`) → Vout_max = **2.048V** (interne Bandgap-Referenz)

### Ausgangsverstärker

**IC:** TL074 (Quad Op-Amp)
**Betriebsspannung:** +12V / −12V (Eurorack-Rail)

**Topologie:** Nicht-invertierender Verstärker

```
MCP4822 ── (+) TL074 ──── Vout ── 1 kΩ ── CV OUT (Buchse)
                  │
               R2=22k
                  │
             (−) ─┤
               R1=10k
                  │
                 GND
```

| Bauteil | Wert |
|---|---|
| R1 | 10 kΩ |
| R2 | 22 kΩ |
| Gain | 1 + 22/10 = **3.2** |
| Vout_max | 2.048V × 3.2 = **6.55V** |

- Schutzwiderstand: **1 kΩ** in Serie am Ausgang (Eurorack-Standard)
- TL074 hat 4 Op-Amps — passt exakt für 3 Kanäle, einer bleibt frei

---

## Clock-In / Reset-In

**Transistor:** MMBT3904 (NPN, SOT-23) — je einer pro Eingang
**Betriebsspannung:** +3.3V Pull-up / GND

**Topologie:**
```
Eurorack IN ── R_base=100k ── Basis ── MMBT3904 ── GND
                                       Collector ── R_pullup=10k ── +3.3V
                                                 └── Teensy Pin
```

**Kennwerte:**
- Eingangsbereich: 0V bis +12V (volle Eurorack-Spannung)
- Ausgang: **active-low** (invertiert)
- Kein separater Schutzdioden nötig (100 kΩ + B-E-Strecke ausreichend)
- Pins: Clock-In = 6, Reset-In = 7

**Firmware-Verhalten:**
- Interrupt auf **fallende Flanke**
- **Auto-Detect:** Externer Clock erkannt → interner BPM-Timer stoppt automatisch
- **Timeout:** 3 Sekunden ohne externen Puls → Rückfall auf internen BPM-Timer
- Gemessene Clock-Periode wird für Gate-Längen-Berechnung übernommen
