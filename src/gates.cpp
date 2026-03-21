#include <gates.h>

// Zweck: Prüft, ob im Pattern an der (rotierten) Position ein Hit liegt.
// Side Effects: keine.
// Assumptions: PatLen[ch] > 0, EPatArr[ch] ist gültig.
static inline bool isHitRotated(int ch, int idx){
    int len = PatLen[ch];
    if(len <= 0) return false;
    int rot = PatRot[ch] % len;
    if(rot < 0) rot += len;
    int src = idx - rot;
    if(src < 0) src += len;
    return EPatArr[ch][src];
}

// Zweck: Berechnet den Quellindex unter Berücksichtigung der Rotation.
// Side Effects: keine.
// Assumptions: PatLen[ch] > 0.
static inline int rotatedSrcIndex(int ch, int idx){
    int len = PatLen[ch];
    if(len <= 0) return idx;
    int rot = PatRot[ch] % len;
    if(rot < 0) rot += len;
    int src = idx - rot;
    if(src < 0) src += len;
    return src;
}

// Zweck: Prüft, ob eine Spur aktuell aktiv ist (Mute/Solo).
// Side Effects: keine.
// Assumptions: MuteSeq/SoloSeq sind gesetzt.
static inline bool isSeqActive(int ch){
    bool anySolo = SoloSeq[0] || SoloSeq[1] || SoloSeq[2];
    if(anySolo){
        return SoloSeq[ch] && !MuteSeq[ch];
    }
    return !MuteSeq[ch];
}

// Zweck: Liefert die Dauer bis zum naechsten Hit im selben Pattern (in us).
// Side Effects: keine.
// Assumptions: DurationOfOneStep > 0 oder GATE_PULSE_US ist gesetzt.
static uint32_t durationToNextHit(int ch, unsigned int step){
    int len = PatLen[ch];
    if(len <= 0){
        return DurationOfOneStep > 0 ? DurationOfOneStep : GATE_PULSE_US;
    }
    int idx = step % len;
    for(int i=1;i<=len;i++){
        int nidx = (idx + i) % len;
        if(isHitRotated(ch, nidx)){
            uint32_t d = (uint32_t)i * DurationOfOneStep;
            return (d > 0) ? d : GATE_PULSE_US;
        }
    }
    return DurationOfOneStep > 0 ? DurationOfOneStep : GATE_PULSE_US;
}

// Zweck: Berechnet die Gate-Laenge fuer einen Step.
// Side Effects: keine.
// Assumptions: GateHold/RotateGateLen sind konsistent mit Arrays; PatLen[ch] > 0.
uint32_t gateLenForStep(int ch, unsigned int step){
    int len = PatLen[ch];
    if(len <= 0){
        return GATE_PULSE_US;
    }
    if(!(*GateHoldArr[ch])){
        return GATE_PULSE_US;
    }
    int idx = step % len;
    int src = RotateGateLen[ch] ? rotatedSrcIndex(ch, idx) : idx;
    uint8_t v = GateLenArr[ch][src];
    if(v == 0){
        return GATE_PULSE_US;
    }
    uint32_t maxLen = durationToNextHit(ch, step);
    if(*GateHoldArr[ch] && v == 255){
        return maxLen;
    }
    return (uint32_t)(GATE_PULSE_US + ((maxLen - GATE_PULSE_US) * (uint32_t)v) / 255U);
}

// Zweck: Triggert Gate-Ausgaenge, wenn der aktuelle Step ein Hit ist.
// Side Effects: schreibt auf Gate-Pins und gateOffAt.
// Assumptions: cnt ist aktuell; GatePins sind als OUTPUT konfiguriert.
void triggerGates(){
    for(int i=0;i<3;i++){
        if(!isSeqActive(i)) continue;
        int len = PatLen[i];
        if(len <= 0) continue;
        int idx = cnt % len;
        if(isHitRotated(i, idx)){
            digitalWrite(GatePins[i], HIGH);
            // Non-blocking: Abschaltzeit merken, im Loop zuruecksetzen.
            gateOffAt[i] = micros() + gateLenForStep(i, cnt);
        }
    }
}

// Zweck: Gibt CV-Werte fuer den aktuellen Step aus.
// Side Effects: schreibt auf DAC0/DAC1/PWM.
// Assumptions: PatLen[ch] > 0; analogWrite ist initialisiert.
void outputValuesForStep(unsigned int step){
    static uint8_t lastOut[3] = { 0, 0, 0 };

    for(int ch = 0; ch < 3; ch++){
        if(!isSeqActive(ch)){
            lastOut[ch] = 0;
            continue;
        }
        int len = PatLen[ch];
        if(len <= 0){
            lastOut[ch] = 0;
            continue;
        }

        int idx = step % len;
        bool hit = isHitRotated(ch, idx);
        int src = RotateValues[ch] ? rotatedSrcIndex(ch, idx) : idx;
        uint8_t v = ValuesArr[ch][src];

        if(*HoldArr[ch]){ // wenn Hold = true
            if(hit){
                lastOut[ch] = v;
            }
        }else{ // wenn Hold = false
            lastOut[ch] = v;
        }
    }

    analogWrite(DAC0_PIN, lastOut[0]);
    analogWrite(DAC1_PIN, lastOut[1]);
    analogWrite(PWM_OUT_PIN, lastOut[2]);
}
