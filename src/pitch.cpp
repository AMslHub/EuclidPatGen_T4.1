#include <pitch.h>

struct ScaleDef {
    const char *name;
    uint8_t semitones[12];  // Halbtonabstaende; 255 = nicht belegt
    uint8_t count;
};

static const ScaleDef SCALES[] = {
    // --- Westliche Modi ---
    { "Major",      { 0, 2, 4, 5, 7, 9,11,255,255,255,255,255 }, 7  },
    { "Minor",      { 0, 2, 3, 5, 7, 8,10,255,255,255,255,255 }, 7  },
    { "Dorian",     { 0, 2, 3, 5, 7, 9,10,255,255,255,255,255 }, 7  },
    { "Phrygian",   { 0, 1, 3, 5, 7, 8,10,255,255,255,255,255 }, 7  },
    { "Lydian",     { 0, 2, 4, 6, 7, 9,11,255,255,255,255,255 }, 7  },
    { "Mixolydian", { 0, 2, 4, 5, 7, 9,10,255,255,255,255,255 }, 7  },
    { "Locrian",    { 0, 1, 3, 5, 6, 8,10,255,255,255,255,255 }, 7  },
    { "Harm. Min",  { 0, 2, 3, 5, 7, 8,11,255,255,255,255,255 }, 7  },
    { "Lydian Dom", { 0, 2, 4, 6, 7, 9,10,255,255,255,255,255 }, 7  },
    { "Altered",    { 0, 1, 3, 4, 6, 8,10,255,255,255,255,255 }, 7  },
    // --- Pentatonik & Sonderformen ---
    { "Penta Maj",  { 0, 2, 4, 7, 9,255,255,255,255,255,255,255 }, 5 },
    { "Penta Min",  { 0, 3, 5, 7,10,255,255,255,255,255,255,255 }, 5 },
    { "Blues",      { 0, 3, 5, 6, 7,10,255,255,255,255,255,255 }, 6  },
    { "Whole Tone", { 0, 2, 4, 6, 8,10,255,255,255,255,255,255 }, 6  },
    { "Chromatic",  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 }, 12 },
    // --- Gypsy / Naher Osten ---
    { "Hungarian",  { 0, 2, 3, 6, 7, 8,11,255,255,255,255,255 }, 7  },  // Gypsy-Moll
    { "Gypsy Maj",  { 0, 1, 4, 5, 7, 8,10,255,255,255,255,255 }, 7  },  // Phryg. Dominant
    { "Dbl Harm",   { 0, 1, 4, 5, 7, 8,11,255,255,255,255,255 }, 7  },  // Byzantinisch
    { "Persian",    { 0, 1, 4, 5, 6, 8,11,255,255,255,255,255 }, 7  },
    // --- Indische Ragas ---
    { "Raga Todi",  { 0, 1, 3, 6, 7, 8,11,255,255,255,255,255 }, 7  },  // komal Re+Ga, tivra Ma
    { "Raga Purvi", { 0, 1, 4, 6, 7, 8,11,255,255,255,255,255 }, 7  },  // shuddha Ga, tivra Ma
    { "Raga Marwa", { 0, 1, 4, 6, 9,11,255,255,255,255,255,255 }, 6  },  // kein Pancham (P5)
    // --- Japanisch ---
    { "Hirajoshi",  { 0, 2, 3, 7, 8,255,255,255,255,255,255,255 }, 5  },  // Koto-Skala
    { "Insen",      { 0, 1, 5, 7,10,255,255,255,255,255,255,255 }, 5  },
    { "Yo",         { 0, 2, 5, 7, 9,255,255,255,255,255,255,255 }, 5  },
};
const int SCALE_COUNT = (int)(sizeof(SCALES) / sizeof(SCALES[0]));

// ---------------------------------------------------------------------------
// Pitch-Presets: vorgefertigte 32-Step Sequenzen (Raw-Werte 0-255).
// Werden durch die aktuelle Scale/Root/Spread-Einstellung quantisiert.
// ---------------------------------------------------------------------------
struct PitchPreset {
    const char *name;
    uint8_t note[32];
};

static const PitchPreset PITCH_PRESETS[] = {
    { "Berlin Arp",   {   // Sanfter Bogen ueber zwei Oktaven (Tangerine Dream)
        0,18,36,72,108,127,163,200,218,236,200,163,127,108,72,36,
        0,18,36,72,108,127,163,200,218,236,200,163,127,108,72,36 } },
    { "Acid 303",     {   // TB-303 Stil: Root-schwer mit Spruengen
        0,0,108,0,72,0,108,36,0,0,127,0,72,108,36,0,
        0,0,108,0,72,0,108,36,0,0,127,0,72,108,36,0 } },
    { "Techno Pump",  {   // Oktav-Pumpen mit Quint-Variation
        0,127,0,127,0,127,0,127,72,200,72,200,36,127,72,200,
        0,127,0,127,0,127,0,127,72,200,72,200,36,127,72,200 } },
    { "Cascade",      {   // Absteigendes Wasserfall-Motiv
        236,218,200,181,163,145,127,108,90,72,54,36,18,0,36,72,
        236,218,200,181,163,145,127,108,90,72,54,36,18,0,36,72 } },
    { "Raga Walk",    {   // Schrittweises Rauf/Runter (indischer Stil)
        0,18,36,54,72,90,108,127,145,163,145,127,108,90,72,54,
        36,18,0,18,36,54,72,90,108,127,108,90,72,54,36,18 } },
    { "Drone",        {   // Grundton-Bordun mit Farbtupfern
        0,0,0,72,0,0,0,127,0,0,72,0,36,0,0,108,
        0,0,0,72,0,0,0,127,0,0,72,0,36,0,0,108 } },
    { "Pentatone",    {   // Pentatonischer Bogen, 8-Step
        0,36,72,127,163,127,72,36,0,36,72,127,163,127,72,36,
        0,36,72,127,163,127,72,36,0,36,72,127,163,127,72,36 } },
    { "Minimal",      {   // 4-Step Techno-Minimal-Loop
        0,72,36,108,0,72,36,108,0,72,36,108,0,72,36,108,
        0,72,36,108,0,72,36,108,0,72,36,108,0,72,36,108 } },
    { "Staircase",    {   // Stufenpyramide mit Sprung
        0,0,72,72,127,127,200,200,0,0,72,72,127,127,200,200,
        0,0,72,72,127,127,200,200,0,0,72,72,127,127,200,200 } },
    { "Sawtooth",     {   // Lineare Aufwaertsrampe (Saegezahn)
        0,8,16,24,32,48,64,80,96,112,128,144,160,180,210,255,
        0,8,16,24,32,48,64,80,96,112,128,144,160,180,210,255 } },
    { "Rev. Saw",     {   // Lineare Abwaertsrampe
        255,210,180,160,144,128,112,96,80,64,48,32,24,16,8,0,
        255,210,180,160,144,128,112,96,80,64,48,32,24,16,8,0 } },
    { "Leapfrog",     {   // Tief/Hoch abwechselnd, spiralfoermig nach oben
        0,127,18,145,36,163,54,181,72,200,90,218,108,236,127,0,
        0,127,18,145,36,163,54,181,72,200,90,218,108,236,127,0 } },
    { "Waltz",        {   // Klassischer Oom-Pah-Pah Bassgang
        0,72,90,0,72,90,127,72,90,0,72,90,36,72,90,127,
        0,72,90,0,72,90,127,72,90,0,72,90,36,72,90,127 } },
    { "Alberti",      {   // Alberti-Bass: Root-Quint-Terz-Quint
        0,72,36,72,0,72,36,72,127,200,163,200,0,72,36,72,
        0,72,36,72,0,72,36,72,127,200,163,200,0,72,36,72 } },
    { "Jazz Walk",    {   // Walking-Bass: stufenweise mit Chromatikhalbton
        0,36,72,108,127,90,54,36,72,108,127,163,145,108,90,72,
        0,36,72,108,127,90,54,36,72,108,127,163,145,108,90,72 } },
    { "Celtic Arp",   {   // Keltische Harfe: springend und energetisch
        0,72,127,72,36,72,127,200,127,72,36,72,0,36,72,36,
        0,72,127,72,36,72,127,200,127,72,36,72,0,36,72,36 } },
    { "Gamelan",      {   // Balinesisches Gamelan: zyklisch-pentatonisch
        0,90,36,72,127,36,90,0,163,72,36,90,0,127,72,36,
        0,90,36,72,127,36,90,0,163,72,36,90,0,127,72,36 } },
    { "Balkan",       {   // Unregelmaessige Schritte, osteuropaeisch
        0,36,54,72,54,36,72,108,0,54,36,72,36,0,108,72,
        0,36,54,72,54,36,72,108,0,54,36,72,36,0,108,72 } },
    { "S & H",        {   // Sample & Hold: grosse Sprünge, zufaellig wirkend
        0,200,36,218,72,163,18,236,127,54,181,36,200,0,145,90,
        0,200,36,218,72,163,18,236,127,54,181,36,200,0,145,90 } },
    { "Heartbeat",    {   // Stark/Schwach-Doppelschlag in zwei Oktaven
        0,36,0,18,0,36,0,18,127,163,127,108,127,163,127,108,
        0,36,0,18,0,36,0,18,127,163,127,108,127,163,127,108 } },
};
const int PITCH_PRESET_COUNT = (int)(sizeof(PITCH_PRESETS) / sizeof(PITCH_PRESETS[0]));

const char *getPitchPresetName(int idx) {
    return PITCH_PRESETS[((idx % PITCH_PRESET_COUNT) + PITCH_PRESET_COUNT) % PITCH_PRESET_COUNT].name;
}

void getPitchPresetNotes(int idx, uint8_t *dest32) {
    const uint8_t *src = PITCH_PRESETS[((idx % PITCH_PRESET_COUNT) + PITCH_PRESET_COUNT) % PITCH_PRESET_COUNT].note;
    for (int i = 0; i < 32; i++) dest32[i] = src[i];
}

// IntervalMask-Bit i -> Skalengrad
// Buttons zeigen: "1","3","5","7","9","11","13"
// bit 0="1"->Grad 0 (Prim/Root)
// bit 1="3"->Grad 2 (Terz)
// bit 2="5"->Grad 4 (Quinte)
// bit 3="7"->Grad 6 (Septime)
// bit 4="9"->Grad 1 (None)
// bit 5="11"->Grad 3 (Undezime)
// bit 6="13"->Grad 5 (Tredezime)
static const uint8_t DEGREE_FOR_BIT[7] = { 0, 2, 4, 6, 1, 3, 5 };

// Skalengrad -> zugehoeriges Mask-Bit (Umkehrung von DEGREE_FOR_BIT)
static const uint8_t BIT_FOR_DEGREE[7] = { 0, 4, 1, 5, 2, 6, 3 };

static const char *const INTERVAL_LABELS[7] = { "1","3","5","7","9","11","13" };

const char *getScaleName(uint8_t scaleIdx) {
    return SCALES[scaleIdx % SCALE_COUNT].name;
}

uint8_t getScaleDegreeCount(uint8_t scaleIdx) {
    return SCALES[scaleIdx % SCALE_COUNT].count;
}

const char *getIntervalLabel(int i) {
    if (i < 0 || i >= 7) return "";
    return INTERVAL_LABELS[i];
}

bool intervalExists(uint8_t scaleIdx, int i) {
    if (i < 0 || i >= 7) return false;
    scaleIdx = scaleIdx % (uint8_t)SCALE_COUNT;
    const ScaleDef &sc = SCALES[scaleIdx];
    if (sc.count == 12) return true;  // chromatisch: alle 7 Tasten nutzbar
    uint8_t deg = DEGREE_FOR_BIT[i];
    return (deg < sc.count && sc.semitones[deg] != 255);
}

int buildNoteList(uint8_t spread, uint8_t scaleIdx, uint8_t root,
                  uint8_t intervalMask, int *notes) {
    if (spread < 1) spread = 1;
    if (spread > 5) spread = 5;
    scaleIdx = scaleIdx % (uint8_t)SCALE_COUNT;
    root = root % 12;

    const ScaleDef &sc = SCALES[scaleIdx];
    int noteCount = 0;
    int baseMidi = 36 + (int)root;  // C2 + Grundton-Offset

    if (sc.count == 12) {
        // Chromatisch: alle Halbtöne, intervalMask wird ignoriert
        for (int oct = 0; oct < (int)spread; oct++) {
            for (int semi = 0; semi < 12; semi++) {
                notes[noteCount++] = baseMidi + oct * 12 + semi;
            }
        }
        return noteCount;
    }

    // Diatonische Skalen: aufsteigend iterieren, intervalMask filtert Grade
    for (int oct = 0; oct < (int)spread; oct++) {
        for (int deg = 0; deg < 7; deg++) {
            uint8_t bit = BIT_FOR_DEGREE[deg];
            if (!(intervalMask & (1u << bit))) continue;
            uint8_t semi = sc.semitones[deg];
            if (semi == 255) continue;
            notes[noteCount++] = baseMidi + oct * 12 + (int)semi;
        }
    }
    return noteCount;
}

int quantizeToMidi(uint8_t rawValue, uint8_t spread, uint8_t scaleIdx,
                   uint8_t root, uint8_t intervalMask) {
    int noteList[60];  // max: Chromatic 12 × 5 octaves = 60
    int noteCount = buildNoteList(spread, scaleIdx, root, intervalMask, noteList);
    if (noteCount == 0) return 36 + (int)(root % 12);  // Fallback: Grundton C2

    int idx = ((int)rawValue * noteCount) / 256;
    if (idx >= noteCount) idx = noteCount - 1;
    return noteList[idx];
}

uint16_t midiToDac(int midiNote) {
    int semitones = midiNote - 36;  // Halbtoene ueber C2
    if (semitones < 0) semitones = 0;
    // DAC 4095 = 2.048V -> x2.52 Amp (Rg=100k, Rf=130k+22k-Trimmer max) -> 5.16V
    // 1V/Oct: Pro Halbton = 1/12 V = 5.16V/4095 * X → X = 4095/(12*2.048*2.52) ≈ 66.1
    // Integer: 66.1 * 792 = 52351 → Konstante 52350
    // Trimmer (0..22k) erlaubt Feinkalibrierung; bei vollem Trimmer (G=2.52) exakt 1V/Oct.

    //uint32_t dac = (uint32_t)semitones * 52350u / 792u;
    uint32_t dac = (uint32_t)semitones * 53000u / 792u;

    if (dac > 4095) dac = 4095;
    return (uint16_t)dac;
}

uint16_t computePitchDac(uint8_t rawValue, uint8_t spread, uint8_t scaleIdx,
                          uint8_t root, uint8_t intervalMask) {
    return midiToDac(quantizeToMidi(rawValue, spread, scaleIdx, root, intervalMask));
}
