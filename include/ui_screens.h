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
void handleGATELEN(int setIdx, int mapX, int mapY, uint16_t tipPos);
void handleGATELENDrag(int setIdx, int mapX, int mapY);
void drawXYPadScreen(int setIdx);
bool handleXYPAD(int setIdx, int mapX, int mapY, uint16_t tipPos);
void handleXYPADRecord(int setIdx, int mapX, int mapY);
void drawXYPlayhead(int setIdx, unsigned int step);
void drawXYDotPlayhead(int setIdx, unsigned int step);
void drawPitchScreen();
void applyPitchFold();   // Ersetzt PitchNote1 durch gefaltetes Pattern (Enc1-Long-Press)
void handlePITCH(int mapX, int mapY, uint16_t tipPos);
void handlePITCHDrag(int mapX, int mapY);
void drawPitchPlayhead(unsigned int step);
void drawPitchBars();
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
void showSaveToast(int slot);  // slot=-1: "Slots voll"-Meldung
bool tickSaveToast();          // gibt true zurück wenn Toast gerade abgelaufen (→ Screen neu zeichnen)
void drawNavScreen(uint16_t fromState);  // Navigation-Übersicht, fromState = vorheriger Screen
void handleNav(int mapX, int mapY);
void moveNavCursor(int delta);           // Enc3-Drehung auf NAV: Cursor verschieben
uint16_t getNavCursorState();            // State des aktuell markierten Tiles
void navigateToScreen(uint16_t target); // zeichnet Ziel-Screen und setzt GUIState
