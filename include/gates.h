#pragma once

#include <app_state.h>

void initCvOutputs();
void triggerGates();
void triggerGateForCh(int ch);
void outputValuesForStep(unsigned int step, uint8_t swingMask = 0);
void outputRatchetValue(int ch, int ratchetIdx, int ratchetTotal);
uint32_t gateLenForStep(int ch, unsigned int step);
