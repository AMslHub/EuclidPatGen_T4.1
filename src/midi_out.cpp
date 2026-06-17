#include <midi_out.h>
#include <app_state.h>
#include <pitch.h>
#include <Arduino.h>

// ---------------------------------------------------------------------------
// MIDI-Ausgabe über Serial7 (TX=Pin 29) → Pico → MIDI-DIN OUT
//
// Akkord-Kanal: MIDI CH2 (Byte 0x9n mit n=1)
// Melodie-Kanal: MIDI CH1 (vorbereitet, noch nicht aktiv)
// ---------------------------------------------------------------------------

static const uint8_t MIDI_CH_CHORD   = 2;   // 1-basiert
static const uint8_t MIDI_CH_MELODY  = 1;

// Aktuell klingende Noten (für Note-Off beim nächsten Wechsel)
static uint8_t s_activeNotes[7]  = {};
static int     s_activeCount     = 0;
static int     s_lastPlayPos     = -1;

// ---------------------------------------------------------------------------
// Hilfsfunktionen: MIDI-Bytes senden
// ---------------------------------------------------------------------------

static void midiNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    if (note > 127) return;
    Serial7.write(0x90 | ((ch - 1) & 0x0F));
    Serial7.write(note & 0x7F);
    Serial7.write(vel & 0x7F);
}

static void midiNoteOff(uint8_t ch, uint8_t note) {
    if (note > 127) return;
    Serial7.write(0x80 | ((ch - 1) & 0x0F));
    Serial7.write(note & 0x7F);
    Serial7.write(0x00);
}

static void stopActiveNotes() {
    for (int i = 0; i < s_activeCount; i++)
        midiNoteOff(MIDI_CH_CHORD, s_activeNotes[i]);
    s_activeCount = 0;
}

// ---------------------------------------------------------------------------
// Akkord-Noten aus ChordDef berechnen
//
// Intervall-Halbtonabstände für Terzschichtung (1,3,5,7,9,11,13 der Dur-Skala):
// Die Töne werden aus der aktuell eingestellten Skala (pitchScale) und dem
// Root (pitchRoot) genommen — nicht immer reine Terzen, sondern skalengerecht.
// ToneMask bit0=1, bit1=3, bit2=5, bit3=7, bit4=9, bit5=11, bit6=13.
// ---------------------------------------------------------------------------

static int buildChordNotes(const ChordDef &d, uint8_t *outNotes) {
    // Skalengrade 0,2,4,6,1,3,5 entsprechen Akkordstufen 1,3,5,7,9,11,13
    // (diatonisch, 0-basiert innerhalb der Skala)
    static const int CHORD_DEGREES[7] = { 0, 2, 4, 6, 1, 3, 5 };

    // Notenliste der aktuellen Skala aufbauen (2 Oktaven reichen für Grundtöne)
    int noteList[60];
    // Spread=2 für Grundton-Auflösung; die eigentliche Spreizung machen wir selbst
    int N = buildNoteList(2, pitchScale, pitchRoot, 0x7F, noteList);
    if (N < 1) return 0;

    // Root-Note: tiefster Ton in der Notenliste (= Grundton in Oktave 0)
    // Skalentöne der ersten Oktave extrahieren (Halbtonabstände vom Root)
    // Aus der Skalen-Definition direkt lesen wäre sauberer, aber buildNoteList
    // liefert uns bereits die korrekten MIDI-Noten — wir nehmen die ersten N/2.
    // Skalengrade: noteList[0]=Grad0, [1]=Grad1 etc. (Spread=2 → 2 Oktaven)
    int scaleNotesPerOct = N / 2;  // Anzahl Töne pro Oktave
    if (scaleNotesPerOct < 1) return 0;

    int count = 0;
    uint8_t notes[7];

    for (int b = 0; b < 7; b++) {
        if (!((d.toneMask >> b) & 1)) continue;
        int deg = CHORD_DEGREES[b];  // Skalengrad (0-basiert)
        if (deg >= scaleNotesPerOct) continue;

        int midi = noteList[deg];  // Grundton + Skalengrad in Okt 0
        // Oktav-Offset anwenden
        midi += (int)d.oct * 12;
        // In gültigen MIDI-Bereich clampen
        while (midi < 24)  midi += 12;
        while (midi > 108) midi -= 12;
        notes[count++] = (uint8_t)midi;
    }
    if (count == 0) return 0;

    // Inversion anwenden: untersten Ton um eine Oktave nach oben verschieben
    for (int inv = 0; inv < (int)d.inv; inv++) {
        if (count < 2) break;
        // Tiefste Note finden
        int minIdx = 0;
        for (int i = 1; i < count; i++)
            if (notes[i] < notes[minIdx]) minIdx = i;
        notes[minIdx] += 12;
    }

    // Spread: Töne über mehrere Oktaven verteilen (jeder Ton eine Oktave höher als der vorherige)
    if (d.spread > 1 && count > 1) {
        // Noten aufsteigend sortieren
        for (int i = 0; i < count - 1; i++)
            for (int j = i + 1; j < count; j++)
                if (notes[j] < notes[i]) { uint8_t t = notes[i]; notes[i] = notes[j]; notes[j] = t; }
        // Jeden Folge-Ton um spread-gesteuerte Oktaven anheben
        for (int i = 1; i < count; i++) {
            int oct = (i * (int)(d.spread - 1)) / (count > 1 ? count - 1 : 1);
            notes[i] += (uint8_t)(oct * 12);
            if (notes[i] > 108) notes[i] = 108;
        }
    }

    for (int i = 0; i < count; i++) outNotes[i] = notes[i];
    return count;
}

// ---------------------------------------------------------------------------
// Velocity aus Val-Feld (10–100 → MIDI 12–127)
// ---------------------------------------------------------------------------
static uint8_t valToVelocity(uint8_t val) {
    if (val == CHORD_SLOT_EMPTY) val = 64;
    val = (val < 10) ? 10 : (val > 100) ? 100 : val;
    return (uint8_t)((int)val * 127 / 100);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void midiOutSetup() {
    Serial7.begin(31250);
}

void midiOutAllNotesOff() {
    stopActiveNotes();
    // CC 123 = All Notes Off auf beiden Kanälen
    Serial7.write(0xB0 | (MIDI_CH_CHORD  - 1)); Serial7.write(123); Serial7.write(0);
    Serial7.write(0xB0 | (MIDI_CH_MELODY - 1)); Serial7.write(123); Serial7.write(0);
}

void midiOutTick() {
    // Nur wenn Chord-Sequencer aktiv (Sequencer läuft)
    if (chordPlayPos < 0 || chordLen == 0) {
        if (s_activeCount > 0) stopActiveNotes();
        s_lastPlayPos = -1;
        return;
    }

    int pos = chordPlayPos;
    if (pos == s_lastPlayPos) return;  // keine Änderung

    // Carry-forward auflösen
    auto resolveAkk = [&](int i) -> uint8_t {
        for (int k = i; k >= 0; k--)
            if (chordSlots[k].akkNr != CHORD_SLOT_EMPTY) return chordSlots[k].akkNr;
        return 0;
    };
    auto resolveVal = [&](int i) -> uint8_t {
        for (int k = i; k >= 0; k--)
            if (chordSlots[k].val != CHORD_SLOT_EMPTY) return chordSlots[k].val;
        return 64;
    };
    auto resolveLeg = [&](int i) -> bool {
        for (int k = i; k >= 0; k--)
            if (chordSlots[k].leg != CHORD_SLOT_EMPTY) return chordSlots[k].leg != 0;
        return false;
    };

    bool muted  = chordSlots[pos].mute;
    bool legato = !muted && resolveLeg(pos);

    if (muted) {
        // Mute: Note-Off, nichts senden
        stopActiveNotes();
        s_lastPlayPos = pos;
        return;
    }

    uint8_t akkNr = resolveAkk(pos);
    if (akkNr >= CHORD_DEF_COUNT || !chordDefs[akkNr].saved) {
        stopActiveNotes();
        s_lastPlayPos = pos;
        return;
    }

    uint8_t newNotes[7];
    int     newCount = buildChordNotes(chordDefs[akkNr], newNotes);
    uint8_t vel      = valToVelocity(resolveVal(pos));

    if (legato && s_activeCount > 0) {
        // Legato: erst neue Note-Ons, dann alte Note-Offs (Überlappung = kein Abriss)
        for (int i = 0; i < newCount; i++)
            midiNoteOn(MIDI_CH_CHORD, newNotes[i], vel);
        for (int i = 0; i < s_activeCount; i++) {
            bool keep = false;
            for (int j = 0; j < newCount; j++)
                if (s_activeNotes[j] == s_activeNotes[i]) { keep = true; break; }
            if (!keep) midiNoteOff(MIDI_CH_CHORD, s_activeNotes[i]);
        }
    } else {
        // Normal: alte Noten aus, neue Noten an
        stopActiveNotes();
        for (int i = 0; i < newCount; i++)
            midiNoteOn(MIDI_CH_CHORD, newNotes[i], vel);
    }

    // Aktive Noten merken
    s_activeCount = newCount;
    for (int i = 0; i < newCount; i++) s_activeNotes[i] = newNotes[i];
    s_lastPlayPos = pos;
}

// ---------------------------------------------------------------------------
// Melodie CH1: Note-On bei jedem Gate-Hit von Ch0
// ---------------------------------------------------------------------------
static uint8_t s_melodyNote = 0xFF;  // 0xFF = keine aktive Note

void midiOutMelodyHit(int midiNote, uint8_t velocity) {
    if (midiNote < 0 || midiNote > 127) return;
    if (s_melodyNote != 0xFF)
        midiNoteOff(MIDI_CH_MELODY, s_melodyNote);  // vorherige Note beenden
    s_melodyNote = (uint8_t)midiNote;
    midiNoteOn(MIDI_CH_MELODY, s_melodyNote, velocity);
}

void midiOutMelodyOff() {
    if (s_melodyNote != 0xFF) {
        midiNoteOff(MIDI_CH_MELODY, s_melodyNote);
        s_melodyNote = 0xFF;
    }
}

// ---------------------------------------------------------------------------
// Rhythmus CH9 (Ch1) / CH10 (Ch2): feste Note C3 (MIDI 48) als Gate-Trigger
// ---------------------------------------------------------------------------
static const uint8_t RHYTHM_NOTE     = 48;   // C3
static const uint8_t RHYTHM_VELOCITY = 100;
static const uint8_t MIDI_CH_RHYTHM[2] = { 9, 10 };  // ch=1→9, ch=2→10

void midiOutRhythmHit(int ch) {
    if (ch < 1 || ch > 2) return;
    midiNoteOn(MIDI_CH_RHYTHM[ch - 1], RHYTHM_NOTE, RHYTHM_VELOCITY);
}

void midiOutRhythmOff(int ch) {
    if (ch < 1 || ch > 2) return;
    midiNoteOff(MIDI_CH_RHYTHM[ch - 1], RHYTHM_NOTE);
}
