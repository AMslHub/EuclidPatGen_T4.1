#pragma once

#include <app_state.h>

unsigned long initialText();
void setMenuItems4EUCLPARAM(uint16_t color);
void setMenuItems4EUCLCIRCS(uint16_t color);
void drawBpmControls();
void drawBpmValue();
void drawPerformanceScreen();
bool handlePerformance(int mapX, int mapY, uint16_t tipPos);
void setPerfEncBrowseSlot(int slot);  // slot=-1: Browse-Modus beenden
bool getPerfPickActive();             // true wenn P&P Pick-Modus aktiv
void cancelPerfPick();                // Pick-Modus abbrechen + UI zurücksetzen
void executePerfPickPlace(int dst);   // Move ausführen: src→dst, deferred SD-Op setzen
void refreshPerfSlotState();          // Slot-Maske neu laden + alle Boxes neu zeichnen
int  getRhythmPresetCount();
int  getRhythmBrowseIdx();
bool getRhythmBrowseActive();
void setRhythmBrowseIdx(int idx);
void setRhythmBrowseActive(bool active);
void resetRhythmBrowseState();
void loadRhythmPreset(int idx);
void drawEncParamIndicator(int ch);   // Encoder-Parameter-Anzeige fuer einen Kanal (z.B. "H3")
void drawEncParamIndicators();        // Alle drei Indikatoren zeichnen
void drawSpeedIndicator(int ch);      // Geschwindigkeits-Anzeige auf dem Parameter-Screen
void triggerProbAction(int ch);       // Neues Zufallspattern fuer Kanal ch (wie Prob-Button per Touch)
void drawParamButtonHighlight(int ch); // Aktiven Parameter-Button rot hervorheben
void drawAutoRotateBox(int ch);        // Auto-Rotate-Auswahlbox (0-4)
void tickPerformanceUi();
void tickPitchUi();
void tickProbButtonFlash();
void drawExtClockCheckbox();
void drawParamButtons(int PatLen, int PatNum, int PatRot, uint8_t PatProb);
void redrawParam(int idx);
void redrawParamFromPattern(int idx);
void handleEUCLPARAM(int idx, int mapX, int mapY, uint16_t tipPos);
void drawValuesButton(int idx);
void drawXYButton(int idx);
void drawProbAutoCheckbox(int setIdx);
void drawValuesScreen(int setIdx);
void drawValuesPlayhead(int setIdx, unsigned int step);
void resetValuesPlayhead(int setIdx);
void drawValuesBars(int setIdx);
void drawValuesBar(int setIdx, int idx);
void drawRatchetBars(int setIdx);
void drawRatchetBar(int setIdx, int idx);
void drawOctaveBars(int setIdx);
void drawOctaveBar(int setIdx, int idx);
void drawIvStepBars(int setIdx);
void drawIvStepBar(int setIdx, int idx);
void drawHoldCheckbox(int setIdx);
void drawRotateValuesCheckbox(int setIdx);
void handleVALUES(int setIdx, int mapX, int mapY, uint16_t tipPos);
void handleVALUESDrag(int setIdx, int mapX, int mapY);
void drawGateLenButton();
void drawGateLenScreen(int setIdx);
void drawGateLenBars(int setIdx);
void drawGateLenBar(int setIdx, int idx);
void drawGateHoldCheckbox(int setIdx);
void drawRotateGateLenCheckbox(int setIdx);
bool getBarPaintMode();
void setBarPaintMode(bool v);
void drawBarPaintModeIndicator();
void drawHoldStepBar(int setIdx, int idx);
void drawHoldStepBars(int setIdx);
void drawMuteStepBar(int setIdx, int idx);
void drawMuteStepBars(int setIdx);
void handleGATELEN(int setIdx, int mapX, int mapY, uint16_t tipPos);
void handleGATELENDrag(int setIdx, int mapX, int mapY);
void drawXYPadScreen(int setIdx);
bool handleXYPAD(int setIdx, int mapX, int mapY, uint16_t tipPos);
void handleXYPADRecord(int setIdx, int mapX, int mapY, bool drawDot = true);
void drawXYPlayhead(int setIdx, unsigned int step);
void drawXYDotPlayhead(int setIdx, unsigned int step);
void drawPitchScreen();
void applyAllTransforms(); // Rotation+Fold aller Kanäle einfrieren (Enc1-Long-Press auf PITCH1)
void invertPitchSequence(int dir); // IV: Wrap — find by raw PitchNote1, apply OctaveNote1 ±1
void aInvPitchSequence(int dir);   // AI: A-Inv — find by actual MIDI pitch, raise/lower lowest/highest OctaveNote1
void transposePitchSequence(int delta); // Tp: alle Steps um delta Skalenstufen verschieben
bool getPitchChordMode();
void flashPitchBars();             // VLP-Feedback: Balken schwarz→weiß→normal
void togglePitchChordMode();       // Enc1 Very-Long-Press auf PITCH1
void movePitchChordIdx(int delta); // Enc1 drehen im Chord-Modus
bool getPitchStepEditActive();
int  getPitchStepEditCursor();
void togglePitchStepEdit();
void movePitchStepCursor(int delta);
void adjustPitchStepNote(int delta);
void adjustPitchStepOctave(int delta);
void togglePitchStepChromatic();
bool getValStepEditActive(int ch);
int  getValStepEditCursor(int ch);
void toggleValStepEdit(int ch);
void moveValStepCursor(int ch, int delta);
void adjustValStep(int ch, int delta);
void shiftValues(int ch, int amount);  // Enc1: alle Values ±amount (0-255 clamped)
void scaleValues(int ch, float mf);    // Enc2: Expand/Compress um Mittelwert
void flashValBars(int ch);
void handlePITCH(int mapX, int mapY, uint16_t tipPos);
void handlePITCHDrag(int mapX, int mapY);
void drawPitchPlayhead(unsigned int step);
void drawPitchBars();
void drawPitchBarRange(int from, int to);
void drawPitchControls();
void drawPitchHoldCheckbox();
void drawPitchRotateCheckbox();
void drawPitchDisplayModeCheckbox();
int  getPitchPresetBrowseIdx();
bool getPitchPresetBrowseActive();
void setPitchPresetBrowseIdx(int idx);
void setPitchPresetBrowseActive(bool active);
void resetPitchPresetBrowseState();
void loadPitchPreset(int idx);
void drawCvConfigScreen();
void handleCvConfig(int mapX, int mapY, uint16_t tipPos);
void tickCvConfigUi();
void drawGConfigScreen();
void handleGConfig(int mapX, int mapY, uint16_t tipPos);
void refreshGConfigSongSelector();   // Main-Loop ruft nach Song-Op auf
void triggerGConfigSaveFlash();      // Save-Button kurz aufleuchten lassen
void tickGConfigUi();                // Flash-Timer ablaufen lassen
void drawSongScreen();
void handleSong(int mapX, int mapY, uint16_t tipPos);
void tickSongUi();
void moveSongCursor(int delta);
void showSaveToast(int slot);  // slot=-1: "Slots voll"-Meldung
bool tickSaveToast();          // gibt true zurück wenn Toast gerade abgelaufen (→ Screen neu zeichnen)
void drawNavScreen(uint16_t fromState);  // Navigation-Übersicht, fromState = vorheriger Screen
void handleNav(int mapX, int mapY);
void moveNavCursor(int delta);           // Enc3-Drehung auf NAV: Cursor verschieben
uint16_t getNavCursorState();            // State des aktuell markierten Tiles
void navigateToScreen(uint16_t target);  // zeichnet Ziel-Screen und setzt GUIState
void requestNavigateTo(uint16_t target); // deferred: setzt pendingNavTarget, Main-Loop zeichnet wenn safe
void markPreFilled();                    // Phase 2: nächsten fillScreen in draw-Funktion überspringen
void setNavOpenedFrom(uint16_t state);   // setzt navFromState vor deferred NAV-Draw
void drawCondScreen(int setIdx);
void drawChordSeqScreen();
void handleChordSeq(int mapX, int mapY, uint16_t tipPos);
void drawChordSeqTitleBar();
void drawChordSeqCell(int page, int col);
uint8_t csResolveAkkNr(int i);
uint8_t csResolveValNr(int i);
void drawChordDefScreen();
void drawChordDefTabs();
void drawChordDefParams();
void drawChordDefSaveButton();
void handleChordDef(int mapX, int mapY, uint16_t tipPos);
void drawCondCell(int setIdx, int page, int col);   // einzelne Spalte (alle 4 Zeilen)
void drawCondTitle(int setIdx);                     // Titelzeile neu zeichnen
void drawCondButton(int setIdx);                    // kleiner COND-Button auf dem GateLen-Screen
void handleCond(int setIdx, int mapX, int mapY, uint16_t tipPos);
void drawCondPlayhead(int setIdx, unsigned int step);
void flashCondBars(int setIdx);                     // VLP-Feedback: Header kurz rot aufleuchten
void applyCondPreset(int ch, int preset);           // Condition-Preset auf Kanal ch anwenden (kein Draw)
bool getCondPresetMode();
int  getCondPresetCh();
void condPresetModeToggle(int ch);   // Enter (first press) or Apply+Exit (second press)
void condPresetModeCancel();         // Exit without applying
void condPresetModeRotate(int ch, int delta);
