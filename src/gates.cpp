#include <gates.h>
#include <hardware_map.h>
#include <euclid.h>
#include <pitch.h>
#include <cv_inputs.h>

namespace {

constexpr uint16_t MCP4822_CMD_DAC_A = 0x0000;
constexpr uint16_t MCP4822_CMD_DAC_B = 0x8000;
// Bit13=1 → 1× interner Gain → Vout_max = 2.048 V (MCP4822 an 3.3V, kein SPI-Levelshifter nötig).
// Externer TL074 (±12V) verstärkt auf 0–6.6V: Gain = 3.22 (R1=10k, R2=22k).
constexpr uint16_t MCP4822_CMD_GAIN1X = 0x2000;
constexpr uint16_t MCP4822_CMD_ACTIVE = 0x1000;
constexpr uint32_t MCP4822_SPI_HZ = 10000000UL;

SPISettings mcp4822SpiSettings(MCP4822_SPI_HZ, MSBFIRST, SPI_MODE0);

uint16_t scale8To12(uint8_t v) {
    return (uint16_t)(((uint32_t)v * 4095u + 127u) / 255u);
}

void writeMcp4822Channel(uint8_t csPin, bool channelB, uint16_t value12) {
    uint16_t cmd = channelB ? MCP4822_CMD_DAC_B : MCP4822_CMD_DAC_A;
    cmd = (uint16_t)(cmd | MCP4822_CMD_GAIN1X | MCP4822_CMD_ACTIVE | (value12 & 0x0FFFu));
    digitalWriteFast(csPin, LOW);
    SPI.transfer16(cmd);
    digitalWriteFast(csPin, HIGH);
}

void writeCvOutputsRaw(uint16_t pitch, uint16_t out1, uint16_t out2, uint16_t out3) {
    SPI.beginTransaction(mcp4822SpiSettings);
    writeMcp4822Channel(MCP4822_CS_DAC1_PIN, false, pitch); // DAC1-A = Pitch
    writeMcp4822Channel(MCP4822_CS_DAC1_PIN, true,  out1);  // DAC1-B = Value1
    writeMcp4822Channel(MCP4822_CS_DAC2_PIN, false, out2);  // DAC2-A = Value2
    writeMcp4822Channel(MCP4822_CS_DAC2_PIN, true,  out3);  // DAC2-B = Value3
    SPI.endTransaction();
}

} // namespace

// Zweck: Initialisiert SPI0 und beide MCP4822 fuer die CV-Ausgabe.
// Side Effects: konfiguriert CS Pins und setzt alle DAC-Kanaele auf 0 V.
// Assumptions: Die MCP4822 haengen an SPI0 (11/12/13) und reagieren auf CS=9/8. LDAC liegt auf GND.
void initCvOutputs() {
    SPI.begin();
    pinMode(MCP4822_CS_DAC1_PIN, OUTPUT);
    pinMode(MCP4822_CS_DAC2_PIN, OUTPUT);
    digitalWriteFast(MCP4822_CS_DAC1_PIN, HIGH);
    digitalWriteFast(MCP4822_CS_DAC2_PIN, HIGH);
    writeCvOutputsRaw(0, 0, 0, 0);
}

// Zweck: Liefert die Dauer bis zum naechsten Hit im selben Pattern (in us).
// Beruecksichtigt den CV PatRot-Offset fuer die Hit-Erkennung.
static uint32_t durationToNextHit(int ch, unsigned int step) {
    int len = PatLen[ch];
    if (len <= 0) {
        return DurationOfOneStep > 0 ? DurationOfOneStep : GATE_PULSE_US;
    }
    int idx = step % len;
    int effRot = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    for (int i = 1; i <= len; i++) {
        int nidx = (idx + i) % len;
        if (EPatArr[ch][euclidRotatedSrc(nidx, len, effRot)]) {
            uint32_t d = (uint32_t)i * DurationOfOneStep;
            return (d > 0) ? d : GATE_PULSE_US;
        }
    }
    return DurationOfOneStep > 0 ? DurationOfOneStep : GATE_PULSE_US;
}

// Zweck: Berechnet die Gate-Laenge fuer einen Step.
// Beruecksichtigt den CV PatRot-Offset bei RotateGateLen.
uint32_t gateLenForStep(int ch, unsigned int step) {
    int len = PatLen[ch];
    if (len <= 0) {
        return GATE_PULSE_US;
    }
    if (!(*GateHoldArr[ch])) {
        return GATE_PULSE_US;
    }
    int idx       = step % len;
    int effRotSel = clampVal(PatRot[ch] + PatRotSel[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    int src = RotateGateLen[ch] ? euclidRotatedSrc(idx, len, effRotSel) : idx;
    uint8_t v = GateLenArr[ch][src];
    if (v == 0) {
        return GATE_PULSE_US;
    }
    uint32_t maxLen = durationToNextHit(ch, step);
    if (v == 255) {
        return maxLen;
    }
    return (uint32_t)(GATE_PULSE_US + ((maxLen - GATE_PULSE_US) * (uint32_t)v) / 255U);
}

// Zweck: Triggert Gate-Ausgaenge, wenn der aktuelle Step ein Hit ist (alle Kanaele).
// Beruecksichtigt isSeqActive, CV PatRot-Offset.
void triggerGates() {
    for (int i = 0; i < 3; i++) {
        if (!isSeqActive(i)) continue;
        int len = PatLen[i];
        if (len <= 0) continue;
        int idx = cntCh[i] % len;
        int effRot = clampVal(PatRot[i] + (int)cvPatRotOffset[i], -(len - 1), len - 1);
        if (EPatArr[i][euclidRotatedSrc(idx, len, effRot)]) {
            digitalWrite(GatePins[i], LOW);
            gateOffAt[i] = micros() + gateLenForStep(i, cntCh[i]);
        }
    }
}

// Zweck: Triggert den Gate-Ausgang fuer einen einzelnen Kanal (Sub-Ticks bei x*N).
// Beruecksichtigt isSeqActive, CV PatRot-Offset.
void triggerGateForCh(int ch) {
    if (ch < 0 || ch > 2) return;
    if (!isSeqActive(ch)) return;
    int len = PatLen[ch];
    if (len <= 0) return;
    int idx = cntCh[ch] % len;
    int effRot = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    if (EPatArr[ch][euclidRotatedSrc(idx, len, effRot)]) {
        digitalWrite(GatePins[ch], LOW);
        gateOffAt[ch] = micros() + gateLenForStep(ch, cntCh[ch]);
    }
}

// Cache für Ratchet-Folgehits: unmodulierte Value-Werte + aktueller Pitch-DAC
static uint8_t  lastOutUnmod[3] = { 0, 0, 0 };
static uint16_t lastPitchDacVal = 0;

// Set by resetGateCvCache(); cleared on next outputValuesForStep call.
static bool s_resetCvCache = false;

void resetGateCvCache() { s_resetCvCache = true; }

// DAC-Zustand-Cache: was zuletzt tatsächlich auf den DAC geschrieben wurde.
// Wird von outputValuesForStep UND outputRatchetValue gepflegt, damit
// Swing-Steps den alten Wert halten können bis das Gate feuert.
static uint16_t sDacPitch  = 0;
static uint16_t sDacOut[3] = { 0, 0, 0 };

// Zweck: Gibt CV-Werte fuer den aktuellen Step aus (erster Ratchet-Hit, idx=0).
// swingMask: Bit pro Kanal (Bit0=Ch0). Fuer gesetzte Kanaele wird der alte DAC-Wert
// gehalten (Gate ist noch verzoegert) und der neue Wert erst bei Gate-Feuer geschrieben.
void outputValuesForStep(unsigned int /*step_unused*/, uint8_t swingMask) {
    static uint8_t  lastOut[3]   = { 0, 0, 0 };
    static uint16_t lastPitchDac = 0;

    // Reset hold-caches after slot load so old-slot CV doesn't leak into new slot
    if (s_resetCvCache) {
        s_resetCvCache = false;
        lastOut[0] = lastOut[1] = lastOut[2] = 0;
        lastPitchDac = 0;
    }

    for (int ch = 0; ch < 3; ch++) {
        if (!isSeqActive(ch)) {
            lastOut[ch] = 0;
            continue;
        }
        int len = PatLen[ch];
        if (len <= 0) {
            lastOut[ch] = 0;
            continue;
        }

        int idx       = cntCh[ch] % len;
        int effRot    = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
        int effRotSel = clampVal(PatRot[ch] + PatRotSel[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
        bool hit   = EPatArr[ch][euclidRotatedSrc(idx, len, effRot)];
        int src    = RotateValues[ch] ? euclidRotatedSrc(idx, len, effRotSel) : idx;
        uint8_t v  = ValuesArr[ch][src];

        if (*HoldArr[ch]) {
            if (hit) lastOut[ch] = v;
        } else {
            lastOut[ch] = v;
        }
        lastOutUnmod[ch] = lastOut[ch];  // vor Modulation cachen
    }

    // Pitch-CV fuer Kanal 1: quantisierter Schritt aus PitchNote1
    uint16_t pitchDac = lastPitchDac;
    {
        int len0 = PatLen[0];
        if (len0 > 0) {
            int pidx      = (int)(cntCh[0] % (unsigned int)len0);
            int effRot0   = clampVal(PatRot[0] + (int)cvPatRotOffset[0], -(len0 - 1), len0 - 1);
            int effRotSel0 = clampVal(PatRot[0] + PatRotSel[0] + (int)cvPatRotOffset[0], -(len0 - 1), len0 - 1);
            bool hit    = EPatArr[0][euclidRotatedSrc(pidx, len0, effRot0)];
            uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
            int effPidx = foldPitchIdx(pidx, len0, effFold);
            int src     = pitchRotate ? euclidRotatedSrc(effPidx, len0, effRotSel0) : effPidx;
            if (!pitchHold || hit) {
                int octSrc = RotateOctave[0] ? euclidRotatedSrc(pidx, len0, effRotSel0) : pidx;
                int totalShift = (int)pitchShift + (int)cvPitchShiftOffset + (int)OctaveNote1[octSrc];
                int midi = quantizeToMidi(PitchNote1[src], pitchSpread, pitchScale,
                                          pitchRoot, pitchIntervalMask);
                midi = clampVal(midi + totalShift * 12 + (int)cvPitchTransposeST, 36, 127);
                lastPitchDac = midiToDac(midi);
                pitchDac = lastPitchDac;
            }
        }
    }
    lastPitchDacVal = pitchDac;

    // Value-Modulation: ratchetIdx=0 (erster Hit des Bursts), Total=cvRatchetCount
    uint8_t modOut[3];
    for (int ch = 0; ch < 3; ch++) {
        int total = (cvRatchetCount[ch] > 1) ? (int)cvRatchetCount[ch] : 1;
        float mod = getValueModFactor(ch, 0, total);
        modOut[ch] = (uint8_t)clampVal((int)((float)lastOutUnmod[ch] * mod + 0.5f), 0, 255);
    }

    // Swing: Kanaele in swingMask halten alten DAC-Wert; neue Werte werden erst beim
    // Gate-Feuer via outputRatchetValue(ch, 0, 1) geschrieben.
    uint16_t writePitch = (swingMask & 0x01u) ? sDacPitch : pitchDac;
    uint16_t writeOut[3];
    for (int ch = 0; ch < 3; ch++) {
        writeOut[ch] = (swingMask & (1u << ch)) ? sDacOut[ch] : scale8To12(modOut[ch]);
    }
    sDacPitch = writePitch;
    sDacOut[0] = writeOut[0];
    sDacOut[1] = writeOut[1];
    sDacOut[2] = writeOut[2];
    writeCvOutputsRaw(writePitch, writeOut[0], writeOut[1], writeOut[2]);
}

// Zweck: Schreibt modulierten Value fuer Ratchet-Sub-Hit i auf den DAC.
// Auch fuer Swing-Gate-Feuer (ratchetIdx=0, ratchetTotal=1): schreibt den
// zuvor berechneten neuen Wert, der beim Tick-Zeitpunkt zurueckgehalten wurde.
void outputRatchetValue(int ch, int ratchetIdx, int ratchetTotal) {
    uint8_t out[3];
    for (int i = 0; i < 3; i++) {
        float mod = (i == ch) ? getValueModFactor(i, ratchetIdx, ratchetTotal) : 1.0f;
        out[i] = (uint8_t)clampVal((int)((float)lastOutUnmod[i] * mod + 0.5f), 0, 255);
    }
    sDacPitch  = lastPitchDacVal;
    sDacOut[0] = scale8To12(out[0]);
    sDacOut[1] = scale8To12(out[1]);
    sDacOut[2] = scale8To12(out[2]);
    writeCvOutputsRaw(sDacPitch, sDacOut[0], sDacOut[1], sDacOut[2]);
}
