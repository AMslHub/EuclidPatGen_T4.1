#include <storage.h>

#include <EEPROM.h>

#include <euclid.h>
#include <cv_inputs.h>

// EEPROM storage for persistent parameters
// Hinweis: Bei Layout-Aenderungen EEPROM_MAGIC_* anpassen.
#define EEPROM_MAGIC_CURRENT 0xEAE6
#define EEPROM_MAGIC_SLOTS   0xEA5F
#define EEPROM_ADDR_CURRENT  0

struct ParamBlock {
    uint8_t len[3];
    uint8_t num[3];
    int8_t rot[3];
    uint8_t prob[3];
    uint8_t probAuto[3];
    uint8_t values[3][32];
    uint8_t hold[3];
    uint8_t gateLen[3][32];
    uint8_t gateHold[3];
    uint8_t rotValues[3];
    uint8_t rotGateLen[3];
    int8_t  speed[3];  // chSpeedIdx: -3..+3 (÷4..×4)
    uint8_t autoRotate[3];  // 0=aus, 1-4=Schritte pro Zyklus
    uint8_t ratchet[3][32]; // 1-4 Sub-Hits pro Hit-Step
    uint8_t rotRatchet[3];
    uint8_t rotOctave[3];
};

struct PitchBlock {
    uint8_t note[32];       // Rohwerte 0-255 pro Step
    uint8_t spread;         // Oktavbereich 1-5
    uint8_t scale;          // Skalenindex
    uint8_t root;           // Grundton 0-11
    uint8_t intervalMask;   // Bitfeld aktiver Intervalle
    int8_t  shift;          // Oktavtransposition -3..+3
    uint8_t hold;           // pitchHold: 0=aus, 1=an
    uint8_t rotate;         // pitchRotate: 0=aus, 1=an
    uint8_t foldMode;       // pitchFoldMode: 0=off, 1-12
    int8_t  octave[32];     // Oktav-Offset pro Step: -3..+3 (nur Kanal 1)
};

struct CurrentParams {
    uint16_t bpm;
    ParamBlock data;
    uint8_t epatSavedMask;
    uint8_t epat[3][32];
    uint8_t extClkMode;  // 0=intern, 1=extern
    PitchBlock pitch;
    uint8_t cvTargetMap[3];  // CV-Eingang → Zielparameter (CvTarget enum)
};

struct SlotParams {
    ParamBlock data;
    uint8_t epatSavedMask;
    uint8_t epat[3][32];
    PitchBlock pitch;
};

struct EucParams {
    uint16_t magic;
    CurrentParams data;
};

struct SlotsHeader {
    uint16_t magic;
    uint8_t usedMask;
    uint8_t reserved;
};

static const uint16_t EEPROM_ADDR_SLOTS = (uint16_t)(EEPROM_ADDR_CURRENT + sizeof(EucParams));
static const uint16_t EEPROM_ADDR_SLOTS_HEADER = EEPROM_ADDR_SLOTS;
static const uint16_t EEPROM_ADDR_SLOTS_DATA = (uint16_t)(EEPROM_ADDR_SLOTS_HEADER + sizeof(SlotsHeader));
static const uint8_t SLOT_COUNT = 7;

static int pendingSlot = -1;
static bool pendingLoad = false;
static int activeSlot = -1;

static void packParamsCore(ParamBlock &p){
    for(int i=0;i<3;i++){
        p.len[i] = (uint8_t) clampVal(PatLen[i], 1, 32);
        p.num[i] = (uint8_t) clampVal(PatNum[i], 0, PatLen[i]);
        int rmax = PatLen[i] > 0 ? PatLen[i] : 0;
        p.rot[i] = (int8_t) clampVal(PatRot[i], -rmax, rmax);
        p.prob[i] = (uint8_t) clampVal(PatProb[i], 0, 20);
        p.probAuto[i] = PatProbAuto[i] ? 1 : 0;
        if(ProbEuclidRebuild[i]){
            p.probAuto[i] = (uint8_t)(p.probAuto[i] | 0x02);
        }
        for(int j=0;j<32;j++){
            p.values[i][j] = ValuesArr[i][j];
            p.gateLen[i][j] = GateLenArr[i][j];
            p.ratchet[i][j] = (uint8_t)clampVal((int)RatchetArr[i][j], 1, 4);
        }
        p.hold[i] = (*HoldArr[i]) ? 1 : 0;
        p.gateHold[i] = (*GateHoldArr[i]) ? 1 : 0;
        p.rotValues[i] = RotateValues[i] ? 1 : 0;
        p.rotGateLen[i] = RotateGateLen[i] ? 1 : 0;
        p.rotRatchet[i] = RotateRatchet[i] ? 1 : 0;
        p.rotOctave[i]  = RotateOctave[i]  ? 1 : 0;
        p.speed[i] = (int8_t)clampVal(chSpeedIdx[i], -3, 3);
        p.autoRotate[i] = (uint8_t)clampVal((int)autoRotateStep[i], 0, 4);
    }
}

static void unpackParamsCore(const ParamBlock &p){
    for(int i=0;i<3;i++){
        PatLen[i] = clampVal(p.len[i], 1, 32);
        PatNum[i] = clampVal(p.num[i], 0, PatLen[i]);
        int rmax = PatLen[i] > 0 ? PatLen[i] : 0;
        PatRot[i] = clampVal((int)p.rot[i], -rmax, rmax);
        PatProb[i] = clampVal(p.prob[i], 0, 20);
        PatProbAuto[i] = (p.probAuto[i] & 0x01) != 0;
        ProbEuclidRebuild[i] = (p.probAuto[i] & 0x02) != 0;
        for(int j=0;j<32;j++){
            ValuesArr[i][j] = p.values[i][j];
            GateLenArr[i][j] = p.gateLen[i][j];
            RatchetArr[i][j] = (uint8_t)clampVal((int)p.ratchet[i][j], 1, 4);
        }
        *HoldArr[i] = (p.hold[i] != 0);
        *GateHoldArr[i] = (p.gateHold[i] != 0);
        RotateValues[i]  = (p.rotValues[i]  != 0);
        RotateGateLen[i] = (p.rotGateLen[i] != 0);
        RotateRatchet[i] = (p.rotRatchet[i] != 0);
        RotateOctave[i]  = (p.rotOctave[i]  != 0);
        chSpeedIdx[i] = clampVal((int)p.speed[i], -3, 3);
        autoRotateStep[i] = (uint8_t)clampVal((int)p.autoRotate[i], 0, 4);
    }
}

static void packCurrent(CurrentParams &p){
    p.bpm = (uint16_t) clampVal((int)bpm, 0, 600);
    packParamsCore(p.data);
    p.epatSavedMask = 0;
    for(int i=0;i<3;i++){
        if(!PatProbAuto[i]){
            p.epatSavedMask = (uint8_t)(p.epatSavedMask | (1u << i));
        }
        for(int j=0;j<32;j++){
            p.epat[i][j] = (uint8_t)(EPatArr[i][j] ? 1 : 0);
        }
    }
    p.extClkMode = extClockMode ? 1 : 0;
    p.pitch.spread       = pitchSpread;
    p.pitch.scale        = pitchScale;
    p.pitch.root         = pitchRoot;
    p.pitch.intervalMask = pitchIntervalMask;
    p.pitch.shift        = pitchShift;
    p.pitch.hold         = pitchHold   ? 1 : 0;
    p.pitch.rotate       = pitchRotate ? 1 : 0;
    p.pitch.foldMode     = (uint8_t)clampVal((int)pitchFoldMode, 0, 12);
    for (int i = 0; i < 32; i++) p.pitch.note[i] = PitchNote1[i];
    for (int i = 0; i < 32; i++) p.pitch.octave[i] = (int8_t)clampVal((int)OctaveNote1[i], -3, 3);
    for (int i = 0; i < 3; i++) p.cvTargetMap[i] = cvTargetMap[i];
}

static void unpackCurrent(const CurrentParams &p){
    bpm = (uint16_t) clampVal((int)p.bpm, 0, 600);
    unpackParamsCore(p.data);
    for(int i=0;i<3;i++){
        bool hasSnapshot = (p.epatSavedMask & (1u << i)) != 0;
        if(hasSnapshot && !PatProbAuto[i]){
            int len = clampVal(PatLen[i], 1, 32);
            for(int j=0;j<len;j++){
                EPatArr[i][j] = (p.epat[i][j] != 0);
            }
            for(int j=len;j<32;j++){
                EPatArr[i][j] = false;
            }
        }else{
            rebuildPattern(PatLen[i], PatNum[i], PatProb[i], EPatArr[i]);
        }
    }
    extClockMode     = (p.extClkMode != 0);
    pitchSpread      = (p.pitch.spread >= 1 && p.pitch.spread <= 5) ? p.pitch.spread : 2;
    pitchScale       = p.pitch.scale;
    pitchRoot        = p.pitch.root % 12;
    pitchIntervalMask = (p.pitch.intervalMask != 0) ? p.pitch.intervalMask : 0x07;
    pitchShift       = clampVal((int)p.pitch.shift, -3, 3);
    pitchHold        = (p.pitch.hold   != 0);
    pitchRotate      = (p.pitch.rotate != 0);
    pitchFoldMode    = (uint8_t)clampVal((int)p.pitch.foldMode, 0, 12);
    for (int i = 0; i < 32; i++) PitchNote1[i] = p.pitch.note[i];
    for (int i = 0; i < 32; i++) OctaveNote1[i] = (int8_t)clampVal((int)p.pitch.octave[i], -3, 3);
    for (int i = 0; i < 3; i++)
        cvTargetMap[i] = (p.cvTargetMap[i] < CV_TARGET_COUNT) ? p.cvTargetMap[i] : CV_TARGET_NONE;
}

static void packSlot(SlotParams &p){
    packParamsCore(p.data);
    p.epatSavedMask = 0;
    for(int i=0;i<3;i++){
        if(!PatProbAuto[i]){
            p.epatSavedMask = (uint8_t)(p.epatSavedMask | (1u << i));
        }
        for(int j=0;j<32;j++){
            p.epat[i][j] = (uint8_t)(EPatArr[i][j] ? 1 : 0);
        }
    }
    p.pitch.spread       = pitchSpread;
    p.pitch.scale        = pitchScale;
    p.pitch.root         = pitchRoot;
    p.pitch.intervalMask = pitchIntervalMask;
    p.pitch.shift        = pitchShift;
    p.pitch.hold         = pitchHold   ? 1 : 0;
    p.pitch.rotate       = pitchRotate ? 1 : 0;
    p.pitch.foldMode     = (uint8_t)clampVal((int)pitchFoldMode, 0, 12);
    for (int i = 0; i < 32; i++) p.pitch.note[i] = PitchNote1[i];
    for (int i = 0; i < 32; i++) p.pitch.octave[i] = (int8_t)clampVal((int)OctaveNote1[i], -3, 3);
}

static void unpackSlot(const SlotParams &p){
    unpackParamsCore(p.data);
    for(int i=0;i<3;i++){
        bool hasSnapshot = (p.epatSavedMask & (1u << i)) != 0;
        if(hasSnapshot && !PatProbAuto[i]){
            int len = clampVal(PatLen[i], 1, 32);
            for(int j=0;j<len;j++){
                EPatArr[i][j] = (p.epat[i][j] != 0);
            }
            for(int j=len;j<32;j++){
                EPatArr[i][j] = false;
            }
        }else{
            rebuildPattern(PatLen[i], PatNum[i], PatProb[i], EPatArr[i]);
        }
    }
    pitchSpread      = (p.pitch.spread >= 1 && p.pitch.spread <= 5) ? p.pitch.spread : 2;
    pitchScale       = p.pitch.scale;
    pitchRoot        = p.pitch.root % 12;
    pitchIntervalMask = (p.pitch.intervalMask != 0) ? p.pitch.intervalMask : 0x07;
    pitchShift       = clampVal((int)p.pitch.shift, -3, 3);
    pitchHold        = (p.pitch.hold   != 0);
    pitchRotate      = (p.pitch.rotate != 0);
    pitchFoldMode    = (uint8_t)clampVal((int)p.pitch.foldMode, 0, 12);
    for (int i = 0; i < 32; i++) PitchNote1[i] = p.pitch.note[i];
    for (int i = 0; i < 32; i++) OctaveNote1[i] = (int8_t)clampVal((int)p.pitch.octave[i], -3, 3);
}

static SlotsHeader readSlotsHeader(){
    SlotsHeader h;
    EEPROM.get(EEPROM_ADDR_SLOTS_HEADER, h);
    if(h.magic != EEPROM_MAGIC_SLOTS){
        h.magic = EEPROM_MAGIC_SLOTS;
        h.usedMask = 0;
        h.reserved = 0;
        EEPROM.put(EEPROM_ADDR_SLOTS_HEADER, h);
    }
    return h;
}

static void writeSlotsHeader(const SlotsHeader &h){
    EEPROM.put(EEPROM_ADDR_SLOTS_HEADER, h);
}

static uint16_t slotAddr(int slot){
    return (uint16_t)(EEPROM_ADDR_SLOTS_DATA + slot * sizeof(SlotParams));
}

// Zweck: Speichert alle relevanten Parameter dauerhaft im EEPROM.
// Side Effects: schreibt in den EEPROM.
// Assumptions: EEPROM ist verfuegbar; Arrays haben Laenge 32.
void saveParams(){
    EucParams p;
    p.magic = EEPROM_MAGIC_CURRENT;
    packCurrent(p.data);
    EEPROM.put(EEPROM_ADDR_CURRENT, p);
}

// Zweck: Laedt Parameter aus dem EEPROM oder setzt Defaults.
// Side Effects: schreibt in globale Parameter-Arrays und Flags.
// Assumptions: EEPROM-Layout passt zur aktuellen Struktur oder Defaults werden gesetzt.
void loadParams(){
    EucParams p;
    EEPROM.get(EEPROM_ADDR_CURRENT, p);
    if(p.magic == EEPROM_MAGIC_CURRENT){
        unpackCurrent(p.data);
    }else{
        // Defaults when EEPROM layout changes
        bpm = 120;
        extClockMode     = false;
        pitchSpread      = 2;
        pitchScale       = 0;
        pitchRoot        = 0;
        pitchIntervalMask = 0x07;  // Root + Terz + Quinte
        pitchShift       = 0;
        pitchHold        = true;
        pitchRotate      = true;
        for (int i = 0; i < 32; i++) PitchNote1[i] = 0;
        for(int i=0;i<3;i++){
            *HoldArr[i] = true;
            *GateHoldArr[i] = false;
            RotateValues[i]  = false;
            RotateGateLen[i] = false;
            RotateRatchet[i] = false;
            RotateOctave[i]  = false;
            PatProb[i] = 10;
            PatProbAuto[i] = false;
            ProbEuclidRebuild[i] = false;
            chSpeedIdx[i] = 0;
            cvTargetMap[i] = CV_TARGET_NONE;
        }
    }
}

void scheduleSaveParams(){
    PendingSave = true;
    PendingSaveAt = millis() + SAVE_DEBOUNCE_MS;
}

uint8_t getSlotsUsedMask(){
    SlotsHeader h = readSlotsHeader();
    return h.usedMask;
}

bool saveParamsSlot(int slot){
    if(slot < 0 || slot >= SLOT_COUNT) return false;
    SlotParams p;
    packSlot(p);
    EEPROM.put(slotAddr(slot), p);
    SlotsHeader h = readSlotsHeader();
    h.usedMask = (uint8_t)(h.usedMask | (1u << slot));
    writeSlotsHeader(h);
    return true;
}

bool deleteParamsSlot(int slot){
    if(slot < 0 || slot >= SLOT_COUNT) return false;
    SlotsHeader h = readSlotsHeader();
    h.usedMask = (uint8_t)(h.usedMask & ~(1u << slot));
    writeSlotsHeader(h);
    if(activeSlot == slot){
        activeSlot = -1;
    }
    return true;
}

static bool loadParamsSlotNow(int slot){
    if(slot < 0 || slot >= SLOT_COUNT) return false;
    SlotsHeader h = readSlotsHeader();
    if((h.usedMask & (1u << slot)) == 0){
        return false;
    }
    SlotParams p;
    EEPROM.get(slotAddr(slot), p);
    unpackSlot(p);
    activeSlot = slot;
    return true;
}

bool requestLoadSlot(int slot){
    if(slot < 0 || slot >= SLOT_COUNT) return false;
    pendingSlot = slot;
    pendingLoad = true;
    return true;
}

bool applyPendingLoadIfReady(unsigned int step){
    if(!pendingLoad) return false;
    if(PatLen[0] <= 0){
        pendingLoad = false;
        return false;
    }
    if((step % (unsigned int)PatLen[0]) != 0) return false;
    bool ok = loadParamsSlotNow(pendingSlot);
    pendingLoad = false;
    pendingSlot = -1;
    return ok;
}

int getActiveSlot(){
    return activeSlot;
}
