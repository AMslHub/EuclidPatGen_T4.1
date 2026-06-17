#include <pitch.h>
#include <string.h>
#include <app_state.h>

struct ScaleDef {
    const char *name;
    uint8_t semitones[12];  // Halbtonabstaende; 255 = nicht belegt
    uint8_t count;
};

static const ScaleDef SCALES[] = {
    // A
    { "8-Tone Span", { 0, 1, 2, 3, 5, 6, 8,10,255,255,255,255 }, 8  },  // 8-Tone Spanish
    { "Acoustic",    { 0, 2, 4, 6, 7, 9,10,255,255,255,255,255 }, 7  },  // =Lydian Dominant
    { "Adonai Mal.", { 0, 2, 4, 5, 7, 8, 9,10,255,255,255,255 }, 8  },  // Adonai Malakh (8-Ton)
    { "Aeolian",     { 0, 2, 3, 5, 7, 8,10,255,255,255,255,255 }, 7  },  // =Natural Minor
    { "Algerian",    { 0, 2, 3, 6, 7, 8,11,255,255,255,255,255 }, 7  },  // =Hungarian Minor variant
    { "Altered",     { 0, 1, 3, 4, 6, 8,10,255,255,255,255,255 }, 7  },
    { "Augmented",   { 0, 3, 4, 7, 8,11,255,255,255,255,255,255 }, 6  },  // sym. Augmented
    // B
    { "Bebop Dom",   { 0, 2, 4, 5, 7, 9,10,11,255,255,255,255 }, 8  },  // Bebop Dominant
    { "Bhairav",     { 0, 1, 4, 5, 7, 8,11,255,255,255,255,255 }, 7  },  // =Dbl Harmonic / Byzantine
    { "Blues",       { 0, 3, 5, 6, 7,10,255,255,255,255,255,255 }, 6  },
    // C
    { "Chromatic",   { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 }, 12 },
    // D
    { "Dbl Harm",    { 0, 1, 4, 5, 7, 8,11,255,255,255,255,255 }, 7  },  // Double Harmonic / Byzantine
    { "Dorian",      { 0, 2, 3, 5, 7, 9,10,255,255,255,255,255 }, 7  },
    { "Dorian #4",   { 0, 2, 3, 6, 7, 9,10,255,255,255,255,255 }, 7  },  // Dorian #4 / Ukr. Dorian
    // E
    { "Enigmatic",   { 0, 1, 4, 6, 8,10,11,255,255,255,255,255 }, 7  },
    // F
    { "Flamenco",    { 0, 1, 3, 4, 5, 7, 8,10,255,255,255,255 }, 8  },  // Flamenco Mode (8-Ton)
    { "Freygish",    { 0, 1, 4, 5, 7, 8,10,255,255,255,255,255 }, 7  },  // =Phryg. Dom. / Ahava Raba
    // G
    { "Gypsy Maj",   { 0, 1, 4, 5, 7, 8,10,255,255,255,255,255 }, 7  },  // Phrygian Dominant
    // H
    { "Half-Whole",  { 0, 1, 3, 4, 6, 7, 9,10,255,255,255,255 }, 8  },  // Half-Whole Diminished
    { "Harm. Maj",   { 0, 2, 4, 5, 7, 8,11,255,255,255,255,255 }, 7  },  // Harmonic Major
    { "Harm. Min",   { 0, 2, 3, 5, 7, 8,11,255,255,255,255,255 }, 7  },  // Harmonic Minor
    { "Hirajoshi",   { 0, 2, 3, 7, 8,255,255,255,255,255,255,255 }, 5  },
    { "Hungarian",   { 0, 2, 3, 6, 7, 8,11,255,255,255,255,255 }, 7  },  // Hungarian / Gypsy Minor
    // I
    { "In",          { 0, 1, 5, 7, 8,255,255,255,255,255,255,255 }, 5  },  // In Scale (Japan.)
    { "Insen",       { 0, 1, 5, 7,10,255,255,255,255,255,255,255 }, 5  },
    { "Istrian",     { 0, 1, 3, 4, 6, 7,255,255,255,255,255,255 }, 6  },  // Istrian Scale
    { "Iwato",       { 0, 1, 5, 6,10,255,255,255,255,255,255,255 }, 5  },  // Japan. Pentatonik
    // K
    { "Kumoi",       { 0, 2, 3, 7, 9,255,255,255,255,255,255,255 }, 5  },  // Japan. Pentatonik
    // L
    { "Locrian",     { 0, 1, 3, 5, 6, 8,10,255,255,255,255,255 }, 7  },
    { "Locrian Sup", { 0, 1, 3, 4, 6, 8,10,255,255,255,255,255 }, 7  },  // Locrian Super / Altered
    { "Lydian",      { 0, 2, 4, 6, 7, 9,11,255,255,255,255,255 }, 7  },
    { "Lydian Aug",  { 0, 2, 4, 6, 8, 9,11,255,255,255,255,255 }, 7  },  // Lydian Augmented
    { "Lydian Dom",  { 0, 2, 4, 6, 7, 9,10,255,255,255,255,255 }, 7  },  // Lydian Dominant
    // M
    { "Maj Locrn",   { 0, 2, 4, 5, 6, 8,10,255,255,255,255,255 }, 7  },  // Major Locrian
    { "Major",       { 0, 2, 4, 5, 7, 9,11,255,255,255,255,255 }, 7  },
    { "Mel. Min D",  { 0, 2, 3, 5, 7, 8,10,255,255,255,255,255 }, 7  },  // Melodic Minor Descending (=Aeolian)
    { "Mel. Min U",  { 0, 2, 3, 5, 7, 9,11,255,255,255,255,255 }, 7  },  // Melodic Minor Ascending
    { "Messiaen 3",  { 0, 2, 3, 4, 6, 7, 8,10,11,255,255,255 }, 9  },  // Mode of Ltd Transposition 3
    { "Messiaen 4",  { 0, 1, 2, 5, 6, 7, 8,11,255,255,255,255 }, 8  },  // Mode of Ltd Transposition 4
    { "Messiaen 5",  { 0, 1, 5, 6, 7,11,255,255,255,255,255,255 }, 6  },  // Mode of Ltd Transposition 5
    { "Messiaen 6",  { 0, 2, 4, 5, 6, 8,10,11,255,255,255,255 }, 8  },  // Mode of Ltd Transposition 6
    { "Messiaen 7",  { 0, 1, 2, 3, 5, 6, 7, 8, 9,11,255,255 }, 10 },  // Mode of Ltd Transposition 7
    { "Mi Sheberach",{ 0, 2, 3, 6, 7, 9,10,255,255,255,255,255 }, 7  },  // Mi Sheberach / Dorian #4
    { "Minor",       { 0, 2, 3, 5, 7, 8,10,255,255,255,255,255 }, 7  },
    { "Minor Blues", { 0, 3, 5, 6, 7,10,255,255,255,255,255,255 }, 6  },  // =Blues
    { "Mixolydian",  { 0, 2, 4, 5, 7, 9,10,255,255,255,255,255 }, 7  },
    // N
    { "Neapol. Maj", { 0, 1, 3, 5, 7, 9,11,255,255,255,255,255 }, 7  },  // Neapolitan Major
    { "Neapol. Min", { 0, 1, 3, 5, 7, 8,11,255,255,255,255,255 }, 7  },  // Neapolitan Minor
    // P
    { "Pelog Sel.",  { 0, 1, 3, 7, 8,255,255,255,255,255,255,255 }, 5  },  // Pelog Selisir
    { "Pelog Tem.",  { 0, 1, 3, 6, 8,255,255,255,255,255,255,255 }, 5  },  // Pelog Tembung
    { "Penta Maj",   { 0, 2, 4, 7, 9,255,255,255,255,255,255,255 }, 5  },
    { "Penta Min",   { 0, 3, 5, 7,10,255,255,255,255,255,255,255 }, 5  },
    { "Persian",     { 0, 1, 4, 5, 6, 8,11,255,255,255,255,255 }, 7  },
    { "Phrygian",    { 0, 1, 3, 5, 7, 8,10,255,255,255,255,255 }, 7  },
    { "Phrygian Dom",{ 0, 1, 4, 5, 7, 8,10,255,255,255,255,255 }, 7  },  // =Freygish / Gypsy Maj
    { "Prometheus",  { 0, 2, 4, 6, 9,10,255,255,255,255,255,255 }, 6  },  // Prometheus / Scriabin
    // R
    { "Raga Marwa",  { 0, 1, 4, 6, 9,11,255,255,255,255,255,255 }, 6  },
    { "Raga Purvi",  { 0, 1, 4, 6, 7, 8,11,255,255,255,255,255 }, 7  },
    { "Raga Todi",   { 0, 1, 3, 6, 7, 8,11,255,255,255,255,255 }, 7  },
    // S
    { "Slendro",     { 0, 2, 5, 7, 9,255,255,255,255,255,255,255 }, 5  },  // Javanisch Pentatonik
    // T
    { "Tritone",     { 0, 1, 4, 6, 7,10,255,255,255,255,255,255 }, 6  },  // Tritone Scale
    // U
    { "Ukr. Dorian", { 0, 2, 3, 6, 7, 9,10,255,255,255,255,255 }, 7  },  // Ukrainian Dorian
    // W
    { "Whole-Half",  { 0, 2, 3, 5, 6, 8, 9,11,255,255,255,255 }, 8  },  // Whole-Half Diminished
    { "Whole Tone",  { 0, 2, 4, 6, 8,10,255,255,255,255,255,255 }, 6  },
    // Y
    { "Yo",          { 0, 2, 5, 7, 9,255,255,255,255,255,255,255 }, 5  },
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
    // --- A ---
    { "Acid 1",      {   // TB-303 klassisch: Root-schwer mit Quinten-Spruengen
        0,0,108,0,72,0,108,36, 0,0,127,0,72,108,36,0,
        0,0,108,0,72,0,108,36, 0,0,127,0,72,108,36,0 } },
    { "Acid 2",      {   // TB-303 mid: denser, mehr Bewegung um den Grundton
        0,72,108,72,36,108,72,36, 0,72,127,72,36,108,72,0,
        0,72,108,72,36,108,72,36, 0,72,127,72,36,108,72,0 } },
    { "Acid 3",      {   // TB-303 aggressiv: weite Sprünge, hohe Energie
        0,200,72,218,36,181,108,236, 0,200,72,218,36,181,108,0,
        0,200,72,218,36,181,108,236, 0,200,72,218,36,181,108,0 } },
    { "Alberti",     {   // Alberti-Bass: Root-Quint-Terz-Quint
        0,72,36,72,0,72,36,72,127,200,163,200,0,72,36,72,
        0,72,36,72,0,72,36,72,127,200,163,200,0,72,36,72 } },
    // --- B ---
    { "Balkan",      {   // Unregelmaessige Schritte, osteuropaeisch
        0,36,54,72,54,36,72,108,0,54,36,72,36,0,108,72,
        0,36,54,72,54,36,72,108,0,54,36,72,36,0,108,72 } },
    { "Berlin 1",    {   // Sanfter Bogen ueber zwei Oktaven (Tangerine Dream)
        0,18,36,72,108,127,163,200,218,236,200,163,127,108,72,36,
        0,18,36,72,108,127,163,200,218,236,200,163,127,108,72,36 } },
    { "Berlin 2",    {   // Asymmetrischer Bogen, driftend nach oben
        0,36,72,127,200,163,127,72, 36,72,108,163,218,200,163,127,
        72,108,145,200,236,200,163,127, 108,145,181,236,218,181,145,108 } },
    { "Berlin 3",    {   // Dramatisch, breiter Sweep mit Klimax
        0,18,54,127,200,236,200,127, 54,18,0,36,108,200,236,218,
        163,127,72,36,0,54,127,200, 236,254,236,200,163,127,54,18 } },
    { "Brwn Fast",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Brwn Mid",    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Brwn Slow",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    // --- C ---
    { "Cascade",     {   // Absteigendes Wasserfall-Motiv
        236,218,200,181,163,145,127,108,90,72,54,36,18,0,36,72,
        236,218,200,181,163,145,127,108,90,72,54,36,18,0,36,72 } },
    { "Celtic Arp",  {   // Keltische Harfe: springend und energetisch
        0,72,127,72,36,72,127,200,127,72,36,72,0,36,72,36,
        0,72,127,72,36,72,127,200,127,72,36,72,0,36,72,36 } },
    { "Chicago 1",   {   // House: sparse, hypnotisch, Root-getrieben
        0,0,72,0, 0,72,0,108, 0,0,72,0, 0,72,127,0,
        0,0,72,0, 0,72,0,108, 0,0,72,0, 0,72,127,0 } },
    { "Chicago 2",   {   // House: waermer, gospel-beeinflusst
        0,72,127,108,72,108,127,163, 0,72,127,108,72,36,72,108,
        0,72,127,108,72,108,127,163, 0,72,127,108,72,36,72,108 } },
    { "Chicago 3",   {   // House: aufbauend, mehr melodische Energie
        0,36,72,108,127,108,72,127, 0,72,108,127,163,127,108,72,
        0,36,72,108,127,163,200,163, 127,108,72,36,72,108,127,108 } },
    // --- D ---
    { "Detroit 1",   {   // Techno: sehr sparse, industriell, Stille als Element
        0,0,0,200, 0,0,0,127, 0,0,0,218, 0,0,0,72,
        0,0,0,200, 0,0,0,127, 0,0,0,218, 0,0,0,72 } },
    { "Detroit 2",   {   // Techno: sparse mit leichter Variation in der zweiten Haelfte
        0,0,127,0, 0,0,72,0, 0,0,127,0, 36,0,72,0,
        0,0,127,0, 0,0,72,0, 0,0,163,0, 36,0,108,0 } },
    { "Detroit 3",   {   // Techno: haerterer Drive, rhythmischer Puls
        0,127,0,200, 72,0,127,0, 181,0,127,36, 0,200,72,127,
        0,127,0,200, 72,0,127,0, 181,0,127,36, 0,200,72,127 } },
    { "Drone",       {   // Grundton-Bordun mit Farbtupfern
        0,0,0,72,0,0,0,127,0,0,72,0,36,0,0,108,
        0,0,0,72,0,0,0,127,0,0,72,0,36,0,0,108 } },
    { "Drunk Walk",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    // --- F ---
    { "Floating",    {   // Weite Intervalle, schwebend, ambient
        0,163,72,218, 36,200,127,181, 72,236,108,163, 0,218,90,127,
        0,163,72,218, 36,200,127,181, 72,236,108,163, 0,218,90,127 } },
    // --- G ---
    { "Gallop",      {   // Tief-Hoch Paare, aggressiv-energetisch
        0,0,218,218, 36,36,200,200, 0,0,218,218, 72,72,200,200,
        0,0,218,218, 36,36,200,200, 0,0,218,218, 72,72,200,200 } },
    { "Gamelan",     {   // Balinesisches Gamelan: zyklisch-pentatonisch
        0,90,36,72,127,36,90,0,163,72,36,90,0,127,72,36,
        0,90,36,72,127,36,90,0,163,72,36,90,0,127,72,36 } },
    // --- H ---
    { "Heartbeat",   {   // Stark/Schwach-Doppelschlag in zwei Oktaven
        0,36,0,18,0,36,0,18,127,163,127,108,127,163,127,108,
        0,36,0,18,0,36,0,18,127,163,127,108,127,163,127,108 } },
    // --- J ---
    { "Jazz Walk",   {   // Walking-Bass: stufenweise mit Chromatikhalbton
        0,36,72,108,127,90,54,36,72,108,127,163,145,108,90,72,
        0,36,72,108,127,90,54,36,72,108,127,163,145,108,90,72 } },
    // --- L ---
    { "Leapfrog",    {   // Tief/Hoch abwechselnd, spiralfoermig nach oben
        0,127,18,145,36,163,54,181,72,200,90,218,108,236,127,0,
        0,127,18,145,36,163,54,181,72,200,90,218,108,236,127,0 } },
    // --- M ---
    { "Mirror",      {   // Palindrom: aufsteigend dann symmetrisch absteigend
        0,36,72,108,127,163,200,236, 236,200,163,127,108,72,36,0,
        0,36,72,108,127,163,200,236, 236,200,163,127,108,72,36,0 } },
    { "Minimal",     {   // 4-Step Techno-Minimal-Loop
        0,72,36,108,0,72,36,108,0,72,36,108,0,72,36,108,
        0,72,36,108,0,72,36,108,0,72,36,108,0,72,36,108 } },
    { "Mk Harmon.",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Mk Stepwise", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Mk Struktur", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    // --- N ---
    { "Note Blur",    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Delay+1", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Dilat.",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Drunk",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Erode",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Grad.",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Echo",    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Freeze",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Inv",     { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Oct+",    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Oct-",    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Retro",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Rotate",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Shadow",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Spread",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Stutter", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Thin",    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Tonic",   { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "Note Zip",     { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    // --- O ---
    { "Ominous",     {   // Dunkel, spannungsgeladen, im tiefen Register
        0,0,36,18, 0,0,18,36, 54,36,18,0, 0,18,36,72,
        0,0,36,18, 0,0,18,36, 54,36,18,0, 0,18,36,72 } },
    // --- P ---
    { "Pendulum",    {   // Schwingt um Mittelpunkt, symmetrisch
        127,90,54,18, 0,18,54,90, 127,163,200,236, 255,236,200,163,
        127,90,54,18, 0,18,54,90, 127,163,200,236, 255,236,200,163 } },
    { "Pentatone",   {   // Pentatonischer Bogen, 8-Step
        0,36,72,127,163,127,72,36,0,36,72,127,163,127,72,36,
        0,36,72,127,163,127,72,36,0,36,72,127,163,127,72,36 } },
    // --- R ---
    { "Raga Walk",   {   // Schrittweises Rauf/Runter (indischer Stil)
        0,18,36,54,72,90,108,127,145,163,145,127,108,90,72,54,
        36,18,0,18,36,54,72,90,108,127,108,90,72,54,36,18 } },
    { "Rev. Saw",    {   // Lineare Abwaertsrampe
        255,210,180,160,144,128,112,96,80,64,48,32,24,16,8,0,
        255,210,180,160,144,128,112,96,80,64,48,32,24,16,8,0 } },
    { "RndWlk Fast", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "RndWlk Mid",  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    { "RndWlk Slow", { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
    // --- S ---
    { "S & H",       {   // Sample & Hold: grosse Sprünge, zufaellig wirkend
        0,200,36,218,72,163,18,236,127,54,181,36,200,0,145,90,
        0,200,36,218,72,163,18,236,127,54,181,36,200,0,145,90 } },
    { "Sawtooth",    {   // Lineare Aufwaertsrampe (Saegezahn)
        0,8,16,24,32,48,64,80,96,112,128,144,160,180,210,255,
        0,8,16,24,32,48,64,80,96,112,128,144,160,180,210,255 } },
    { "Spiral",      {   // Schrittweise ansteigend ueber alle 32 Steps
        0,18,36,54, 72,90,108,127, 36,54,72,90, 108,127,145,163,
        72,90,108,127, 145,163,181,200, 108,127,145,163, 181,200,218,236 } },
    { "Staircase",   {   // Stufenpyramide mit Sprung
        0,0,72,72,127,127,200,200,0,0,72,72,127,127,200,200,
        0,0,72,72,127,127,200,200,0,0,72,72,127,127,200,200 } },
    // --- T ---
    { "TD Logos",    {   // Langsam driftender Arp, schwebend (Encore/Logos 1977)
        0,36,72,108, 90,72,36,72, 108,127,108,90, 72,54,36,54,
        72,90,127,145, 127,108,90,72, 54,72,90,108, 90,72,54,36 } },
    { "TD Phaedra",  {   // Langsam steigende Spannung, 2-Noten-Zelle (Phaedra 1974)
        0,0,72,0, 0,72,0,36, 0,0,72,0, 36,72,36,72,
        0,36,72,36, 72,108,72,108, 36,72,108,72, 108,145,72,36 } },
    { "TD Ricochet", {   // Weite Sprünge, rhythmisch-energetisch (Ricochet 1975)
        0,181,72,218, 36,181,109,0, 0,181,72,218, 36,163,90,0,
        0,181,72,218, 36,181,109,36, 0,163,72,200, 36,163,90,0 } },
    { "TD Rubycon",  {   // Hypnotische 3-Noten-Zelle mit kleiner Drift (Rubycon 1975)
        0,72,145,72, 0,72,145,72, 0,72,145,108, 0,72,145,127,
        0,72,145,72, 0,72,145,90, 0,36,72,108, 72,36,0,36 } },
    { "TD Strato",   {   // Lyrischer Melodiebogen, auf/ab (Stratosfear 1976)
        72,90,108,127, 145,127,108,90, 72,90,108,127, 163,145,127,108,
        90,72,90,108, 127,108,90,72, 54,72,90,72, 54,36,54,72 } },
    { "Techno Pump", {   // Oktav-Pumpen mit Quint-Variation
        0,127,0,127,0,127,0,127,72,200,72,200,36,127,72,200,
        0,127,0,127,0,127,0,127,72,200,72,200,36,127,72,200 } },
    // --- W ---
    { "Waltz",       {   // Klassischer Oom-Pah-Pah Bassgang
        0,72,90,0,72,90,127,72,90,0,72,90,36,72,90,127,
        0,72,90,0,72,90,127,72,90,0,72,90,36,72,90,127 } },
};
const int PITCH_PRESET_COUNT = (int)(sizeof(PITCH_PRESETS) / sizeof(PITCH_PRESETS[0]));

const char *getPitchPresetName(int idx) {
    if (idx == PITCH_PRESET_COUNT) return "Random";
    return PITCH_PRESETS[((idx % PITCH_PRESET_COUNT) + PITCH_PRESET_COUNT) % PITCH_PRESET_COUNT].name;
}

int getNoteEchoPresetIdx() {
    for (int i = 0; i < PITCH_PRESET_COUNT; i++)
        if (!strcmp(PITCH_PRESETS[i].name, "Note Echo")) return i;
    return -1;
}

// 0 = normal, 1 = algorithmisch (Brownian/RndWlk/Drunk/Markov), 2 = Note-Effekt
uint8_t getPitchPresetCategory(int idx) {
    if (idx < 0 || idx >= PITCH_PRESET_COUNT) return 0;
    const char *n = PITCH_PRESETS[idx].name;
    if (!strncmp(n, "Note ", 5))   return 2;
    if (!strncmp(n, "Brwn ", 5))   return 1;
    if (!strncmp(n, "RndWlk", 6))  return 1;
    if (!strncmp(n, "Drunk", 5))   return 1;
    if (!strncmp(n, "Mk ", 3))     return 1;
    return 0;
}

// Hilfsfunktion: Note-Index → rawValue (Mitte des Quantisierungs-Intervalls)
static uint8_t noteIdxToRaw(int idx, int noteCount) {
    if (noteCount <= 0) return 0;
    idx = idx < 0 ? 0 : idx >= noteCount ? noteCount - 1 : idx;
    return (uint8_t)clampVal((idx * 256 + 128) / noteCount, 0, 255);
}

// Wählt zufällig einen Grundton-Index (Root in beliebiger Oktave) aus der Notenliste.
// Fallback: Index 0 wenn kein Root gefunden.
static int randomRootIdx(const int *noteList, int N) {
    int rootMidi = 36 + (int)(pitchRoot % 12);  // tiefster Root (C2 + offset)
    int hits[5]; int nHits = 0;
    for (int i = 0; i < N && nHits < 5; i++) {
        if ((noteList[i] - rootMidi) % 12 == 0) hits[nHits++] = i;
    }
    if (nHits == 0) return 0;
    return hits[(int)(random(nHits))];
}

// Brownian: immer Bewegung, Schrittgröße (Skalenstufen) variiert je Temperatur
static void genBrownian(uint8_t *dest32, int stepRange) {
    int noteList[60];
    int N = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
    if (N < 2) { for (int i = 0; i < 32; i++) dest32[i] = 0; return; }
    int cur = randomRootIdx(noteList, N);  // Start: Grundton (zufällige Oktave)
    for (int i = 0; i < 32; i++) {
        dest32[i] = noteIdxToRaw(cur, N);
        int step = (int)(random(stepRange * 2 + 1)) - stepRange;
        if (step == 0) step = (random(2) ? 1 : -1);
        cur += step;
        if (cur < 0)  cur = 1;      // Wand: abprallen
        if (cur >= N) cur = N - 2;
    }
}

// Random Walk: immer ±1 Skalenstufe, aber nur mit Wahrscheinlichkeit pct%
static void genRndWlk(uint8_t *dest32, int pct) {
    int noteList[60];
    int N = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
    if (N < 2) { for (int i = 0; i < 32; i++) dest32[i] = 0; return; }
    int cur = randomRootIdx(noteList, N);  // Start: Grundton (zufällige Oktave)
    for (int i = 0; i < 32; i++) {
        dest32[i] = noteIdxToRaw(cur, N);
        if ((int)(random(100)) < pct) {
            cur += (random(2) ? 1 : -1);
            if (cur < 0)  cur = 1;
            if (cur >= N) cur = N - 2;
        }
    }
}

// Drunk Walk: Brownian mit Anziehung zur Mitte (Grundton als Startpunkt)
static void genDrunkWalk(uint8_t *dest32) {
    int noteList[60];
    int N = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
    if (N < 2) { for (int i = 0; i < 32; i++) dest32[i] = 0; return; }
    int cur = randomRootIdx(noteList, N);  // Start: Grundton (zufällige Oktave)
    int mid = N / 2;
    for (int i = 0; i < 32; i++) {
        dest32[i] = noteIdxToRaw(cur, N);
        int step = (int)(random(7)) - 3;  // -3..+3 Stufen
        int gravity = (mid - cur) / 4;    // Anziehung zur Mitte
        cur += step + gravity;
        if (cur < 0)  cur = 0;
        if (cur >= N) cur = N - 1;
    }
}

// Markov Stepwise: bevorzugt kleine Schritte, selten große Sprünge
static void genMkStepwise(uint8_t *dest32) {
    int noteList[60];
    int N = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
    if (N < 2) { for (int i = 0; i < 32; i++) dest32[i] = 0; return; }
    int cur = randomRootIdx(noteList, N);  // Start: Grundton (zufällige Oktave)
    for (int i = 0; i < 32; i++) {
        dest32[i] = noteIdxToRaw(cur, N);
        int r = (int)(random(10));
        int delta;
        if (r < 6)      delta = ((int)(random(3)) - 1);      // 60%: ±0/1
        else if (r < 9) delta = ((int)(random(3)) - 1) * 2;  // 30%: ±0/2
        else            delta = ((int)(random(3)) - 1) * 4;  // 10%: ±0/4
        cur += delta;
        if (cur < 0)  cur = -cur;          // Wand abprallen
        if (cur >= N) cur = 2 * N - cur - 2;
        cur = cur < 0 ? 0 : cur >= N ? N - 1 : cur;
    }
}

// Markov Harmonisch: Übergangsgewichte auf echten Skalenstufen-Indizes
static void genMkHarmonisch(uint8_t *dest32) {
    int noteList[60];
    int N = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
    if (N < 2) { for (int i = 0; i < 32; i++) dest32[i] = 0; return; }
    // Gewichte: Root→(3/5 stark), Mitte→(Root/Okt), Okt→Root
    // Vereinfacht: 5 repräsentative Indizes aus der Notenliste
    int step = N > 4 ? N / 4 : 1;
    int deg[5] = { 0, step, step*2, step*3, N-1 };
    static const uint8_t WEIGHTS[5][5] = {
        { 1, 2, 4, 1, 3 },
        { 2, 1, 3, 2, 2 },
        { 3, 2, 1, 3, 4 },
        { 2, 3, 3, 1, 2 },
        { 4, 1, 2, 1, 1 },
    };
    int cur = 0;  // Start: deg[0] = Root (Markov-Zustand, nicht noteList-Index)
    for (int i = 0; i < 32; i++) {
        dest32[i] = noteIdxToRaw(deg[cur], N);
        int total = 0;
        for (int j = 0; j < 5; j++) total += WEIGHTS[cur][j];
        int r = (int)(random(total)), acc = 0;
        for (int j = 0; j < 5; j++) {
            acc += WEIGHTS[cur][j];
            if (r < acc) { cur = j; break; }
        }
    }
}

// Markov Struktur: Richtungs-Zustand auf Skalenstufen-Indizes
static void genMkStruktur(uint8_t *dest32) {
    int noteList[60];
    int N = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
    if (N < 2) { for (int i = 0; i < 32; i++) dest32[i] = 0; return; }
    int cur = randomRootIdx(noteList, N);  // Start: Grundton (zufällige Oktave)
    int dir = +1;
    for (int i = 0; i < 32; i++) {
        dest32[i] = noteIdxToRaw(cur, N);
        if (cur >= N - 1)    dir = -1;
        else if (cur <= 0)   dir = +1;
        else if ((int)(random(8)) == 0) dir = -dir;
        int step = (int)(random(3) + 1);  // 1–3 Stufen
        if ((int)(random(5)) == 0) step = 0;  // 20% Pause
        cur += dir * step;
        cur = cur < 0 ? 0 : cur >= N ? N - 1 : cur;
    }
}

void getPitchPresetNotes(int idx, uint8_t *dest32) {
    if (idx == PITCH_PRESET_COUNT) {
        for (int i = 0; i < 32; i++) dest32[i] = (uint8_t)random(256);
        return;
    }
    int i = ((idx % PITCH_PRESET_COUNT) + PITCH_PRESET_COUNT) % PITCH_PRESET_COUNT;
    const char *name = PITCH_PRESETS[i].name;
    if      (!strcmp(name, "Brwn Slow"))    { genBrownian(dest32, 1);   return; }
    else if (!strcmp(name, "Brwn Mid"))    { genBrownian(dest32, 3);   return; }
    else if (!strcmp(name, "Brwn Fast"))   { genBrownian(dest32, 5);   return; }
    else if (!strcmp(name, "RndWlk Slow")) { genRndWlk(dest32, 25);    return; }
    else if (!strcmp(name, "RndWlk Mid"))  { genRndWlk(dest32, 50);    return; }
    else if (!strcmp(name, "RndWlk Fast")) { genRndWlk(dest32, 85);    return; }
    else if (!strcmp(name, "Drunk Walk"))  { genDrunkWalk(dest32);     return; }
    else if (!strcmp(name, "Mk Stepwise"))  { genMkStepwise(dest32);   return; }
    else if (!strcmp(name, "Mk Harmon."))   { genMkHarmonisch(dest32); return; }
    else if (!strcmp(name, "Mk Struktur"))  { genMkStruktur(dest32);   return; }
    else if (!strcmp(name, "Note Echo")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++) {
            if (j % 2 == 0) dest32[j] = lastNonEchoPitchNotes[j];
            else             dest32[j] = lastNonEchoPitchNotes[(j - 3 + len) % len];
        }
        return;
    }
    else if (!strcmp(name, "Note Delay+1")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++)
            dest32[j] = lastNonEchoPitchNotes[(j - 1 + len) % len];
        return;
    }
    else if (!strcmp(name, "Note Dilat.")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++) {
            uint8_t a = lastNonEchoPitchNotes[(j - 1 + len) % len];
            uint8_t b = lastNonEchoPitchNotes[j];
            uint8_t c = lastNonEchoPitchNotes[(j + 1) % len];
            dest32[j] = (a > b ? (a > c ? a : c) : (b > c ? b : c));
        }
        return;
    }
    else if (!strcmp(name, "Note Erode")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++) {
            uint8_t a = lastNonEchoPitchNotes[(j - 1 + len) % len];
            uint8_t b = lastNonEchoPitchNotes[j];
            uint8_t c = lastNonEchoPitchNotes[(j + 1) % len];
            dest32[j] = (a < b ? (a < c ? a : c) : (b < c ? b : c));
        }
        return;
    }
    else if (!strcmp(name, "Note Grad.")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++) {
            uint8_t a = lastNonEchoPitchNotes[(j - 1 + len) % len];
            uint8_t b = lastNonEchoPitchNotes[j];
            uint8_t c = lastNonEchoPitchNotes[(j + 1) % len];
            uint8_t mx = (a > b ? (a > c ? a : c) : (b > c ? b : c));
            uint8_t mn = (a < b ? (a < c ? a : c) : (b < c ? b : c));
            dest32[j] = mx - mn;
        }
        return;
    }
    else if (!strcmp(name, "Note Blur")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++)
            dest32[j] = (uint8_t)(((int)lastNonEchoPitchNotes[(j - 1 + len) % len]
                                 + (int)lastNonEchoPitchNotes[j]
                                 + (int)lastNonEchoPitchNotes[(j + 1) % len]) / 3);
        return;
    }
    else if (!strcmp(name, "Note Stutter")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++)
            dest32[j] = lastNonEchoPitchNotes[(j / 2) * 2 % len];
        return;
    }
    else if (!strcmp(name, "Note Retro")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++) {
            if (j % 2 == 0) dest32[j] = lastNonEchoPitchNotes[j];
            else             dest32[j] = lastNonEchoPitchNotes[(len - 1 - j + len) % len];
        }
        return;
    }
    else if (!strcmp(name, "Note Shadow")) {
        int len = clampVal(PatLen[0], 1, 32);
        int noteList[60];
        int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
        for (int j = 0; j < len; j++) {
            if (j % 2 == 0) {
                dest32[j] = lastNonEchoPitchNotes[j];
            } else {
                int srcMidi = quantizeToMidi(lastNonEchoPitchNotes[(j - 1 + len) % len],
                                             pitchSpread, pitchScale, pitchRoot, pitchIntervalMask);
                int shadowMidi = srcMidi;
                for (int k = 0; k < nc; k++) {
                    if (noteList[k] > srcMidi) { shadowMidi = noteList[k]; break; }
                }
                int bestK = 0;
                for (int k = 0; k < nc; k++)
                    if (noteList[k] == shadowMidi) { bestK = k; break; }
                dest32[j] = noteIdxToRaw(bestK, nc > 0 ? nc : 1);
            }
        }
        return;
    }
    else if (!strcmp(name, "Note Thin")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++)
            dest32[j] = (j % 2 == 0) ? lastNonEchoPitchNotes[j] : 0;
        return;
    }
    else if (!strcmp(name, "Note Inv")) {
        int len = clampVal(PatLen[0], 1, 32);
        for (int j = 0; j < len; j++)
            dest32[j] = 255 - lastNonEchoPitchNotes[j];
        return;
    }
    else if (!strcmp(name, "Note Rotate")) {
        int len = clampVal(PatLen[0], 1, 32);
        int half = len / 2;
        for (int j = 0; j < len; j++)
            dest32[j] = lastNonEchoPitchNotes[(j + half) % len];
        return;
    }
    else if (!strcmp(name, "Note Zip")) {
        int len = clampVal(PatLen[0], 1, 32);
        int half = len / 2;
        for (int j = 0; j < len; j++) {
            int src = (j % 2 == 0) ? (j / 2) : (half + j / 2);
            dest32[j] = lastNonEchoPitchNotes[src % len];
        }
        return;
    }
    else if (!strcmp(name, "Note Spread")) {
        int len = clampVal(PatLen[0], 1, 32);
        int half = len / 2;
        for (int j = 0; j < len; j++)
            dest32[j] = lastNonEchoPitchNotes[(j / 2) % len];
        // zweite Hälfte: rückwärts für Symmetrie
        for (int j = half; j < len; j++)
            dest32[j] = lastNonEchoPitchNotes[((len - 1 - j) + half) % len];
        return;
    }
    else if (!strcmp(name, "Note Oct+")) {
        int len = clampVal(PatLen[0], 1, 32);
        int noteList[60];
        int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
        for (int j = 0; j < len; j++) {
            if (j % 2 == 0) {
                dest32[j] = lastNonEchoPitchNotes[j];
            } else {
                // Eine Oktave höher: +12 Halbtöne im MIDI-Raum
                int srcMidi = quantizeToMidi(lastNonEchoPitchNotes[j],
                                             pitchSpread, pitchScale, pitchRoot, pitchIntervalMask);
                int octMidi = srcMidi + 12;
                // Nächsten verfügbaren Ton in der Liste suchen
                int bestK = nc - 1;
                for (int k = 0; k < nc; k++)
                    if (noteList[k] >= octMidi) { bestK = k; break; }
                dest32[j] = noteIdxToRaw(bestK, nc > 0 ? nc : 1);
            }
        }
        return;
    }
    else if (!strcmp(name, "Note Oct-")) {
        int len = clampVal(PatLen[0], 1, 32);
        int noteList[60];
        int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
        for (int j = 0; j < len; j++) {
            if (j % 2 == 0) {
                dest32[j] = lastNonEchoPitchNotes[j];
            } else {
                int srcMidi = quantizeToMidi(lastNonEchoPitchNotes[j],
                                             pitchSpread, pitchScale, pitchRoot, pitchIntervalMask);
                int octMidi = srcMidi - 12;
                int bestK = 0;
                for (int k = nc - 1; k >= 0; k--)
                    if (noteList[k] <= octMidi) { bestK = k; break; }
                dest32[j] = noteIdxToRaw(bestK, nc > 0 ? nc : 1);
            }
        }
        return;
    }
    else if (!strcmp(name, "Note Tonic")) {
        int len = clampVal(PatLen[0], 1, 32);
        int noteList[60];
        int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
        // Root-Töne in der Notenliste finden (alle Oktaven)
        int rootMidi = 36 + (int)(pitchRoot % 12);
        for (int j = 0; j < len; j++) {
            if (j % 2 == 0) {
                dest32[j] = lastNonEchoPitchNotes[j];
            } else {
                // Nächsten Root-Ton (mod 12) zum aktuellen Step suchen
                int srcMidi = quantizeToMidi(lastNonEchoPitchNotes[j],
                                             pitchSpread, pitchScale, pitchRoot, pitchIntervalMask);
                int bestK = 0, bestDist = 127;
                for (int k = 0; k < nc; k++) {
                    if ((noteList[k] - rootMidi) % 12 == 0) {
                        int d = abs(noteList[k] - srcMidi);
                        if (d < bestDist) { bestDist = d; bestK = k; }
                    }
                }
                dest32[j] = noteIdxToRaw(bestK, nc > 0 ? nc : 1);
            }
        }
        return;
    }
    else if (!strcmp(name, "Note Drunk")) {
        int len = clampVal(PatLen[0], 1, 32);
        int noteList[60];
        int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot, pitchIntervalMask, noteList);
        if (nc < 2) { memcpy(dest32, lastNonEchoPitchNotes, len); return; }
        for (int j = 0; j < len; j++) {
            int curIdx = (lastNonEchoPitchNotes[j] * nc) / 256;
            curIdx = curIdx < 0 ? 0 : curIdx >= nc ? nc - 1 : curIdx;
            int delta = (int)(random(3)) - 1;  // -1, 0, +1
            curIdx += delta;
            curIdx = curIdx < 0 ? 0 : curIdx >= nc ? nc - 1 : curIdx;
            dest32[j] = noteIdxToRaw(curIdx, nc);
        }
        return;
    }
    else if (!strcmp(name, "Note Freeze")) {
        int len = clampVal(PatLen[0], 1, 32);
        int freezeStep = (int)(random(len));
        uint8_t freezeVal = lastNonEchoPitchNotes[freezeStep];
        int half = len / 2;
        for (int j = 0; j < len; j++)
            dest32[j] = (j < half) ? lastNonEchoPitchNotes[j] : freezeVal;
        return;
    }
    const uint8_t *src = PITCH_PRESETS[i].note;
    for (int j = 0; j < 32; j++) dest32[j] = src[j];
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

// Hilfsfunktion: Spiegelung oder Wiederholung eines Segments [offset, offset+size)
static int foldSegment(int idx, int offset, int size, bool mirror) {
    if (size <= 0) return idx;
    if (mirror) {
        int cycle = size * 2;
        int local = idx % cycle;
        return offset + (local < size ? local : cycle - 1 - local);
    } else {
        return offset + (idx % size);
    }
}

int foldPitchIdx(int idx, int len, uint8_t foldMode) {
    if (foldMode == 0 || len <= 0) return idx;
    int half    = len / 2;
    int quarter = len / 4;
    // Modi 1-4: Quell-Segment = vorderer Bereich (Rückwärts-Kompatibilität)
    // Modi 5-12: neue Segmente (2. Hälfte, Q2/Q3/Q4)
    switch (foldMode) {
        case  1: return foldSegment(idx, 0,          half,    true);
        case  2: return foldSegment(idx, 0,          half,    false);
        case  3: return foldSegment(idx, 0,          quarter, true);
        case  4: return foldSegment(idx, 0,          quarter, false);
        case  5: return foldSegment(idx, half,       half,    true);
        case  6: return foldSegment(idx, half,       half,    false);
        case  7: return foldSegment(idx, quarter,    quarter, true);
        case  8: return foldSegment(idx, 2*quarter,  quarter, true);
        case  9: return foldSegment(idx, 3*quarter,  quarter, true);
        case 10: return foldSegment(idx, quarter,    quarter, false);
        case 11: return foldSegment(idx, 2*quarter,  quarter, false);
        case 12: return foldSegment(idx, 3*quarter,  quarter, false);
        default: return idx;
    }
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

// ---------------------------------------------------------------------------
// Chord Presets
// Bit-Mask-Semantik: bit0=1(Deg0), bit1=3(Deg2), bit2=5(Deg4),
//                   bit3=7(Deg6), bit4=9(Deg1), bit5=11(Deg3), bit6=13(Deg5)
// ---------------------------------------------------------------------------
struct ChordPreset {
    const char *name;
    uint8_t     scaleIdx;
    uint8_t     intervalMask;
};

static const ChordPreset CHORD_PRESETS[] = {
    // --- Basic Triads ---
    { "Major",     34,   7 },   // Major:      1+3+5
    { "Minor",     43,   7 },   // Minor:      1+b3+5
    { "Dim",       28,   7 },   // Locrian:    1+b3+b5
    { "Aug",       63,   7 },   // Whole Tone: 1+3+#5
    { "Sus2",      34,  21 },   // Major:      1+2+5  (bits 0+4+2)
    { "Sus4",      34,  37 },   // Major:      1+4+5  (bits 0+5+2)
    // --- Seventh Chords ---
    { "Maj7",      34,  15 },   // Major:      1+3+5+7
    { "Min7",      12,  15 },   // Dorian:     1+b3+5+b7
    { "Dom7",      45,  15 },   // Mixolydian: 1+3+5+b7
    { "mMaj7",     20,  15 },   // Harm.Min:   1+b3+5+maj7
    { "m7b5",      28,  15 },   // Locrian:    1+b3+b5+b7
    // --- Jazz Extended ---
    { "Maj9",      34,  31 },   // Major:      1+3+5+7+9
    { "Min9",      12,  31 },   // Dorian:     1+b3+5+b7+9
    { "Dom9",      45,  31 },   // Mixolydian: 1+3+5+b7+9
    { "Maj7#11",   30,  47 },   // Lydian:     1+3+5+7+#11 (bits 0+1+2+3+5)
    { "7#11",      32,  47 },   // Lydian Dom: 1+3+5+b7+#11
    { "7b9",       54,  31 },   // Phryg.Dom:  1+3+5+b7+b9
    { "7#9",        5,  31 },   // Altered:    1+3+5+b7+#9
    { "Maj13",     34, 127 },   // Major:      all 7 degrees
    { "Min13",     12, 127 },   // Dorian:     all 7 degrees
    // --- Quartal / Modern ---
    { "Quartal",   45,  41 },   // Mixolydian: 1+4+b7 (bits 0+5+3)
    // --- World Music ---
    { "Hijaz",     54,  23 },   // Phryg.Dom:  1+b2+3+5 (bits 0+4+1+2)
    { "Byzantin",  11,  23 },   // Dbl Harm:   1+b2+3+5
    { "Persian",   52,  23 },   // Persian:    1+b2+3+b5
    { "Hirajoshi", 21,  55 },   // Hirajoshi:  all 5 available (bits 0+1+2+4+5)
    { "Yo",        64,  55 },   // Yo:         all 5 available
};
const int CHORD_COUNT = (int)(sizeof(CHORD_PRESETS) / sizeof(CHORD_PRESETS[0]));

const char *getChordName(int idx) {
    return CHORD_PRESETS[idx % CHORD_COUNT].name;
}

void getChordPreset(int idx, uint8_t &scaleOut, uint8_t &maskOut) {
    const ChordPreset &c = CHORD_PRESETS[idx % CHORD_COUNT];
    scaleOut = c.scaleIdx;
    maskOut  = c.intervalMask;
}
