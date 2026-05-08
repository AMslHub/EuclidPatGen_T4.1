#include <encoders.h>
#include <hardware_map.h>
#include <app_state.h>
#include <euclid.h>
#include <pitch.h>
#include <storage.h>
#include <ui_screens.h>
#include <cv_inputs.h>
#include <Encoder.h>

static Encoder enc1(ENC1_CLK_PIN, ENC1_DT_PIN);
static Encoder enc2(ENC2_CLK_PIN, ENC2_DT_PIN);
static Encoder enc3(ENC3_CLK_PIN, ENC3_DT_PIN);
static Encoder *encoders[3] = { &enc1, &enc2, &enc3 };

static const uint8_t swPins[3] = { ENC1_SW_PIN, ENC2_SW_PIN, ENC3_SW_PIN };

// 0=PatLen, 1=PatNum, 2=PatRot, 3=PatProb  (extern in app_state.h)
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

// ---------------------------------------------------------------------------
// Wendet eine Parameteraenderung fuer Kanal ch an.
//   rotOnly=true: nur PatRot geaendert → kein Rebuild, sofortige Rotation
//   sonst: Aenderung erst am Pattern-Ende sichtbar (pendingCircleRedraw)
// ---------------------------------------------------------------------------
static void applyParamChange(int ch, bool rotOnly) {
    PatLen[ch]  = clampVal(PatLen[ch], 1, 32);
    PatNum[ch]  = clampVal(PatNum[ch], 1, PatLen[ch]);
    PatRot[ch]  = clampVal(PatRot[ch], -(PatLen[ch] - 1), PatLen[ch] - 1);
    PatProb[ch] = (uint8_t)clampVal((int)PatProb[ch], 0, 20);
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
// Reihenfolge: 0=PatLen(L), 1=PatNum(H), 2=PatRot(R), 3=PatProb(P), 4=Speed(S)
// ---------------------------------------------------------------------------
static void handleNormalEncoder(int ch, int delta) {
    if (encParamSel[ch] == 4) {
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
    if (encParamSel[ch] == 3) {
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
    encParamSel[ch] = (encParamSel[ch] + 1) % 5;
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
//   1. Druck → Browse-Modus aktivieren, Slot CYAN hervorheben
//   2. Druck → Slot laden (wenn belegt), Browse-Modus beenden
// ---------------------------------------------------------------------------
static void handlePerfButton1() {
    if (cvSlotSel >= 0) return;  // CV hat Kontrolle: manuelle Auswahl gesperrt
    if (!encBrowseActive) {
        encBrowseSlot = getActiveSlot();
        if (encBrowseSlot < 0) encBrowseSlot = 0;
        encBrowseActive = true;
        setPerfEncBrowseSlot(encBrowseSlot);
    } else {
        encBrowseActive = false;
        setPerfEncBrowseSlot(-1);
        uint8_t used = getSlotsUsedMask();
        if (used & (1u << encBrowseSlot)) {
            requestLoadSlot(encBrowseSlot);
            nextSaveSlot = -1;
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
                pitchBoxCursor = ((pitchBoxCursor + delta) % 4 + 4) % 4;
                drawPitchControls();
            } else {
                switch (pitchBoxCursor) {
                    case 0:
                        if (getPitchChordMode()) {
                            movePitchChordIdx(delta);
                        } else {
                            pitchScale = (uint8_t)((pitchScale + delta + SCALE_COUNT) % SCALE_COUNT);
                            scheduleSaveParams();
                            drawPitchControls();
                            drawPitchBars();
                        }
                        break;
                    case 1:
                        pitchRoot = (uint8_t)((pitchRoot + delta + 12) % 12);
                        scheduleSaveParams();
                        drawPitchControls();
                        drawPitchBars();
                        break;
                    case 2: {
                        uint8_t oldSpread = pitchSpread;
                        pitchSpread = (uint8_t)clampVal((int)pitchSpread + delta, 1, 5);
                        if (pitchSpread != oldSpread) {
                            int len = clampVal(PatLen[0], 1, 32);
                            for (int i = 0; i < len; i++) {
                                int v = ((int)PitchNote1[i] * pitchSpread + oldSpread / 2) / oldSpread;
                                PitchNote1[i] = (uint8_t)clampVal(v, 0, 255);
                            }
                        }
                        scheduleSaveParams();
                        drawPitchControls();
                        drawPitchBars();
                        break;
                    }
                    case 3:
                        invertPitchSequence(delta);
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
                drawPitchControls();
                drawPitchBars();
            }
            break;
        case 2:
            pitchItvlCursor = ((pitchItvlCursor + delta) % 7 + 7) % 7;
            drawPitchControls();
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
        drawPitchControls();
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
        scheduleSaveParams();
        drawPitchControls();
        drawPitchBars();
    }
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
    saveParamsSlot(slot);
    showSaveToast(slot);

    // Schreibzeiger auf nächsten freien Slot nach slot vorrücken
    uint16_t used = getSlotsUsedMask();
    nextSaveSlot = -1;
    for (int s = slot + 1; s < SLOT_COUNT; s++) {
        if (!(used & (1u << s))) { nextSaveSlot = s; break; }
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
        if (lastGUIState == PERFORMANCE) resetRhythmBrowseState();
        if (lastGUIState == PITCH1) {
            pitchBoxCursor   = 0;
            pitchBoxEditMode = false;
            resetPitchPresetBrowseState();
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
                    } else if (GUIState == PITCH1) {
                        handlePitchButton(1);
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
                if (enc3VlpFlashed) {
                    togglePitchStepEdit();
                } else if (held >= LONG_PRESS_MS) {
                    if (GUIState == PITCH1 && getPitchStepEditActive()) {
                        togglePitchStepEdit();
                    } else if (GUIState == NAV) {
                        navigateToScreen(navPrevState);
                    } else {
                        navPrevState = GUIState;
                        GUIState = NAV;
                        drawNavScreen(navPrevState);
                    }
                } else {
                    if (GUIState == NAV) {
                        navigateToScreen(getNavCursorState());
                    } else if (GUIState == PITCH1) {
                        handlePitchButton(2);
                    } else if (GUIState == EUCLCIRCS || GUIState == EUCLPARAM1 ||
                               GUIState == EUCLPARAM2 || GUIState == EUCLPARAM3) {
                        handleNormalButton(2);
                    }
                }
            }
        }
        // Continuous VLP-Check für Enc3 auf PITCH1
        if (btnLastState[2] == LOW && GUIState == PITCH1 && !enc3VlpFlashed &&
            !getPitchStepEditActive()) {
            if (now - enc3PressStartMs >= VERY_LONG_PRESS_MS) {
                enc3VlpFlashed = true;
                flashPitchBars();
            }
        }
    }

    // Rotation und Enc1-Button nur auf relevanten Screens
    bool encActive = (GUIState == EUCLCIRCS   || GUIState == PERFORMANCE ||
                      GUIState == EUCLPARAM1  || GUIState == EUCLPARAM2  || GUIState == EUCLPARAM3 ||
                      GUIState == PITCH1      || GUIState == NAV);
    if (!encActive) return;

    for (int i = 0; i < 3; i++) {
        // --- Rotation ---
        long pos      = encoders[i]->read();
        int  rawDelta = (int)((pos - encLastPos[i]) / ENC_STEPS_PER_DETENT);
        int  delta    = -rawDelta;
        if (rawDelta != 0) {
            encLastPos[i] += (long)rawDelta * ENC_STEPS_PER_DETENT;
            if (GUIState == NAV) {
                if (i == 2) moveNavCursor(delta);
            } else if (GUIState == PERFORMANCE && i == 0) {
                handlePerfEncoder1(delta);
            } else if (GUIState == PERFORMANCE && i == 1) {
                handlePerfEncoder2(delta);
            } else if (GUIState == PITCH1) {
                handlePitchEncoder(i, delta);
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
                } else if (GUIState != NAV && GUIState != PERFORMANCE) {
                    handleNormalButton(0);
                }
            }
        }
        // Continuous VLP-Check: Flash bei Erreichen der Schwelle (während gehalten)
        if (btnLastState[0] == LOW && GUIState == PITCH1 && !enc1VlpFlashed) {
            if (now - enc1PressStartMs >= VERY_LONG_PRESS_MS) {
                enc1VlpFlashed = true;
                flashPitchBars();
            }
        }
    }
}
