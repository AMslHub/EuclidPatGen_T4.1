#pragma once

#include <Arduino.h>
#include <IntervalTimer.h>
#include <ILI9341_t3.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// GUI states and touch positions
enum {
  EUCLCIRCS,
  PERFORMANCE,
  EUCLPARAM1,
  EUCLPARAM2,
  EUCLPARAM3,
  VALUES1,
  VALUES2,
  VALUES3,
  GATELEN1,
  GATELEN2,
  GATELEN3,
  XY1,
  XY2,
  XY3
};
enum {UL, UR, LL, LR, CP, P1U, P1L, P2U, P2L, P3U, P3L, P4U, P4L};

// Shared display/touch objects
extern XPT2046_Touchscreen ts;
extern ILI9341_t3 tft;

// Shared constants
extern const int R1;
extern const int R2;
extern const int R3;
extern const int r2;
extern const int r3;
extern const int CX;
extern const int CY;

// Shared GUI variables
extern uint16_t GUIState;
extern uint16_t tipPos;
extern bool stillPressed;

// Pattern parameters
extern int PatLen[3];
extern int PatNum[3];
extern int PatRot[3];
extern uint8_t PatProb[3];
extern bool PatProbAuto[3];
extern bool ProbEuclidRebuild[3];

// Pattern arrays
extern bool EPat1[32];
extern bool EPat2[32];
extern bool EPat3[32];
extern bool *EPatArr[3];
extern bool EPatB1[32];
extern bool EPatB2[32];
extern bool EPatB3[32];
extern bool *EPatBArr[3];

// Values arrays
extern uint8_t Values1[32];
extern uint8_t Values2[32];
extern uint8_t Values3[32];
extern uint8_t *ValuesArr[3];
extern uint8_t GateLen1[32];
extern uint8_t GateLen2[32];
extern uint8_t GateLen3[32];
extern uint8_t *GateLenArr[3];
extern bool GateHold1;
extern bool GateHold2;
extern bool GateHold3;
extern bool *GateHoldArr[3];
extern bool Hold1;
extern bool Hold2;
extern bool Hold3;
extern bool *HoldArr[3];
extern bool RotateValues[3];
extern bool RotateGateLen[3];

// Performance (Mute/Solo)
extern bool MuteSeq[3];
extern bool SoloSeq[3];

// Deferred save
extern bool PendingSave;
extern uint32_t PendingSaveAt;
extern const uint32_t SAVE_DEBOUNCE_MS;

extern uint32_t DurationOfOneStep;
extern uint16_t bpm;

// Timing
extern unsigned int cnthold;
extern unsigned int cnt;
extern volatile uint32_t pendingTicks;  // ISR-shared: volatile korrekt

// Gate handling
extern const uint32_t GATE_PULSE_US;
extern const uint8_t GatePins[3];
extern volatile uint32_t gateOffAt[3];

inline int clampVal(int v, int lo, int hi){
  if(v < lo) return lo;
  if(v > hi) return hi;
  return v;
}
