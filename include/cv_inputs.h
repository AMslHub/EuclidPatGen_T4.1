#pragma once

#include <Arduino.h>

enum CvTarget : uint8_t {
    CV_TARGET_NONE = 0,
    CV_TARGET_RATCHET_CH1,
    CV_TARGET_RATCHET_CH2,
    CV_TARGET_RATCHET_CH3,
    CV_TARGET_SWING,
    CV_TARGET_PITCH_SHIFT,
    CV_TARGET_PAT_ROT_CH1,
    CV_TARGET_PAT_ROT_CH2,
    CV_TARGET_PAT_ROT_CH3,
    CV_TARGET_VALUE_MOD_CH1,
    CV_TARGET_VALUE_MOD_CH2,
    CV_TARGET_VALUE_MOD_CH3,
    CV_TARGET_SLOT_SEL,
    CV_TARGET_PITCH_FOLD,       // 0-4095 → pitchFoldMode 0-4 (off/Mir½/Rep½/Mir¼/Rep¼)
    CV_TARGET_PITCH_TRANSPOSE,  // 1V/Okt → Halbtontransposition addiert nach Quantisierung
    CV_TARGET_PITCH_IV,         // 0-4095 → 0..7 Inversions-Stufen on-the-fly
    CV_TARGET_PITCH_AI_UP,      // 0-4095 → 0..3 Okt: tiefste Ebene anheben on-the-fly
    CV_TARGET_PITCH_AI_DOWN,    // 0-4095 → 0..3 Okt: höchste Ebene absenken on-the-fly
    CV_TARGET_MORPH,            // 0-4095 → 0.0..1.0 (Values + GateLen A→B Morph, alle Kanäle)
    CV_TARGET_MORPH_CH1,        // Morph nur Kanal 1
    CV_TARGET_MORPH_CH2,        // Morph nur Kanal 2
    CV_TARGET_MORPH_CH3,        // Morph nur Kanal 3
    CV_TARGET_SLOT_KEY,         // 1V/Okt White-Keys C..D'' → Slot 0-15 am Pattern-Ende laden
    CV_TARGET_COMPRESS_ALL,    // 0-4095 → 0.0..1.0 Kompression aller 3 Kanäle gleichzeitig
    CV_TARGET_VAL_PLUS_CH1,    // 0-4095 → +0..+255 additiver Offset auf Values-Output Ch1
    CV_TARGET_VAL_PLUS_CH2,    // 0-4095 → +0..+255 additiver Offset auf Values-Output Ch2
    CV_TARGET_VAL_PLUS_CH3,    // 0-4095 → +0..+255 additiver Offset auf Values-Output Ch3
    CV_TARGET_VAL_PLUS_ALL,    // 0-4095 → +0..+255 additiver Offset auf Values-Output alle Kanäle
    CV_TARGET_VAL_MINUS_CH1,   // 0-4095 → -0..-255 subtraktiver Offset auf Values-Output Ch1
    CV_TARGET_VAL_MINUS_CH2,   // 0-4095 → -0..-255 subtraktiver Offset auf Values-Output Ch2
    CV_TARGET_VAL_MINUS_CH3,   // 0-4095 → -0..-255 subtraktiver Offset auf Values-Output Ch3
    CV_TARGET_VAL_MINUS_ALL,   // 0-4095 → -0..-255 subtraktiver Offset auf Values-Output alle Kanäle
    CV_TARGET_COUNT
};

extern uint8_t  cvTargetMap[3];  // CvTarget per CV-Eingang
extern uint16_t cvSmooth[3];     // IIR-geglättete ADC-Werte alpha=1/8 (0–4095)
extern uint16_t cvSlow[3];       // IIR-geglättete ADC-Werte alpha=1/64, für Slot-Erkennung
extern uint16_t cvRaw[3];        // Invertierte Rohwerte (0–4095 = 0V–3.3V)

// Abgeleitete Werte – werden von applyCvTargets() aktualisiert
extern uint8_t cvRatchetCount[3];    // 1–4 Sub-Hits pro Hit-Step, pro Sequencer-Kanal
extern uint8_t cvSwingPct;           // 0–50 (% von DurationOfOneStep)
extern int8_t  cvPitchShiftOffset;   // –3..+3 Oktaven (addiert zu pitchShift)
extern int8_t  cvPatRotOffset[3];    // Offset auf PatRot pro Kanal
extern int8_t  cvSlotSel;            // -1=inaktiv, 0-6=Slot (CV_TARGET_SLOT_SEL)
extern int8_t  cvPitchFold;          // -1=inaktiv, 0-4=pitchFoldMode (CV_TARGET_PITCH_FOLD)
extern int8_t  cvPitchTransposeST;  // 0-79 Halbtöne (1V/Okt; 0V=keine Transposition)
extern uint8_t cvPitchIvSteps;      // 0=inaktiv, 1..7: wie viele untere Stufen +1 Okt (on-the-fly)
extern uint8_t cvPitchAiUpOct;      // 0=inaktiv, 1..3: tiefste Ebene N Okt anheben (on-the-fly)
extern uint8_t cvPitchAiDownOct;    // 0=inaktiv, 1..3: höchste Ebene N Okt absenken (on-the-fly)
extern float   cvMorph;             // 0.0..1.0 (Values + GateLen A→B Interpolation)
extern uint8_t morphChannelMask;    // Bit 0=Ch1, Bit 1=Ch2, Bit 2=Ch3 — welche Kanäle morphen
extern int8_t  cvSlotKey;           // -1=inaktiv, 0-15=Slot-Index (CV_TARGET_SLOT_KEY)
extern float   cvCompress[3];       // 0.0=unkomprimiert, 1.0=alle Values auf Mittelwert
extern int16_t cvValOffset[3];      // -255..+255 additiver Offset auf Values-Output (Val+/Val-)

// Exponentieller Lautstärke-Faktor innerhalb eines Ratchet-Bursts.
// ratchetIdx=0 → erster Hit, ratchetTotal = Gesamtzahl der Hits im Burst.
// Gibt 1.0 zurück wenn kein CV auf VALUE_MOD_CHx gemappt oder ratchetTotal<=1.
float getValueModFactor(int ch, int ratchetIdx, int ratchetTotal);

// Liest alle 3 CV-Eingänge, invertiert (Schaltung = invertierender Summierverstärker)
// und aktualisiert cvRaw + cvSmooth. Aufruf: einmal pro Main-Loop-Iteration.
void readCvInputs();

// Mappt cvSmooth[] auf die konfigurierten Zielparameter.
// Aktualisiert cvRatchetCount, cvSwingPct, cvPitchShiftOffset, cvPatRotOffset.
// Aufruf: nach readCvInputs(), einmal pro Main-Loop-Iteration.
void applyCvTargets();

// Gibt den Kurznamen des CvTarget zurück (max. 5 Zeichen).
const char* cvTargetLabel(uint8_t target);
