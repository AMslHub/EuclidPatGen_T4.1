#pragma once

#include <app_state.h>

void updateEucledianCircle(const int R, int len, uint16_t color, bool *pattern);
void drawEucledianCircle(const int R, int len, int PatNum, int PatRot, int PatProb, bool *pattern);
void rebuildPattern(int len, int PatNum, int PatProb, bool *pattern);
void drawEucledianCircleFromPattern(const int R, int len, int PatRot, bool *pattern);
void updateEucledianCircle(const int R, int len, int PatRot, uint16_t color, bool *pattern);
void Eucledian(bool *pattern, int steps, int PatNum, int PatFirstHit);
void mutatePatternKeepHits(const bool *src, bool *dst, int len, int hitCount, int PatProb);
void buildProbPattern(const bool *current, bool *dst, int len, int PatNum, int PatProb, bool euclidRebuild);
