#pragma once

// ---------------------------------------------------------------------------
// UART-Protokoll: Teensy 4.1 → Raspberry Pi Pico → MIDI-DIN OUT
//
// Der Teensy sendet rohes Standard-MIDI auf Serial7 (TX = Pin 29).
// Der Pico empfängt auf UART0 (GP0/GP1) und leitet direkt an MIDI-DIN weiter.
// Kein proprietäres Framing notwendig — Standard-MIDI-Bytestream.
//
// Baudrate: 31250 Baud (MIDI-Standard)
//
// MIDI-Kanäle (1-basiert):
//   CH1  — Melodie  (Pitch-Sequenzer, Gate Ch0)
//   CH2  — Akkord   (Chord-Sequenzer, polyphon bis 7 Noten)
//   CH9  — Rhythmus Kanal 1 (Gate Ch1, Note C3 = MIDI 48)
//   CH10 — Rhythmus Kanal 2 (Gate Ch2, Note C3 = MIDI 48)
//
// Zukünftig (MIDI-IN, Pico → Teensy):
//   Pico empfängt MIDI von externem Gerät auf UART1 und leitet
//   an Teensy via UART0 RX weiter (noch nicht implementiert).
// ---------------------------------------------------------------------------

#define MIDI_BAUD           31250

#define MIDI_CH_MELODY      1
#define MIDI_CH_CHORD       2
#define MIDI_CH_RHYTHM_1    9
#define MIDI_CH_RHYTHM_2    10
