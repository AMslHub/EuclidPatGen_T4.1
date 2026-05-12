#include <storage.h>

#include <EEPROM.h>
#include <SD.h>

#include <euclid.h>
#include <cv_inputs.h>

// EEPROM: Autosave des aktuellen Zustands (schnell, nur geänderte Bytes).
// SD-Karte: Slot-Speicher (explizite User-Action, schnell durch internes FTL).
static const uint16_t EEPROM_MAGIC   = 0xEB44;  // bumped: +ratchetDecay
static const uint16_t SD_MAGIC_SLOTS = 0xEB5F;
static const uint16_t SD_MAGIC_SONG  = 0xEB61;

static bool sdOK = false;

static const char *SLOTS_HDR_PATH = "slots.hdr";
static const int   SLOT_COUNT     = 16;
static const char *SONGS_HDR_PATH = "songs.hdr";
static const int   SONG_COUNT     = 100;

static void getSlotPath(int slot, char *buf) {
    snprintf(buf, 16, "slot_%02d.bin", slot);
}

static void getSongPath(int song, char *buf) {
    snprintf(buf, 16, "song_%02d.bin", song);
}

// ---------------------------------------------------------------------------
// Datenstrukturen
// ---------------------------------------------------------------------------

struct ParamBlock {
    uint8_t len[3];
    uint8_t num[3];
    int8_t  rot[3];
    uint8_t prob[3];
    uint8_t probAuto[3];
    uint8_t values[3][32];
    uint8_t hold[3];
    uint8_t gateLen[3][32];
    uint8_t gateHold[3];
    uint8_t rotValues[3];
    uint8_t rotGateLen[3];
    int8_t  speed[3];
    uint8_t autoRotate[3];
    uint8_t ratchet[3][32];
    uint8_t rotRatchet[3];
    uint8_t rotOctave[3];
};

struct PitchBlock {
    uint8_t note[32];
    uint8_t spread;
    uint8_t scale;
    uint8_t root;
    uint8_t intervalMask;
    int8_t  shift;
    uint8_t hold;
    uint8_t rotate;
    uint8_t foldMode;
    int8_t  octave[32];
};

struct CurrentParams {
    uint16_t   bpm;
    ParamBlock data;
    uint8_t    epatSavedMask;
    uint8_t    epat[3][32];
    uint8_t    extClkMode;
    PitchBlock pitch;
    uint8_t    cvTargetMap[3];
    uint8_t    ratchetDecayVal;
};

struct SlotParams {
    ParamBlock data;
    uint8_t    epatSavedMask;
    uint8_t    epat[3][32];
    PitchBlock pitch;
};

struct EucParams {
    uint16_t     magic;
    CurrentParams data;
};

struct SlotsHeader {
    uint16_t magic;
    uint16_t usedMask;
};

struct SongsHeader {
    uint16_t magic;
    uint8_t  usedBits[13];  // bit i → song i used (100 songs ≤ 104 bits)
};

static int  pendingSlot = -1;
static bool pendingLoad = false;
static int  activeSlot  = -1;

// ---------------------------------------------------------------------------
// Pack / Unpack
// ---------------------------------------------------------------------------

static void packParamsCore(ParamBlock &p) {
    for (int i = 0; i < 3; i++) {
        p.len[i]  = (uint8_t)clampVal(PatLen[i], 1, 32);
        p.num[i]  = (uint8_t)clampVal(PatNum[i], 0, PatLen[i]);
        int rmax  = PatLen[i] > 0 ? PatLen[i] : 0;
        p.rot[i]  = (int8_t)clampVal(PatRot[i], -rmax, rmax);
        p.prob[i] = (uint8_t)clampVal(PatProb[i], 0, 20);
        p.probAuto[i] = PatProbAuto[i] ? 1 : 0;
        if (ProbEuclidRebuild[i])
            p.probAuto[i] = (uint8_t)(p.probAuto[i] | 0x02);
        for (int j = 0; j < 32; j++) {
            p.values[i][j]  = ValuesArr[i][j];
            p.gateLen[i][j] = GateLenArr[i][j];
            p.ratchet[i][j] = (uint8_t)clampVal((int)RatchetArr[i][j], 1, 4);
        }
        p.hold[i]      = (*HoldArr[i])     ? 1 : 0;
        p.gateHold[i]  = (*GateHoldArr[i]) ? 1 : 0;
        p.rotValues[i] = RotateValues[i]   ? 1 : 0;
        p.rotGateLen[i]= RotateGateLen[i]  ? 1 : 0;
        p.rotRatchet[i]= RotateRatchet[i]  ? 1 : 0;
        p.rotOctave[i] = RotateOctave[i]   ? 1 : 0;
        p.speed[i]     = (int8_t)clampVal(chSpeedIdx[i], -3, 3);
        p.autoRotate[i]= (uint8_t)clampVal((int)autoRotateStep[i], 0, 4);
    }
}

static void unpackParamsCore(const ParamBlock &p) {
    for (int i = 0; i < 3; i++) {
        PatLen[i]  = clampVal(p.len[i], 1, 32);
        PatNum[i]  = clampVal(p.num[i], 0, PatLen[i]);
        int rmax   = PatLen[i] > 0 ? PatLen[i] : 0;
        PatRot[i]  = clampVal((int)p.rot[i], -rmax, rmax);
        PatProb[i] = clampVal(p.prob[i], 0, 20);
        PatProbAuto[i]     = (p.probAuto[i] & 0x01) != 0;
        ProbEuclidRebuild[i] = (p.probAuto[i] & 0x02) != 0;
        for (int j = 0; j < 32; j++) {
            ValuesArr[i][j]  = p.values[i][j];
            GateLenArr[i][j] = p.gateLen[i][j];
            RatchetArr[i][j] = (uint8_t)clampVal((int)p.ratchet[i][j], 1, 4);
        }
        *HoldArr[i]      = (p.hold[i]      != 0);
        *GateHoldArr[i]  = (p.gateHold[i]  != 0);
        RotateValues[i]  = (p.rotValues[i]  != 0);
        RotateGateLen[i] = (p.rotGateLen[i] != 0);
        RotateRatchet[i] = (p.rotRatchet[i] != 0);
        RotateOctave[i]  = (p.rotOctave[i]  != 0);
        chSpeedIdx[i]    = clampVal((int)p.speed[i], -3, 3);
        autoRotateStep[i]= (uint8_t)clampVal((int)p.autoRotate[i], 0, 4);
    }
}

static void packPitch(PitchBlock &b) {
    b.spread       = pitchSpread;
    b.scale        = pitchScale;
    b.root         = pitchRoot;
    b.intervalMask = pitchIntervalMask;
    b.shift        = pitchShift;
    b.hold         = pitchHold   ? 1 : 0;
    b.rotate       = pitchRotate ? 1 : 0;
    b.foldMode     = (uint8_t)clampVal((int)pitchFoldMode, 0, 12);
    for (int i = 0; i < 32; i++) b.note[i]   = PitchNote1[i];
    for (int i = 0; i < 32; i++) b.octave[i] = (int8_t)clampVal((int)OctaveNote1[i], -3, 3);
}

static void unpackPitch(const PitchBlock &b) {
    pitchSpread       = (b.spread >= 1 && b.spread <= 5) ? b.spread : 2;
    pitchScale        = b.scale;
    pitchRoot         = b.root % 12;
    pitchIntervalMask = (b.intervalMask != 0) ? b.intervalMask : 0x07;
    pitchShift        = clampVal((int)b.shift, -3, 3);
    pitchHold         = (b.hold   != 0);
    pitchRotate       = (b.rotate != 0);
    pitchFoldMode     = (uint8_t)clampVal((int)b.foldMode, 0, 12);
    for (int i = 0; i < 32; i++) PitchNote1[i]  = b.note[i];
    for (int i = 0; i < 32; i++) OctaveNote1[i] = (int8_t)clampVal((int)b.octave[i], -3, 3);
}

static void packCurrent(CurrentParams &p) {
    p.bpm = (uint16_t)clampVal((int)bpm, 0, 600);
    packParamsCore(p.data);
    p.epatSavedMask = 0;
    for (int i = 0; i < 3; i++) {
        if (!PatProbAuto[i])
            p.epatSavedMask = (uint8_t)(p.epatSavedMask | (1u << i));
        for (int j = 0; j < 32; j++)
            p.epat[i][j] = (uint8_t)(EPatArr[i][j] ? 1 : 0);
    }
    p.extClkMode = extClockMode ? 1 : 0;
    packPitch(p.pitch);
    for (int i = 0; i < 3; i++) p.cvTargetMap[i] = cvTargetMap[i];
    p.ratchetDecayVal = ratchetDecay;
}

static void unpackCurrent(const CurrentParams &p) {
    bpm = (uint16_t)clampVal((int)p.bpm, 0, 600);
    unpackParamsCore(p.data);
    for (int i = 0; i < 3; i++) {
        bool hasSnapshot = (p.epatSavedMask & (1u << i)) != 0;
        if (hasSnapshot && !PatProbAuto[i]) {
            int len = clampVal(PatLen[i], 1, 32);
            for (int j = 0; j < len; j++) EPatArr[i][j] = (p.epat[i][j] != 0);
            for (int j = len; j < 32; j++) EPatArr[i][j] = false;
        } else {
            rebuildPattern(PatLen[i], PatNum[i], PatProb[i], EPatArr[i]);
        }
    }
    extClockMode = (p.extClkMode != 0);
    unpackPitch(p.pitch);
    for (int i = 0; i < 3; i++)
        cvTargetMap[i] = (p.cvTargetMap[i] < CV_TARGET_COUNT) ? p.cvTargetMap[i] : CV_TARGET_NONE;
    ratchetDecay = p.ratchetDecayVal;
}

static void packSlot(SlotParams &p) {
    packParamsCore(p.data);
    p.epatSavedMask = 0;
    for (int i = 0; i < 3; i++) {
        if (!PatProbAuto[i])
            p.epatSavedMask = (uint8_t)(p.epatSavedMask | (1u << i));
        for (int j = 0; j < 32; j++)
            p.epat[i][j] = (uint8_t)(EPatArr[i][j] ? 1 : 0);
    }
    packPitch(p.pitch);
}

static void unpackSlot(const SlotParams &p) {
    unpackParamsCore(p.data);
    for (int i = 0; i < 3; i++) {
        bool hasSnapshot = (p.epatSavedMask & (1u << i)) != 0;
        if (hasSnapshot && !PatProbAuto[i]) {
            int len = clampVal(PatLen[i], 1, 32);
            for (int j = 0; j < len; j++) EPatArr[i][j] = (p.epat[i][j] != 0);
            for (int j = len; j < 32; j++) EPatArr[i][j] = false;
        } else {
            rebuildPattern(PatLen[i], PatNum[i], PatProb[i], EPatArr[i]);
        }
    }
    unpackPitch(p.pitch);
}

// ---------------------------------------------------------------------------
// Slot-Header (SD)
// ---------------------------------------------------------------------------

static SlotsHeader readSlotsHeader() {
    SlotsHeader h = { SD_MAGIC_SLOTS, 0 };
    if (!sdOK) return h;
    File f = SD.open(SLOTS_HDR_PATH);
    if (f) {
        if (f.size() == sizeof(h)) f.read((uint8_t*)&h, sizeof(h));
        f.close();
        if (h.magic != SD_MAGIC_SLOTS) { h.magic = SD_MAGIC_SLOTS; h.usedMask = 0; }
    }
    return h;
}

static void writeSlotsHeader(const SlotsHeader &h) {
    if (!sdOK) return;
    SD.remove(SLOTS_HDR_PATH);
    File f = SD.open(SLOTS_HDR_PATH, FILE_WRITE);
    if (f) { f.write((uint8_t*)&h, sizeof(h)); f.close(); }
}

static SongsHeader readSongsHeader() {
    SongsHeader h;
    h.magic = SD_MAGIC_SONG;
    memset(h.usedBits, 0, sizeof(h.usedBits));
    if (!sdOK) return h;
    File f = SD.open(SONGS_HDR_PATH);
    if (f) {
        if (f.size() == sizeof(h)) f.read((uint8_t*)&h, sizeof(h));
        f.close();
        if (h.magic != SD_MAGIC_SONG) {
            h.magic = SD_MAGIC_SONG;
            memset(h.usedBits, 0, sizeof(h.usedBits));
        }
    }
    return h;
}

static void writeSongsHeader(const SongsHeader &h) {
    if (!sdOK) return;
    SD.remove(SONGS_HDR_PATH);
    File f = SD.open(SONGS_HDR_PATH, FILE_WRITE);
    if (f) { f.write((uint8_t*)&h, sizeof(h)); f.close(); }
}

// ---------------------------------------------------------------------------
// Öffentliche API
// ---------------------------------------------------------------------------

void initStorage() {
    sdOK = SD.begin(BUILTIN_SDCARD);
    if (Serial) Serial.printf("[storage] SD init: %s\n", sdOK ? "OK" : "FAIL");
}

void saveParams() {
    // EEPROM: schreibt nur geänderte Bytes → <1 ms typisch
    EucParams p;
    p.magic = EEPROM_MAGIC;
    packCurrent(p.data);
    EEPROM.put(0, p);
}

void loadParams() {
    EucParams p;
    EEPROM.get(0, p);
    if (p.magic == EEPROM_MAGIC) {
        unpackCurrent(p.data);
        return;
    }
    // Fallback: Defaultwerte
    bpm               = 120;
    ratchetDecay      = 0;
    extClockMode      = false;
    pitchSpread       = 2;
    pitchScale        = 0;
    pitchRoot         = 0;
    pitchIntervalMask = 0x07;
    pitchShift        = 0;
    pitchHold         = true;
    pitchRotate       = true;
    for (int i = 0; i < 32; i++) PitchNote1[i] = 0;
    for (int i = 0; i < 3; i++) {
        *HoldArr[i]        = true;
        *GateHoldArr[i]    = false;
        RotateValues[i]    = false;
        RotateGateLen[i]   = false;
        RotateRatchet[i]   = false;
        RotateOctave[i]    = false;
        PatProb[i]         = 10;
        PatProbAuto[i]     = false;
        ProbEuclidRebuild[i] = false;
        chSpeedIdx[i]      = 0;
        cvTargetMap[i]     = CV_TARGET_NONE;
    }
}

void scheduleSaveParams() {
    PendingSave   = true;
    PendingSaveAt = millis() + SAVE_DEBOUNCE_MS;
}

uint16_t getSlotsUsedMask() {
    return readSlotsHeader().usedMask;
}

bool saveParamsSlot(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) return false;
    if (!sdOK) return false;
    SlotParams p;
    packSlot(p);
    char path[16];
    getSlotPath(slot, path);
    SD.remove(path);  // SD FILE_WRITE appends — remove first to overwrite
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write((uint8_t*)&p, sizeof(p));
    f.close();
    if (written != sizeof(p)) return false;
    SlotsHeader h = readSlotsHeader();
    h.usedMask = (uint16_t)(h.usedMask | (1u << slot));
    writeSlotsHeader(h);
    return true;
}

bool deleteParamsSlot(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) return false;
    char path[16];
    getSlotPath(slot, path);
    if (sdOK) SD.remove(path);
    SlotsHeader h = readSlotsHeader();
    h.usedMask = (uint16_t)(h.usedMask & ~(1u << slot));
    writeSlotsHeader(h);
    if (activeSlot == slot) activeSlot = -1;
    return true;
}

// Kopiert alle Felder eines Kanals i aus src-ParamBlock nach dst.
static void copyParamChannel(ParamBlock &dst, const ParamBlock &src, int i) {
    dst.len[i]       = src.len[i];
    dst.num[i]       = src.num[i];
    dst.rot[i]       = src.rot[i];
    dst.prob[i]      = src.prob[i];
    dst.probAuto[i]  = src.probAuto[i];
    dst.hold[i]      = src.hold[i];
    dst.gateHold[i]  = src.gateHold[i];
    dst.rotValues[i] = src.rotValues[i];
    dst.rotGateLen[i]= src.rotGateLen[i];
    dst.rotRatchet[i]= src.rotRatchet[i];
    dst.rotOctave[i] = src.rotOctave[i];
    dst.speed[i]     = src.speed[i];
    dst.autoRotate[i]= src.autoRotate[i];
    for (int j = 0; j < 32; j++) {
        dst.values[i][j]  = src.values[i][j];
        dst.gateLen[i][j] = src.gateLen[i][j];
        dst.ratchet[i][j] = src.ratchet[i][j];
    }
}

// Kopiert Kanäle laut copyMask (bit i=1 → Kanal i von src übernehmen) in dst-Slot.
// Quell-Slot bleibt erhalten. Wenn dst leer: kompletter Copy unabhängig von Maske.
bool mergeParamsSlot(int src, int dst, uint8_t copyMask) {
    if (src < 0 || src >= SLOT_COUNT || dst < 0 || dst >= SLOT_COUNT) return false;
    if (src == dst) return true;
    if (!sdOK) return false;
    char srcPath[16], dstPath[16];
    getSlotPath(src, srcPath);
    getSlotPath(dst, dstPath);

    SlotParams srcP;
    {
        File f = SD.open(srcPath);
        if (!f) return false;
        bool ok = (f.size() == sizeof(srcP));
        if (ok) f.read((uint8_t*)&srcP, sizeof(srcP));
        f.close();
        if (!ok) return false;
    }

    SlotParams result = srcP;  // default: vollständiger Copy
    if (copyMask != 0x07) {
        // Partieller Copy: Ziel laden und Kanäle selektiv überschreiben
        File f = SD.open(dstPath);
        if (f && f.size() == sizeof(result)) {
            f.read((uint8_t*)&result, sizeof(result));
            f.close();
            for (int i = 0; i < 3; i++) {
                if (!(copyMask & (1u << i))) continue;  // gemuted = Ziel behalten
                copyParamChannel(result.data, srcP.data, i);
                result.epatSavedMask &= ~(uint8_t)(1u << i);
                if (srcP.epatSavedMask & (1u << i)) {
                    result.epatSavedMask |= (uint8_t)(1u << i);
                    for (int j = 0; j < 32; j++) result.epat[i][j] = srcP.epat[i][j];
                }
            }
            if (copyMask & 0x01) result.pitch = srcP.pitch;  // Pitch gehört zu Ch1
        } else {
            if (f) f.close();
            // Ziel leer/ungültig → kompletter Copy (result = srcP bereits)
        }
    }

    SD.remove(dstPath);
    File fout = SD.open(dstPath, FILE_WRITE);
    if (!fout) return false;
    fout.write((uint8_t*)&result, sizeof(result));
    fout.close();

    SlotsHeader h = readSlotsHeader();
    h.usedMask = (uint16_t)(h.usedMask | (1u << dst));
    writeSlotsHeader(h);
    return true;
}

static bool loadParamsSlotNow(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) return false;
    SlotsHeader h = readSlotsHeader();
    if ((h.usedMask & (1u << slot)) == 0) return false;
    if (!sdOK) return false;
    char path[16];
    getSlotPath(slot, path);
    SlotParams p;
    File f = SD.open(path);
    if (!f) return false;
    bool ok = (f.size() == sizeof(p));
    if (ok) f.read((uint8_t*)&p, sizeof(p));
    f.close();
    if (!ok) return false;
    unpackSlot(p);
    activeSlot = slot;
    return true;
}

bool requestLoadSlot(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) return false;
    pendingSlot = slot;
    pendingLoad = true;
    return true;
}

bool applyPendingLoadIfReady(unsigned int step, bool forceNow) {
    if (!pendingLoad) return false;
    if (PatLen[0] <= 0) { pendingLoad = false; return false; }
    if (!forceNow && (step % (unsigned int)PatLen[0]) != 0) return false;
    bool ok = loadParamsSlotNow(pendingSlot);
    pendingLoad = false;
    pendingSlot = -1;
    return ok;
}

int getActiveSlot() {
    return activeSlot;
}

// ---------------------------------------------------------------------------
// Song-Speicher (100 Songs × 16 Slots)
// ---------------------------------------------------------------------------

bool saveSong(int songNum) {
    if (songNum < 0 || songNum >= SONG_COUNT) return false;
    if (!sdOK) return false;

    SlotsHeader sh = readSlotsHeader();
    char path[16];
    getSongPath(songNum, path);
    SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;

    // 4-byte header: magic + usedSlotMask
    uint16_t hdr[2] = { SD_MAGIC_SONG, sh.usedMask };
    f.write((uint8_t*)hdr, sizeof(hdr));

    // 16 SlotParams, read from each slot_XX.bin if it exists
    for (int i = 0; i < SLOT_COUNT; i++) {
        SlotParams p;
        memset(&p, 0, sizeof(p));
        if (sh.usedMask & (uint16_t)(1u << i)) {
            char sp[16];
            getSlotPath(i, sp);
            File sf = SD.open(sp);
            if (sf) {
                if (sf.size() == sizeof(p)) sf.read((uint8_t*)&p, sizeof(p));
                sf.close();
            }
        }
        f.write((uint8_t*)&p, sizeof(p));
    }
    f.close();

    SongsHeader sh2 = readSongsHeader();
    sh2.usedBits[songNum / 8] = (uint8_t)(sh2.usedBits[songNum / 8] | (1u << (songNum % 8)));
    writeSongsHeader(sh2);
    return true;
}

bool loadSong(int songNum) {
    if (songNum < 0 || songNum >= SONG_COUNT) return false;
    if (!sdOK) return false;

    char path[16];
    getSongPath(songNum, path);
    File f = SD.open(path);
    if (!f) return false;

    uint16_t hdr[2];
    if (f.size() < sizeof(hdr)) { f.close(); return false; }
    f.read((uint8_t*)hdr, sizeof(hdr));
    if (hdr[0] != SD_MAGIC_SONG) { f.close(); return false; }
    uint16_t mask = hdr[1];

    for (int i = 0; i < SLOT_COUNT; i++) {
        SlotParams p;
        f.read((uint8_t*)&p, sizeof(p));
        char sp[16];
        getSlotPath(i, sp);
        SD.remove(sp);
        if (mask & (uint16_t)(1u << i)) {
            File sf = SD.open(sp, FILE_WRITE);
            if (sf) { sf.write((uint8_t*)&p, sizeof(p)); sf.close(); }
        }
    }
    f.close();

    SlotsHeader sh = { SD_MAGIC_SLOTS, mask };
    writeSlotsHeader(sh);
    activeSlot = -1;
    return true;
}

bool deleteSong(int songNum) {
    if (songNum < 0 || songNum >= SONG_COUNT) return false;
    char path[16];
    getSongPath(songNum, path);
    if (sdOK) SD.remove(path);
    SongsHeader h = readSongsHeader();
    h.usedBits[songNum / 8] = (uint8_t)(h.usedBits[songNum / 8] & ~(1u << (songNum % 8)));
    writeSongsHeader(h);
    return true;
}

bool getSongUsedBit(int songNum) {
    if (songNum < 0 || songNum >= SONG_COUNT) return false;
    SongsHeader h = readSongsHeader();
    return (h.usedBits[songNum / 8] & (1u << (songNum % 8))) != 0;
}
