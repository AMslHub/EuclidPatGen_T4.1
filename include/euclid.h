#pragma once

#include <app_state.h>

// ---------------------------------------------------------------------------
// Zentrale Rotations-Hilfsfunktionen (ehemals static-Kopien in gates.cpp,
// ui_screens.cpp und euclid.cpp — jetzt eine einzige Quelle der Wahrheit).
// ---------------------------------------------------------------------------

// Primitiv: liefert den unverdrehten Quellindex fuer Anzeigeindex idx.
inline int euclidRotatedSrc(int idx, int len, int rot) {
    if (len <= 0) return idx;
    int r = rot % len;
    if (r < 0) r += len;
    int src = idx - r;
    if (src < 0) src += len;
    return src;
}

// Gibt den unverdrehten Quellindex fuer Kanal ch und Step idx zurueck.
inline int patternRotatedSrc(int ch, int idx) {
    return euclidRotatedSrc(idx, PatLen[ch], PatRot[ch]);
}

// Prueft, ob an der (rotierten) Position idx im Pattern von Kanal ch ein Hit liegt.
inline bool patternIsHit(int ch, int idx) {
    int len = PatLen[ch];
    if (len <= 0) return false;
    return EPatArr[ch][patternRotatedSrc(ch, idx)];
}

// ---------------------------------------------------------------------------
// Zeichnen / Pattern-Erzeugung
// ---------------------------------------------------------------------------
void clearEucledianCircle(const int R, int len);
void clearAndRestoreRingArc(const int R, double ang, int ox, int oy);
void drawEucledianCircleFromPatternFast(const int R, int len, int PatRot, bool *pattern);
void redrawEucledianCircleLenChange(const int R, int oldLen, int newLen, int PatRot, bool *pattern);
void drawEucledianCircle(const int R, int len, int PatNum, int PatRot, int PatProb, bool *pattern);
void rebuildPattern(int len, int PatNum, int PatProb, bool *pattern);
void drawEucledianCircleFromPattern(const int R, int len, int PatRot, bool *pattern);
void updateEucledianCircle(const int R, int len, int PatRot, uint16_t color, bool *pattern, unsigned int prevStep, unsigned int curStep);
void Eucledian(bool *pattern, int steps, int PatNum, int PatFirstHit);
void mutatePatternKeepHits(const bool *src, bool *dst, int len, int hitCount, int PatProb);
void buildProbPattern(const bool *current, bool *dst, int len, int PatNum, int PatProb, bool euclidRebuild);

// ---------------------------------------------------------------------------
// Pattern-Sync-Hilfsfunktionen (ehemals static in main.cpp und ui_screens.cpp)
// ---------------------------------------------------------------------------

// Kopiert EPatArr[idx] vollstaendig in EPatBArr[idx] (Staged-Buffer).
void syncEPatBFromEPat(int idx);

// Bereitet das naechste Prob-/Auto-Pattern fuer Kanal idx vor (schreibt in EPatBArr).
void stageProbPatternFromCurrent(int idx);
