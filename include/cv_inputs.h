#pragma once

#include <Arduino.h>

// CV-Eingang Rohwerte (12-bit ADC, bereits invertiert: 0 = 6.6V, 4095 = 0V)
// Schaltung: Invertierender Summierverstärker MCP6002, R1=100k, R2=47k, R_bias=170k@-12V
// Software-Inversion: raw = 4095 - analogRead(pin)
extern uint16_t cvRaw[3];

// Liest alle 3 CV-Eingänge und speichert invertierte Rohwerte in cvRaw[].
// Aufruf: einmal pro Main-Loop-Iteration (nicht im Tick-Handler).
void readCvInputs();
