#include <cv_inputs.h>
#include <hardware_map.h>
#include <app_state.h>
#include <math.h>

uint16_t cvRaw[3]    = {0, 0, 0};
uint16_t cvSmooth[3] = {0, 0, 0};

uint8_t cvTargetMap[3]     = {CV_TARGET_NONE, CV_TARGET_NONE, CV_TARGET_NONE};
uint8_t cvRatchetCount[3]  = {1, 1, 1};
uint8_t cvSwingPct         = 0;
int8_t  cvPitchShiftOffset = 0;
int8_t  cvPatRotOffset[3]  = {0, 0, 0};
int8_t  cvSlotSel          = -1;
int8_t  cvPitchFold        = -1;
int8_t  cvSlotKey          = -1;
int8_t  cvPitchTransposeST = 0;
uint8_t cvPitchIvSteps     = 0;
uint8_t cvPitchAiUpOct     = 0;
uint8_t cvPitchAiDownOct   = 0;
float   cvMorph            = 0.0f;
uint8_t morphChannelMask   = 0b111;  // default: alle Kanäle

float   cvCompress[3]  = {0.0f, 0.0f, 0.0f};  // 0=unkomprimiert, 1=voll auf MW
int16_t cvValOffset[3] = {0, 0, 0};           // -255..+255 Val+/Val- Offset

// Slot-Sel Hysterese: letzter bestätigter Slot (-1 = noch nicht initialisiert)
static int8_t   cvSlotSelHyst      = -1;
static const int CV_SLOT_ZONE      = 4096 / 16;  // ~256 ADC-Schritte pro Slot (0-15)
static const int CV_SLOT_HYST      = 20;          // Hysterese in ADC-Schritten

static const uint8_t CV_PINS[3] = {CV_IN_1_PIN, CV_IN_2_PIN, CV_IN_3_PIN};

static const char* const CV_TARGET_LABELS[CV_TARGET_COUNT] = {
    "---", "Rat1", "Rat2", "Rat3", "Swing", "P.Sh",
    "Rot1", "Rot2", "Rot3", "Cmp1", "Cmp2", "Cmp3", "Slot", "Fold", "1V/O",
    "IV", "AI+", "AI-", "Mrph", "Mrp1", "Mrp2", "Mrp3", "PatK", "CmpA",
    "V1+", "V2+", "V3+", "VA+", "V1-", "V2-", "V3-", "VA-"
};

const char* cvTargetLabel(uint8_t t) {
    if (t >= CV_TARGET_COUNT) return "---";
    return CV_TARGET_LABELS[t];
}

void readCvInputs() {
    for (int i = 0; i < 3; i++) {
        // Invertierung der Schaltung kompensieren: 0V am Eingang → 3.3V am ADC-Pin
        uint16_t raw = 4095u - (uint16_t)analogRead(CV_PINS[i]);
        cvRaw[i]    = raw;
        // IIR-Glättung alpha=1/8 (träge genug für stabile Werte, schnell genug für Live-Modulation)
        cvSmooth[i] = (uint16_t)((cvSmooth[i] * 7u + raw) / 8u);
    }
}

void applyCvTargets() {
    // Rücksetzung auf Defaults
    for (int ch = 0; ch < 3; ch++) {
        cvRatchetCount[ch]  = 1;
        cvPatRotOffset[ch]  = 0;
        cvCompress[ch]      = 0.0f;
    }
    cvValOffset[0] = cvValOffset[1] = cvValOffset[2] = 0;
    cvSwingPct         = 0;
    cvPitchShiftOffset = 0;
    cvSlotSel          = -1;
    cvPitchFold        = -1;
    cvSlotKey          = -1;
    cvPitchTransposeST = 0;
    cvPitchIvSteps     = 0;
    cvPitchAiUpOct     = 0;
    cvPitchAiDownOct   = 0;
    cvMorph            = 0.0f;
    morphChannelMask   = 0;

    for (int ci = 0; ci < 3; ci++) {
        uint8_t  target = cvTargetMap[ci];
        uint16_t cv     = cvSmooth[ci];  // 0–4095

        switch (target) {
            case CV_TARGET_RATCHET_CH1:
            case CV_TARGET_RATCHET_CH2:
            case CV_TARGET_RATCHET_CH3: {
                int ch = target - CV_TARGET_RATCHET_CH1;
                // 0..4095 → 1..4 (gleichmäßig aufgeteilt)
                uint8_t r = (uint8_t)(1u + (cv * 4u) / 4096u);
                cvRatchetCount[ch] = (r > 4) ? 4 : r;
                break;
            }
            case CV_TARGET_SWING:
                // 0..4095 → 0..50%
                cvSwingPct = (uint8_t)((cv * 51u) / 4096u);
                if (cvSwingPct > 50) cvSwingPct = 50;
                break;
            case CV_TARGET_PITCH_SHIFT:
                // 0..4095 → –3..+3 (7 Stufen, Mitte = 0)
                cvPitchShiftOffset = (int8_t)((int)(cv * 7u / 4096u) - 3);
                break;
            case CV_TARGET_PAT_ROT_CH1:
            case CV_TARGET_PAT_ROT_CH2:
            case CV_TARGET_PAT_ROT_CH3: {
                int ch = target - CV_TARGET_PAT_ROT_CH1;
                int maxRot = PatLen[ch] - 1;
                if (maxRot < 1) maxRot = 1;
                // Unipolar: 0 V → Offset 0, Max V → PatLen-1
                int offset = ((int)cv * (maxRot + 1)) / 4096;
                if (offset > maxRot) offset = maxRot;
                cvPatRotOffset[ch] = (int8_t)offset;
                break;
            }
            case CV_TARGET_VALUE_MOD_CH1:
            case CV_TARGET_VALUE_MOD_CH2:
            case CV_TARGET_VALUE_MOD_CH3: {
                int ch = target - CV_TARGET_VALUE_MOD_CH1;
                cvCompress[ch] = (float)cv / 4095.0f;  // 0V=unkomprimiert, max=auf MW
                break;
            }
            case CV_TARGET_SLOT_SEL: {
                // 0-4095 → Slot 0-6, Schmitt-Trigger-Hysterese gegen Zittern
                int rawSlot = (int)cv / CV_SLOT_ZONE;
                if (rawSlot > 15) rawSlot = 15;
                if (cvSlotSelHyst < 0) {
                    cvSlotSelHyst = (int8_t)rawSlot;
                } else if (rawSlot > cvSlotSelHyst) {
                    if ((int)cv >= (cvSlotSelHyst + 1) * CV_SLOT_ZONE + CV_SLOT_HYST)
                        cvSlotSelHyst = (int8_t)rawSlot;
                } else if (rawSlot < cvSlotSelHyst) {
                    if ((int)cv < cvSlotSelHyst * CV_SLOT_ZONE - CV_SLOT_HYST)
                        cvSlotSelHyst = (int8_t)rawSlot;
                }
                cvSlotSel = cvSlotSelHyst;
                break;
            }
            case CV_TARGET_PITCH_FOLD: {
                // 0-4095 → 0-12 (13 Faltungs-Modi)
                int8_t f = (int8_t)((cv * 13u) / 4096u);
                cvPitchFold = (f > 12) ? 12 : f;
                break;
            }
            case CV_TARGET_PITCH_TRANSPOSE: {
                // 1V/Okt: 4095 ADC-Schritte = 6.6V = 79 Halbtöne
                // ~52 ADC-Schritte pro Halbton; Hysterese ±10 Schritte gegen Jitter
                static int8_t hyst = 0;
                int st = (int)((uint32_t)cv * 12u) / 620;
                if (st > 79) st = 79;
                int thresh = (int)hyst * 620 / 12;
                if (st > hyst && (int)cv >= thresh + 620/12 - 10)
                    hyst = (int8_t)st;
                else if (st < hyst && (int)cv < thresh - 10)
                    hyst = (int8_t)st;
                cvPitchTransposeST = hyst;
                break;
            }
            case CV_TARGET_PITCH_IV: {
                // 0..4095 → 0..7 Inversions-Stufen (0 = Grundstellung = kein Effekt)
                uint8_t steps = (uint8_t)((cv * 8u) / 4096u);
                cvPitchIvSteps = steps;
                break;
            }
            case CV_TARGET_PITCH_AI_UP: {
                // 0..4095 → 0..3 Oktaven (0 = keine Änderung)
                uint8_t oct = (uint8_t)((cv * 4u) / 4096u);
                cvPitchAiUpOct = oct;
                break;
            }
            case CV_TARGET_PITCH_AI_DOWN: {
                // 0..4095 → 0..3 Oktaven (0 = keine Änderung)
                uint8_t oct = (uint8_t)((cv * 4u) / 4096u);
                cvPitchAiDownOct = oct;
                break;
            }
            case CV_TARGET_MORPH:
                cvMorph = (float)cv / 4095.0f;
                morphChannelMask |= 0b111;
                break;
            case CV_TARGET_MORPH_CH1:
                cvMorph = (float)cv / 4095.0f;
                morphChannelMask |= 0b001;
                break;
            case CV_TARGET_MORPH_CH2:
                cvMorph = (float)cv / 4095.0f;
                morphChannelMask |= 0b010;
                break;
            case CV_TARGET_MORPH_CH3:
                cvMorph = (float)cv / 4095.0f;
                morphChannelMask |= 0b100;
                break;
            case CV_TARGET_COMPRESS_ALL:
                cvCompress[0] = cvCompress[1] = cvCompress[2] = (float)cv / 4095.0f;
                break;
            case CV_TARGET_SLOT_KEY: {
                // 1V/Oct, C=0V. ADC-Schritte pro Halbton: 4095/6.6V/12 ≈ 51.8
                // 16 White Keys C..D'' → Slot 0-15
                // Semitöne:     0 2 4 5 7 9 11 12 14 16 17 19 21 23 24 26
                static const int8_t WK[16] = {0,2,4,5,7,9,11,12,14,16,17,19,21,23,24,26};
                int st = (int)((uint32_t)cv * 12u + 310u) / 621u;
                int best = 0, bestDist = abs(st - (int)WK[0]);
                for (int k = 1; k < 16; k++) {
                    int d = abs(st - (int)WK[k]);
                    if (d < bestDist) { bestDist = d; best = k; }
                }
                if (bestDist <= 1) cvSlotKey = (int8_t)best;
                break;
            }
            case CV_TARGET_VAL_PLUS_CH1:
                cvValOffset[0] += (int16_t)((cv * 255u) / 4095u);
                break;
            case CV_TARGET_VAL_PLUS_CH2:
                cvValOffset[1] += (int16_t)((cv * 255u) / 4095u);
                break;
            case CV_TARGET_VAL_PLUS_CH3:
                cvValOffset[2] += (int16_t)((cv * 255u) / 4095u);
                break;
            case CV_TARGET_VAL_PLUS_ALL: {
                int16_t off = (int16_t)((cv * 255u) / 4095u);
                cvValOffset[0] += off; cvValOffset[1] += off; cvValOffset[2] += off;
                break;
            }
            case CV_TARGET_VAL_MINUS_CH1:
                cvValOffset[0] -= (int16_t)((cv * 255u) / 4095u);
                break;
            case CV_TARGET_VAL_MINUS_CH2:
                cvValOffset[1] -= (int16_t)((cv * 255u) / 4095u);
                break;
            case CV_TARGET_VAL_MINUS_CH3:
                cvValOffset[2] -= (int16_t)((cv * 255u) / 4095u);
                break;
            case CV_TARGET_VAL_MINUS_ALL: {
                int16_t off = (int16_t)((cv * 255u) / 4095u);
                cvValOffset[0] -= off; cvValOffset[1] -= off; cvValOffset[2] -= off;
                break;
            }
            default:
                break;
        }
    }
}

// Ratchet-Decay-Hüllkurve: globale Dämpfung (ratchetDecay=0 → flach, 255 → max Abfall)
float getValueModFactor(int ch, int ratchetIdx, int ratchetTotal) {
    (void)ch;
    if (ratchetTotal <= 1 || ratchetDecay == 0) return 1.0f;
    float s = ratchetDecay / 255.0f;
    float t = (float)ratchetIdx / (float)(ratchetTotal - 1);
    return 1.0f - s * t;
}
