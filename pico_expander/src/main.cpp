#include <Arduino.h>
#include "protocol.h"

// ---------------------------------------------------------------------------
// Pico MIDI-Expander
//
// Hardware:
//   UART0 RX (GP1) ← Teensy Serial7 TX (Pin 29): MIDI-IN vom Teensy
//   UART0 TX (GP0) → 6N138-Optokoppler → MIDI-DIN OUT (5-pol DIN)
//
// Der Pico empfängt MIDI-Bytes vom Teensy und leitet sie transparent
// an den MIDI-DIN-Ausgang weiter.
// ---------------------------------------------------------------------------

void setup() {
    Serial1.begin(MIDI_BAUD);   // UART0: GP0=TX, GP1=RX — MIDI-Bytestream vom Teensy
}

void loop() {
    while (Serial1.available()) {
        Serial1.write(Serial1.read());
    }
}
