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

// Index des CV-Eingangs der VALUE_MOD für Kanal ch steuert (-1 = inaktiv)
static int8_t   cvValueModCvIdx[3] = {-1, -1, -1};
// Geglätteter CV-Wert für Value-Modulation
static uint16_t cvValueModRaw[3]   = {0, 0, 0};

// Slot-Sel Hysterese: letzter bestätigter Slot (-1 = noch nicht initialisiert)
static int8_t   cvSlotSelHyst      = -1;
static const int CV_SLOT_ZONE      = 4096 / 7;   // ~585 ADC-Schritte pro Slot
static const int CV_SLOT_HYST      = 40;          // Hysterese in ADC-Schritten

static const uint8_t CV_PINS[3] = {CV_IN_1_PIN, CV_IN_2_PIN, CV_IN_3_PIN};

static const char* const CV_TARGET_LABELS[CV_TARGET_COUNT] = {
    "---", "Rat1", "Rat2", "Rat3", "Swing", "P.Sh",
    "Rot1", "Rot2", "Rot3", "Val1", "Val2", "Val3", "Slot", "Fold"
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
        cvValueModCvIdx[ch] = -1;
    }
    cvSwingPct         = 0;
    cvPitchShiftOffset = 0;
    cvSlotSel          = -1;
    cvPitchFold        = -1;

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
                int halfLen = PatLen[ch] / 2;
                if (halfLen < 1) halfLen = 1;
                int range = halfLen * 2 + 1;
                cvPatRotOffset[ch] = (int8_t)(((int)cv * range) / 4096 - halfLen);
                break;
            }
            case CV_TARGET_VALUE_MOD_CH1:
            case CV_TARGET_VALUE_MOD_CH2:
            case CV_TARGET_VALUE_MOD_CH3: {
                int ch = target - CV_TARGET_VALUE_MOD_CH1;
                cvValueModCvIdx[ch] = (int8_t)ci;
                cvValueModRaw[ch]   = cv;
                break;
            }
            case CV_TARGET_SLOT_SEL: {
                // 0-4095 → Slot 0-6, Schmitt-Trigger-Hysterese gegen Zittern
                int rawSlot = (int)cv / CV_SLOT_ZONE;
                if (rawSlot > 6) rawSlot = 6;
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
            default:
                break;
        }
    }
}

// Exponentielle Hüllkurve innerhalb eines Ratchet-Bursts:
//   cvNorm = 0.0  → starker Abfall   (1. Hit laut, letzte leise)
//   cvNorm = 0.5  → flach            (alle Ratchet-Hits gleich laut)
//   cvNorm = 1.0  → starker Anstieg  (1. Hit leise, letzter laut)
//   ratchetIdx  = Position im Burst (0 = erster Hit)
//   ratchetTotal = Gesamtzahl der Hits im Burst
float getValueModFactor(int ch, int ratchetIdx, int ratchetTotal) {
    if (ch < 0 || ch > 2 || cvValueModCvIdx[ch] < 0 || ratchetTotal <= 1) return 1.0f;
    float cvNorm = cvValueModRaw[ch] / 4095.0f;
    float t = (float)ratchetIdx / (float)(ratchetTotal - 1);  // 0..1
    float s = fabsf(cvNorm - 0.5f) * 2.0f;  // 0..1 (Stärke)
    if (cvNorm >= 0.5f) {
        // Crescendo (linear): 1. Hit bei (1-s), letzter bei 1.0
        return (1.0f - s) + s * t;
    } else {
        // Decrescendo (linear): 1. Hit bei 1.0, letzter bei (1-s)
        return 1.0f - s * t;
    }
}
