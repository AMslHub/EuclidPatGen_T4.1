# EuclidPatGen_T4.1

## Projektbeschreibung
3-Kanal Euklidischer Pattern-Generator als **Eurorack-Modul** auf Basis des Teensy 4.1.
Erzeugt rhythmische Gate-Muster (Bjorklund-Algorithmus) und gibt analoge CV-Spannungen aus.

## Status
- Hardware noch nicht verfügbar — kein Hardware-Test möglich
- Code ist funktional, aber noch nicht auf echter Hardware verifiziert

## Build & Flash
```bash
pio run                  # Kompilieren
pio run -t upload        # Auf Teensy 4.1 flashen
pio device monitor       # Serial Monitor (USB)
```

## Hardware
- **MCU**: Teensy 4.1 (ARM Cortex-M7, 600 MHz)
- **SPI0**: XPT2046 Touch (CS=10) + MCP4822 DAC #1 (CS=9) + MCP4822 DAC #2 (CS=8)
- **SPI1**: ILI9341 TFT 320×240 (CS=21, 60 MHz)
- **Gate-Ausgänge**: Pin 3, 4, 5 (Eurorack-Pegel)
- **CV-Ausgänge**: 0–10V via 2× MCP4822 (12-bit DAC)
- **Sync**: Clock-In Pin 6, Reset-In Pin 7
- **Pin-Details**: siehe `include/hardware_map.h`

## Architektur
- **ISR** (`timerISR`): Nur `pendingTicks++` — bewusst minimal gehalten
- **Main Loop**: Verarbeitet einen Tick pro Iteration atomar (`consumePendingTick`)
- **Gate-Pulse**: Non-blocking via `gateOffAt[]`-Array, Overflow-sicher mit `(int32_t)(now - offAt) >= 0`
- **EEPROM-Save**: Debounced, 400ms nach letzter Benutzeraktion
- **`delayMicroseconds(500)`** in `loop()` vor `triggerGates()`: Zweck klären / ggf. entfernen

## Bekannte Schwachstellen (noch nicht behoben)
- `EPat1/2/3`, `GateHold1/2/3`, `Hold1/2/3` etc. sollten `bool EPat[3][32]` etc. sein
- `cnt` und `cnthold` sind unnötig `volatile`
- Touch-Handler hat Code-Duplikation (erster Touch vs. Drag)
- `GUIState` hat keine Typsicherheit (anonymes enum, kein `enum class`)
- Globaler Zustand vollständig in `app_state.h` — keine Kapselung
