#include <gates.h>
#include <hardware_map.h>
#include <euclid.h>
#include <pitch.h>
#include <cv_inputs.h>
#include <midi_out.h>

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
    int effRot    = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    int effRotSel = clampVal(PatRot[ch] + PatRotSel[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    int src = RotateGateLen[ch] ? euclidRotatedSrc(idx, len, effRotSel) : euclidRotatedSrc(idx, len, effRot);
    uint8_t vA = GateLenArr[ch][src];
    uint8_t vB = GateLenBArr[ch][src];
    uint8_t v  = (cvMorph > 0.0f && (morphChannelMask & (1u << ch)))
        ? (uint8_t)clampVal((int)((float)vA * (1.0f - cvMorph) + (float)vB * cvMorph + 0.5f), 0, 255)
        : vA;
    if (condGateLenOvr[ch] > 0) v = condGateLenOvr[ch];
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
            if (i == 1 || i == 2) midiOutRhythmHit(i);
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
    int effRot    = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    int effRotSel = clampVal(PatRot[ch] + PatRotSel[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    if (!EPatArr[ch][euclidRotatedSrc(idx, len, effRot)]) return;
    // Per-Step Mute: wie Non-Hit behandeln
    int muteSrc = RotateMuteStep[ch] ? euclidRotatedSrc(idx, len, effRotSel)
                                     : euclidRotatedSrc(idx, len, effRot);
    if (MuteStepArr[ch][muteSrc]) return;
    // Hold-Legato: Gate nur neu zünden wenn nicht schon gehalten
    bool stepHold = (bool)HoldStepArr[ch][RotateHoldStep[ch]
                       ? euclidRotatedSrc(idx, len, effRotSel)
                       : euclidRotatedSrc(idx, len, effRot)];
    if (!gateIsHeld[ch]) digitalWrite(GatePins[ch], LOW);
    if (stepHold) {
        gateIsHeld[ch] = true;
        gateOffAt[ch]  = 0;
    } else {
        gateIsHeld[ch] = false;
        gateOffAt[ch]  = micros() + gateLenForStep(ch, cntCh[ch]);
    }
    // MIDI: Ch1→CH9, Ch2→CH10 (Ch0 Melodie wird in outputValuesForStep gesendet)
    if (ch == 1 || ch == 2) midiOutRhythmHit(ch);
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
        int src    = RotateValues[ch] ? euclidRotatedSrc(idx, len, effRotSel) : euclidRotatedSrc(idx, len, effRot);
        uint8_t vA = ValuesArr[ch][src];
        uint8_t vB = ValuesBArr[ch][src];
        uint8_t v  = (cvMorph > 0.0f && (morphChannelMask & (1u << ch)))
            ? (uint8_t)clampVal((int)((float)vA * (1.0f - cvMorph) + (float)vB * cvMorph + 0.5f), 0, 255)
            : vA;
        if (cvCompress[ch] > 0.0f) {
            float sumA = 0.0f;
            for (int j = 0; j < len; j++) sumA += (float)ValuesArr[ch][j];
            float mw = sumA / (float)len;
            if (cvMorph > 0.0f && (morphChannelMask & (1u << ch))) {
                float sumB = 0.0f;
                for (int j = 0; j < len; j++) sumB += (float)ValuesBArr[ch][j];
                mw = mw * (1.0f - cvMorph) + (sumB / (float)len) * cvMorph;
            }
            float vf = ((float)v - mw) * (1.0f - cvCompress[ch]) + mw;
            v = (uint8_t)clampVal((int)(vf + 0.5f), 0, 255);
        }
        if (condAccentActive[ch]) v = 255;
        if (condValueMul[ch] > 0 && v > 0) {
            uint16_t vv = v;
            switch (condValueMul[ch]) {
                case 1: vv = (uint16_t)v * 2u;       break;  // ×2
                case 2: vv = (uint16_t)v * 3u / 2u;  break;  // ×3/2
                case 3: vv = (uint16_t)v * 4u / 3u;  break;  // ×4/3
                case 4: vv = (uint16_t)v * 5u / 4u;  break;  // ×5/4
            }
            v = (vv > 255u) ? 255u : (uint8_t)vv;
        }
        if (cvValOffset[ch] != 0) {
            float scale = (255.0f + (float)cvValOffset[ch]) / 255.0f;
            int vv = (int)((float)v * scale + 0.5f);
            v = (vv < 0) ? 0 : (vv > 255) ? 255 : (uint8_t)vv;
        }

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
            int src     = pitchRotate ? euclidRotatedSrc(effPidx, len0, effRotSel0) : euclidRotatedSrc(effPidx, len0, effRot0);
            if (!pitchHold || hit) {
                int octSrc = RotateOctave[0] ? euclidRotatedSrc(pidx, len0, effRotSel0) : euclidRotatedSrc(pidx, len0, effRot0);
                int totalShift = (int)pitchShift + (int)cvPitchShiftOffset + (int)OctaveNote1[octSrc];
                int midi = pitchNotesFrozen
                    ? frozenMidi[src]
                    : quantizeToMidi(PitchNote1[src], pitchSpread, pitchScale,
                                     pitchRoot, pitchIntervalMask);

                // Per-Step IV: IvStep1[step]>0 überschreibt CV-IV
                int ivSrc = RotateIvStep ? euclidRotatedSrc(pidx, len0, effRotSel0) : euclidRotatedSrc(pidx, len0, effRot0);
                int effectiveCvIv = ((int)IvStep1[ivSrc] > 0) ? (int)IvStep1[ivSrc] : (int)cvPitchIvSteps;

                // On-the-fly IV / AI: Basis-MIDI aller Hit-Steps scannen
                int cvPitchAdj = 0;
                if (effectiveCvIv > 0 || cvPitchAiUpOct > 0 || cvPitchAiDownOct > 0) {
                    int rawMidi = pitchNotesFrozen
                        ? frozenMidi[pidx]
                        : quantizeToMidi(PitchNote1[pidx], pitchSpread, pitchScale,
                                         pitchRoot, pitchIntervalMask);
                    int bms[32]; int bmN = 0;
                    for (int si = 0; si < len0; si++) {
                        if (!EPatArr[0][euclidRotatedSrc(si, len0, effRot0)]) continue;
                        bms[bmN++] = pitchNotesFrozen
                            ? frozenMidi[si]
                            : quantizeToMidi(PitchNote1[si], pitchSpread, pitchScale,
                                             pitchRoot, pitchIntervalMask);
                    }
                    if (bmN > 0) {
                        if (effectiveCvIv > 0) {
                            int unique[32]; int uN = 0;
                            for (int k = 0; k < bmN; k++) {
                                bool found = false;
                                for (int j = 0; j < uN; j++) if (unique[j] == bms[k]) { found = true; break; }
                                if (!found) unique[uN++] = bms[k];
                            }
                            for (int i = 0; i < uN - 1; i++)
                                for (int j = i + 1; j < uN; j++)
                                    if (unique[j] < unique[i]) { int t = unique[i]; unique[i] = unique[j]; unique[j] = t; }
                            int rank = 0;
                            for (int j = 0; j < uN; j++) if (unique[j] == rawMidi) { rank = j; break; }
                            if (uN > 1 && rank < effectiveCvIv) cvPitchAdj += 1;
                        }
                        if (cvPitchAiUpOct > 0 || cvPitchAiDownOct > 0) {
                            int minMidi = bms[0], maxMidi = bms[0];
                            for (int k = 1; k < bmN; k++) {
                                if (bms[k] < minMidi) minMidi = bms[k];
                                if (bms[k] > maxMidi) maxMidi = bms[k];
                            }
                            if (cvPitchAiUpOct   > 0 && rawMidi == minMidi) cvPitchAdj += (int)cvPitchAiUpOct;
                            if (cvPitchAiDownOct > 0 && rawMidi == maxMidi) cvPitchAdj -= (int)cvPitchAiDownOct;
                        }
                    }
                }

                // Scale-step shift: find current note in noteList, shift by ±N steps
                if (condScaleStepAdd[0] != 0) {
                    int noteList[60]; int nCount = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
                    int ni = 0;
                    for (int k = 0; k < nCount; k++) if (noteList[k] == midi) { ni = k; break; }
                    ni = clampVal(ni + (int)condScaleStepAdd[0], 0, nCount - 1);
                    midi = noteList[ni];
                }
                midi = clampVal(midi + totalShift * 12 + (int)cvPitchTransposeST + cvPitchAdj * 12 + (int)condTransposeAdd[0], 36, 127);
                lastPitchDac = midiToDac(midi);
                pitchDac = lastPitchDac;
                if (hit) midiOutMelodyHit(midi, 100);
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
