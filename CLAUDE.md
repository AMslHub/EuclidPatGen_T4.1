# EuclidPatGen_T4.1

## Projektbeschreibung
3-Kanal Euklidischer Pattern-Generator als **Eurorack-Modul** auf Basis des Teensy 4.1.
Erzeugt rhythmische Gate-Muster (Bjorklund-Algorithmus) und gibt analoge CV-Spannungen aus.

## Status
- Hardware verfügbar und in aktivem Test
- Code läuft auf echter Hardware; Bugs werden laufend gefunden und behoben

## Build & Flash
```bash
pio run                  # Kompilieren
pio run -t upload        # Auf Teensy 4.1 flashen
pio device monitor       # Serial Monitor (USB)
```

## Hardware
- **MCU**: Teensy 4.1 (ARM Cortex-M7, 600 MHz)
- **SPI0**: XPT2046 Touch (CS=10) + MCP4822 DAC #1 (CS=9) + MCP4822 DAC #2 (CS=8)
- **SPI1**: ILI9341 TFT 320×240 (CS=21, 40 MHz) — 60 MHz führte zu DMA-Instabilität
- **Gate-Ausgänge**: Pin 3, 4, 5 (Eurorack-Pegel)
- **CV-Ausgänge**: 0–10V via 2× MCP4822 (12-bit DAC)
- **Sync**: Clock-In Pin 6, Reset-In Pin 7
- **Pin-Details**: siehe `include/hardware_map.h`

## Architektur
- **ISR** (`timerISR`): `pendingTicks++` + **Pre-Arm** (Gate Ch1/Ch2 direkt aus ISR feuern, auch während SPI-Draw)
- **Gate-Off-ISR** (1 kHz): Schaltet Gate-Pulse ab — unabhängig vom Main Loop
- **Main Loop**: Verarbeitet einen Tick pro Iteration atomar (`consumePendingTick`); Gate Ch0 immer **nach** `outputValuesForStep()` (Pitch-CV vor Gate)
- **Gate-Pulse**: Non-blocking via `gateOffAt[]`-Array, Overflow-sicher mit `(int32_t)(now - offAt) >= 0`
- **Deferred Rendering**: Screen-Draws nie sofort aus Touch/Encoder-Handlern — `requestNavigateTo()` setzt Flag, Main Loop zeichnet bei `pendingTicks == 0`
- **Zwei-Phasen-Draw**: `fillScreen` (Phase 1) und Inhalte (Phase 2) an zwei separaten sicheren Momenten — max. ~20ms Blockierung pro Phase
- **EEPROM/LittleFS-Save**: Debounced, 400ms nach letzter Benutzeraktion
- **`delayMicroseconds(500)`** in `loop()` vor `triggerGates()`: Zweck klären / ggf. entfernen

## Bekannte Schwachstellen (noch nicht behoben)
- `EPat1/2/3`, `GateHold1/2/3`, `Hold1/2/3` etc. sollten `bool EPat[3][32]` etc. sein
- `cnt` und `cnthold` sind unnötig `volatile`
- Touch-Handler hat Code-Duplikation (erster Touch vs. Drag)
- `GUIState` hat keine Typsicherheit (anonymes enum, kein `enum class`)
- Globaler Zustand vollständig in `app_state.h` — keine Kapselung
