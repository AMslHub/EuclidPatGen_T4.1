#pragma once

#include <app_state.h>

unsigned long initialText();
void setMenuItems4EUCLPARAM(uint16_t color);
void setMenuItems4EUCLCIRCS(uint16_t color);
void drawBpmControls();
void drawBpmValue();
void drawPerformanceScreen();
bool handlePerformance(int mapX, int mapY, uint16_t tipPos);
void tickPerformanceUi();
void tickProbButtonFlash();
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
void handleXYPAD(int setIdx, int mapX, int mapY, uint16_t tipPos);
void drawXYPlayhead(int setIdx, unsigned int step);
