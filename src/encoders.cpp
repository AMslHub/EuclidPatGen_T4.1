#include <encoders.h>
#include <hardware_map.h>
#include <app_state.h>
#include <euclid.h>
#include <pitch.h>
#include <storage.h>
#include <ui_screens.h>
#include <cv_inputs.h>
#include <midi_out.h>
#include <Encoder.h>

static Encoder enc1(ENC1_CLK_PIN, ENC1_DT_PIN);
static Encoder enc2(ENC2_CLK_PIN, ENC2_DT_PIN);
static Encoder enc3(ENC3_CLK_PIN, ENC3_DT_PIN);
static Encoder *encoders[3] = { &enc1, &enc2, &enc3 };

static const uint8_t swPins[3] = { ENC1_SW_PIN, ENC2_SW_PIN, ENC3_SW_PIN };

// 0=PatLen, 1=PatNum, 2=PatRot(R), 3=PatRotSel(r), 4=PatProb, 5=Speed  (extern in app_state.h)
uint8_t encParamSel[3] = { 0, 0, 0 };
static long encLastPos[3] = { 0, 0, 0 };
static int  pitchItvlCursor = 0;  // Enc3-Cursor: aktuell hervorgehobenes Intervall (0..6)

int getPitchItvlCursor() { return pitchItvlCursor; }

// PERFORMANCE screen browse state (ENC1 only)
static bool encBrowseActive = false;
static int  encBrowseSlot   = 0;

// Button debounce
static uint32_t btnLastTime[3]  = { 0, 0, 0 };
static bool     btnLastState[3] = { HIGH, HIGH, HIGH };
static const uint32_t BTN_DEBOUNCE_MS = 30;

// Long Press Enc1: Replace-by-Fold auf PITCH1
static uint32_t enc1PressStartMs    = 0;
static bool     enc1VlpFlashed      = false;
// Long Press Enc2: Quick Save (alle Screens ausser PERFORMANCE)
static uint32_t enc2PressStartMs    = 0;
// Long Press Enc3: NAV-Screen Toggle
static uint32_t enc3PressStartMs    = 0;
static bool     enc3VlpFlashed      = false;
static uint16_t navPrevState        = EUCLCIRCS;
static const uint32_t LONG_PRESS_MS      =  600;
static const uint32_t VERY_LONG_PRESS_MS = 2000;

static const int ENC_STEPS_PER_DETENT = 4;
static const int SLOT_COUNT = 16;
static int nextSaveSlot = -1;  // Schreibzeiger fuer Quick-Save (-1 = normales Verhalten)

// PITCH1 screen: Enc1 browse/edit state
static int  pitchBoxCursor   = 0;   // 0=Scale, 1=Root, 2=Spread
static bool pitchBoxEditMode = false;

int  getPitchBoxCursor()   { return pitchBoxCursor; }
bool getPitchBoxEditMode() { return pitchBoxEditMode; }

// COND1/2/3 screen: per-channel step cursor
static int condStepCursor[3] = { 0, 0, 0 };
int getCondStepCursor(int ch) { return (ch >= 0 && ch < 3) ? condStepCursor[ch] : 0; }

// ---------------------------------------------------------------------------
// Wendet eine Parameteraenderung fuer Kanal ch an.
//   rotOnly=true: nur PatRot geaendert → kein Rebuild, sofortige Rotation
//   sonst: Aenderung erst am Pattern-Ende sichtbar (pendingCircleRedraw)
// ---------------------------------------------------------------------------
static void applyParamChange(int ch, bool rotOnly) {
    PatLen[ch]    = clampVal(PatLen[ch], 1, 32);
    PatNum[ch]    = clampVal(PatNum[ch], 1, PatLen[ch]);
    PatRot[ch]    = clampVal(PatRot[ch],    -(PatLen[ch] - 1), PatLen[ch] - 1);
    PatRotSel[ch] = clampVal(PatRotSel[ch], -(PatLen[ch] - 1), PatLen[ch] - 1);
    PatProb[ch]   = (uint8_t)clampVal((int)PatProb[ch], 0, 20);
    scheduleSaveParams();

    const int Rs[3] = { R1, R2, R3 };

    if (GUIState == (uint16_t)(EUCLPARAM1 + ch)) {
        // Auf dem Param-Screen: sofortiger Redraw
        if (rotOnly)
            redrawParamFromPattern(ch);
        else
            redrawParam(ch);

    } else if (GUIState == EUCLCIRCS) {
        if (rotOnly) {
            // PatRot: sofort zeichnen, aber nur wenn kein struktureller Redraw aussteht.
            // Bei austehendem pendingCircleRedraw wuerde PatLen noch nicht displayedPatLen
            // entsprechen → die Rotation wird beim naechsten Redraw automatisch uebernommen.
            if (!pendingCircleRedraw[ch]) {
                drawEucledianCircleFromPattern(Rs[ch], PatLen[ch], PatRot[ch], EPatArr[ch]);
            }
            drawEncParamIndicator(ch);
        } else {
            // Alle anderen Parameter (L/H/P): erst am Pattern-Ende anwenden
            pendingCircleRedraw[ch] = true;
            drawEncParamIndicator(ch);
        }
    }
}

// ---------------------------------------------------------------------------
// Normaler Encoder-Dreh: aendert den aktuell selektierten Parameter von ch.
// Reihenfolge: 0=PatLen(L), 1=PatNum(H), 2=PatRot(R), 3=PatRotSel(r), 4=PatProb(P), 5=Speed(S)
// ---------------------------------------------------------------------------
static void handleNormalEncoder(int ch, int delta) {
    if (encParamSel[ch] == 5) {
        // Speed: -3..+3 (÷4..×4)
        chSpeedIdx[ch] = clampVal(chSpeedIdx[ch] + delta, -3, 3);
        scheduleSaveParams();
        if (GUIState == (uint16_t)(EUCLPARAM1 + ch)) {
            drawSpeedIndicator(ch);
        } else if (GUIState == EUCLCIRCS) {
            drawEncParamIndicator(ch);
        }
        return;
    }
    if (encParamSel[ch] == 4) {
        if (GUIState == EUCLCIRCS) {
            // Kreisscreen: Zufallspattern am naechsten Pattern-Ende generieren
            pendingProbRegen[ch] = true;
        } else {
            // Parameter-Screen: PatProb direkt einstellen
            PatProb[ch] = (uint8_t)clampVal((int)PatProb[ch] + delta, 0, 20);
            applyParamChange(ch, false);
        }
        return;
    }
    if (encParamSel[ch] == 3) {
        // Selective Rot (r): nur Checkbox-markierte Schichten
        PatRotSel[ch] += delta;
        applyParamChange(ch, true);
        return;
    }
    bool rotOnly = false;
    switch (encParamSel[ch]) {
        case 0: PatLen[ch] += delta; break;
        case 1: PatNum[ch] += delta; break;
        case 2: PatRot[ch] += delta; rotOnly = true; break;
    }
    applyParamChange(ch, rotOnly);
}

// ---------------------------------------------------------------------------
// Normaler Encoder-Knopfdruck: zykliert den aktiven Parameter fuer ch.
// ---------------------------------------------------------------------------
static void handleNormalButton(int ch) {
    encParamSel[ch] = (encParamSel[ch] + 1) % 6;
    if (GUIState == (uint16_t)(EUCLPARAM1 + ch)) {
        drawParamButtonHighlight(ch);
        drawSpeedIndicator(ch);
    } else if (GUIState == EUCLCIRCS) {
        drawEncParamIndicator(ch);
    }
}

// ---------------------------------------------------------------------------
// ENC1-Dreh im PERFORMANCE-Screen: scrollt durch Preset-Slots.
// ---------------------------------------------------------------------------
static void handlePerfEncoder1(int delta) {
    if (cvSlotSel >= 0) return;  // CV hat Kontrolle: manuelle Auswahl gesperrt
    if (!encBrowseActive) return;
    int prev = encBrowseSlot;
    encBrowseSlot = (encBrowseSlot + delta + SLOT_COUNT) % SLOT_COUNT;
    if (encBrowseSlot != prev) {
        setPerfEncBrowseSlot(encBrowseSlot);
    }
}

// ---------------------------------------------------------------------------
// ENC1-Knopf im PERFORMANCE-Screen:
//   Normal: 1. Druck → Browse-Modus, Slot CYAN; 2. Druck → Slot laden
//   P&P-Modus: 1. Druck → Browse starten; 2. Druck → Slot platzieren (Move)
// ---------------------------------------------------------------------------
static void handlePerfButton1() {
    if (cvSlotSel >= 0) return;
    if (!encBrowseActive) {
        encBrowseSlot = getActiveSlot();
        if (encBrowseSlot < 0) encBrowseSlot = 0;
        encBrowseActive = true;
        setPerfEncBrowseSlot(encBrowseSlot);
    } else {
        encBrowseActive = false;
        setPerfEncBrowseSlot(-1);
        if (getPerfPickActive()) {
            executePerfPickPlace(encBrowseSlot);
        } else {
            uint16_t used = getSlotsUsedMask();
            if (used & (1u << encBrowseSlot)) {
                requestLoadSlot(encBrowseSlot);
                nextSaveSlot = -1;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ENC2-Dreh im PERFORMANCE-Screen: scrollt durch Rhythmus-Presets.
// ---------------------------------------------------------------------------
static void handlePerfEncoder2(int delta) {
    if (!getRhythmBrowseActive()) return;
    setRhythmBrowseIdx(getRhythmBrowseIdx() + delta);
}

// ---------------------------------------------------------------------------
// ENC2-Knopf im PERFORMANCE-Screen:
//   1. Druck → Browse-Modus aktivieren (CYAN-Rahmen)
//   2. Druck → Preset laden, Browse-Modus beenden
// ---------------------------------------------------------------------------
static void handlePerfButton2() {
    if (!getRhythmBrowseActive()) {
        setRhythmBrowseActive(true);
    } else {
        setRhythmBrowseActive(false);
        loadRhythmPreset(getRhythmBrowseIdx());
    }
}

// ---------------------------------------------------------------------------
// PITCH1-Screen: Encoder-Dreh
//   Enc1 (i=0): pitchSpread 1..5
//   Enc2 (i=1): pitchShift -3..+3
//   Enc3 (i=2): Intervall-Cursor 0..6 durchblättern
// ---------------------------------------------------------------------------
static void handlePitchEncoder(int enc, int delta) {
    if (getPitchStepEditActive()) {
        switch (enc) {
            case 0: adjustPitchStepNote(delta);   return;
            case 1: adjustPitchStepOctave(delta); return;
            case 2: movePitchStepCursor(delta);   return;
        }
    }
    switch (enc) {
        case 0:
            if (!pitchBoxEditMode) {
                pitchBoxCursor = ((pitchBoxCursor + delta) % 6 + 6) % 6;
                pendingPitchDraw = true;
            } else {
                switch (pitchBoxCursor) {
                    case 0:
                        if (getPitchChordMode()) {
                            movePitchChordIdx(delta);
                        } else {
                            pitchScale = (uint8_t)((pitchScale + delta + SCALE_COUNT) % SCALE_COUNT);
                        }
                        pitchNotesFrozen = false;
                        transposeOffset  = 0;
                        scheduleSaveParams();
                        pendingPitchDraw = true;
                        break;
                    case 1:
                        pitchRoot = (uint8_t)((pitchRoot + delta + 12) % 12);
                        pitchNotesFrozen = false;
                        transposeOffset  = 0;
                        scheduleSaveParams();
                        pendingPitchDraw = true;
                        break;
                    case 2: {
                        uint8_t oldSpread = pitchSpread;
                        pitchSpread = (uint8_t)clampVal((int)pitchSpread + delta, 1, 5);
                        if (pitchSpread != oldSpread) {
                            // Spread-Änderung setzt Transpose-Offset zurück
                            pitchNotesFrozen = false;
                            transposeOffset  = 0;
                        }
                        scheduleSaveParams();
                        pendingPitchDraw = true;
                        break;
                    }
                    case 3:
                        invertPitchSequence(delta);
                        break;
                    case 4:
                        aInvPitchSequence(delta);
                        break;
                    case 5:
                        transposePitchSequence(delta);
                        break;
                }
            }
            break;
        case 1:
            if (getPitchPresetBrowseActive()) {
                setPitchPresetBrowseIdx(getPitchPresetBrowseIdx() + delta);
            } else {
                pitchShift = (int8_t)clampVal((int)pitchShift + delta, -3, 3);
                scheduleSaveParams();
                pendingPitchDraw = true;
            }
            break;
        case 2:
            pitchItvlCursor = ((pitchItvlCursor + delta) % 7 + 7) % 7;
            pendingPitchDraw = true;
            break;
    }
}

// ---------------------------------------------------------------------------
// PITCH1-Screen: Encoder-Knopfdruck
//   Enc3-Button: aktives Intervall am Cursor-Bit toggeln
// ---------------------------------------------------------------------------
static void handlePitchButton(int enc) {
    if (getPitchStepEditActive()) {
        if (enc == 2) { togglePitchStepChromatic(); return; }
    }
    if (enc == 0) {
        pitchBoxEditMode = !pitchBoxEditMode;
        pendingPitchDraw = true;
        return;
    }
    if (enc == 1) {
        if (!getPitchPresetBrowseActive()) {
            setPitchPresetBrowseActive(true);
        } else {
            int loadIdx = getPitchPresetBrowseIdx();
            setPitchPresetBrowseActive(false);
            loadPitchPreset(loadIdx);
        }
        return;
    }
    if (enc != 2) return;
    if (!intervalExists(pitchScale, pitchItvlCursor)) return;  // Grad existiert nicht
    uint8_t toggled = pitchIntervalMask ^ (uint8_t)(1u << pitchItvlCursor);
    if (toggled != 0) {  // mindestens ein Intervall aktiv halten
        pitchIntervalMask = toggled;
        // Freeze bleibt aktiv: frozenMidiBase neu berechnen mit neuer Mask,
        // dann frozenMidi[] mit bestehendem transposeOffset neu anwenden.
        int len = clampVal(PatLen[0], 1, 32);
        int fullNoteList[60];
        int fullCount = buildNoteList(5, pitchScale, pitchRoot, 0x7F, fullNoteList);
        if (pitchNotesFrozen && fullCount >= 2) {
            // Basis neu quantisieren mit neuer Mask
            for (int i = 0; i < len; i++) {
                frozenMidiBase[i] = quantizeToMidi(PitchNote1[i], pitchSpread,
                                                   pitchScale, pitchRoot, pitchIntervalMask);
            }
            // Offset validieren: falls er jetzt out-of-bounds wäre, auf 0 klemmen
            bool valid = true;
            for (int i = 0; i < len; i++) {
                int baseIdx = 0, bestDist = 127;
                for (int k = 0; k < fullCount; k++) {
                    int d = abs(fullNoteList[k] - frozenMidiBase[i]);
                    if (d < bestDist) { bestDist = d; baseIdx = k; }
                }
                int newIdx = baseIdx + transposeOffset;
                if (newIdx < 0 || newIdx >= fullCount) { valid = false; break; }
            }
            if (!valid) transposeOffset = 0;
            // frozenMidi[] neu berechnen
            for (int i = 0; i < len; i++) {
                int baseIdx = 0, bestDist = 127;
                for (int k = 0; k < fullCount; k++) {
                    int d = abs(fullNoteList[k] - frozenMidiBase[i]);
                    if (d < bestDist) { bestDist = d; baseIdx = k; }
                }
                frozenMidi[i] = fullNoteList[baseIdx + transposeOffset];
            }
        }
        scheduleSaveParams();
        pendingPitchDraw = true;
    }
}

// ---------------------------------------------------------------------------
// COND1/2/3-Screen: Encoder-Drehung
//   Enc1 (i=0): Schritt-Cursor 0..PatLen-1 bewegen (+ Seite wechseln)
//   Enc2 (i=1): Bedingungstyp am Cursor-Step (0..COND_TYPE_COUNT-1)
//   Enc3 (i=2): Aktion am Cursor-Step (0..COND_ACT_COUNT-1)
// ---------------------------------------------------------------------------
static void handleCondEncoder(int ch, int enc, int delta) {
    int cursor = condStepCursor[ch];
    int len    = clampVal(PatLen[ch], 1, 32);
    int oldPage = cursor / 8;

    if (enc == 0) {
        int oldCursor = cursor;
        cursor = ((cursor + delta) % len + len) % len;
        condStepCursor[ch] = cursor;
        int newPage = cursor / 8;
        if (newPage != oldPage) {
            // Kein fillScreen: drawCondCell löscht seinen Spaltenbereich selbst.
            // Labels ändern sich nicht → drawCondLabels entfällt.
            drawCondTitle(ch);
            for (int c = 0; c < 8; c++) drawCondCell(ch, newPage, c);
        } else {
            drawCondCell(ch, newPage, oldCursor % 8);  // erase old border
            drawCondCell(ch, newPage, cursor    % 8);  // draw new border
            drawCondTitle(ch);
        }
    } else if (enc == 1) {
        if (getCondPresetMode() && getCondPresetCh() == ch) {
            condPresetModeRotate(ch, delta);
            return;
        }
        uint8_t &ct = condTypeArr[ch][cursor];
        ct = (uint8_t)(((int)ct + delta + COND_TYPE_COUNT) % COND_TYPE_COUNT);
        scheduleSaveParams();
        drawCondCell(ch, cursor / 8, cursor % 8);
    } else if (enc == 2) {
        uint8_t &ca = condActionArr[ch][cursor];
        ca = (uint8_t)(((int)ca + delta + COND_ACT_COUNT) % COND_ACT_COUNT);
        scheduleSaveParams();
        drawCondCell(ch, cursor / 8, cursor % 8);
    }
}

// ---------------------------------------------------------------------------
// COND screen: Encoder-Knopfdruck
//   Enc1 Long-Press: Step löschen (COND_NONE + COND_ACT_NONE)
// ---------------------------------------------------------------------------
static void handleCondButtonPress(int ch) {
    int cursor = condStepCursor[ch];
    condTypeArr[ch][cursor]   = COND_NONE;
    condActionArr[ch][cursor] = COND_ACT_NONE;
    scheduleSaveParams();
    drawCondCell(ch, cursor / 8, cursor % 8);
}

static void clearAllCondSteps(int ch) {
    int len = clampVal(PatLen[ch], 1, 32);
    for (int i = 0; i < len; i++) {
        condTypeArr[ch][i]   = COND_NONE;
        condActionArr[ch][i] = COND_ACT_NONE;
    }
    scheduleSaveParams();
    drawCondScreen(ch);
}

// ---------------------------------------------------------------------------
// Quick Save: aktiver Slot überschreiben, dann Schreibzeiger auf nächsten freien Slot.
// ---------------------------------------------------------------------------
void resetQuickSavePointer() { nextSaveSlot = -1; }

static void performQuickSave() {
    int slot;
    if (nextSaveSlot >= 0) {
        slot = nextSaveSlot;
    } else {
        slot = getActiveSlot();
        if (slot < 0) {
            uint16_t used = getSlotsUsedMask();
            for (int s = 0; s < SLOT_COUNT; s++) {
                if (!(used & (1u << s))) { slot = s; break; }
            }
        }
    }
    if (slot < 0) {
        showSaveToast(-1);
        return;
    }
    // Toast sofort anzeigen; LittleFS-Write auf Main-Loop verschieben (kein Blocking im Encoder-Handler)
    showSaveToast(slot);
    pendingSlotSaveSlot = slot;

    // Schreibzeiger auf nächsten freien Slot nach slot vorrücken
    uint16_t used = getSlotsUsedMask();
    nextSaveSlot = -1;
    for (int s = slot + 1; s < SLOT_COUNT; s++) {
        if (!(used & (1u << s))) { nextSaveSlot = s; break; }
    }
}

// ---------------------------------------------------------------------------
// CHORD_SEQ-Screen: Encoder-Drehung
//   Enc1: Step-Cursor bewegen
//   Enc2: Wert am aktiven Feld ändern
//   Enc3: Seite direkt wechseln
// ---------------------------------------------------------------------------
static void handleChordSeqEncoder(int enc, int delta) {
    int len  = (int)chordLen;
    int page = chordStepCursor / 8;

    if (enc == 0) {
        // Step-Cursor
        int oldCursor = chordStepCursor;
        chordStepCursor = ((chordStepCursor + delta) % len + len) % len;
        int newPage = chordStepCursor / 8;
        if (newPage != page) {
            drawChordSeqTitleBar();
            for (int c = 0; c < 8; c++) drawChordSeqCell(newPage, c);
        } else {
            drawChordSeqCell(page, oldCursor % 8);
            drawChordSeqCell(page, chordStepCursor % 8);
        }
    } else if (enc == 1) {
        // Wert am aktiven Feld
        int s = chordStepCursor;
        switch (chordFieldCursor) {
            case 0: { // AkkNr — nur gespeicherte ChordDefs erlaubt
                uint8_t cur = (chordSlots[s].akkNr == CHORD_SLOT_EMPTY) ? csResolveAkkNr(s) : chordSlots[s].akkNr;
                int v = (int)cur + delta;
                if (v < 0) {
                    chordSlots[s].akkNr = CHORD_SLOT_EMPTY;
                } else {
                    v = clampVal(v, 0, CHORD_DEF_COUNT - 1);
                    // Nur Slots anwählen, die gespeichert wurden
                    if (chordDefs[v].saved) chordSlots[s].akkNr = (uint8_t)v;
                }
                break;
            }
            case 1: // Mute toggle
                chordSlots[s].mute = !chordSlots[s].mute;
                break;
            case 2: { // Legato
                uint8_t cur = (chordSlots[s].leg == CHORD_SLOT_EMPTY) ? 0 : chordSlots[s].leg;
                int v = (int)cur + delta;
                if (v < 0) { chordSlots[s].leg = CHORD_SLOT_EMPTY; }
                else        { chordSlots[s].leg = (uint8_t)clampVal(v, 0, 1); }
                break;
            }
            case 3: { // Val 10–100 in 10er-Schritten
                uint8_t cur = (chordSlots[s].val == CHORD_SLOT_EMPTY) ? csResolveValNr(s) : chordSlots[s].val;
                int v = (int)cur + delta * 10;
                if (v < 10) { chordSlots[s].val = CHORD_SLOT_EMPTY; }
                else         { chordSlots[s].val = (uint8_t)clampVal(v, 10, 100); }
                break;
            }
            case 4: { // Div
                static const uint8_t divVals[] = { 1, 2, 4, 8, 16, 32 };
                int cur = 0;
                for (int k = 0; k < 6; k++) if (divVals[k] == chordDiv) { cur = k; break; }
                cur = clampVal(cur + delta, 0, 5);
                chordDiv = divVals[cur];
                drawChordSeqTitleBar();
                return;
            }
            case 5: // Len
                chordLen = (uint8_t)clampVal((int)chordLen + delta, 1, 32);
                drawChordSeqTitleBar();
                return;
        }
        drawChordSeqCell(page, s % 8);
    } else if (enc == 2) {
        // Seite direkt wechseln
        int maxPage = ((int)chordLen - 1) / 8;
        int newPage = clampVal(page + delta, 0, maxPage);
        if (newPage != page) {
            chordStepCursor = newPage * 8;
            for (int c = 0; c < 8; c++) drawChordSeqCell(newPage, c);
        }
    }
}

static void handleChordSeqButton(int enc) {
    if (enc == 0) {
        // Feld-Cursor weiterschalten: AkkNr→Mute→Leg→Val→Div→Len→AkkNr
        chordFieldCursor = (chordFieldCursor + 1) % 6;
        int page = chordStepCursor / 8;
        drawChordSeqTitleBar();
        drawChordSeqCell(page, chordStepCursor % 8);
    } else if (enc == 1) {
        // LIVE toggle — bei Einschalten zu CHORD_DEF navigieren (wie Touch-Handler)
        chordLive = !chordLive;
        if (chordLive) {
            midiOutLiveChord();         // sofort aktuellen Tab spielen
            requestNavigateTo(CHORD_DEF);
        } else {
            midiOutAllNotesOff();       // Live-Akkord stoppen, Sequencer übernimmt
            drawChordSeqTitleBar();
        }
    }
}

// ---------------------------------------------------------------------------
// CHORD_DEF-Screen: Encoder-Drehung
//   Enc1: Akkord-Tab wechseln (0-7)
//   Enc2: Parameter-Zeile wechseln (Spread/Inv/Oct/Tone)
//   Enc3: Wert der aktuellen Zeile ändern
// ---------------------------------------------------------------------------
static void handleChordDefEncoder(int enc, int delta) {
    ChordDef &d = chordDefs[chordDefCursor];
    if (enc == 0) {
        int next = clampVal(chordDefCursor + delta, 0, CHORD_DEF_COUNT - 1);
        bool canSelect = chordDefs[next].saved ||
                         (next == 0) ||
                         (next > 0 && chordDefs[next-1].saved);
        if (canSelect) {
            chordDefCursor = next;
            chordToneCursor = 0;
            if (chordLive) midiOutLiveChord();  // Tab-Wechsel → sofort neuen Akkord spielen
        }
        drawChordDefScreen();
    } else if (enc == 1) {
        chordDefField = clampVal(chordDefField + delta, 0, 3);
        drawChordDefParams();
    } else if (enc == 2) {
        switch (chordDefField) {
            case 0:
                d.spread = (uint8_t)clampVal((int)d.spread + delta, 1, 5);
                break;
            case 1:
                d.inv = (uint8_t)clampVal((int)d.inv + delta, 0, 3);
                break;
            case 2:
                d.oct = (int8_t)clampVal((int)d.oct + delta, -2, 2);
                break;
            case 3: // Tone-Cursor bewegen — kein Toggle, nur Cursor verschieben
                chordToneCursor = clampVal(chordToneCursor + delta, 0, 6);
                break;
        }
        if (chordDefField != 3) {
            scheduleSaveParams();
            if (chordLive) midiOutLiveChord();  // Parameteränderung → sofort aktualisieren
        }
        drawChordDefParams();
    }
}

// ---------------------------------------------------------------------------

void setupEncoders() {
    for (int i = 0; i < 3; i++) {
        pinMode(swPins[i], INPUT_PULLUP);
    }
    delay(10);  // Pullup stabilisieren lassen
    for (int i = 0; i < 3; i++) {
        btnLastState[i] = digitalRead(swPins[i]);
    }
}

void handleEncoders() {
    static uint16_t lastGUIState = 0xFFFF;
    if (GUIState != lastGUIState) {
        if (lastGUIState == PERFORMANCE && encBrowseActive) encBrowseActive = false;
        if (lastGUIState == PERFORMANCE) { resetRhythmBrowseState(); cancelPerfPick(); }
        if (lastGUIState == PITCH1) {
            pitchBoxCursor   = 0;
            pitchBoxEditMode = false;
            resetPitchPresetBrowseState();
        }
        if (lastGUIState == COND1 || lastGUIState == COND2 || lastGUIState == COND3) {
            condPresetModeCancel();
        }
        lastGUIState = GUIState;
    }

    uint32_t now = millis();

    // --- Enc2-Button: screen-unabhängig prüfen (Long Press = Quick Save) ---
    {
        bool s = digitalRead(swPins[1]);
        if (s != btnLastState[1] && (now - btnLastTime[1]) > BTN_DEBOUNCE_MS) {
            btnLastTime[1]  = now;
            btnLastState[1] = s;
            if (s == LOW) {
                enc2PressStartMs = now;
            } else {
                uint32_t held = now - enc2PressStartMs;
                if (held >= LONG_PRESS_MS && GUIState != PERFORMANCE) {
                    performQuickSave();
                } else if (held < LONG_PRESS_MS) {
                    if (GUIState == PERFORMANCE) {
                        handlePerfButton2();
                    } else if (GUIState == COND1 || GUIState == COND2 || GUIState == COND3) {
                        int ch = (GUIState == COND1) ? 0 : (GUIState == COND2) ? 1 : 2;
                        condPresetModeToggle(ch);
                    } else if (GUIState == PITCH1) {
                        handlePitchButton(1);
                    } else if (GUIState == CHORD_SEQ) {
                        handleChordSeqButton(1);  // LIVE toggle + navigiert zu CHORD_DEF
                    } else if (GUIState == CHORD_DEF) {
                        requestNavigateTo(CHORD_SEQ);  // zurück
                    } else if (GUIState == EUCLCIRCS || GUIState == EUCLPARAM1 ||
                               GUIState == EUCLPARAM2 || GUIState == EUCLPARAM3) {
                        handleNormalButton(1);
                    }
                }
            }
        }
    }

    // --- Enc3-Button: screen-unabhängig prüfen (Long Press = NAV-Toggle) ---
    {
        bool s = digitalRead(swPins[2]);
        if (s != btnLastState[2] && (now - btnLastTime[2]) > BTN_DEBOUNCE_MS) {
            btnLastTime[2]  = now;
            btnLastState[2] = s;
            if (s == LOW) {
                enc3PressStartMs = now;
                enc3VlpFlashed   = false;
            } else {
                uint32_t held = now - enc3PressStartMs;
                bool isValGate = (GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
                                  GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3);
                int vgCh = isValGate ? ((GUIState >= GATELEN1) ? GUIState - GATELEN1 : GUIState - VALUES1) : -1;
                if (enc3VlpFlashed) {
                    if (GUIState == PITCH1) {
                        togglePitchStepEdit();
                    } else if (isValGate) {
                        toggleValStepEdit(vgCh);
                    }
                } else if (held >= LONG_PRESS_MS) {
                    if (GUIState == PITCH1 && getPitchStepEditActive()) {
                        togglePitchStepEdit();
                    } else if (isValGate && getValStepEditActive(vgCh)) {
                        toggleValStepEdit(vgCh);
                    } else if (GUIState == NAV) {
                        requestNavigateTo(navPrevState);
                    } else {
                        navPrevState = GUIState;
                        setNavOpenedFrom(navPrevState);
                        requestNavigateTo(NAV);
                    }
                } else {
                    if (GUIState == NAV) {
                        requestNavigateTo(getNavCursorState());
                    } else if (GUIState == PITCH1) {
                        handlePitchButton(2);
                    } else if (GUIState == CHORD_DEF) {
                        // Ton an chordToneCursor togglen
                        if (chordDefField == 3) {
                            ChordDef &d = chordDefs[chordDefCursor];
                            uint8_t toggled = d.toneMask ^ (uint8_t)(1u << chordToneCursor);
                            if (toggled != 0) d.toneMask = toggled;
                            scheduleSaveParams();
                            if (chordLive) midiOutLiveChord();
                            drawChordDefParams();
                        }
                    } else if (GUIState == EUCLCIRCS || GUIState == EUCLPARAM1 ||
                               GUIState == EUCLPARAM2 || GUIState == EUCLPARAM3) {
                        handleNormalButton(2);
                    }
                }
            }
        }
        // Continuous VLP-Check für Enc3 auf PITCH1 und VALUES/GATELEN
        if (btnLastState[2] == LOW && !enc3VlpFlashed) {
            bool isVG = (GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
                         GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3);
            int vgCh2 = isVG ? ((GUIState >= GATELEN1) ? GUIState - GATELEN1 : GUIState - VALUES1) : -1;
            if (GUIState == PITCH1 && !getPitchStepEditActive()) {
                if (now - enc3PressStartMs >= VERY_LONG_PRESS_MS) {
                    enc3VlpFlashed = true;
                    flashPitchBars();
                }
            } else if (isVG && !getValStepEditActive(vgCh2)) {
                if (now - enc3PressStartMs >= VERY_LONG_PRESS_MS) {
                    enc3VlpFlashed = true;
                    flashValBars(vgCh2);
                }
            }
        }
    }

    // Rotation und Enc1-Button nur auf relevanten Screens
    bool encActive = (GUIState == EUCLCIRCS   || GUIState == PERFORMANCE ||
                      GUIState == EUCLPARAM1  || GUIState == EUCLPARAM2  || GUIState == EUCLPARAM3 ||
                      GUIState == PITCH1      || GUIState == NAV         || GUIState == SONG      ||
                      GUIState == COND1       || GUIState == COND2       || GUIState == COND3     ||
                      GUIState == VALUES1     || GUIState == VALUES2     || GUIState == VALUES3   ||
                      GUIState == GATELEN1    || GUIState == GATELEN2    || GUIState == GATELEN3  ||
                      GUIState == CHORD_SEQ   ||
                      GUIState == CHORD_DEF);
    if (!encActive) return;

    for (int i = 0; i < 3; i++) {
        // --- Rotation ---
        long pos      = encoders[i]->read();
        int  rawDelta = (int)((pos - encLastPos[i]) / ENC_STEPS_PER_DETENT);
        int  delta    = -rawDelta;
        if (rawDelta != 0) {
            encLastPos[i] += (long)rawDelta * ENC_STEPS_PER_DETENT;
            if (GUIState == SONG && i == 0) {
                moveSongCursor(delta);
            } else if (GUIState == NAV) {
                if (i == 2) moveNavCursor(delta);
            } else if (GUIState == PERFORMANCE && i == 0) {
                handlePerfEncoder1(delta);
            } else if (GUIState == PERFORMANCE && i == 1) {
                handlePerfEncoder2(delta);
            } else if (GUIState == PITCH1) {
                handlePitchEncoder(i, delta);
            } else if (GUIState == COND1 || GUIState == COND2 || GUIState == COND3) {
                int ch = (GUIState == COND1) ? 0 : (GUIState == COND2) ? 1 : 2;
                handleCondEncoder(ch, i, delta);
            } else if (GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
                       GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3) {
                int ch = (GUIState >= GATELEN1) ? GUIState - GATELEN1 : GUIState - VALUES1;
                if (getValStepEditActive(ch)) {
                    if (i == 0) adjustValStep(ch, delta);
                    else if (i == 2) moveValStepCursor(ch, delta);
                } else {
                    if (i == 2) {
                        // Enc3 links = Einzelschritt-Modus, rechts = Mal-Modus
                        setBarPaintMode(delta > 0);
                    } else if (GUIState < GATELEN1) {  // VALUES screen only
                        if (i == 0) shiftValues(ch, (delta > 0) ? 4 : -4);
                        else if (i == 1) scaleValues(ch, (delta > 0) ? 1.05f : 0.95f);
                    }
                }
            } else if (GUIState == CHORD_SEQ) {
                handleChordSeqEncoder(i, delta);
            } else if (GUIState == CHORD_DEF) {
                handleChordDefEncoder(i, delta);
            } else if (GUIState != PERFORMANCE) {
                handleNormalEncoder(i, delta);
            }
        }

        // --- Enc1-Button: Long Press auf PITCH1 = applyPitchFold ---
        if (i != 0) continue;  // Enc2/Enc3 bereits oben behandelt
        bool s = digitalRead(swPins[0]);
        if (s != btnLastState[0] && (now - btnLastTime[0]) > BTN_DEBOUNCE_MS) {
            btnLastTime[0]  = now;
            btnLastState[0] = s;
            if (s == LOW) {
                enc1PressStartMs = now;
                enc1VlpFlashed   = false;
                if (GUIState == PERFORMANCE) {
                    handlePerfButton1();  // PERFORMANCE: sofort auf Key-Down
                }
            } else {
                uint32_t held = now - enc1PressStartMs;
                if (GUIState == PITCH1) {
                    if (enc1VlpFlashed) {
                        togglePitchChordMode();
                    } else if (held >= LONG_PRESS_MS) {
                        applyAllTransforms();
                    } else {
                        handlePitchButton(0);
                    }
                } else if (GUIState == COND1 || GUIState == COND2 || GUIState == COND3) {
                    int ch = (GUIState == COND1) ? 0 : (GUIState == COND2) ? 1 : 2;
                    if (enc1VlpFlashed) {
                        clearAllCondSteps(ch);
                    } else if (held >= LONG_PRESS_MS) {
                        handleCondButtonPress(ch);
                    }
                    // Short press: no action
                } else if (GUIState == CHORD_DEF) {
                    if (held < LONG_PRESS_MS) {
                        // Short Press: aktuellen Slot speichern (wie SAVE-Button)
                        chordDefs[chordDefCursor].saved = true;
                        scheduleSaveParams();
                        drawChordDefSaveButton();
                        drawChordDefTabs();
                    }
                } else if (GUIState == CHORD_SEQ) {
                    uint32_t held2 = now - enc1PressStartMs;
                    if (held2 >= LONG_PRESS_MS) {
                        // Long Press: Slot löschen
                        chordSlots[chordStepCursor] = { CHORD_SLOT_EMPTY, false, CHORD_SLOT_EMPTY, CHORD_SLOT_EMPTY };
                        drawChordSeqCell(chordStepCursor / 8, chordStepCursor % 8);
                    } else {
                        handleChordSeqButton(0);
                    }
                } else if (GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
                           GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3) {
                    // Short/long press on Values/GateLen: no action (encoders control step-edit)
                } else if (GUIState != NAV && GUIState != PERFORMANCE && GUIState != SONG) {
                    handleNormalButton(0);
                }
            }
        }
        // Continuous VLP-Check: Flash für PITCH1 und COND
        if (btnLastState[0] == LOW && !enc1VlpFlashed) {
            if (now - enc1PressStartMs >= VERY_LONG_PRESS_MS) {
                if (GUIState == PITCH1) {
                    enc1VlpFlashed = true;
                    flashPitchBars();
                } else if (GUIState == COND1 || GUIState == COND2 || GUIState == COND3) {
                    int ch = (GUIState == COND1) ? 0 : (GUIState == COND2) ? 1 : 2;
                    enc1VlpFlashed = true;
                    flashCondBars(ch);
                }
            }
        }
    }
}
