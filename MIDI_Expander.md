# MIDI Expander — Ideen & Planung

Eurorack-Erweiterungsmodul für den EuclidPatGen T4.1.  
Ermöglicht Pattern-Wechsel, Transpose, Parameter-Steuerung über MIDI-IN  
**und** Akkord-Ausgabe an externe Rompler/Synthesizer über MIDI-OUT.

---

## Musikalisches Gesamtkonzept

Mit dem Expander hat der EuclidPatGen **4 musikalische Kanäle:**

| Kanal | Ausgang | Inhalt |
| ----- | ------- | ------ |
| Ch1 | CV/Gate (1V/Oct) | Solo-Melodie, pitch-quantisiert |
| Ch2 | Gate | Rhythmus / Jump |
| Ch3 | Gate | Rhythmus / Jump |
| Ch4 | MIDI-OUT DIN | Akkorde → externer Pad-Synth (z.B. JV-1080) |

Der Akkord-Kanal folgt automatisch `pitchRoot` und dem aktiven Chord-Preset —
Root und Voicing bleiben immer zur eingestellten Scale passend.

### Melodie + Pad: zwei JV-1080-Kanäle

**Ch1 (Melodie)** läuft parallel auf zwei Wegen:
- Analog: CV/Gate wie bisher (1V/Oct → VCO, Eurorack-Pegel)
- MIDI CH1: dieselbe quantisierte Note als Note-On → JV-1080 Solo-Part

**Ch4 (Pad)** ist eine untergeordnete Seite des Pitch1-Screens (Long Press oder eigener Nav-Punkt):
- MIDI CH2: Akkord dauerhaft gehalten (kein rhythmischer Trigger)
- Akkord folgt automatisch `pitchRoot` + aktivem Chord-Preset
- Parametrierbar: Oktave, Inversion, Akkordtöne (1 / 1+3 / 1+3+5 / 1+3+5+7 / 1+3+5+7+9), MIDI-Kanal
- Akkord wird neu gesendet wenn Root oder Chord-Preset wechselt (Note-Off + Note-On)
- Kein rhythmischer Gate nötig — Pad bleibt einfach klingen

---

## Akkord-Sequencer Page (UI-Konzept)

Eigenständiger Screen im EuclidPatGen — erreichbar über Long-Press auf der Pitch1-Seite
oder als eigener NAV-Eintrag. Steuert den MIDI-OUT Akkord-Kanal (JV-1080 CH2/Pad).

### Globale Parameter (Titelzeile)

```
Chord   Div:[4]   Len:[8]   [LIVE]
```

- **Div** (Divisor): 1, 2, 4, 8, 16, 32 — alle n Steps wechselt der Akkord-Pointer
- **ChordLen**: 1–32 — wie viele Slots aktiv sind, dann Rücksprung auf 0
- **LIVE**: wenn aktiv, klingt der aktuell eingestellte Akkord dauerhaft (kein Listenabspiel)

### Slot-Liste (8 Slots pro Seite, bis 32 Slots gesamt)

```
Y   0– 39  Titel: "Chord  Div:[4]  Len:[8]  [LIVE]"
Y  40– 69  Fill-Buttons: [Leg=1]  [Val=50]  [CLR]
Y  70– 93  Row 0: Slot-Nummer (1–32)
Y  94–117  Row 1: Mute   — Einzel-Event, Override über alles (auch Legato)
Y 118–141  Row 2: Legato — Carry-Forward; gemeinsame Töne klingen durch
Y 142–165  Row 3: Val    — 10–100% in 10er-Schritten, Carry-Forward
Y 166–189  Row 4: Akk-Nr — 0–7 (welcher der 8 definierten Akkorde), Carry-Forward
Y 190–239  Reserve / Cursor-Rahmen
```

X: Label-Spalte 24px, 8 Spalten à 37px (36px Inhalt + 1px Gap) = 320px

### Carry-Forward Regelwerk

| Feld   | Leer =          | Mute-Verhalten                        |
|--------|-----------------|---------------------------------------|
| Akk-Nr | letzter Wert    | Akkord schweigt (kein Note-On)        |
| Val    | letzter Wert    | schweigt                              |
| Leg    | letzter Wert    | Mute unterbricht Legato (Note-Off)    |
| Mute   | Einzel-Event    | Override über Leg und Val             |

### Encoder-Belegung

| Encoder | Drehen | Short Press | Long Press |
|---------|--------|-------------|------------|
| Enc1 | Step-Cursor bewegen (+ Seitenwechsel) | Feld-Cursor: Akk-Nr→Mute→Leg→Val→Div→Len→… | Aktuellen Slot löschen |
| Enc2 | Wert am aktiven Feld ändern | LIVE-Button toggle | Quick Save |
| Enc3 | Seite direkt wechseln (0–3) | NAV öffnen | NAV öffnen |

### Akkord-Definition (8 Slots, pro Slot editierbar)

Jeder der 8 Akkord-Slots hat:
- **Akk-Nr**: Referenz auf einen der 8 gespeicherten Akkorde
- **Spread**: wie weit der Akkord über Oktaven gespreizt wird
- **Inv**: Akkord-Inversion (0=Grundstellung, 1=erste Umkehrung, …)
- **Oct**: Oktave verschieben (±3)
- **Töne**: Auswahl 1 / 1+3 / 1+3+5 / 1+3+5+7 / 1+3+5+7+9
- **MIDI-Kanal**: pro Slot frei wählbar (1–16), default CH2

### Synchronisation

- Akkord-Pointer startet bei Reset / Pattern-Start auf Slot 0
- Divisor zählt Steps (nicht Hits) — unabhängig vom Euklid-Gate-Pattern
- Gesamtzyklus in Steps: `Divisor × ChordLen`
  - Beispiel Div=4, Len=6 → alle 24 Steps ein voller Akkord-Durchlauf
  - Melodie (PatLen=16) und Akkord (24 Steps) kommen nach kgV(16,24)=48 Steps zusammen

### MIDI-Kanal-Zuordnung (konfigurierbar auf Config-Page)

| Signal | Default MIDI-Kanal | Inhalt |
|--------|--------------------|--------|
| Melodie CH1 | 1 | Monophone Note-On/Off (quantisiert) |
| Akkord/Pad | 2 | Polyphone Akkord-Töne (Legato-fähig) |
| Rhythmus CH2 | 9 | Gate → Note-On/Off auf festem Key |
| Rhythmus CH3 | 10 | Gate → Note-On/Off auf festem Key |

---

## Konzept

Ein eigenständiges Eurorack-Modul das intern per 4-poliges Kabel mit dem
EuclidPatGen verbunden wird. Pico übernimmt das komplette MIDI-Routing:
empfängt MIDI-IN (USB + DIN), leitet MIDI-OUT durch, merged alle Streams
über einen einzigen UART-Kanal zum Teensy.

```
[USB-A Buchse]  [MIDI-DIN IN]  [MIDI-DIN OUT]
      ↓                ↓              ↑
      └──────── Pico (RP2040) ────────┘
                      ↕
               4 Leitungen
               (GND, 5V, TX, RX)
                      ↕
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

### MIDI-DIN 5-pol Ausgang (MIDI-OUT)
- Für externe Rompler/Synthesizer (z.B. Roland JV-1080, Pad-Synths)
- Einfache Transistorschaltung (3 Bauteile: 1× BC547, 2× 220Ω)
- Teensy sendet Akkordtöne (Note-On/Off) über UART → Pico → MIDI-DIN OUT
- Chord-Preset, Root und Voicing folgen der aktuellen EuclidPatGen-Einstellung
- Trigger: Gate Ch1 (oder eigenes Euklid-Pattern für Akkord-Rhythmus)
- Note-Off nach einstellbarer Gate-Length oder beim nächsten Hit

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

---

## Implementierungsstand (Stand 2026-06-17)

> **Hier weitermachen**, wenn das Pico-Extension-Board aufgebaut ist.

### ✅ Teensy-Firmware — MIDI-OUT vollständig implementiert

Alle vier musikalischen Kanäle werden bereits auf dem Teensy fertig als MIDI-Bytes berechnet
und über **Serial7 (TX=Pin 29, 31250 Baud)** ausgegeben.
Der Pico muss diese Bytes nur 1:1 auf MIDI-DIN OUT durchleiten — kein MIDI-Parsing im Pico nötig.

| Kanal | MIDI-Kanal | Implementierung | Datei |
|-------|-----------|-----------------|-------|
| Melodie Ch0 | CH1 | Note-On bei Gate-Hit (`outputValuesForStep()`), Note-Off via `midiNoteOffPending`-Flag aus ISR | `src/gates.cpp`, `src/midi_out.cpp` |
| Akkord/Pad | CH2 | `midiOutTick()` bei `chordPlayPos`-Wechsel, Legato-Logik, Carry-Forward | `src/midi_out.cpp` |
| Rhythmus Ch1 | CH9 | Note-On in `triggerGateForCh()` / `triggerGates()`, Note-Off via Flag | `src/gates.cpp` |
| Rhythmus Ch2 | CH10 | Note-On in `triggerGateForCh()` / `triggerGates()`, Note-Off via Flag | `src/gates.cpp` |

**ISR-Sicherheit:** `gateOffISR` setzt `volatile uint8_t midiNoteOffPending` (Bit-Flags),
Main-Loop liest atomar und ruft `midiOutMelodyOff()` / `midiOutRhythmOff()` auf.
`Serial7.write()` wird **nie** aus dem ISR heraus aufgerufen.

**Akkord-Berechnung:** `buildChordNotes()` in `src/midi_out.cpp` — skalengerecht via `buildNoteList()`,
ToneMask (Bits 0–6 = Stufen 1,3,5,7,9,11,13), Inversion, Oct-Offset, Spread über Oktaven.

**Legato:** Note-On neue Noten → dann Note-Off nur die Töne die im neuen Akkord nicht vorkommen
(gemeinsame Töne klingen durch ohne Unterbrechung).

### ⏳ Teensy-Firmware — MIDI-IN (noch nicht implementiert)

Wartet auf Pico-Hardware. Zu implementieren wenn Extension-Board fertig:

| Funktion | MIDI-Befehl | Ziel |
|----------|-------------|------|
| Pattern-Wechsel | Note-On C3–B3 oder Program Change 0–127 | `perfSlot` wählen |
| BPM | CC 1 | `bpm` setzen |
| Spread | CC 2 | `spread` aller Kanäle |
| Transpose | CC 3 | `pitchShift` (Skalenstufen) |
| Echtzeit-Transpose | Pitch Bend | `pitchShift` |

Serial7 RX (Pin 28) ist frei — noch kein Parser implementiert.

### ⏳ Pico-Firmware — nicht begonnen

Zu implementieren wenn Hardware-Teile vorliegen:

| Aufgabe | Details |
|---------|---------|
| MIDI-DIN OUT Durchleitung | UART vom Teensy → MIDI-DIN OUT (top priority — damit Akkorde/Melodie sofort klingen) |
| MIDI-DIN IN Parser | Optokoppler (6N138) → UART → zum Teensy weiterleiten |
| USB-MIDI-Host | Launchpad X / Keystep über USB-A |
| Stream-Merger | USB-MIDI + DIN-IN → ein UART zum Teensy |

**Empfehlung Pico-Firmware-Reihenfolge:**
1. UART-Durchleitung (Teensy → MIDI-DIN OUT) — 10 Zeilen Code, sofort nutzbar
2. MIDI-DIN IN Passthrough (DIN IN → Teensy)
3. USB-MIDI-Host (Launchpad X)

### ⏳ Chord-Daten im Save/Load (noch nicht implementiert)

`chordSlots[]`, `chordDefs[]`, `chordDiv`, `chordLen` sind noch nicht im LittleFS-Slot-Save enthalten.
Wird beim Abschalten nicht gespeichert und geht verloren.

---

## Nächste Schritte (wenn Pico-Hardware bereit)

**Hardware:**
- [ ] KiCad-Schaltplan anlegen (Pico + USB-A + MIDI-DIN IN/OUT + DIP-Switch)
- [ ] PCB-Layout (Modulgröße: ca. 6-8HP für alle Buchsen)
- [ ] Frontplatte designen (USB-A, MIDI-DIN IN, MIDI-DIN OUT, DIP-Switch)
- [ ] Verbindungskabel EuclidPatGen ↔ Expander (4-polig JST)

**Pico-Firmware (in dieser Reihenfolge):**
- [ ] UART-Durchleitung Teensy TX → MIDI-DIN OUT (Prio 1 — sofort nutzbar)
- [ ] MIDI-DIN IN Parser (Optokoppler → UART → Teensy)
- [ ] USB-MIDI-Host (Launchpad X empfangen)
- [ ] Stream-Merger (USB + DIN → ein UART zum Teensy)

**Teensy-Firmware (EuclidPatGen):**
- [ ] Serial7 MIDI-IN Parser implementieren (Serial7 RX = Pin 28)
- [ ] Pattern-Wechsel via Note-On / Program Change
- [ ] Transpose via CC / Pitch Bend
- [ ] Chord-Daten in LittleFS-Slot-Save einbauen (`chordSlots[]`, `chordDefs[]`, `chordDiv`, `chordLen`)
