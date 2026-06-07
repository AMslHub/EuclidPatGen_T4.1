#pragma once

#include <Arduino.h>
#include <IntervalTimer.h>
#include <ILI9341_t3n.h>
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
  XY3,
  PITCH1,
  CV_CONFIG,
  GCONFIG,
  SONG,
  NAV,
  COND1,
  COND2,
  COND3
};

enum CondType : uint8_t {
    COND_NONE = 0,
    COND_ODD,    // odd cycle  (1, 3, 5, …)
    COND_EVEN,   // even cycle (2, 4, 6, …)
    COND_MOD3,   // cycle % 3 == 0
    COND_MOD4,   // cycle % 4 == 0
    COND_P25,    // 25% random
    COND_P50,    // 50% random
    COND_P75,    // 75% random
    COND_TYPE_COUNT
};

enum CondAction : uint8_t {
    COND_ACT_NONE = 0,
    COND_ACT_MUTE,
    COND_ACT_ACCENT,
    COND_ACT_GATE_S,      // gate short  (64)
    COND_ACT_GATE_M,      // gate medium (128)
    COND_ACT_GATE_L,      // gate long   (192)
    COND_ACT_GATE_TIE,    // gate tie    (255)
    COND_ACT_R2,          // ratchet 2
    COND_ACT_R3,          // ratchet 3
    COND_ACT_R4,          // ratchet 4
    COND_ACT_T_PLUS_1,    // transpose +1 semitone
    COND_ACT_T_PLUS_12  = COND_ACT_T_PLUS_1 + 11,
    COND_ACT_T_MINUS_1,   // transpose -1 semitone (= 22)
    COND_ACT_T_MINUS_12 = COND_ACT_T_MINUS_1 + 11,
    COND_ACT_VAL_X2,      // value ×2, capped 255       (= 34)
    COND_ACT_VAL_X1_5,    // value ×1.5, capped 255     (= 35)
    COND_ACT_VAL_X1_33,   // value ×4/3, capped 255     (= 36)
    COND_ACT_VAL_X1_25,   // value ×5/4, capped 255     (= 37)
    COND_ACT_T_PLUS_24,   // transpose +24 semitones    (= 38)
    COND_ACT_T_MINUS_24,  // transpose -24 semitones    (= 39)
    COND_ACT_SC_PLUS_1,   // +1 scale step              (= 40)
    COND_ACT_SC_PLUS_4  = COND_ACT_SC_PLUS_1 + 3,
    COND_ACT_SC_MINUS_1,  // -1 scale step              (= 44)
    COND_ACT_SC_MINUS_4 = COND_ACT_SC_MINUS_1 + 3,
    COND_ACT_COUNT        // = 48
};
enum {UL, UR, LL, LR, CP, P1U, P1L, P2U, P2L, P3U, P3L, P4U, P4L};

// Shared display/touch objects
extern XPT2046_Touchscreen ts;
extern ILI9341_t3n tft;

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
// Aufgeschobener Screen-Wechsel: 0xFFFF = kein ausstehender Wechsel.
// Wird von requestNavigateTo() gesetzt; Main-Loop führt den Draw aus wenn pendingTicks==0.
extern uint16_t pendingNavTarget;
// Zwei-Phasen-Draw: Phase 1 = fillScreen, Phase 2 = Inhalte. 0xFFFF = keine Phase 2 ausstehend.
extern uint16_t pendingNavPhase2;

// Pattern parameters
extern int PatLen[3];
extern int PatNum[3];
extern int PatRot[3];    // Global Rot (R): verschiebt alles gleichmäßig
extern int PatRotSel[3]; // Selective Rot (r): verschiebt nur Checkbox-markierte Schichten
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
// A/B Morph — B-Layer für Values und GateLen
extern uint8_t ValuesB1[32], ValuesB2[32], ValuesB3[32];
extern uint8_t *ValuesBArr[3];
extern uint8_t GateLenB1[32], GateLenB2[32], GateLenB3[32];
extern uint8_t *GateLenBArr[3];
extern bool    abEditMode;   // false=A editieren, true=B editieren
extern uint8_t Ratchet1[32];
extern uint8_t Ratchet2[32];
extern uint8_t Ratchet3[32];
extern uint8_t *RatchetArr[3];
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
extern bool RotateRatchet[3];
extern bool RotateOctave[3];
extern bool RotateCond[3];

// Per-Step Hold: Gate bleibt bis zum nächsten (nicht gemuteten) Hit HIGH (Legato/Tie)
extern uint8_t HoldStep1[32];
extern uint8_t HoldStep2[32];
extern uint8_t HoldStep3[32];
extern uint8_t *HoldStepArr[3];
extern bool RotateHoldStep[3];

// Per-Step Mute: unterdrückt Gate an dieser Position unabhängig vom euklid. Pattern
extern uint8_t MuteStep1[32];
extern uint8_t MuteStep2[32];
extern uint8_t MuteStep3[32];
extern uint8_t *MuteStepArr[3];
extern bool RotateMuteStep[3];

// Performance (Mute/Solo)
extern bool MuteSeq[3];
extern bool SoloSeq[3];

// Deferred save
extern bool PendingSave;
extern uint32_t PendingSaveAt;
extern const uint32_t SAVE_DEBOUNCE_MS;
extern uint8_t autosaveMode;  // 0=Aus (Live), 1=Ständig, 2=Bei Clock-Stop
extern int pendingSlotSaveSlot;  // >=0: Slot-Save aufgeschoben (Encoder setzt, Main-Loop schreibt)
extern int     pendingSlotMoveFrom;   // >=0: P&P-Merge aufgeschoben (Quell-Slot)
extern int     pendingSlotMoveTo;     // Ziel-Slot des aufgeschobenen Merge
extern uint8_t pendingSlotCopyMask;  // Kanal-Maske: bit i=1 → Kanal i von Quelle kopieren
extern int pendingSongOp;        // 0=none,1=save,2=load,3=delete; Main-Loop führt aus
extern int pendingSongNum;       // Song-Nummer für pendingSongOp
extern int activeSongNum;        // zuletzt gespeicherter/geladener Song (nicht persistiert)

extern uint32_t DurationOfOneStep;
extern uint16_t bpm;

// Encoder-Parameter-Auswahl pro Kanal (0=PatLen, 1=PatNum, 2=PatRot, 3=PatProb, 4=Speed)
extern uint8_t encParamSel[3];
// Ausstehender Circle-Redraw fuer jeden Kanal (erst am Pattern-Ende anwenden)
extern bool pendingCircleRedraw[3];
// Ausstehende Prob-Neugenerierung per Encoder (erst am Pattern-Ende anwenden)
extern bool pendingProbRegen[3];
// Aktuell angezeigte Kreislaenge — muss nach jedem Kreis-Redraw mit PatLen synced werden
extern int displayedPatLen[3];
// Ausstehender Vollbild-Redraw nach Slot-Load
extern bool PendingCircsRedraw;
// Ausstehender Pitch-Screen-Redraw (controls + bars); erst nach Tick-Schleife
extern bool pendingPitchDraw;

// Per-Kanal Geschwindigkeit: -3=÷4, -2=÷3, -1=÷2, 0=×1, 1=×2, 2=×3, 3=×4
extern int chSpeedIdx[3];
// Per-Kanal Schrittzaehler (wird mit der kanalspezifischen Geschwindigkeit inkrementiert)
extern unsigned int cntCh[3];
// Auto-Rotate: Schritte pro Zyklus (0=aus, 1-4=aktiv)
extern uint8_t autoRotateStep[3];

// Pitch (nur Kanal 1 / Hardware-CV-Ausgang Pitch)
extern uint8_t PitchNote1[32];   // Rohwert 0-255 pro Step
extern int8_t  OctaveNote1[32];  // Oktav-Offset pro Step: -3..+3 (nur Kanal 1)
extern uint8_t IvStep1[32];      // Per-Step Inversions-Index 0..7 (nur Kanal 1)
extern bool    RotateIvStep;     // true: IvStep1 relativ zur Rotation
extern uint8_t pitchSpread;      // Oktavbereich 1-5
extern uint8_t pitchScale;       // Skalenindex (0..SCALE_COUNT-1)
extern uint8_t pitchRoot;        // Grundton 0=C .. 11=H
extern uint8_t pitchIntervalMask; // Bitfeld: bit0="1",bit1="3",bit2="5",bit3="7",bit4="9",bit5="11",bit6="13"
extern int8_t  pitchShift;        // Oktavtransposition: -3..+3
extern bool    pitchHold;         // true: Pitch-CV nur bei Hit aktualisieren
extern bool    pitchRotate;       // true: Pitch-Pattern relativ zur Rotation
extern uint8_t pitchFoldMode;    // 0=off, 1=Spiegel½, 2=Repeat½, 3=Spiegel¼, 4=Repeat¼

// Song Sequencer
extern uint8_t songSeq[64];      // Slot-Indices 0-15, max 64 Einträge
extern uint8_t songLen;          // Anzahl gültiger Einträge (0 = leer)
extern bool    songPlaying;      // true = Song läuft gerade
extern bool    songHalted;       // true = Song angehalten (Ticks blockiert, Zähler auf 0)
extern bool    songLoop;         // true = Song wiederholt sich endlos (nicht gespeichert)
extern bool    pendingSongHalt;  // true = STOP-Anforderung (Haupt-Loop führt Zähler-Reset aus)
extern uint8_t songPos;          // nächste Position zum Laden (Lookahead)
extern uint8_t songLoadedPos;    // aktuell spielende Position (Anzeige)

// Globale Ratchet-Dämpfung: 0=flat, 255=max Decay (letzter Hit leise)
extern uint8_t ratchetDecay;

// Conditional Actions — per-step conditions + actions
extern uint8_t condType1[32], condType2[32], condType3[32];
extern uint8_t *condTypeArr[3];
extern uint8_t condAction1[32], condAction2[32], condAction3[32];
extern uint8_t *condActionArr[3];
extern uint32_t cycleCount[3];    // 1-based cycle counter, resets on slot-load/reset
// Delivery vars: reset each tick, read by gates.cpp and gate trigger
extern bool    condMuteActive[3];
extern bool    condAccentActive[3];
extern int8_t  condTransposeAdd[3];
extern int8_t  condScaleStepAdd[3]; // +/-1..4 scale steps (0=inactive)
extern uint8_t condRatchetOvr[3];   // 0=no override, 2..4=ratchet count
extern uint8_t condGateLenOvr[3];   // 0=no override, else gate-len value
extern uint8_t condValueMul[3];     // 0=none, 1=×2, 2=×3/2, 3=×4/3, 4=×5/4

// Clock-Modus: false=intern (IntervalTimer), true=extern (Clock-In-Pin)
extern volatile bool extClockMode;
// Wechselt den Clock-Modus und startet/stoppt den internen Timer entsprechend.
void setExtClockMode(bool v);

// Pin 7 Funktion: 0=Reset-Puls (FALLING ISR), 1=Run/Stop-Pegel (HIGH=stopp, LOW=läuft)
extern uint8_t       pin7Mode;
extern volatile bool extRunStop;  // true=läuft; nur in Mode 1 aktiv
void applyPin7Mode();             // ISR attach/detach je nach Mode; aus setup() und UI aufrufen
void triggerManualReset();        // setzt pendingReset=true; für UI-Touch-Handler

// Song-Sequencer: Playback sofort stoppen + alle Lookahead-Flags löschen.
// Wird beim Navigieren zum Song-Screen aufgerufen.
void resetSongPlayback();

// Timing
extern unsigned int cnthold;
extern unsigned int cnt;
extern volatile uint32_t pendingTicks;  // ISR-shared: volatile korrekt

// Gate handling
extern const uint32_t GATE_PULSE_US;
extern const uint8_t GatePins[3];
extern volatile uint32_t gateOffAt[3];
// Hold-Gate: wenn true bleibt Gate HIGH bis zum nächsten nicht-gemuteten Hit (Legato)
extern volatile bool gateIsHeld[3];
// Pre-Arm Hold: ISR liest diesen Wert und entscheidet über Hold beim nächsten Gate
extern volatile bool nextGateIsHold[3];

inline int clampVal(int v, int lo, int hi){
  if(v < lo) return lo;
  if(v > hi) return hi;
  return v;
}

// Prüft ob Kanal ch aktiv ist (Mute/Solo-Logik).
inline bool isSeqActive(int ch) {
  bool anySolo = SoloSeq[0] || SoloSeq[1] || SoloSeq[2];
  if (anySolo) return SoloSeq[ch] && !MuteSeq[ch];
  return !MuteSeq[ch];
}

// Verwirft akkumulierte Ticks atomar und löscht den ISR-Gate-Fired-Flag.
// Überall verwenden wo bisher noInterrupts(); pendingTicks=0; interrupts() stand,
// damit kein gateWasFiredByISR-Überläufer in den nächsten Tick durchsickert.
void discardPendingTicks();
