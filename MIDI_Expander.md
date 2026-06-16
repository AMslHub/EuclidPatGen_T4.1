# MIDI Expander — Ideen & Planung

Eurorack-Erweiterungsmodul für den EuclidPatGen T4.1.  
Ermöglicht Pattern-Wechsel, Transpose und Parameter-Steuerung über MIDI.

---

## Konzept

Ein eigenständiges Eurorack-Modul das intern per 4-poliges Kabel mit dem
EuclidPatGen verbunden wird. Auf der Frontplatte: USB-A Buchse + MIDI-DIN Buchse.
Innen: Raspberry Pi Pico als MIDI-Host und Konverter.

```
[USB-A Buchse]     [MIDI-DIN 5pol]
      ↓                   ↓
      └────── Pico ───────┘
                  ↓
            4 Leitungen
                  ↓
           Teensy 4.1
        (EuclidPatGen)
```

---

## Hardware

### Raspberry Pi Pico (Kern)
- MCU: RP2040, Dual-Core ARM Cortex-M0+, 133 MHz
- Flash: 2MB, RAM: 264KB
- USB 1.1 — vollständig programmierbar als Host oder Device
- 3.3V Logik — direkt kompatibel mit Teensy 4.1 (kein Pegelwandler nötig)
- Preis: ~5€
- Programmierbar mit PlatformIO + VS Code (gleicher Workflow wie EuclidPatGen)

### USB-A Eingang
- Für Launchpad X (wird darüber auch versorgt)
- 5V/500mA — Kondensator 470µF auf 5V-Linie empfohlen (Einschaltimpuls)
- Pico übernimmt USB-Host-Funktion

### MIDI-DIN 5-pol Eingang
- Für Keystep, andere Hardware-Sequencer, Keyboards
- Klassische Optokoppler-Schaltung: 6N138 oder PC900
- 220Ω + 220Ω auf der Eingangsseite
- Schaltung im offiziellen MIDI Specification Document (midi.org)

### DIP-Switch (4-polig)
- Switch 1: MIDI-Kanal filtern (nur Kanal 1, oder alle)
- Switch 2: Transpose-Modus aktiv/inaktiv
- Switch 3+4: Reserve / Erweiterung
- Beide Quellen (USB + DIN) können gleichzeitig aktiv sein — Pico merged die Streams

### Verbindung zum EuclidPatGen
Nur 4 Leitungen über JST oder Dupont:

| Leitung | Von | Nach |
|---------|-----|------|
| GND | Expander | Teensy GND |
| 5V | Teensy VUSB | Pico VSYS |
| MIDI TX | Pico TX (Pin 28) | Teensy RX7 (Pin 28) |
| MIDI RX (optional) | Pico RX | Teensy TX7 (Pin 29) |

RX-Leitung nur nötig wenn LED-Feedback auf Launchpad gewünscht.

---

## Teensy-Seite (EuclidPatGen Firmware)

Freier UART: **Serial7** (RX=Pin 28, TX=Pin 29) — beide Pins aktuell unbelegt.

### MIDI-Mapping (Ideen)

| MIDI-Befehl | Funktion |
|-------------|----------|
| Note-On C3–B3 | Pattern 1–12 direkt anwählen |
| Program Change 0–127 | Pattern-Slot wählen |
| CC 1 | BPM |
| CC 2 | Spread |
| CC 3 | Transpose (pitchShift) |
| Pitch Bend | Echtzeit-Transpose |

---

## Software Pico

**Empfehlung: PlatformIO + Arduino-Pico von Earle Philhower**  
Gleicher Workflow wie EuclidPatGen — `platformio.ini`, Library Manager etc.

Libraries:
- `EZ_USB_MIDI_HOST` — USB-MIDI-Host für RP2040
- `Adafruit TinyUSB` — USB-Stack

**Alternative: Raspberry Pi Pico SDK + VS Code Extension**  
Offizielle Extension "Raspberry Pi Pico" von raspberry-pi, mehr Kontrolle aber mehr Setup.

---

## Referenzen & Schaltplan-Quellen

- **rppicomidi/PicoMIDI** (GitHub) — Pico USB-MIDI-Host + MIDI-DIN, Schaltplan + Code
- **Raspberry Pi Pico Datasheet** — raspberrypi.com, offizielles Pinout
- **MIDI Specification** — midi.org, Optokoppler-Eingangsschaltung
- **Mutable Instruments** (GitHub: pichenettes/eurorack) — Eurorack Schaltpläne open source, gut als Referenz für Stromversorgung
- **Doepfer DIY** — doepfer.de, Eurorack Busboard-Pinout und Spezifikation
- **Moritz Klein** (YouTube) — Eurorack-Schaltungen didaktisch erklärt
- **Eurorack KiCad Library** (GitHub) — Footprints und Symbole für KiCad

---

## Nächste Schritte (wenn bereit)

- [ ] KiCad-Schaltplan anlegen
- [ ] PCB-Layout (Modulgröße: ca. 4-6HP)
- [ ] Pico-Firmware schreiben (USB-Host + DIN-Parser + UART-Out)
- [ ] Teensy-Firmware: Serial7 MIDI-Parser + Pattern-Wechsel implementieren
- [ ] Gehäuse/Frontplatte designen
