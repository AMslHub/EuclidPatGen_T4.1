#pragma once

#include <app_state.h>

void saveParams();
void loadParams();
void scheduleSaveParams();

uint8_t getSlotsUsedMask();
bool saveParamsSlot(int slot);
bool deleteParamsSlot(int slot);
bool requestLoadSlot(int slot);
bool applyPendingLoadIfReady(unsigned int step);
int getActiveSlot();
