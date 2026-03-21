# EuclidPatGen03

Dreikanaliger Euklidischer Pattern-Generator fuer Teensy 3.6 mit Touch-GUI und TFT-Display. Das Projekt erzeugt drei Euclid-Pattern, gibt Gate-Signale sowie drei CV-Werte (2x DAC + 1x PWM) aus und speichert Parameter dauerhaft im EEPROM.

## Features
- 3 Euclid-Kreise mit Laenge, Anzahl Schlaege und Rotation
- Touch-UI zum Editieren von Pattern, Values (CV) und Gate-Laengen
- Gate-Ausgaenge mit einstellbarer Laenge pro Step
- CV-Ausgabe: DAC0, DAC1 und PWM (8-bit)
- Persistente Speicherung der Parameter im EEPROM

## Hardware
- Board: Teensy 3.6
- Display: ILI9341 (SPI)
- Touch: XPT2046 (SPI)

### Pinout (aus `src/main.cpp`)
- Touch: `TOUCH_IRQ=2`, `TOUCH_CS=10`
- TFT: `TFT_MOSI=7`, `TFT_SCLK=14`, `TFT_MISO=12`, `TFT_DC=20`, `TFT_CS=21`, `TFT_RST=255`
- Gate Out: `GATE_OUT1=3`, `GATE_OUT2=4`, `GATE_OUT3=5`
- CV: `DAC0=A21`, `DAC1=A22`, `PWM_OUT=29`

## Build und Upload (PlatformIO)
`platformio.ini` nutzt Teensy 3.6 mit Arduino-Framework und `USB_SERIAL`.

Beispiel:
```powershell
pio run
pio run -t upload
```

## Bedienung der GUI

### Startscreen / Euclid-Kreise
- Drei Kreise fuer Pattern 1–3.
- Tippe auf Ecke 1/2/3, um in das Parameter-Menue des jeweiligen Patterns zu wechseln.

### Parameter-Menue (EUCLPARAM)
- Setzt `Len`, `Num` und `Rot` des Patterns.
- Ruecksprung zum Kreis-Screen ueber Pfeil (UL).
- Button `V1/V2/V3` wechselt zu Values-Screen.

### Values-Screen
- Balken editieren die CV-Werte je Step.
- `H` (Hold): Wenn aktiv, werden neue CV-Werte nur bei Hits gesetzt.
- `RV` (Rotate Values): Werte werden relativ zur Pattern-Rotation interpretiert.
- `GLen` wechselt in den Gate-Laengen-Screen.

### GateLen-Screen
- Balken setzen Gate-Laenge pro Step.
- `GH` (Gate Hold): aktiviert variable Gate-Laengen, sonst fixe Pulslaenge.
- `RGL` (Rotate GateLen): Gate-Laengen relativ zur Rotation interpretieren.

## Timing
- BPM-Takt ueber `IntervalTimer`.
- Pro Tick:
  - GUI-Playhead/Pattern-Anzeige
  - CV-Ausgabe
  - Gate-Trigger (pulsed, nicht blockierend)

## Persistente Speicherung
Parameter werden im EEPROM gespeichert (`src/storage.cpp`).
Gespeichert werden:
- Len/Num/Rot fuer alle 3 Patterns
- Values (0..255) und GateLen (0..255)
- Hold/GateHold sowie Rotate-Flags

## Projektstruktur
- `src/main.cpp`     App-Einstieg, Timer/Loop, HW-Setup
- `src/euclid.cpp`   Pattern-Berechnung und Kreis-Rendering
- `src/ui_screens.cpp`  GUI-Screens und Interaktion
- `src/ui_touch.cpp` Touch-Mapping/Hit-Tests
- `src/gates.cpp`    Gate-Trigger und CV-Ausgabe
- `src/storage.cpp`  EEPROM-Persistenz
- `include/*.h`      Gemeinsame Typen und Prototypen

## Hinweise
- Die Anzeige- und Touch-Kalibrierung ist in `src/ui_touch.cpp` hart codiert.
- Maximale Pattern-Laenge: 32 Steps.
