#include <euclid.h>

#include <math.h>

static void copyPatternRange(bool *dst, const bool *src, int len){
    for(int i=0;i<len;i++){
        dst[i] = src[i];
    }
    for(int i=len;i<32;i++){
        dst[i] = false;
    }
}

static int collectPatternIndices(const bool *pattern, int len, bool value, int *out){
    int count = 0;
    for(int i=0;i<len;i++){
        if(pattern[i] == value){
            out[count++] = i;
        }
    }
    return count;
}

static int normalizePatternHitCount(bool *pattern, int len, int targetHits){
    int hitIdx[32] = { 0 };
    int emptyIdx[32] = { 0 };
    int currentHits = collectPatternIndices(pattern, len, true, hitIdx);

    while(currentHits > targetHits){
        int pick = random(0, currentHits);
        int pos = hitIdx[pick];
        pattern[pos] = false;
        hitIdx[pick] = hitIdx[currentHits - 1];
        currentHits--;
    }

    int emptyCount = collectPatternIndices(pattern, len, false, emptyIdx);
    while(currentHits < targetHits && emptyCount > 0){
        int pick = random(0, emptyCount);
        int pos = emptyIdx[pick];
        pattern[pos] = true;
        emptyIdx[pick] = emptyIdx[emptyCount - 1];
        emptyCount--;
        currentHits++;
    }

    return currentHits;
}

static void mutatePatternByMoveCount(const bool *src, bool *dst, int len, int hitCount, int moveCount){
    len = clampVal(len, 0, 32);
    int targetHits = clampVal(hitCount, 0, len);
    int requestedMoves = clampVal(moveCount, 0, 32);

    if(len <= 0){
        for(int i=0;i<32;i++){
            dst[i] = false;
        }
        return;
    }

    bool work[32] = { false };
    int hitIdx[32] = { 0 };
    int emptyIdx[32] = { 0 };
    copyPatternRange(work, src, len);
    normalizePatternHitCount(work, len, targetHits);

    int hitCountNow = collectPatternIndices(work, len, true, hitIdx);
    int emptyCount = collectPatternIndices(work, len, false, emptyIdx);
    int moves = clampVal(requestedMoves, 0, hitCountNow);
    if(moves > emptyCount){
        moves = emptyCount;
    }

    for(int i=0;i<hitCountNow - 1;i++){
        int j = i + random(0, hitCountNow - i);
        int tmp = hitIdx[i];
        hitIdx[i] = hitIdx[j];
        hitIdx[j] = tmp;
    }

    for(int i=0;i<moves;i++){
        int from = hitIdx[i];
        int pickEmpty = random(0, emptyCount);
        int to = emptyIdx[pickEmpty];

        work[from] = false;
        work[to] = true;
        emptyIdx[pickEmpty] = from;
    }

    copyPatternRange(dst, work, len);
}

// Zweck: Aktualisiert die Kreisdarstellung und den Laufpunkt fuer den aktuellen Step.
// Aufruf: Im Main-Loop, einmal pro BPM-Tick (NICHT im ISR-Kontext).
// Side Effects: zeichnet direkt auf das TFT.
// Assumptions: len > 0, cnthold/cnt sind gueltig.
void updateEucledianCircle(const int R, int len, int PatRot, uint16_t color, bool *pattern, unsigned int prevStep, unsigned int curStep){
    int idx = prevStep % len;
    double angleOld = 2.0 * M_PI / len * idx;
    int ox = (int)(CX + R * sin(angleOld) + 0.5);
    int oy = (int)(CY - R * cos(angleOld) + 0.5);

    // Erase old playhead dot
    tft.fillCircle(ox, oy, r3+1, ILI9341_BLACK);

    // Restore the ring arc in the erased zone
    {
        double step = 1.0 / R;
        int kmax = r3 + 3;
        int lim2 = (r3+2) * (r3+2);
        for (int k = -kmax; k <= kmax; k++) {
            double a = angleOld + k * step;
            int px = (int)(CX + R * sin(a) + 0.5);
            int py = (int)(CY - R * cos(a) + 0.5);
            int dx = px - ox, dy = py - oy;
            if (dx*dx + dy*dy <= lim2) {
                tft.drawPixel(px, py, ILI9341_LIGHTGREY);
            }
        }
    }

    // Redraw every dot whose center fell within the erase zone.
    // This covers the old position AND adjacent dots (important at high step counts
    // on small circles where dot spacing < erase diameter).
    {
        int eraseLim2 = (r3+1) * (r3+1);
        for (int i = 0; i < len; i++) {
            double a = 2.0 * M_PI / len * i;
            int px = (int)(CX + R * sin(a) + 0.5);
            int py = (int)(CY - R * cos(a) + 0.5);
            int dx = px - ox, dy = py - oy;
            if (dx*dx + dy*dy > eraseLim2) continue;
            int src = euclidRotatedSrc(i, len, PatRot);
            if (pattern[src]) tft.fillCircle(px, py, r2+2, ILI9341_WHITE);
            else              tft.drawCircle(px, py, r2,   ILI9341_WHITE);
        }
    }

    // Draw new playhead dot (rounded coordinates — consistent with erase logic above)
    int idx2 = curStep % len;
    double angleNew = 2.0 * M_PI / len * idx2;
    int nx = (int)(CX + R * sin(angleNew) + 0.5);
    int ny = (int)(CY - R * cos(angleNew) + 0.5);
    tft.fillCircle(nx, ny, r3, color);
}

// Zweck: Berechnet ein Euclid-Pattern und zeichnet den Kreis samt Hits.
// Side Effects: schreibt in das Pattern-Array und zeichnet auf das TFT.
// Assumptions: len in 1..32, pattern zeigt auf ein gueltiges Array.
static void applyHitProbability(bool *pattern, int len, int prob){
    // Wahrscheinlichkeits-Logik deaktiviert (UI bleibt sichtbar)
    (void)pattern;
    (void)len;
    (void)prob;
    return;
}

// Zweck: Erzeugt ein Pattern und wendet die Hitwahrscheinlichkeit an (ohne Zeichnen).
// Side Effects: schreibt in das uebergebene Array.
// Assumptions: len in 1..32; pattern zeigt auf ein Array mit mindestens 32 Eintraegen.
void rebuildPattern(int len, int PatNum, int PatProb, bool *pattern){
    Eucledian(pattern, len, PatNum, 0);
    applyHitProbability(pattern, len, PatProb);
}

// Zweck: Mutiert ein Pattern, ohne die Zielzahl der Hits zu veraendern.
// Side Effects: schreibt in dst.
// Assumptions: len in 1..32; src/dst zeigen auf Arrays mit mindestens 32 Eintraegen.
void mutatePatternKeepHits(const bool *src, bool *dst, int len, int hitCount, int PatProb){
    len = clampVal(len, 0, 32);
    int targetHits = clampVal(hitCount, 0, len);
    int prob = clampVal(PatProb, 0, 20);

    if(len <= 0){
        for(int i=0;i<32;i++){
            dst[i] = false;
        }
        return;
    }

    bool work[32] = { false };
    int hitIdx[32] = { 0 };
    int emptyIdx[32] = { 0 };
    copyPatternRange(work, src, len);
    normalizePatternHitCount(work, len, targetHits);

    int hitCountNow = collectPatternIndices(work, len, true, hitIdx);
    int emptyCount = collectPatternIndices(work, len, false, emptyIdx);
    long moveThreshold = (long)(20 - prob) * 500L;

    for(int i=0;i<hitCountNow - 1;i++){
        int j = i + random(0, hitCountNow - i);
        int tmp = hitIdx[i];
        hitIdx[i] = hitIdx[j];
        hitIdx[j] = tmp;
    }

    for(int i=0;i<hitCountNow;i++){
        if(emptyCount <= 0 || moveThreshold <= 0){
            break;
        }
        if(random(0, 10000) >= moveThreshold){
            continue;
        }

        int from = hitIdx[i];
        int pickEmpty = random(0, emptyCount);
        int to = emptyIdx[pickEmpty];

        work[from] = false;
        work[to] = true;
        emptyIdx[pickEmpty] = from;
    }

    copyPatternRange(dst, work, len);
}

// Zweck: Erzeugt ein Prob-Pattern wahlweise aus dem aktuellen Pattern oder aus einem frischen Euclid-Rebuild.
// Side Effects: schreibt in dst.
// Assumptions: len in 1..32; current/dst zeigen auf Arrays mit mindestens 32 Eintraegen.
void buildProbPattern(const bool *current, bool *dst, int len, int PatNum, int PatProb, bool euclidRebuild){
    len = clampVal(len, 0, 32);
    int hitCount = clampVal(PatNum, 0, len);
    int prob = clampVal(PatProb, 0, 20);

    if(!euclidRebuild){
        mutatePatternKeepHits(current, dst, len, hitCount, prob);
        return;
    }

    bool base[32] = { false };
    Eucledian(base, len, hitCount, 0);
    int maxMoves = hitCount;
    if(maxMoves > 4){
        maxMoves = 4;
    }
    int moves = ((20 - prob) * maxMoves + 10) / 20;
    if(prob < 20 && maxMoves > 0 && moves == 0){
        moves = 1;
    }
    mutatePatternByMoveCount(base, dst, len, hitCount, moves);
}

// Loescht alle Dot-Positionen eines Kreises (inkl. Laufpunkt-Groesse).
// Muss mit der aktuell ANGEZEIGTEN Laenge aufgerufen werden, bevor neu gezeichnet wird.
void clearEucledianCircle(const int R, int len) {
    for(int i = 0; i < len; i++) {
        tft.fillCircle(CX + R*sin(2*M_PI/len*i), CY - R*cos(2*M_PI/len*i), r3+1, ILI9341_BLACK);
    }
}

// Loescht eine Dot-Position und stellt den Ring-Arc wieder her.
// Vermeidet das teure drawCircle(R) fuer den gesamten Ring.
void clearAndRestoreRingArc(const int R, double ang, int ox, int oy) {
    tft.fillCircle(ox, oy, r3+1, ILI9341_BLACK);
    double step = 1.0 / R;
    int kmax = r3 + 3, lim2 = (r3+2)*(r3+2);
    for (int k = -kmax; k <= kmax; k++) {
        double a = ang + k * step;
        int px = (int)(CX + R * sin(a) + 0.5);
        int py = (int)(CY - R * cos(a) + 0.5);
        int dx = px-ox, dy = py-oy;
        if (dx*dx+dy*dy <= lim2) tft.drawPixel(px, py, ILI9341_LIGHTGREY);
    }
}

// Schnelle Variante von drawEucledianCircleFromPattern:
// Kein drawCircle(R) fuer den gesamten Ring — Arc-Restore je Position stattdessen.
// Fuer same-len Redraws (PatProb, PatRot-Aenderungen).
void drawEucledianCircleFromPatternFast(const int R, int len, int PatRot, bool *pattern) {
    for (int i = 0; i < len; i++) {
        double ang = 2.0 * M_PI / len * i;
        int ox = (int)(CX + R * sin(ang) + 0.5);
        int oy = (int)(CY - R * cos(ang) + 0.5);
        clearAndRestoreRingArc(R, ang, ox, oy);
        int src = euclidRotatedSrc(i, len, PatRot);
        if (pattern[src]) tft.fillCircle(ox, oy, r2+2, ILI9341_WHITE);
        else              tft.drawCircle(ox, oy, r2, ILI9341_WHITE);
    }
}

// Fuer Laengen-Aenderungen: loescht alle alten Positionen (mit Arc-Restore),
// zeichnet dann alle neuen Positionen. Kein drawCircle(R) noetig.
void redrawEucledianCircleLenChange(const int R, int oldLen, int newLen, int PatRot, bool *pattern) {
    for (int i = 0; i < oldLen; i++) {
        double ang = 2.0 * M_PI / oldLen * i;
        int ox = (int)(CX + R * sin(ang) + 0.5);
        int oy = (int)(CY - R * cos(ang) + 0.5);
        clearAndRestoreRingArc(R, ang, ox, oy);
    }
    for (int i = 0; i < newLen; i++) {
        int src = euclidRotatedSrc(i, newLen, PatRot);
        double ang = 2.0 * M_PI / newLen * i;
        int ox = (int)(CX + R * sin(ang) + 0.5);
        int oy = (int)(CY - R * cos(ang) + 0.5);
        if (pattern[src]) tft.fillCircle(ox, oy, r2+2, ILI9341_WHITE);
        else              tft.drawCircle(ox, oy, r2, ILI9341_WHITE);
    }
}

void drawEucledianCircle(const int R, int len, int PatNum, int PatRot, int PatProb, bool *pattern){
    rebuildPattern(len, PatNum, PatProb, pattern);
    tft.drawCircle(CX, CY, R, ILI9341_LIGHTGREY);
    for(int i=0; i<len; i++){
      int src = euclidRotatedSrc(i, len, PatRot);
      int px = (int)(CX + R*sin(2*M_PI/len*i) + 0.5);
      int py = (int)(CY - R*cos(2*M_PI/len*i) + 0.5);
      if(pattern[src]==false){
        tft.drawCircle(px, py, r2, ILI9341_WHITE);
      }else{
        tft.fillCircle(px, py, r2+2, ILI9341_WHITE);
      }
    }
}

// Zweck: Zeichnet einen Eucledian-Kreis aus einem bereits berechneten Pattern.
// Side Effects: zeichnet auf das TFT.
// Assumptions: len in 1..32, pattern zeigt auf ein gueltiges Array.
void drawEucledianCircleFromPattern(const int R, int len, int PatRot, bool *pattern){
    tft.drawCircle(CX, CY, R, ILI9341_LIGHTGREY);
    for(int i=0; i<len; i++){
      int src = euclidRotatedSrc(i, len, PatRot);
      int px = (int)(CX + R*sin(2*M_PI/len*i) + 0.5);
      int py = (int)(CY - R*cos(2*M_PI/len*i) + 0.5);
      tft.fillCircle(px, py, r3+1, ILI9341_BLACK);
      if(pattern[src]==false){
        tft.drawCircle(px, py, r2,   ILI9341_WHITE);
      }else{
        tft.fillCircle(px, py, r2+2, ILI9341_WHITE);
      }
    }
}

// Erzeugt ein Eucledian-Pattern im Array EPat (Länge PatLen, max 32)
// PatNum = Anzahl der Schläge (Pulses)
// PatFirstHit = Index, an dem der erste Schlag stehen soll (Rotation)
// Zweck: Erzeugt ein Euclid-Pattern im Array pattern.
// Side Effects: schreibt in das uebergebene Array.
// Assumptions: steps in 1..32; pattern zeigt auf ein Array mit mindestens 32 Eintraegen.
void Eucledian(bool *pattern, int steps, int PatNum, int PatFirstHit){
    if(steps <= 0) return;
    if(steps > 32) steps = 32; // Sicherheitslimit

    // Spezialfälle
    if(PatNum <= 0){
        for(int i=0;i<steps;i++) pattern[i] = false;
        return;
    }
    if(PatNum >= steps){
        for(int i=0;i<steps;i++) pattern[i] = true;
        return;
    }

    // Einfache, effiziente Erzeugung: Verwende Modulo-Verhältnis
    // pattern[i] = ((i * PatNum) % steps) < PatNum
    for(int i=0;i<steps;i++){
        pattern[i] = ( ((i * PatNum) % steps) < PatNum );
    }

    // Falls gewünscht, rotiere so, dass der erste Hit an PatFirstHit steht
    if(PatFirstHit != 0){
        if(PatFirstHit < 0) PatFirstHit = (PatFirstHit % steps + steps) % steps;
        int firstHit = -1;
        for(int i=0;i<steps;i++) if(pattern[i]){ firstHit = i; break; }
        if(firstHit != -1){
            int offset = (PatFirstHit - firstHit + steps) % steps;
            if(offset != 0){
                bool tmp[32];
                for(int i=0;i<steps;i++) tmp[(i+offset)%steps] = pattern[i];
                for(int i=0;i<steps;i++) pattern[i] = tmp[i];
            }
        }
    }
}

// Zweck: Kopiert EPatArr[idx] vollstaendig in EPatBArr[idx] (Staged-Buffer).
// Side Effects: schreibt in EPatBArr[idx].
// Assumptions: idx in 0..2; PatLen[idx] gueltig.
void syncEPatBFromEPat(int idx) {
    if (idx < 0 || idx > 2) return;
    int len = clampVal(PatLen[idx], 1, 32);
    for (int i = 0; i < len; i++)  EPatBArr[idx][i] = EPatArr[idx][i];
    for (int i = len; i < 32; i++) EPatBArr[idx][i] = false;
}

// Zweck: Bereitet das naechste Prob-/Auto-Pattern fuer Kanal idx vor.
// Side Effects: schreibt in EPatBArr[idx].
// Assumptions: idx in 0..2; aktuelles Pattern in EPatArr[idx] ist gueltig.
void stageProbPatternFromCurrent(int idx) {
    if (idx < 0 || idx > 2) return;
    int len = clampVal(PatLen[idx], 1, 32);
    buildProbPattern(EPatArr[idx], EPatBArr[idx], len, PatNum[idx], PatProb[idx], ProbEuclidRebuild[idx]);
}
