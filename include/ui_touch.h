#pragma once

#include <app_state.h>

uint16_t getTipPosition();
uint16_t getTipPositionFromXY(int mapX, int mapY);
bool readTouchMapped(int &mapX, int &mapY);
