# EuclidPatGen_T4.1

Dreikanaliger Euklidischer Pattern-Generator für Teensy 4.1 mit Touch-GUI, TFT-Display, Pitch-CV-Ausgang und Song-Sequencer. Das Modul erzeugt drei Euklid-Rhythmen, gibt Gate-Signale sowie CV-Werte über externe DACs aus und speichert Parameter dauerhaft im EEPROM/LittleFS.

## Features

- 3 Euklid-Kanäle mit Länge, Hits, Rotation, Wahrscheinlichkeit, Speed (÷4 … ×4)
- Pitch-CV-Ausgang (Ch 1): 1 V/Oct, 25 Skalen, Quantisierung, Fold-Modi, Step-Edit
- XY-Pad-Recorder: X = Value/CV, Y = Gate-Länge oder Pitch (1/3/5 Oktaven)
- Step-Values (CV), Gate-Länge, Ratchet (1–4 Sub-Hits), Oktav-Offset pro Step
- 31 Rhythmus-Presets (Latin, Afrika, Jazz, Electronic, Odd-Meter)
- 16+ Pitch-Presets (Berlin Arp, Acid 303, Celtic Arp, …)
- Performance-Screen: Mute, Solo, 7 Slots (Quick-Save / Browse / Pick & Place)
- Song-Sequencer: bis zu 64 Schritte, Slot-Sequenz mit Mute-Arm pro Kanal
- CV-Eingänge (3×): Modulation von Rotation, Values, Ratchet, Pitch-Shift, Fold, Slot
- Ext. Clock + Reset-Eingang
- Auto-Save (EEPROM, 400 ms Debounce); Slot-Save (LittleFS/SD)
- NAV-Screen: 4×4-Kacheln, Enc3-Long-Press von jedem Screen

## Hardware

- **MCU**: Teensy 4.1 (ARM Cortex-M7, 600 MHz)
- **Display**: ILI9341 320×240 auf SPI1 (40 MHz)
- **Touch**: XPT2046 auf SPI0
- **CV-DACs**: 2× MCP4822 (12-bit) auf SPI0
- **Gate-Ausgänge**: Pin 3, 4, 5 (Eurorack-Pegel, active-low über 74HCT14)
- **CV-Eingänge**: Pin 38/39/40 (A14/A15/A16), 0–3,3 V intern (Schutzschaltung auf 0–6,6 V)

### Pinout (aus `include/hardware_map.h`)

| Signal | Pin |
|---|---|
| Touch IRQ | 2 |
| Touch CS | 10 |
| TFT MOSI/SCLK/MISO/DC/CS | 26/27/1/20/21 |
| Gate Out 1/2/3 | 3/4/5 |
| DAC CS (Values) | 9 |
| DAC CS (Pitch) | 8 |
| CV In 1/2/3 | 38/39/40 |
| Enc 1 CLK/DT/SW | 15/16/24 |
| Enc 2 CLK/DT/SW | 17/18/25 |
| Enc 3 CLK/DT/SW | 22/23/32 |
| Clock In | 6 |
| Reset In | 7 |

### CV-Ausgänge

| DAC-Kanal | Signal |
|---|---|
| MCP4822 #1 / A | Value Out Ch 1 |
| MCP4822 #1 / B | Value Out Ch 2 |
| MCP4822 #2 / A | Value Out Ch 3 |
| MCP4822 #2 / B | Pitch Out Ch 1 (1 V/Oct) |

## Build und Upload (PlatformIO)

```powershell
pio run              # Kompilieren
pio run -t upload    # Auf Teensy 4.1 flashen
pio device monitor   # Serial Monitor (USB)
```

## Timing-Architektur

Das Modul verwendet ein ISR/Main-Loop-Modell mit Deferred Rendering um Gate-Timing und Display-Draws zu entkoppeln:

- **`timerISR`**: `pendingTicks++` + Pre-Arm (Gate Ch1/Ch2 direkt aus ISR — pünktlich auch während SPI-Draw)
- **Gate-Off-ISR** (1 kHz): Schaltet Gate-Pulse ab, unabhängig vom Main Loop
- **Gate Ch0**: Immer im Main Loop, **nach** `outputValuesForStep()` — Pitch-CV wird vor Gate gesetzt
- **Deferred Rendering**: `requestNavigateTo()` aus Touch/Encoder-Handlern → Draw nur bei `pendingTicks == 0`
- **Zwei-Phasen-Draw**: `fillScreen` und Inhalte an zwei separaten sicheren Momenten (~20 ms je Phase)
- **`discardPendingTicks()`**: Nur nach echten Sequencer-Resets (Slot-Load, Song-Halt) — nie nach Zeichenoperationen

## Projektstruktur

| Datei | Inhalt |
|---|---|
| `src/main.cpp` | App-Einstieg, Timer-ISR, Main Loop, Touch-Dispatch |
| `src/ui_screens.cpp` | Alle GUI-Screens, Touch-Handler, Deferred-Navigation |
| `src/ui_touch.cpp` | Touch-Kalibrierung und Hit-Test-Hilfsfunktionen |
| `src/encoders.cpp` | Encoder-Auswertung (Drehen, Kurz-/Lang-Druck) |
| `src/euclid.cpp` | Bjorklund-Algorithmus, Kreis-Rendering |
| `src/gates.cpp` | Gate-Trigger, CV-Ausgabe (MCP4822), Ratchet |
| `src/pitch.cpp` | Pitch-Quantisierung, Skalen, Fold-Modi |
| `src/cv_inputs.cpp` | CV-Eingangs-Abtastung und Modulations-Routing |
| `src/storage.cpp` | EEPROM/LittleFS-Persistenz, Slot-Save/Load |
| `include/app_state.h` | Globaler Zustand, extern-Deklarationen |
| `include/hardware_map.h` | Pin-Zuweisungen |

## Hinweise

- TFT auf SPI1 (40 MHz); Touch und beide MCP4822 auf SPI0
- Alle externen Eingänge am Teensy 4.1 nur mit 0–3,3 V anlegen (Schutzschaltung vorhanden)
- Maximale Pattern-Länge: 32 Steps
- Hardware noch nicht auf echter Platine verifiziert (Prototyp-Status)
