# Contributing

## Kommentar-Guideline

Ziel: Kommentare sollen den Code leichter wartbar machen, nicht ersetzen. Schreibe Kommentare nur dort, wo der Code nicht selbsterklaerend ist oder wo wichtige Randbedingungen gelten.

Wichtig: Kommentare sind auf Deutsch zu schreiben.

### Wo Kommentare sinnvoll sind
- Zeitkritische Pfade (ISR, Audio/Timing, Gate-Trigger) mit kurzen Hinweisen zu Nebenwirkungen oder Blockierverboten.
- Hardwareabhaengige Werte (Pins, Kalibrierung, Timing-Konstanten) mit Begruendung.
- Persistente Datenformate (EEPROM-Layout, Magic Values).
- Nicht-offensichtliche Algorithmen oder mathematische Schritte.

### Wo Kommentare entfallen sollten
- Selbsterklaerende Setter/Getters oder einfache Flusslogik.
- Wiederholungen dessen, was der Code ohnehin sagt.
- Lange Auflistungen von aufgerufenen Funktionen.

### Funktions-Header (Pflicht fuer alle nicht-trivialen Funktionen)
Jede nicht-triviale Funktion bekommt einen kurzen Header direkt ueber der Funktion mit genau diesen drei Punkten:
- Zweck in 1 Zeile.
- Side Effects (Nebenwirkungen).
- Assumptions (Annahmen/Invarianten).

Beispiel:
```cpp
// Zweck: Aktualisiert den Playhead und triggert Ausgaenge fuer den aktuellen Step.
// Side Effects: schreibt auf TFT, DAC/PWM und Gate-Pins.
// Assumptions: PatLen[ch] in 1..32, DurationOfOneStep > 0.
```

### Dateikopf (optional, aber hilfreich)
Am Anfang einer Datei 1-3 Zeilen, was das Modul leistet:
```cpp
// UI rendering and touch handling for pattern parameter screens.
```

### Stilregeln
- Kurz und praezise (ein Kommentar soll in Sekunden lesbar sein).
- Keine ASCII-Kaesten oder Banner, die viel Platz verbrauchen.
- Kommentare aktuell halten; veraltete Kommentare sind schlimmer als keine.

## Umgang mit Aenderungen
- Wenn eine Aenderung eine Annahme verletzt, aktualisiere den Kommentar.
- Wenn Logik selbsterklaerend geworden ist, Kommentar entfernen.
