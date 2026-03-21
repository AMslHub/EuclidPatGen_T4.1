#include <ui_screens.h>

#include <font_Arial.h>
#include <font_AwesomeF100.h>

#include <euclid.h>
#include <storage.h>
#include <gates.h>

static int lastValuesPlayIdx[3] = { -1, -1, -1 };
static int lastXYPlayIdx[3] = { -1, -1, -1 };

static const int PARAM_BTN_W = 30;
static const int PARAM_BTN_H = 30;
static const int PARAM_BTN_TOPY = 70;
static const int PARAM_BTN_BOTY = 140;
static const int PARAM_COLX[4] = { 85, 125, 165, 205 };
static const int PARAM_VAL_Y = (PARAM_BTN_TOPY + PARAM_BTN_H + PARAM_BTN_BOTY) / 2 - 6;
static const int PROB_BTN_X = PARAM_COLX[1];
static const int PROB_BTN_Y = PARAM_BTN_TOPY - 43;
static const int PROB_BTN_W = (PARAM_COLX[2] + PARAM_BTN_W) - PARAM_COLX[1];
static const int PROB_BTN_H = 35;
static const int PARAM_PBOX_X = 286;
static const int PARAM_PBOX_Y = PARAM_VAL_Y - 6;
static const int PARAM_PBOX_S = 24;
static const int PARAM_ERBOX_X = 12;
static const int PARAM_ERBOX_Y = 108;
static const int PARAM_ERBOX_S = 24;
static const int PERF_BOX_Y = 190;
static const int PERF_BOX_W = 30;
static const int PERF_BOX_H = 30;
static const int PERF_BTN_Y = 150;
static const int PERF_BTN_W = 60;
static const int PERF_BTN_H = 30;
static const int PERF_BOX_XS[7] = {0, 40, 80, 120, 160, 200, 240};
static const int PERF_BTN_XS[3] = {0, 140, 210};
static const int PERF_SEQ_X = 30;
static const int PERF_SEQ_Y = 10;
static const int PERF_SEQ_W = 260;
static const int PERF_SEQ_H = 5;
static const int PERF_SEQ_BOX = 3;
static const int PERF_MS_X1 = 80;
static const int PERF_MS_X2 = 120;
static const int PERF_MS_W = 25;
static const int PERF_MS_H = 25;
static const int PERF_MS_PAD = 0;
static int perfSelected = -1;
static int perfActive = -1;
static uint8_t perfUsedMask = 0;
static bool perfButtonFlash[3] = { false, false, false };
static uint32_t perfButtonFlashUntil[3] = { 0, 0, 0 };
static int perfSeq1LastStep = -1;
static int perfSeq1LastX = -1;
static bool probFlash[3] = { false, false, false };
static uint32_t probFlashUntil[3] = { 0, 0, 0 };
// Zweck: Prueft, ob im rotierten Pattern an der Position ein Hit liegt.
// Side Effects: keine.
// Assumptions: PatLen[setIdx] > 0, EPatArr[setIdx] ist gueltig.
static inline bool isHitRotated(int setIdx, int idx){
  int len = PatLen[setIdx];
  if(len <= 0) return false;
  int rot = PatRot[setIdx] % len;
  if(rot < 0) rot += len;
  int src = idx - rot;
  if(src < 0) src += len;
  return EPatArr[setIdx][src];
}

// Zweck: Berechnet den Quellindex unter Beruecksichtigung der Rotation.
// Side Effects: keine.
// Assumptions: PatLen[setIdx] > 0.
static inline int rotatedSrcIndex(int setIdx, int idx){
  int len = PatLen[setIdx];
  if(len <= 0) return idx;
  int rot = PatRot[setIdx] % len;
  if(rot < 0) rot += len;
  int src = idx - rot;
  if(src < 0) src += len;
  return src;
}

// Zweck: Prueft, ob ein Punkt innerhalb einer Box mit zusaetzlichem Rand liegt.
// Side Effects: keine.
// Assumptions: mapX/mapY sind in Screen-Koordinaten.
static inline bool hitBox(int mapX, int mapY, int x, int y, int w, int h, int pad){
  return (mapX >= (x - pad) && mapX <= (x + w + pad) &&
          mapY >= (y - pad) && mapY <= (y + h + pad));
}

// Zweck: Zeichnet eine vertikale Beschriftung als gestapelte Zeichen.
// Side Effects: schreibt auf das TFT.
// Assumptions: text ist nullterminiert, Schrift ist gesetzt.
static void drawVerticalLabel(int x, int y, const char *text){
  int yPos = y;
  for(const char *p = text; *p != '\0'; ++p){
    tft.setCursor(x, yPos);
    tft.print(*p);
    yPos += 12;
  }
}

// Zweck: Setzt den gemerkten XY-Playhead fuer ein Pattern zurueck.
// Side Effects: schreibt in lastXYPlayIdx.
// Assumptions: setIdx in 0..2.
static void resetXYPlayhead(int setIdx){
  if(setIdx < 0 || setIdx > 2) return;
  lastXYPlayIdx[setIdx] = -1;
}

// Zweck: Zeichnet den Playhead oberhalb des XY-Pads.
// Side Effects: schreibt auf das TFT und aktualisiert lastXYPlayIdx.
// Assumptions: PatLen[setIdx] in 1..32.
// Zweck: Zeichnet den Playhead oberhalb des XY-Pads.
// Side Effects: schreibt auf das TFT und aktualisiert lastXYPlayIdx.
// Assumptions: PatLen[setIdx] in 1..32.
void drawXYPlayhead(int setIdx, unsigned int step){
  if(setIdx < 0 || setIdx > 2) return;
  int len = clampVal(PatLen[setIdx], 1, 32);

  int x0 = 90;
  int w  = 180;
  int y  = 40 - 6;
  int r  = 3;

  int idx = step % len;
  int last = lastXYPlayIdx[setIdx];

  if(last >= 0 && last < len){
    int lastX = x0 + (last * w) / len + (w / len) / 2;
    tft.fillCircle(lastX, y, r, ILI9341_BLACK);
  }

  int x = x0 + (idx * w) / len + (w / len) / 2;
  tft.fillCircle(x, y, r, ILI9341_WHITE);
  lastXYPlayIdx[setIdx] = idx;
}

// Zweck: Zeigt den Starttext an und liefert die Dauer der Ausgabe.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
unsigned long initialText() {
  tft.fillScreen(ILI9341_BLACK);
  unsigned long start = micros();
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setFont(Arial_16);
  tft.println("Eucledian_Test_03");
  tft.println("");
    
  tft.setTextColor(ILI9341_GREEN);
  tft.setFont(Arial_12);
  tft.println("3-Spur-Eucledian/Turing-Sequencer ");
  tft.println("Jede Spur hat Values- und Gateausgang");
  tft.println("Performance-Section mit 7 Sequenz-Speichern");
  tft.println("XY-Pad für jede Spur");    
  return micros() - start;
}

// Zweck: Zeichnet die Standard-Menueelemente fuer das Parameter-Menue.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void setMenuItems4EUCLPARAM(uint16_t color){
  // Rücksprungpfeil
  tft.setTextColor(color); 
  tft.setFont(Arial_12);
  tft.setFont(AwesomeF100_24);
  tft.setCursor(20, 20);
  tft.print((char)18);
}

// Zweck: Zeichnet die Menueelemente fuer die Kreis-Uebersicht.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void setMenuItems4EUCLCIRCS(uint16_t color){
  // Rücksprungpfeil
  tft.setTextColor(color); 
  tft.setFont(Arial_24);
  tft.setCursor(20, 20);
  tft.printf("1");
  tft.setCursor(280, 20);
  tft.printf("2");
  tft.setCursor(280, 200);
  tft.printf("3");
  tft.setCursor(20, 200);
  tft.printf("P");
}

// Zweck: Zeichnet die BPM-Buttons und den aktuellen Wert im Hauptscreen.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void drawBpmControls(){
  // Buttons an den Positionen wie PatNum/PatRot im Parameter-Screen
  const int bpmPlusYOffset = 3;
  const int bpmMinusYOffset = -3;
  for(int i=1;i<=2;i++){
    tft.drawRect(PARAM_COLX[i], PARAM_BTN_TOPY + bpmPlusYOffset, PARAM_BTN_W, PARAM_BTN_H, ILI9341_DARKGREY);
    tft.drawRect(PARAM_COLX[i], PARAM_BTN_BOTY + bpmMinusYOffset, PARAM_BTN_W, PARAM_BTN_H, ILI9341_DARKGREY);
  }

  tft.setFont(Arial_24);
  tft.setTextColor(ILI9341_LIGHTGREY);
  for(int i=1;i<=2;i++){
    tft.setCursor(PARAM_COLX[i] + 5, PARAM_BTN_TOPY + 4 + bpmPlusYOffset);
    tft.println("+");
    tft.setCursor(PARAM_COLX[i] + 10, PARAM_BTN_BOTY - 1 + bpmMinusYOffset);
    tft.println("-");
  }

  drawBpmValue();
}

// Zweck: Aktualisiert nur die BPM-Wertanzeige.
// Side Effects: schreibt auf das TFT.
// Assumptions: drawBpmControls() wurde bereits aufgerufen.
void drawBpmValue(){
  // Wertbereich säubern (verhindert Artefakte beim Stellenwechsel)
  tft.fillRect(140, PARAM_VAL_Y - 4, 40, 22, ILI9341_BLACK);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(140, PARAM_VAL_Y);
  tft.printf("%3u", bpm);
}

// Zweck: Zeichnet zentrierten Text in ein Rechteck.
// Side Effects: schreibt auf das TFT.
// Assumptions: label ist nullterminiert.
static void drawCenteredLabel(int x, int y, int w, int h, const char *label, int charW, int charH, int xOffset = 0, int yOffset = 0){
  int len = 0;
  for(const char *p = label; *p != '\0'; ++p){
    len++;
  }
  int textW = len * charW;
  int textH = charH;
  int cx = x + (w - textW) / 2 + xOffset;
  int cy = y + (h - textH) / 2 + 1 + yOffset;
  tft.setCursor(cx, cy);
  tft.print(label);
}

// Zweck: Zeichnet den Prob-Button (optional aktiv).
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
static void drawProbButton(bool active){
  uint16_t border = ILI9341_DARKGREY;
  uint16_t fill = active ? ILI9341_LIGHTGREY : ILI9341_BLACK;
  uint16_t text = active ? ILI9341_BLACK : ILI9341_LIGHTGREY;
  tft.drawRect(PROB_BTN_X, PROB_BTN_Y, PROB_BTN_W, PROB_BTN_H, border);
  tft.fillRect(PROB_BTN_X + 1, PROB_BTN_Y + 1, PROB_BTN_W - 2, PROB_BTN_H - 2, fill);
  tft.setFont(Arial_16);
  tft.setTextColor(text);
  drawCenteredLabel(PROB_BTN_X, PROB_BTN_Y, PROB_BTN_W, PROB_BTN_H, "Prob", 8, 16, -7, 0);
}

// Zweck: Startet einen kurzen Flash fuer den Prob-Button.
// Side Effects: schreibt auf das TFT und setzt Timer-Status.
// Assumptions: idx in 0..2.
static void startProbButtonFlash(int idx){
  if(idx < 0 || idx > 2) return;
  probFlash[idx] = true;
  probFlashUntil[idx] = millis() + 5;
  drawProbButton(true);
}

static void syncStagedPatternFromCurrent(int idx){
  if(idx < 0 || idx > 2) return;
  int len = clampVal(PatLen[idx], 1, 32);
  for(int i=0;i<len;i++){
    EPatBArr[idx][i] = EPatArr[idx][i];
  }
  for(int i=len;i<32;i++){
    EPatBArr[idx][i] = false;
  }
}

static void stageProbAutoPatternFromCurrent(int idx){
  if(idx < 0 || idx > 2) return;
  int len = clampVal(PatLen[idx], 1, 32);
  buildProbPattern(EPatArr[idx], EPatBArr[idx], len, PatNum[idx], PatProb[idx], ProbEuclidRebuild[idx]);
}

void tickProbButtonFlash(){
  uint32_t now = millis();
  for(int i=0;i<3;i++){
    if(probFlash[i] && (int32_t)(now - probFlashUntil[i]) >= 0){
      probFlash[i] = false;
      if((i == 0 && GUIState == EUCLPARAM1) ||
         (i == 1 && GUIState == EUCLPARAM2) ||
         (i == 2 && GUIState == EUCLPARAM3)){
        drawProbButton(false);
      }
    }
  }
}

// Zweck: Wendet die Probabilistik auf das Pattern an (P nicht gesetzt).
// Side Effects: schreibt in EPatArr[idx].
// Assumptions: PatLen/PatNum/PatProb sind gueltig; idx in 0..2.
static void applyProbToPattern(int idx){
  if(idx < 0 || idx > 2) return;
  int len = clampVal(PatLen[idx], 1, 32);
  buildProbPattern(EPatArr[idx], EPatArr[idx], len, PatNum[idx], PatProb[idx], ProbEuclidRebuild[idx]);
  syncStagedPatternFromCurrent(idx);
}

// Zweck: Zeichnet die Checkbox fuer Euclid-Rebuild plus kleine Mutation.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
static void drawProbEuclidRebuildCheckbox(int setIdx){
  (void)setIdx;
  tft.setFont(Arial_12);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(PARAM_ERBOX_X - 1, PARAM_ERBOX_Y - 14);
  tft.print("ER");

  tft.drawRect(PARAM_ERBOX_X, PARAM_ERBOX_Y, PARAM_ERBOX_S, PARAM_ERBOX_S, ILI9341_DARKGREY);
  tft.fillRect(PARAM_ERBOX_X + 1, PARAM_ERBOX_Y + 1, PARAM_ERBOX_S - 2, PARAM_ERBOX_S - 2, ILI9341_BLACK);
  if(ProbEuclidRebuild[setIdx]){
    tft.drawLine(PARAM_ERBOX_X + 4, PARAM_ERBOX_Y + 12, PARAM_ERBOX_X + 10, PARAM_ERBOX_Y + 18, ILI9341_GREEN);
    tft.drawLine(PARAM_ERBOX_X + 10, PARAM_ERBOX_Y + 18, PARAM_ERBOX_X + 20, PARAM_ERBOX_Y + 6, ILI9341_GREEN);
  }
}

// Zweck: Zeichnet ein Patternnummer-Kaestchen (Status/Selection).
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..6.
static void drawPerfSlotBox(int idx){
  int x = PERF_BOX_XS[idx];
  bool selected = (idx == perfSelected);
  uint16_t border = ILI9341_DARKGREY;
  uint16_t fill = selected ? ILI9341_GREEN
                           : ((perfUsedMask & (1u << idx)) ? ILI9341_WHITE : ILI9341_BLACK);
  tft.fillRect(x + 1, PERF_BOX_Y + 1, PERF_BOX_W - 2, PERF_BOX_H - 2, fill);
  tft.drawRect(x, PERF_BOX_Y, PERF_BOX_W, PERF_BOX_H, border);
  if(idx == perfActive){
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_BLACK);
    drawCenteredLabel(x, PERF_BOX_Y, PERF_BOX_W, PERF_BOX_H, "A", 8, 16, -5, 0);
  }
}

// Zweck: Zeichnet einen Performance-Button (Load/Save/Del) mit optionalem Flash.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..2.
static void drawPerfButton(int idx){
  int x = PERF_BTN_XS[idx];
  uint16_t border = (idx == 1 || idx == 2) ? ILI9341_RED : ILI9341_WHITE;
  uint16_t fill = perfButtonFlash[idx] ? ILI9341_LIGHTGREY : ILI9341_BLACK;
  tft.drawRect(x, PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, border);
  tft.drawRect(x + 1, PERF_BTN_Y + 1, PERF_BTN_W - 2, PERF_BTN_H - 2, border);
  tft.drawRect(x + 2, PERF_BTN_Y + 2, PERF_BTN_W - 4, PERF_BTN_H - 4, border);
  tft.drawRect(x + 3, PERF_BTN_Y + 3, PERF_BTN_W - 6, PERF_BTN_H - 6, border);
  tft.fillRect(x + 4, PERF_BTN_Y + 4, PERF_BTN_W - 8, PERF_BTN_H - 8, fill);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_LIGHTGREY);
  const char *labels[3] = { "Load", "Save", "Del" };
  drawCenteredLabel(x, PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, labels[idx], 8, 16, -8, -1);
}

// Zweck: Startet nicht-blockierenden Flash fuer einen Button.
// Side Effects: setzt Timer-Status.
// Assumptions: idx in 0..2.
static void startPerfButtonFlash(int idx){
  perfButtonFlash[idx] = true;
  perfButtonFlashUntil[idx] = millis() + 80;
  drawPerfButton(idx);
}

// Zweck: Aktualisiert Button-Flash (nicht-blockierend).
// Side Effects: schreibt auf das TFT.
static void updatePerfButtonFlash(){
  uint32_t now = millis();
  for(int i=0;i<3;i++){
    if(perfButtonFlash[i] && (int32_t)(now - perfButtonFlashUntil[i]) >= 0){
      perfButtonFlash[i] = false;
      drawPerfButton(i);
    }
  }
}

// Zweck: Zeichnet das Fenster fuer die Sequencer-Position (Seq1).
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
static void drawPerfSeqWindow(){
  tft.fillRect(PERF_SEQ_X, PERF_SEQ_Y, PERF_SEQ_W, PERF_SEQ_H, ILI9341_BLACK);
  tft.drawRect(PERF_SEQ_X, PERF_SEQ_Y, PERF_SEQ_W, PERF_SEQ_H, ILI9341_DARKGREY);
  perfSeq1LastStep = -1;
  perfSeq1LastX = -1;
}

// Zweck: Aktualisiert die Abspielposition von Sequencer 1.
// Side Effects: schreibt auf das TFT.
// Assumptions: cnt und PatLen[0] sind gueltig.
static void drawPerfSeq1Playhead(){
  int len = PatLen[0];
  if(len <= 0){
    if(perfSeq1LastX >= 0){
      int y = PERF_SEQ_Y + (PERF_SEQ_H - PERF_SEQ_BOX) / 2;
      tft.fillRect(perfSeq1LastX, y, PERF_SEQ_BOX, PERF_SEQ_BOX, ILI9341_BLACK);
    }
    perfSeq1LastStep = -1;
    perfSeq1LastX = -1;
    return;
  }

  int step = (int)(cnt % (unsigned int)len);
  if(step == perfSeq1LastStep) return;

  int xMin = PERF_SEQ_X + 1;
  int xMax = PERF_SEQ_X + PERF_SEQ_W - 1 - PERF_SEQ_BOX;
  int x = xMin;
  if(len > 1 && xMax > xMin){
    x = xMin + (step * (xMax - xMin)) / (len - 1);
  }
  int y = PERF_SEQ_Y + (PERF_SEQ_H - PERF_SEQ_BOX) / 2;

  if(perfSeq1LastX >= 0){
    tft.fillRect(perfSeq1LastX, y, PERF_SEQ_BOX, PERF_SEQ_BOX, ILI9341_BLACK);
  }
  tft.fillRect(x, y, PERF_SEQ_BOX, PERF_SEQ_BOX, ILI9341_WHITE);
  perfSeq1LastX = x;
  perfSeq1LastStep = step;
}

void tickPerformanceUi(){
  updatePerfButtonFlash();
  drawPerfSeq1Playhead();
}

// Zweck: Zeichnet Mute/Solo-Box fuer eine Sequenz.
// Side Effects: schreibt auf das TFT.
// Assumptions: row in 0..2.
static void drawPerfMsBox(int row, bool isSoloColumn, int yTop){
  int x = isSoloColumn ? PERF_MS_X2 : PERF_MS_X1;
  bool on = isSoloColumn ? SoloSeq[row] : MuteSeq[row];
  uint16_t fill = on ? ILI9341_RED : ILI9341_BLACK;
  tft.fillRect(x + 2, yTop + 2, PERF_MS_W - 4, PERF_MS_H - 4, fill);
  tft.drawRect(x, yTop, PERF_MS_W, PERF_MS_H, ILI9341_WHITE);
  tft.drawRect(x + 1, yTop + 1, PERF_MS_W - 2, PERF_MS_H - 2, ILI9341_WHITE);
}

// Zweck: Zeichnet den Performance-Menue-Screen.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void drawPerformanceScreen(){
  tft.fillScreen(ILI9341_BLACK);
  // Ruecksprungpfeil im Performance-Menue (eigene Position)
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setFont(AwesomeF100_24);
  tft.setCursor(10, 15);
  tft.print((char)18);

  // Sequencer-Positionsfenster (Seq1)
  drawPerfSeqWindow();

  // Patternnummer-Kaestchen
  tft.setFont(Arial_12);
  tft.setTextColor(ILI9341_LIGHTGREY);
  perfSelected = -1;
  perfActive = getActiveSlot();
  perfUsedMask = getSlotsUsedMask();
  for(int i=0;i<7;i++){
    drawPerfSlotBox(i);
  }

  // Load/Save/Del Buttons
  for(int i=0;i<3;i++){
    perfButtonFlash[i] = false;
    perfButtonFlashUntil[i] = 0;
    drawPerfButton(i);
  }

  // Sequencer-Labels ueber dem Load-Button
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_LIGHTGREY);
  int seqCharH = 16;
  int seqBottom3 = PERF_BTN_Y - 15;
  int seqBottom2 = seqBottom3 - 30;
  int seqBottom1 = seqBottom2 - 30;
  drawCenteredLabel(PERF_BTN_XS[0], seqBottom1 - seqCharH, PERF_BTN_W, seqCharH, "Seq1", 8, seqCharH, -5, 0);
  drawCenteredLabel(PERF_BTN_XS[0], seqBottom2 - seqCharH, PERF_BTN_W, seqCharH, "Seq2", 8, seqCharH, -5, 0);
  drawCenteredLabel(PERF_BTN_XS[0], seqBottom3 - seqCharH, PERF_BTN_W, seqCharH, "Seq3", 8, seqCharH, -5, 0);

  // Mute/Solo-Boxen neben Seq-Labels
  int msY1 = seqBottom1 - PERF_MS_H + 5;
  int msY2 = seqBottom2 - PERF_MS_H + 5;
  int msY3 = seqBottom3 - PERF_MS_H + 5;

  drawPerfMsBox(0, false, msY1);
  drawPerfMsBox(0, true, msY1);
  drawPerfMsBox(1, false, msY2);
  drawPerfMsBox(1, true, msY2);
  drawPerfMsBox(2, false, msY3);
  drawPerfMsBox(2, true, msY3);

  drawCenteredLabel(PERF_MS_X1, msY1 - seqCharH, PERF_MS_W, seqCharH, "M", 8, seqCharH, -5, -5);
  drawCenteredLabel(PERF_MS_X2, msY1 - seqCharH, PERF_MS_W, seqCharH, "S", 8, seqCharH, -5, -5);
}

// Zweck: Behandelt Touch-Events im Performance-Menue.
// Side Effects: wechselt GUIState und schreibt auf das TFT.
// Assumptions: mapX/mapY sind gemappt; tipPos ist gueltig.
// Return: true, wenn eine Aktion ausgefuehrt wurde.
bool handlePerformance(int mapX, int mapY, uint16_t tipPos){
  updatePerfButtonFlash();
  if(tipPos == UL){
    GUIState = EUCLCIRCS;
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
    drawBpmControls();
    drawBpmValue();
    drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
    drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
    drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
    return true;
  }

  // Pattern-Boxen
  for(int i=0;i<7;i++){
    if(hitBox(mapX, mapY, PERF_BOX_XS[i], PERF_BOX_Y, PERF_BOX_W, PERF_BOX_H, 6)){
      int prev = perfSelected;
      perfSelected = (perfSelected == i) ? -1 : i;
      if(prev >= 0) drawPerfSlotBox(prev);
      if(perfSelected >= 0) drawPerfSlotBox(perfSelected);
      return true;
    }
  }

  // Mute/Solo-Boxen
  tft.setFont(Arial_16);
  int seqBottom3 = PERF_BTN_Y - 15;
  int seqBottom2 = seqBottom3 - 30;
  int seqBottom1 = seqBottom2 - 30;
  int msY[3] = { seqBottom1 - PERF_MS_H + 5, seqBottom2 - PERF_MS_H + 5, seqBottom3 - PERF_MS_H + 5 };

  for(int row = 0; row < 3; row++){
    if(hitBox(mapX, mapY, PERF_MS_X1, msY[row], PERF_MS_W, PERF_MS_H, PERF_MS_PAD)){
      // Mute aktiviert -> alle Solos aus
      SoloSeq[0] = false;
      SoloSeq[1] = false;
      SoloSeq[2] = false;
      MuteSeq[row] = !MuteSeq[row];
      drawPerfMsBox(row, false, msY[row]);
      drawPerfMsBox(0, true, msY[0]);
      drawPerfMsBox(1, true, msY[1]);
      drawPerfMsBox(2, true, msY[2]);
      return true;
    }
    if(hitBox(mapX, mapY, PERF_MS_X2, msY[row], PERF_MS_W, PERF_MS_H, PERF_MS_PAD)){
      // Solo aktiviert -> alle Mutes aus
      MuteSeq[0] = false;
      MuteSeq[1] = false;
      MuteSeq[2] = false;
      if(SoloSeq[row]){
        SoloSeq[row] = false;
      }else{
        SoloSeq[0] = false;
        SoloSeq[1] = false;
        SoloSeq[2] = false;
        SoloSeq[row] = true;
      }
      drawPerfMsBox(0, false, msY[0]);
      drawPerfMsBox(1, false, msY[1]);
      drawPerfMsBox(2, false, msY[2]);
      drawPerfMsBox(0, true, msY[0]);
      drawPerfMsBox(1, true, msY[1]);
      drawPerfMsBox(2, true, msY[2]);
      return true;
    }
  }

  // Buttons: Load / Save / Del
  if(hitBox(mapX, mapY, PERF_BTN_XS[0], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(0);
    if(perfSelected >= 0 && (perfUsedMask & (1u << perfSelected))){
      requestLoadSlot(perfSelected);
      int was = perfSelected;
      perfSelected = -1;
      drawPerfSlotBox(was);
    }
    return true;
  }
  if(hitBox(mapX, mapY, PERF_BTN_XS[1], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(1);
    if(perfSelected >= 0){
      if(saveParamsSlot(perfSelected)){
        perfUsedMask = (uint8_t)(perfUsedMask | (1u << perfSelected));
        int was = perfSelected;
        perfSelected = -1;
        drawPerfSlotBox(was);
      }
    }
    return true;
  }
  if(hitBox(mapX, mapY, PERF_BTN_XS[2], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(2);
    if(perfSelected >= 0){
      if(deleteParamsSlot(perfSelected)){
        perfUsedMask = (uint8_t)(perfUsedMask & ~(1u << perfSelected));
        int was = perfSelected;
        perfSelected = -1;
        drawPerfSlotBox(was);
      }
    }
    return true;
  }
  return false;
}

// Parameter-Buttons zeichnen für Len, Num und Acc
// Zweck: Zeichnet Parameter-Buttons fuer Len, Num und Rot.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void drawParamButtons(int PatLen, int PatNum, int PatRot, uint8_t PatProb){
  // Prob-Button oberhalb der +/- Zeichen (zwischen Hits und Rotation)
  drawProbButton(false);

  for(int i=0;i<4;i++){
    tft.drawRect(PARAM_COLX[i], PARAM_BTN_TOPY, PARAM_BTN_W, PARAM_BTN_H, ILI9341_DARKGREY);
    tft.drawRect(PARAM_COLX[i], PARAM_BTN_BOTY, PARAM_BTN_W, PARAM_BTN_H, ILI9341_DARKGREY);
  }

  tft.setFont(Arial_16);
  tft.setCursor(PARAM_COLX[0] - 2, PARAM_VAL_Y);
  tft.printf("%3d", PatLen);

  tft.setCursor(PARAM_COLX[1] - 2, PARAM_VAL_Y);
  tft.printf("%3d", PatNum);

  tft.setCursor(PARAM_COLX[2] - 2, PARAM_VAL_Y);
  tft.printf("%3d", PatRot);

  tft.setCursor(PARAM_COLX[3] - 2, PARAM_VAL_Y);
  tft.printf("%1.2f", PatProb / 20.0f);

  tft.setFont(Arial_24);
  for(int i=0;i<4;i++){
    tft.setCursor(PARAM_COLX[i] + 8, PARAM_BTN_TOPY + 2);
    tft.println("+");
    tft.setCursor(PARAM_COLX[i] + 12, PARAM_BTN_BOTY + 2);
    tft.println("-");
  }
}

// Redraw helper for parameter menus (zeigt Kreis mit R1)
// Zweck: Zeichnet das Parameter-Menue fuer das gewaehlte Pattern neu.
// Side Effects: schreibt auf das TFT und aktualisiert PatLen/PatNum/PatRot.
// Assumptions: idx in 0..2; Arrays sind gueltig.
void redrawParam(int idx){
    if(idx<0 || idx>2) return;

    // Bildschirm für Parameter-Menü löschen (verhindert Überlagerung alter Zeichnungen)
    tft.fillScreen(ILI9341_BLACK);

    // Werte beschränken
    PatLen[idx] = clampVal(PatLen[idx], 1, 32);
    PatNum[idx] = clampVal(PatNum[idx], 0, PatLen[idx]);
    PatRot[idx] = clampVal(PatRot[idx], PatLen[idx] > 0 ? -PatLen[idx] : 0, PatLen[idx] > 0 ? PatLen[idx] : 0);

    PatProb[idx] = clampVal(PatProb[idx], 0, 20);

    drawEucledianCircle(R1, PatLen[idx], PatNum[idx], PatRot[idx], PatProb[idx], EPatArr[idx]);
    if(PatProbAuto[idx]){
      stageProbAutoPatternFromCurrent(idx);
    }else{
      syncStagedPatternFromCurrent(idx);
    }
    drawParamButtons(PatLen[idx], PatNum[idx], PatRot[idx], PatProb[idx]);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    if(idx >= 0 && idx <= 2){
      drawValuesButton(idx);
      drawXYButton(idx);
      drawProbAutoCheckbox(idx);
      drawProbEuclidRebuildCheckbox(idx);
    }
}

// Zweck: Zeichnet das Parameter-Menue neu, ohne das Pattern erneut zu erzeugen.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..2; EPatArr[idx] ist bereits aktuell.
void redrawParamFromPattern(int idx){
    if(idx<0 || idx>2) return;

    tft.fillScreen(ILI9341_BLACK);

    PatLen[idx] = clampVal(PatLen[idx], 1, 32);
    PatNum[idx] = clampVal(PatNum[idx], 0, PatLen[idx]);
    PatRot[idx] = clampVal(PatRot[idx], PatLen[idx] > 0 ? -PatLen[idx] : 0, PatLen[idx] > 0 ? PatLen[idx] : 0);
    PatProb[idx] = clampVal(PatProb[idx], 0, 20);

    drawEucledianCircleFromPattern(R1, PatLen[idx], PatRot[idx], EPatArr[idx]);
    drawParamButtons(PatLen[idx], PatNum[idx], PatRot[idx], PatProb[idx]);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    drawValuesButton(idx);
    drawXYButton(idx);
    drawProbAutoCheckbox(idx);
    drawProbEuclidRebuildCheckbox(idx);
}

// Zweck: Zeichnet den V1/V2/V3-Button im Parameter-Menue.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..2.
void drawValuesButton(int idx){
  tft.setFont(Arial_12);
  tft.drawRect(260, 10, 50, 30, ILI9341_DARKGREY);
  tft.setCursor(270, 18);
  if(idx == 0){
    tft.print("V1");
  }else if(idx == 1){
    tft.print("V2");
  }else{
    tft.print("V3");
  }
}

// Zweck: Zeichnet den XY-Button im Parameter-Menue.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..2.
void drawXYButton(int idx){
  (void)idx;
  tft.setFont(Arial_12);
  tft.drawRect(260, 200, 50, 30, ILI9341_DARKGREY);
  tft.setCursor(270, 208);
  tft.print("XY");
}

// Zweck: Zeichnet die Checkbox fuer die Auto-Hitwahrscheinlichkeit.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawProbAutoCheckbox(int setIdx){
  tft.setFont(Arial_12);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(PARAM_PBOX_X + 6, PARAM_PBOX_Y - 14);
  tft.print("P");

  tft.drawRect(PARAM_PBOX_X, PARAM_PBOX_Y, PARAM_PBOX_S, PARAM_PBOX_S, ILI9341_DARKGREY);
  tft.fillRect(PARAM_PBOX_X + 1, PARAM_PBOX_Y + 1, PARAM_PBOX_S - 2, PARAM_PBOX_S - 2, ILI9341_BLACK);
  if(PatProbAuto[setIdx]){
    tft.drawLine(PARAM_PBOX_X + 4, PARAM_PBOX_Y + 12, PARAM_PBOX_X + 10, PARAM_PBOX_Y + 18, ILI9341_GREEN);
    tft.drawLine(PARAM_PBOX_X + 10, PARAM_PBOX_Y + 18, PARAM_PBOX_X + 20, PARAM_PBOX_Y + 6, ILI9341_GREEN);
  }
}

// Zweck: Baut den Values-Screen fuer ein Pattern auf.
// Side Effects: schreibt auf das TFT und setzt Playhead-Status.
// Assumptions: setIdx in 0..2.
void drawValuesScreen(int setIdx){
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    drawHoldCheckbox(setIdx);
    drawRotateValuesCheckbox(setIdx);
    drawGateLenButton();
    // Rahmen um den Values-Bereich
    {
      int x0 = 10;
      int y0 = 240 - 5 - 160;
      int h  = 160;
      tft.drawRect(x0 - 1, y0 - 1, 320 - 2 * x0 + 2, h + 2, ILI9341_DARKGREY);
    }
    drawValuesBars(setIdx);
    resetValuesPlayhead(setIdx);
    drawValuesPlayhead(setIdx, cnt);
}

// Zweck: Zeichnet alle Werte-Balken fuer ein Pattern.
// Side Effects: schreibt auf das TFT.
// Assumptions: PatLen[setIdx] in 1..32.
void drawValuesBars(int setIdx){
    int len = clampVal(PatLen[setIdx], 1, 32);
    for(int i=0;i<len;i++){
        drawValuesBar(setIdx, i);
    }
}

// Zweck: Setzt den gemerkten Playhead fuer ein Pattern zurueck.
// Side Effects: schreibt in lastValuesPlayIdx.
// Assumptions: setIdx in 0..2.
void resetValuesPlayhead(int setIdx){
    if(setIdx < 0 || setIdx > 2) return;
    lastValuesPlayIdx[setIdx] = -1;
}

// Zweck: Zeichnet den Playhead fuer einen Step.
// Side Effects: schreibt auf das TFT und aktualisiert lastValuesPlayIdx.
// Assumptions: PatLen[setIdx] in 1..32.
void drawValuesPlayhead(int setIdx, unsigned int step){
    if(setIdx < 0 || setIdx > 2) return;
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int w  = (320 - 2 * x0) / len;

    int idx = step % len;
    int last = lastValuesPlayIdx[setIdx];

    int y = y0 - 6;
    int r = 3;

    if(last >= 0 && last < len){
        int lastX = x0 + last * w + w / 2;
        tft.fillCircle(lastX, y, r, ILI9341_BLACK);
    }

    int x = x0 + idx * w + w / 2;
    tft.fillCircle(x, y, r, ILI9341_WHITE);
    lastValuesPlayIdx[setIdx] = idx;
}

// Zweck: Zeichnet einen einzelnen Werte-Balken.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx im gueltigen Bereich 0..len-1.
void drawValuesBar(int setIdx, int idx){
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int wStep  = (320 - 2 * x0) / len;
    int w  = 6;
    int xStep = x0 + idx * wStep;
    int x = xStep + (wStep - w) / 2;

    int src = RotateValues[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    uint8_t val = ValuesArr[setIdx][src];
    int fillH = (int)((val * (long)h) / 255L);
    bool active = RotateValues[setIdx] ? EPatArr[setIdx][src] : isHitRotated(setIdx, idx);

    // Clear full step area
    tft.fillRect(xStep, y0, wStep, h, ILI9341_BLACK);
    if(fillH > 0){
        int y = y0 + h - fillH;
        if(active){
            tft.fillRect(x, y, w, fillH, ILI9341_WHITE);
        }else{
            tft.drawRect(x, y, w, fillH, ILI9341_WHITE);
        }
    }
}

// Zweck: Zeichnet die Hold-Checkbox.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawHoldCheckbox(int setIdx){
    int x = 260;
    int y = 10;
    int s = 24;

    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 30, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("H");

    if(*HoldArr[setIdx]){
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

// Zweck: Zeichnet die Rotate-Values-Checkbox.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawRotateValuesCheckbox(int setIdx){
    int x = 260;
    int y = 42;
    int s = 24;

    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 24, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("RV");

    if(RotateValues[setIdx]){
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

// Zweck: Zeichnet den Button zum GateLen-Screen.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void drawGateLenButton(){
    int x = 135;
    int y = 10;
    int w = 50;
    int h = 24;
    tft.drawRect(x, y, w, h, ILI9341_DARKGREY);
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(x + 6, y + 6);
    tft.print("GLen");
}

// Generische Handler-Funktion für EUCLPARAMx Menüs
// Zweck: Behandelt Touch-Events im Parameter-Menue.
// Side Effects: veraendert Pattern-Parameter, schreibt auf TFT, speichert EEPROM.
// Assumptions: idx in 0..2; mapX/mapY sind gemappt; tipPos ist gueltig.
void handleEUCLPARAM(int idx, int mapX, int mapY, uint16_t tipPos){
    if(hitBox(mapX, mapY, PROB_BTN_X, PROB_BTN_Y, PROB_BTN_W, PROB_BTN_H, 6)){
        if(!PatProbAuto[idx]){
            applyProbToPattern(idx);
            scheduleSaveParams();
            redrawParamFromPattern(idx);
            startProbButtonFlash(idx);
        }
        return;
    }
    if(hitBox(mapX, mapY, PARAM_PBOX_X, PARAM_PBOX_Y, PARAM_PBOX_S, PARAM_PBOX_S, 8)){
        PatProbAuto[idx] = !PatProbAuto[idx];
        if(PatProbAuto[idx]){
            stageProbAutoPatternFromCurrent(idx);
        }else{
            syncStagedPatternFromCurrent(idx);
        }
        scheduleSaveParams();
        drawProbAutoCheckbox(idx);
        return;
    }
    if(hitBox(mapX, mapY, PARAM_ERBOX_X, PARAM_ERBOX_Y, PARAM_ERBOX_S, PARAM_ERBOX_S, 8)){
        ProbEuclidRebuild[idx] = !ProbEuclidRebuild[idx];
        if(PatProbAuto[idx]){
            stageProbAutoPatternFromCurrent(idx);
        }else{
            syncStagedPatternFromCurrent(idx);
        }
        scheduleSaveParams();
        drawProbEuclidRebuildCheckbox(idx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 200, 50, 30, 6)){
        GUIState = (idx == 0) ? XY1 : (idx == 1) ? XY2 : XY3;
        drawXYPadScreen(idx);
        return;
    }
    switch(tipPos){
      case P1U:
        PatLen[idx]++;
        redrawParam(idx);
        scheduleSaveParams();
        break;
      case P1L:
        PatLen[idx]--;
        redrawParam(idx);
        scheduleSaveParams();
        break;
      case P2U:
        PatNum[idx]++;
        redrawParam(idx);
        scheduleSaveParams();
        break;
      case P2L:
        PatNum[idx]--;
        redrawParam(idx);
        scheduleSaveParams();
        break;
      case P3U:
        PatRot[idx]++;
        redrawParamFromPattern(idx);
        scheduleSaveParams();
        break;
      case P3L:
        PatRot[idx]--;
        redrawParamFromPattern(idx);
        scheduleSaveParams();
        break;
      case P4U:
        PatProb[idx] = clampVal((int)PatProb[idx] + 1, 0, 20);
        redrawParam(idx);
        scheduleSaveParams();
        break;
      case P4L:
        PatProb[idx] = clampVal((int)PatProb[idx] - 1, 0, 20);
        redrawParam(idx);
        scheduleSaveParams();
        break;
      case UL: // Rückkehr zu den drei Kreisen
        // Ensure current params are saved
        scheduleSaveParams();
        GUIState = EUCLCIRCS; // new GUI-State
        tft.fillScreen(ILI9341_BLACK);
        setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
        // Restore Eucledian circles aus aktuellem Pattern (kein Rebuild)
        drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
        drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
        drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
        drawBpmControls();
        break;
      case UR:
        if(idx == 0){
          GUIState = VALUES1;
          drawValuesScreen(0);
        }else if(idx == 1){
          GUIState = VALUES2;
          drawValuesScreen(1);
        }else if(idx == 2){
          GUIState = VALUES3;
          drawValuesScreen(2);
        }
        break;
      default:
        break;
    }
}

// Zweck: Behandelt Touch-Events im Values-Screen.
// Side Effects: veraendert Values/Hold/Rotate, schreibt auf TFT, speichert EEPROM.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt.
void handleVALUES(int setIdx, int mapX, int mapY, uint16_t tipPos){
    if(tipPos == UL){
        GUIState = (setIdx == 0) ? EUCLPARAM1 : (setIdx == 1) ? EUCLPARAM2 : EUCLPARAM3;
        redrawParamFromPattern(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 42, 24, 24, 8)){
        // Rotate Values: Werte werden relativ zur Pattern-Rotation interpretiert.
        RotateValues[setIdx] = !RotateValues[setIdx];
        scheduleSaveParams();
        drawRotateValuesCheckbox(setIdx);
        drawValuesBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 10, 24, 24, 8)){
        // Hold: Werte werden nur bei Hits uebernommen.
        *HoldArr[setIdx] = !(*HoldArr[setIdx]);
        scheduleSaveParams();
        drawHoldCheckbox(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 135, 10, 50, 24, 6)){
        GUIState = (setIdx == 0) ? GATELEN1 : (setIdx == 1) ? GATELEN2 : GATELEN3;
        drawGateLenScreen(setIdx);
        return;
    }

    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int w  = (320 - 2 * x0) / len;

    if(mapX < x0 || mapX >= (x0 + len * w) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    int idx = (mapX - x0) / w;
    int writeIdx = RotateValues[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    ValuesArr[setIdx][writeIdx] = (uint8_t)v;
    scheduleSaveParams();
    drawValuesBar(setIdx, idx);
}

// Zweck: Behandelt Drag-Events im Values-Screen.
// Side Effects: veraendert Values, schreibt auf TFT, speichert EEPROM.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt.
void handleVALUESDrag(int setIdx, int mapX, int mapY){
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int w  = (320 - 2 * x0) / len;

    if(mapX < x0 || mapX >= (x0 + len * w) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    int idx = (mapX - x0) / w;
    int writeIdx = RotateValues[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    ValuesArr[setIdx][writeIdx] = (uint8_t)v;
    scheduleSaveParams();
    drawValuesBar(setIdx, idx);
}

// Zweck: Baut den GateLen-Screen fuer ein Pattern auf.
// Side Effects: schreibt auf das TFT und setzt Playhead-Status.
// Assumptions: setIdx in 0..2.
void drawGateLenScreen(int setIdx){
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    drawGateHoldCheckbox(setIdx);
    drawRotateGateLenCheckbox(setIdx);
    // Rahmen um den GateLen-Bereich
    {
      int x0 = 10;
      int y0 = 240 - 5 - 160;
      int h  = 160;
      tft.drawRect(x0 - 1, y0 - 1, 320 - 2 * x0 + 2, h + 2, ILI9341_DARKGREY);
    }
    drawGateLenBars(setIdx);
    resetValuesPlayhead(setIdx);
    drawValuesPlayhead(setIdx, cnt);
}

// Zweck: Zeichnet alle GateLen-Balken fuer ein Pattern.
// Side Effects: schreibt auf das TFT.
// Assumptions: PatLen[setIdx] in 1..32.
void drawGateLenBars(int setIdx){
    int len = clampVal(PatLen[setIdx], 1, 32);

    for(int i=0;i<len;i++){
        drawGateLenBar(setIdx, i);
    }
}

// Zweck: Zeichnet einen einzelnen GateLen-Balken.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx im gueltigen Bereich 0..len-1.
void drawGateLenBar(int setIdx, int idx){
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int wStep  = (320 - 2 * x0) / len;
    int w  = 6;
    int xStep = x0 + idx * wStep;
    int x = xStep + (wStep - w) / 2;

    int src = RotateGateLen[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    uint8_t val = GateLenArr[setIdx][src];
    int fillH = (int)((val * (long)h) / 255L);
    bool active = RotateGateLen[setIdx] ? EPatArr[setIdx][src] : isHitRotated(setIdx, idx);

    // Clear full step area
    tft.fillRect(xStep, y0, wStep, h, ILI9341_BLACK);
    if(fillH > 0){
        int y = y0 + h - fillH;
        if(active){
            tft.fillRect(x, y, w, fillH, ILI9341_WHITE);
        }else{
            tft.drawRect(x, y, w, fillH, ILI9341_WHITE);
        }
    }
}

// Zweck: Zeichnet die GateHold-Checkbox.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawGateHoldCheckbox(int setIdx){
    int x = 260;
    int y = 10;
    int s = 24;

    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 30, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("GH");

    if(*GateHoldArr[setIdx]){
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

// Zweck: Zeichnet die Rotate-GateLen-Checkbox.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawRotateGateLenCheckbox(int setIdx){
    int x = 260;
    int y = 42;
    int s = 24;

    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 28, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("RGL");

    if(RotateGateLen[setIdx]){
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

// Zweck: Behandelt Touch-Events im GateLen-Screen.
// Side Effects: veraendert GateLen/GateHold/Rotate, schreibt auf TFT.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt.
void handleGATELEN(int setIdx, int mapX, int mapY, uint16_t tipPos){
    if(tipPos == UL){
        GUIState = (setIdx == 0) ? VALUES1 : (setIdx == 1) ? VALUES2 : VALUES3;
        drawValuesScreen(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 42, 24, 24, 8)){
        // Rotate GateLen: Gate-Laengen relativ zur Pattern-Rotation interpretieren.
        RotateGateLen[setIdx] = !RotateGateLen[setIdx];
        scheduleSaveParams();
        drawRotateGateLenCheckbox(setIdx);
        drawGateLenBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 10, 24, 24, 8)){
        // GateHold: Aktiviert variable Gate-Laengen pro Step.
        *GateHoldArr[setIdx] = !(*GateHoldArr[setIdx]);
        drawGateHoldCheckbox(setIdx);
        return;
    }

    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int w  = (320 - 2 * x0) / len;

    if(mapX < x0 || mapX >= (x0 + len * w) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    int idx = (mapX - x0) / w;
    int writeIdx = RotateGateLen[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    GateLenArr[setIdx][writeIdx] = (uint8_t)v;
    drawGateLenBar(setIdx, idx);
}

// Zweck: Behandelt Drag-Events im GateLen-Screen.
// Side Effects: veraendert GateLen, schreibt auf TFT.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt.
void handleGATELENDrag(int setIdx, int mapX, int mapY){
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int w  = (320 - 2 * x0) / len;

    if(mapX < x0 || mapX >= (x0 + len * w) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    int idx = (mapX - x0) / w;
    int writeIdx = RotateGateLen[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    GateLenArr[setIdx][writeIdx] = (uint8_t)v;
    drawGateLenBar(setIdx, idx);
}

// Zweck: Zeichnet den XY-Pad-Screen fuer ein Pattern.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawXYPadScreen(int setIdx){
    (void)setIdx;
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);

    int x = 90;
    int y = 40;
    int w = 180;
    int h = 180;
    tft.drawRect(x, y, w, h, ILI9341_DARKGREY);

    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(x + 60, y + h + 8);
    tft.print("Value");

    // Vertikale Beschriftung links vom Pad (etwas nach unten versetzt)
    drawVerticalLabel(x - 20, y + 34, "Gatelength");

    resetXYPlayhead(setIdx);
    drawXYPlayhead(setIdx, cnt);
}

// Zweck: Behandelt Touch-Events im XY-Pad-Screen.
// Side Effects: wechselt GUIState und schreibt auf das TFT.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt; tipPos ist gueltig.
void handleXYPAD(int setIdx, int mapX, int mapY, uint16_t tipPos){
    if(tipPos == UL){
        GUIState = (setIdx == 0) ? EUCLPARAM1 : (setIdx == 1) ? EUCLPARAM2 : EUCLPARAM3;
        redrawParamFromPattern(setIdx);
        return;
    }

    int x = 90;
    int y = 40;
    int w = 180;
    int h = 180;
    if(mapX < x || mapX >= (x + w) || mapY < y || mapY >= (y + h)){
        return;
    }

    int len = clampVal(PatLen[setIdx], 1, 32);
    int idx = cnt % len;
    int writeValIdx = RotateValues[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;
    int writeGateIdx = RotateGateLen[setIdx] ? rotatedSrcIndex(setIdx, idx) : idx;

    int v = map(mapX, x, x + w - 1, 0, 255);
    int g = map(mapY, y + h - 1, y, 0, 255);
    v = clampVal(v, 0, 255);
    g = clampVal(g, 0, 255);

    ValuesArr[setIdx][writeValIdx] = (uint8_t)v;
    GateLenArr[setIdx][writeGateIdx] = (uint8_t)g;

    // Werte sofort ausgeben, damit die Aenderung direkt hoerbar/effektiv ist.
    outputValuesForStep(cnt);
}
