#pragma once
#include <Arduino.h>

// Anzahl verfuegbarer Skalen
extern const int SCALE_COUNT;

// Anzahl verfuegbarer Pitch-Presets
extern const int PITCH_PRESET_COUNT;

// Name des Presets idx
const char *getPitchPresetName(int idx);

// Kopiert 32 Raw-Werte (0-255) des Presets idx nach dest32
void getPitchPresetNotes(int idx, uint8_t *dest32);

// Name der Skala scaleIdx
const char *getScaleName(uint8_t scaleIdx);

// Anzahl Grad im Scale (1-7)
uint8_t getScaleDegreeCount(uint8_t scaleIdx);

// Baut die sortierte Liste aktiver MIDI-Noten auf.
// notes: Array mit mind. 35 Eintraegen (7 Grade x 5 Oktaven).
// Rueckgabe: Anzahl Noten.
int buildNoteList(uint8_t spread, uint8_t scaleIdx, uint8_t root,
                  uint8_t intervalMask, int *notes);

// Rohdaten (0-255) -> quantisierter MIDI-Ton
int quantizeToMidi(uint8_t rawValue, uint8_t spread, uint8_t scaleIdx,
                   uint8_t root, uint8_t intervalMask);

// MIDI-Ton -> 12-Bit-DAC-Wert (0-4095) fuer MCP4822 (1V/Oct, Base C2=0V)
uint16_t midiToDac(int midiNote);

// Rohdaten -> DAC-Wert (quantizeToMidi + midiToDac kombiniert)
uint16_t computePitchDac(uint8_t rawValue, uint8_t spread, uint8_t scaleIdx,
                          uint8_t root, uint8_t intervalMask);

// Intervalltaste i (0-6) -> Label "1","3","5","7","9","11","13"
const char *getIntervalLabel(int i);

// Gibt an, ob Intervalltaste i (0-6) einen gueltigen Skalengrad in scaleIdx hat.
// Greyed-out-Logik: Skalen mit < 7 Toenen haben keine Entsprechung fuer alle Tasten.
bool intervalExists(uint8_t scaleIdx, int i);

// Bildet Spielposition idx (0..len-1) per Faltungs-Modus auf effektive Quellposition ab.
// foldMode: 0=off, 1=mH1, 2=rH1, 3=mQ1, 4=rQ1,
//           5=mH2, 6=rH2, 7=mQ2, 8=mQ3, 9=mQ4, 10=rQ2, 11=rQ3, 12=rQ4
int foldPitchIdx(int idx, int len, uint8_t foldMode);
