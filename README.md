# EuclidPatGen03

Dreikanaliger Euklidischer Pattern-Generator fuer Teensy 4.1 mit Touch-GUI und TFT-Display. Das Projekt erzeugt drei Euclid-Pattern, gibt Gate-Signale sowie drei CV-Werte ueber externe DACs aus und speichert Parameter dauerhaft im EEPROM.

## Features
- 3 Euclid-Kreise mit Laenge, Anzahl Schlaege und Rotation
- Touch-UI zum Editieren von Pattern, Values (CV) und Gate-Laengen
- Gate-Ausgaenge mit einstellbarer Laenge pro Step
- CV-Ausgabe ueber 2x MCP4822 (3 genutzte Kanaele)
- Persistente Speicherung der Parameter im EEPROM

## Hardware
- Board: Teensy 4.1
- Display: ILI9341 auf SPI1
- Touch: XPT2046 auf SPI0
- CV-DACs: 2x MCP4822 auf SPI0

### Pinout (aus `include/hardware_map.h`)
- Touch: `TOUCH_IRQ=2`, `TOUCH_CS=10`
- TFT: `TFT_MOSI=26`, `TFT_SCLK=27`, `TFT_MISO=1`, `TFT_DC=20`, `TFT_CS=21`, `TFT_RST=255`
- Gate Out: `GATE_OUT1=3`, `GATE_OUT2=4`, `GATE_OUT3=5`
- MCP4822: `CS_VALUE=9`, `CS_PITCH=8` (LDAC auf GND)
- CV In: `CV_IN_1=38 (A14)`, `CV_IN_2=39 (A15)`, `CV_IN_3=40 (A16)`
- Encoder 1: `CLK=15`, `DT=16`, `SW=24`
- Encoder 2: `CLK=17`, `DT=18`, `SW=25`
- Encoder 3: `CLK=22`, `DT=23`, `SW=32`
- Externe Eingaenge: `CLOCK_IN=6`, `RESET_IN=7`

### CV-Ausgaenge
- `MCP4822 #1 / Kanal A` = `VALUE_OUT1`
- `MCP4822 #1 / Kanal B` = `VALUE_OUT2`
- `MCP4822 #2 / Kanal A` = `VALUE_OUT3`
- `MCP4822 #2 / Kanal B` = Reserve / optional `PITCH_OUT`

## Build und Upload (PlatformIO)
`platformio.ini` nutzt Teensy 4.1 mit Arduino-Framework und `USB_SERIAL`.

Beispiel:
```powershell
pio run
pio run -t upload
```

## Bedienung der GUI

### Startscreen / Euclid-Kreise
- Drei Kreise fuer Pattern 1-3.
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
  - CV-Ausgabe ueber MCP4822
  - Gate-Trigger (pulsed, nicht blockierend)

## Persistente Speicherung
Parameter werden im EEPROM gespeichert (`src/storage.cpp`).
Gespeichert werden:
- Len/Num/Rot fuer alle 3 Patterns
- Values (0..255) und GateLen (0..255)
- Hold/GateHold sowie Rotate-Flags

## Projektstruktur
- `src/main.cpp` App-Einstieg, Timer/Loop, HW-Setup
- `src/euclid.cpp` Pattern-Berechnung und Kreis-Rendering
- `src/ui_screens.cpp` GUI-Screens und Interaktion
- `src/ui_touch.cpp` Touch-Mapping/Hit-Tests
- `src/gates.cpp` Gate-Trigger und CV-Ausgabe ueber MCP4822
- `src/storage.cpp` EEPROM-Persistenz
- `include/*.h` Gemeinsame Typen und Prototypen

## Hinweise
- Die Anzeige- und Touch-Kalibrierung ist in `src/ui_touch.cpp` hart codiert.
- TFT laeuft auf `SPI1`, Touch und beide MCP4822 auf `SPI0`.
- Alle externen Eingange am Teensy 4.1 nur mit `0..3.3V` anlegen.
- Maximale Pattern-Laenge: 32 Steps.
