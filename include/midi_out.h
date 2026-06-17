#pragma once
#include <stdint.h>

void midiOutSetup();                            // Serial7.begin(31250) — aus setup() aufrufen
void midiOutTick();                             // Chord-MIDI bei chordPlayPos-Wechsel
void midiOutAllNotesOff();                      // Panic — alle laufenden Noten beenden

// Melodie CH1: Note-On bei Gate-Hit (mit fertig berechnetem MIDI-Ton aus gates.cpp)
void midiOutMelodyHit(int midiNote, uint8_t velocity);
void midiOutMelodyOff();                        // Note-Off für letzte Melodie-Note

// Rhythmus CH9/CH10: Gate-Hit / Gate-Off für Ch1 und Ch2
void midiOutRhythmHit(int ch);                  // ch=1 → MIDI CH9, ch=2 → MIDI CH10
void midiOutRhythmOff(int ch);
