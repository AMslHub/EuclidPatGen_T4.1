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
    CV_TARGET_PITCH_FOLD,   // 0-4095 → pitchFoldMode 0-4 (off/Mir½/Rep½/Mir¼/Rep¼)
    CV_TARGET_COUNT
};

extern uint8_t  cvTargetMap[3];  // CvTarget per CV-Eingang
extern uint16_t cvSmooth[3];     // IIR-geglättete ADC-Werte (0–4095)
extern uint16_t cvRaw[3];        // Invertierte Rohwerte (0–4095 = 0V–3.3V)

// Abgeleitete Werte – werden von applyCvTargets() aktualisiert
extern uint8_t cvRatchetCount[3];    // 1–4 Sub-Hits pro Hit-Step, pro Sequencer-Kanal
extern uint8_t cvSwingPct;           // 0–50 (% von DurationOfOneStep)
extern int8_t  cvPitchShiftOffset;   // –3..+3 Oktaven (addiert zu pitchShift)
extern int8_t  cvPatRotOffset[3];    // Offset auf PatRot pro Kanal
extern int8_t  cvSlotSel;            // -1=inaktiv, 0-6=Slot (CV_TARGET_SLOT_SEL)
extern int8_t  cvPitchFold;          // -1=inaktiv, 0-4=pitchFoldMode (CV_TARGET_PITCH_FOLD)

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
