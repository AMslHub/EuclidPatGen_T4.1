# EuclidPatGen T4.1 — Bedienungshandbuch

**3-Kanal Euklidischer Pattern-Generator** · Eurorack-Modul · Teensy 4.1

---

## Übersicht

Der EuclidPatGen erzeugt drei unabhängige rhythmische Gate-Muster nach dem Bjorklund-Algorithmus (Euklidische Rhythmen) und gibt für Kanal 1 eine quantisierte Pitch-CV-Spannung aus. Bedient wird das Modul über ein 2,8″-Touchscreen und drei Drehregler mit Drucktaste.

---

## Hardware & Anschlüsse

| Anschluss | Funktion |
|---|---|
| Gate Out 1–3 | Euklidische Gate-Signale, Eurorack-Pegel |
| CV Out 1 (Pitch) | 1 V/Oct, 0–6,55 V, Ch 1 Pitch-Sequenz |
| CV Out 2–3 | 0–6,55 V, Step-Values Ch 2 / Ch 3 |
| CV In 1–3 | Analoge Modulations-Eingänge, 0–6,6 V |
| Clock In | Externer Clock (MMBT3904 Treiber, active-low, Pin 6) |
| Reset In | Pattern-Reset (Pin 7) |
| Enc 1 | Linker Drehregler + Taste (Kanal-bezogen) |
| Enc 2 | Mittlerer Drehregler + Taste (Kanal-bezogen) |
| Enc 3 | Rechter Drehregler + Taste (global) |

---

## Grundbedienung der Encoder

Alle drei Encoder haben dieselbe Grundregel: **Drehen** ändert Werte, **kurzer Druck** schaltet oder bestätigt, **langer Druck** (≥ 600 ms) löst eine Sonderfunktion aus.

| Encoder | Langer Druck (global, alle Screens) |
|---|---|
| Enc 1 | Auf PITCH1: Gefaltetes Pattern auf Original zurückschreiben |
| Enc 2 | **Quick-Save** — aktuellen Zustand in Slot speichern |
| Enc 3 | **NAV-Screen** öffnen / schließen |

---

## Screen-Übersicht (NAV-Raster)

Der NAV-Screen zeigt alle Screens als 4 × 4-Kacheln. Die aktuelle Position ist farbig hervorgehoben, der Encoder-Cursor weiß umrandet.

```
┌──────────┬──────────┬──────────┬──────────┐
│ Circles  │ Perform  │  Pitch   │  CV Cfg  │   ← Global
├──────────┼──────────┼──────────┼──────────┤
│ Ch1 Prm  │ Ch1 Val  │ Ch1 Gate │  Ch1 XY  │   ← Kanal 1
├──────────┼──────────┼──────────┼──────────┤
│ Ch2 Prm  │ Ch2 Val  │ Ch2 Gate │  Ch2 XY  │   ← Kanal 2
├──────────┼──────────┼──────────┼──────────┤
│ Ch3 Prm  │ Ch3 Val  │ Ch3 Gate │  Ch3 XY  │   ← Kanal 3
└──────────┴──────────┴──────────┴──────────┘
```

**Navigation im NAV-Screen:**
- Enc 3 drehen → Cursor verschieben
- Enc 3 drücken → zum markierten Screen wechseln
- Touch auf Kachel → direkter Sprung

---

## Screen 1: Circles (EUCLCIRCS)

**Hauptansicht** — zeigt alle drei Kanäle gleichzeitig als Kreise.

```
┌─────────────────────────────────────────────┐
│  [●]     [●●]     [●●●]    Kreisanzeige     │
│  Ch1      Ch2      Ch3                      │
│                                             │
│  [L:H3]  [L:H3]  [L:H3]  Enc-Parameter    │
│                                             │
│  Rhythmus-Preset-Box      BPM-Anzeige      │
└─────────────────────────────────────────────┘
```

Jeder Kanal zeigt seinen Pattern-Kreis. Aktive Hits sind helle Punkte, der Playhead läuft im Uhrzeigersinn.

### Encoder auf CIRCLES

Jeder Encoder gehört einem Kanal: **Enc 1 → Ch 1, Enc 2 → Ch 2, Enc 3 → Ch 3**

| Aktion | Funktion |
|---|---|
| Drehen | Gewählten Parameter ändern |
| Kurzer Druck | Nächsten Parameter wählen (Zyklus: L → H → R → P → S) |

**Parameter-Anzeige** (kleine Box unter jedem Kreis, z. B. `H3`):

| Kürzel | Parameter | Bereich |
|---|---|---|
| L | Länge (PatLen) | 1 – 32 |
| H | Hits (PatNum) | 1 – PatLen |
| R | Rotation (PatRot) | –(L-1) – +(L-1) |
| P | Wahrscheinlichkeit (PatProb) | 0 – 20 |
| S | Geschwindigkeit (Speed) | ÷4, ÷3, ÷2, ×1, ×2, ×3, ×4 |

**Geschwindigkeits-Index:**

| Index | Faktor | Bedeutung |
|---|---|---|
| –3 | ÷4 | Jeder 4. Haupt-Tick |
| –2 | ÷3 | Jeder 3. Haupt-Tick |
| –1 | ÷2 | Jeder 2. Haupt-Tick |
|  0 | ×1 | Gleich wie Master-Clock |
| +1 | ×2 | Doppelte Geschwindigkeit |
| +2 | ×3 | Dreifache Geschwindigkeit |
| +3 | ×4 | Vierfache Geschwindigkeit |

### Touch auf CIRCLES

- Tippen auf Kreis → **EUCLPARAM**-Screen für diesen Kanal
- Tippen auf Rhythmus-Preset-Box → Preset-Browse-Modus (Kanal 2 macht dies auch)

---

## Screen 2: Performance (PERFORMANCE)

Live-Performance-Ansicht: Mute, Solo, Slot-Verwaltung, Rhythmus-Presets.

```
┌─────────────────────────────────────────────┐
│  Sequence-Leiste (Ch1/Ch2/Ch3 farbig)       │
│                                             │
│  [Mute 1] [Mute 2] [Mute 3]                │
│  [Solo 1] [Solo 2] [Solo 3]                │
│                                             │
│  [Slot 1][Slot 2][Slot 3][Slot 4]...       │
│  [Load]            [Save]  [Slot-Nr]        │
│                                             │
│  [Rhythmus-Preset-Name     ]  [BPM ±]      │
└─────────────────────────────────────────────┘
```

### Encoder auf PERFORMANCE

| Encoder | Drehen | Druck |
|---|---|---|
| Enc 1 | Im Browse-Modus: Slots durchblättern | 1. Druck: Browse-Modus, 2. Druck: Slot laden |
| Enc 2 | Im Browse-Modus: Rhythmus-Presets durchblättern | 1. Druck: Preset-Browse, 2. Druck: Preset laden |
| Enc 3 | — | NAV-Screen öffnen |

### Slot-Verwaltung

- **7 Slots** (EEPROM), angezeigt als nummerierte Boxen
- Belegte Slots: farbige Boxen, freie: grau
- Aktiver Slot: hervorgehoben
- **Quick-Save** (Enc 2 lang): speichert in aktiven Slot; ist keiner aktiv, in ersten freien. Nach dem Speichern rückt der Schreibzeiger automatisch zum nächsten freien Slot — so können mehrere Varianten schnell hintereinander gespeichert werden.
- **Slot laden** zieht alle Pattern, Values, GateLen, Ratchet, Pitch und Speed-Einstellungen.

### Rhythmus-Presets (31 Stück)

Fertige 3-Kanal-Patterns mit voreingestellter Länge, Hits und Rotation:

| Kategorie | Presets |
|---|---|
| Latin / Afro-Cuban | Tresillo, Rumba-Clave, Son Clave, Cinquillo, Habanera, Bossa Nova, Samba, Cascara, Bolero |
| Afrika | Afrobeat, Bembé, Flamenco |
| Jazz / Swing | Jazz 12, Shuffle, Waltz |
| Ungerade Metren | 5/4, 7/8, Aksak 9, Polyrhythm, World, Wonky |
| Electronic | Techno, Straight, Minimal, Funk, Hip-Hop, Reggae, Breakbeat, DnB, Trap, IDM, Dense |

### Touch auf PERFORMANCE

- **Mute-Button** (Kanal 1–3): Kanal stummschalten/aktivieren
- **Solo-Button** (Kanal 1–3): Nur diesen Kanal aktiv — alle anderen gemutet. Nochmal drücken hebt Solo auf.
- **Slot-Box**: Slot auswählen und laden
- **Save-Button**: Quick-Save
- **BPM ±-Buttons**: Tempo anpassen

---

## Screen 3: Kanal-Parameter (EUCLPARAM1 / 2 / 3)

Detailansicht für einen Kanal: alle Pattern-Parameter direkt editierbar.

```
┌─────────────────────────────────────────────┐
│  [Prob-Button] "Probability"                │
│                                             │
│  [L: 16] [H:  5] [R:  0] [P:  0]  Buttons  │
│                                             │
│  Auto-Rotate: [0]          EuclidRebuild[□] │
│  Probability-Auto [□]                       │
│  Speed: [×1]                                │
└─────────────────────────────────────────────┘
```

### Encoder auf EUCLPARAM

| Encoder | Drehen | Kurzer Druck |
|---|---|---|
| Enc (Kanal) | Aktiven Parameter ändern | Nächsten Parameter wählen (L→H→R→P→S) |
| Enc 3 | — | NAV-Screen |
| Enc 2 | — | Quick-Save (lang) |

### Touch auf EUCLPARAM

- **L / H / R / P -Buttons**: Direkt antippen → Parameter aktiv setzen, Encoder ändert ihn
- **Aktiver Button** erscheint rot hervorgehoben
- **Prob-Button** (oben): Neues Zufalls-Pattern generieren
- **Probability-Auto-Checkbox**: Wahrscheinlichkeit bei jedem Zyklus automatisch anwenden
- **EuclidRebuild-Checkbox**: Pattern bei Prob-Änderung neu berechnen statt mutieren
- **Auto-Rotate-Box** (0–4): Pattern nach jedem Zyklus automatisch um N Schritte rotieren
- **Speed-Anzeige**: Klicken → nächste Geschwindigkeit (entspricht Enc-Button-Druck)

---

## Screen 4: Values (VALUES1 / 2 / 3)

Step-Werte-Editor — drei Edit-Modi über Tabs wählbar.

```
┌─────────────────────────────────────────────┐
│  [Values] [Ratchet] [Octave]    Tab-Auswahl │
│                                             │
│  ▐▌▐ ▌▐▌ ▐▌▐▌▐▌▐▌▐▌ ▐▌▐▌▐▌  Balken        │
│  1  2  3  4 ...              (16 od. 32)   │
│                                             │
│  Hold [□]   Rotate [□]                      │
└─────────────────────────────────────────────┘
```

### Modi

| Tab | Parameter | Bereich | Funktion |
|---|---|---|---|
| Values | Values[ch][step] | 0 – 255 | CV-Ausgangsspannung pro Step |
| Ratchet | Ratchet[ch][step] | 1 – 4 | Sub-Hits pro Hit-Step (nur auf Hits) |
| Octave | OctaveNote1[step] | –3 – +3 | Oktav-Offset pro Step (nur Ch 1 / Pitch) |

### Touch auf VALUES

- **Drag** auf Balkenfläche → Step-Wert setzen (proportional zur Höhe)
- **Hold-Checkbox**: CV-Wert bei nicht-aktiven Steps halten (kein Zurückfallen auf 0)
- **Rotate-Checkbox**: Values rotieren gemeinsam mit dem Euklid-Pattern

### Encoder auf VALUES

Encoder des Kanals (z. B. Enc 1 für Ch 1) ist auf diesem Screen deaktiviert; Werte werden per Touch gesetzt.

---

## Screen 5: Gate Length (GATELEN1 / 2 / 3)

Gate-Länge pro Step editieren — selbe Bedienung wie VALUES.

```
┌─────────────────────────────────────────────┐
│  [Gate Length]                              │
│  ▐▌▐ ▌▐▌ ▐▌▐▌▐▌▐▌▐▌ ▐▌▐▌▐▌  Balken        │
│                                             │
│  Hold [□]   Rotate [□]                      │
└─────────────────────────────────────────────┘
```

- **Balken** = relative Gate-Länge des Steps (0 = kürzester Puls, 255 = längster)
- **Hold**: Gate-Länge auf Nicht-Hit-Steps aus dem letzten Hit halten
- **Rotate**: Gate-Längen rotieren gemeinsam mit Pattern
- **Drag** → Länge setzen

---

## Screen 6: XY-Pad (XY1 / XY2 / XY3)

Zweidimensionaler Performance-Recorder. X-Achse = Value (CV-Pegel), Y-Achse = Gate-Länge oder Pitch.

```
┌─────────────────────────────────────────────┐
│ Y │                                         │
│   │  •  • •    •        •                  │
│   │      •       •  •                      │
│   └────────────────────── X                │
│                                             │
│  [GateLen][PV1][PV3][PV5]   Modus-Buttons  │
│  Playhead: Schritt-Nr                       │
└─────────────────────────────────────────────┘
```

### Modus-Buttons (nur XY1 / Kanal 1)

| Button | Y-Achse | Bereich |
|---|---|---|
| GateLen | Gate-Länge | 0 – 255 |
| PV 1 oct | Pitch (1 Oktave) | Noten im gewählten Scale/Root |
| PV 3 oct | Pitch (3 Oktaven) | — |
| PV 5 oct | Pitch (5 Oktaven) | — |

XY2 und XY3 haben nur den GateLen-Modus.

### Keyboard-Hintergrund (PV-Modi)

In den Pitch-Modi erscheint ein Piano-Tastatur-Hintergrund:
- **Hellgrau** = Weißtaste (diatonischer Ton)
- **Dunkelgrau** = Schwarztaste (chromatischer Ton, nicht in Scale)
- **Schwarzer Strich** = Trenner zwischen zwei Tönen
- **Grüner Strich** = Oktavgrenze (Root-Note)

Der Hintergrund passt sich automatisch an Scale, Root und Spread an.

### Touch auf XY-Pad

- **Drag** auf Pad-Fläche → Dot für den aktuellen Playhead-Step setzen (X=Value, Y=Gate/Pitch)
- Die Dots werden live gespeichert und bei jedem Sequencer-Step abgespielt
- Dots können durch Überschreiben aktualisiert werden

---

## Screen 7: Pitch (PITCH1)

Pitch-Sequenz-Editor für Kanal 1. Zeigt eine Balkengrafik mit dem Roh-Pitch pro Step.

```
┌─────────────────────────────────────────────┐
│  Scale: [Major    ] Root: [C] Spread: [3]   │
│  Shift: [+1]                                │
│  Intervalle: [1][3][5][7][9][11][13]        │
│                                             │
│  ▐▌▐ ▌▐▌ ▐▌▐▌▐▌▐▌▐▌ ▐▌▐▌▐▌  Balken        │
│                                             │
│  Hold [□]  Rotate [□]  Fold: [off]         │
│  [Preset: Berlin Arp    ]  DisplayMode [□]  │
└─────────────────────────────────────────────┘
```

### Encoder auf PITCH1

| Encoder | Drehen (normal) | Druck (kurz) | Druck (lang) |
|---|---|---|---|
| Enc 1 | Cursor in Kontroll-Box verschieben (Scale / Root / Spread) | Edit-Mode ein/aus | Gefaltetes Pattern zurückschreiben |
| Enc 2 | Oktav-Transposition (Shift, –3 .. +3) *oder* Preset-Cursor | 1. Druck: Preset-Browse; 2. Druck: Preset laden | — |
| Enc 3 | Intervall-Cursor (0 – 6) verschieben | Intervall am Cursor ein/aus schalten | NAV-Screen |

**Edit-Mode (Enc 1 gedrückt):**
Im Edit-Mode wird Enc 1 zum Wert-Regler:
- Cursor auf **Scale** → Skala durchblättern
- Cursor auf **Root** → Grundton (C, C#, D … H)
- Cursor auf **Spread** → Oktavbereich 1 – 5

### Skalen (25 verfügbar)

| Gruppe | Skalen |
|---|---|
| Westliche Modi | Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harm. Min, Lydian Dom, Altered |
| Pentatonik & Sonderformen | Penta Maj, Penta Min, Blues, Whole Tone, Chromatic |
| Gypsy / Naher Osten | Hungarian, Gypsy Maj, Dbl Harm, Persian |
| Indische Ragas | Raga Todi, Raga Purvi, Raga Marwa |
| Japanisch | Hirajoshi, Insen, Yo |

### Intervall-Filter

Die 7 Intervall-Tasten (1, 3, 5, 7, 9, 11, 13) steuern, welche Skalengrade im Pitch-Raster verfügbar sind. Ausgegraute Tasten existieren in der gewählten Skala nicht (z. B. bei Pentatonik).

### Pitch-Balken und Touch

- **Drag** auf Balkenbereich → Roh-Wert (0 – 255) pro Step setzen
- Der Roh-Wert wird zur Laufzeit auf den nächsten Skalenton quantisiert

### Pitch-Parameter

| Parameter | Bereich | Bedeutung |
|---|---|---|
| Scale | 0 – 24 | Skala-Index |
| Root | C – H (0 – 11) | Grundton / Transposition |
| Spread | 1 – 5 | Oktavbereich der Pitch-Balken |
| Shift | –3 – +3 | Globale Oktav-Transposition des CV-Ausgangs |
| Hold | Ein/Aus | Pitch-CV nur bei Hit aktualisieren |
| Rotate | Ein/Aus | Pitch-Pattern relativ zur Pattern-Rotation |
| Fold | off, mH1, rH1, mQ1, rQ1, mH2, rH2, mQ2, mQ3, mQ4, rQ2, rQ3, rQ4 | Falt-Modus (s. u.) |

### Fold-Modi (13 Modi)

Der Fold-Modus legt fest, wie das Pattern bei der Wiedergabe gespiegelt oder wiederholt wird, ohne die gespeicherten Noten zu verändern.

| Kürzel | Modus | Beschreibung |
|---|---|---|
| off | Aus | Normales Pattern |
| mH1 | Spiegel½ | Zweite Hälfte ist gespiegelte erste Hälfte |
| rH1 | Repeat½ | Zweite Hälfte wiederholt erste Hälfte |
| mQ1 | Spiegel¼ | Zweites Viertel gespiegelt, Rest wiederholt |
| rQ1 | Repeat¼ | Alle Viertel gleich wie erstes |
| mH2 | Spiegel½ (v2) | Variante der Halbspiegel-Faltung |
| rH2..rQ4 | weitere Varianten | Repeat/Spiegel auf ¼-Segmenten |

**Enc 1 langer Druck** auf PITCH1: Das aktuell gespielte (gefaltete) Pattern wird fest ins Original zurückgeschrieben. Damit kann man ein gefaltetes Muster als neuen Ausgangspunkt einfrieren. Funktioniert auch, wenn Fold per CV gesteuert wird.

### Pitch-Presets (Beispiele)

Berlin Arp, Acid 303, Techno Pump, Cascade, Raga Walk, Drone, Pentatone, Minimal, Staircase, Sawtooth, Rev. Saw, Leapfrog, Waltz, Alberti, Jazz Walk, Celtic Arp, …

Gesamt: 16+ vorgefertigte 32-Step-Melodielinien, quantisiert auf aktuelle Scale/Root/Spread.

---

## Screen 8: CV-Konfiguration (CV_CONFIG)

Weist den drei CV-Eingängen Modulationsziele zu.

```
┌─────────────────────────────────────────────┐
│  CV In 1:  [Rot1     ]   ← Tippen = weiter │
│  CV In 2:  [Val2     ]                      │
│  CV In 3:  [---      ]   (= inaktiv)        │
│                                             │
│  [Ext Clock □]                              │
└─────────────────────────────────────────────┘
```

### CV-Ziele (14 Optionen)

| Label | Ziel |
|---|---|
| --- | Kein Ziel (inaktiv) |
| Rat1 / Rat2 / Rat3 | Ratchet-Anzahl für Kanal 1 / 2 / 3 |
| Swing | Swing-Prozentsatz (0 – 50 %) |
| P.Sh | Pitch-Shift Oktave-Offset |
| Rot1 / Rot2 / Rot3 | Pattern-Rotation für Kanal 1 / 2 / 3 |
| Val1 / Val2 / Val3 | Value-Modulation für Kanal 1 / 2 / 3 |
| Slot | Slot-Selektion (0 V = Slot 1, max. V = Slot 7) |
| Fold | Pitch-Fold-Modus (steuert Faltungs-Index) |

**Tippen** auf eine CV-Zeile schaltet das Ziel zyklisch durch alle 14 Optionen. Aktive Ziele erscheinen grün, inaktive dunkelgrau.

### Ext-Clock-Checkbox

Wechselt zwischen internem BPM-Taktgeber und externem Clock-Eingang. Bei aktivem externem Clock ist BPM-Anpassung gesperrt.

---

## Globale Funktionen

### Auto-Save

Alle Parameter werden 400 ms nach der letzten Änderung automatisch im EEPROM gesichert. Kein manuelles Speichern notwendig für den laufenden Betrieb.

### Quick-Save (Enc 2 lang, alle Screens außer PERFORMANCE)

1. **Erster Druck**: Speichert in den aktiven Slot; falls kein Slot aktiv, in den ersten freien.
2. **Folge-Drücke**: Schreibzeiger rückt automatisch zum nächsten freien Slot vor — ideal zum schnellen Archivieren mehrerer Varianten.
3. **Nach Slot-Load**: Schreibzeiger wird zurückgesetzt.

Ein kurzes Toast-Overlay bestätigt den gespeicherten Slot (`Gespeichert → Slot N`). Sind alle 7 Slots belegt, erscheint `Alle Slots belegt!`.

### Mute / Solo (PERFORMANCE)

- **Mute**: Kanal erzeugt keine Gate-Signale und keine CV-Ausgabe. Pattern läuft intern weiter.
- **Solo**: Nur der Solo-Kanal ist aktiv; alle anderen sind implizit gemutet. Mehrere Solos gleichzeitig möglich.

---

## Signalfluss

```
Clock-In ──→ timerISR ──→ pendingTicks
                              │
                         main loop
                         consumePendingTick
                              │
                    ┌─────────┼─────────┐
                    ↓         ↓         ↓
                 Ch 1       Ch 2       Ch 3
              (×Speed)   (×Speed)   (×Speed)
                    │         │         │
               Euklid     Euklid     Euklid
               Pattern    Pattern    Pattern
                    │         │         │
               Gate Out1  Gate Out2  Gate Out3
               CV Out1    CV Out2    CV Out3
               (Pitch)    (Values)   (Values)
```

**Ratchet**: Pro Hit-Step erzeugt der Sequencer 1–4 Sub-Gates mit verkürzter Gate-Länge.  
**Swing**: Gerade Steps werden leicht verzögert (Swing-Prozentsatz per CV steuerbar).

---

## Pitch-CV-Ausgang (Ch 1)

Der Pitch-CV-Ausgang folgt dem 1 V/Oktave-Standard (Basis C2 = 0 V):

```
Roh-Wert (0-255)
     │
     ↓
Quantisierung auf nächsten Skalenton
(abhängig von Scale, Root, Spread, IntervalMask)
     │
     ↓
Oktav-Offset per Step (OctaveNote1, –3..+3)
+ Globaler Shift (pitchShift, –3..+3)
+ CV Pitch-Shift-Offset
     │
     ↓
MIDI-Note → DAC-Wert (MCP4822, 12-bit)
     │
     ↓
TL074 Verstärker (Gain 3,2) → 0–6,55 V am Ausgang
```

---

## Tipps & Workflows

**Euklidischen Rhythmus aufbauen:**
1. CIRCLES-Screen → Enc 1 Druck bis `L` → Länge setzen (z. B. 16)
2. Enc 1 Druck → `H` → Hits setzen (z. B. 5)
3. Enc 1 Druck → `R` → Rotation verschieben für Groove

**Rhythmus-Preset schnell wechseln:**
1. PERFORMANCE-Screen → Enc 2 drücken (Browse-Modus startet)
2. Enc 2 drehen → Preset wählen
3. Enc 2 drücken → laden (alle 3 Kanäle neu)

**Pitch-Linie zeichnen:**
1. PITCH1-Screen → Enc 1 drücken (Edit-Mode) → Skala und Root einstellen
2. Touch auf Balkenfläche → Noten einzeichnen
3. Enc 2 → Oktav-Shift für globale Transposition

**XY-Pad als Echtzeit-Recorder:**
1. XY1-Screen → Modus-Button `PV 3 oct` wählen
2. Sequencer laufen lassen → Finger über Pad ziehen
3. Dots werden pro Step live aufgezeichnet

**Mehrere Varianten archivieren:**
1. Zustand A einrichten → Enc 2 lang → Slot 1 belegt, Zeiger springt auf Slot 2
2. Zustand B einrichten → Enc 2 lang → Slot 2 belegt, Zeiger springt auf Slot 3
3. Im PERFORMANCE-Screen Slots per Touch oder Enc 1 laden

---

*Handbuch generiert für Firmware-Stand: Mai 2026*
