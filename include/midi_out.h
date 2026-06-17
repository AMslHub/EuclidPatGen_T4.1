#pragma once
#include <stdint.h>

void midiOutSetup();              // Serial7.begin(31250) — aus setup() aufrufen
void midiOutTick();               // aus loop() aufrufen: sendet bei chordPlayPos-Wechsel
void midiOutAllNotesOff();        // Panic — alle laufenden Noten beenden
