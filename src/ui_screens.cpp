#include <ui_screens.h>

#include <ili9341_t3n_font_Arial.h>
#include <font_AwesomeF100.h>

#include <euclid.h>
#include <storage.h>
#include <gates.h>
#include <pitch.h>
#include <encoders.h>
#include <cv_inputs.h>

static int lastValuesPlayIdx[3] = { -1, -1, -1 };
static int lastXYPlayIdx[3]    = { -1, -1, -1 };
static int lastXYDotIdx[3]     = { -1, -1, -1 };
static const uint16_t XY_GRID_COLOR = 0x2104;  // dunkles Grau fuer XY-Raster
static bool xyPadPitchMode = false;  // true: Kanal-1-XY-Pad zeigt Pitch/Value statt GateLen/Value

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
static const int PARAM_ARBOX_X = 12;
static const int PARAM_ARBOX_Y = 154;
static const int PARAM_ARBOX_S = 24;
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
static int perfEncSlot = -1;   // Encoder-Browse-Hervorhebung (-1 = inaktiv)
static bool perfButtonFlash[3] = { false, false, false };
static uint32_t perfButtonFlashUntil[3] = { 0, 0, 0 };
static int perfSeq1LastStep = -1;
static int perfSeq1LastX = -1;
static bool probFlash[3] = { false, false, false };
static uint32_t probFlashUntil[3] = { 0, 0, 0 };

// ---------------------------------------------------------------------------
// Rhythmus-Presets
// ---------------------------------------------------------------------------
struct RhythmPreset {
    const char *name;
    uint8_t len[3];
    uint8_t hits[3];
    int8_t  rot[3];
};

static const RhythmPreset RHYTHM_PRESETS[] = {
    //          name           len               hits           rot
    // --- Latin / Afro-Cuban ---
    { "Tresillo",    {  8,  8,  8 }, {  3,  5,  2 }, {  0,  0,  0 } },
    { "Rumba-Clave", {  8,  8, 16 }, {  3,  5,  3 }, {  0,  2,  0 } },
    { "Son Clave",   {  8,  8,  8 }, {  2,  3,  5 }, {  0,  1,  0 } },
    { "Cinquillo",   {  8,  8,  8 }, {  5,  3,  2 }, {  0,  0,  1 } },
    { "Habanera",    {  8,  8, 16 }, {  4,  3,  5 }, {  1,  0,  0 } },
    { "Bossa Nova",  { 16,  8, 16 }, {  5,  3,  7 }, {  0,  0,  0 } },
    { "Samba",       { 16, 16,  8 }, {  7,  5,  3 }, {  0,  1,  0 } },
    { "Cascara",     { 16, 16, 16 }, {  9,  5,  3 }, {  0,  0,  2 } },
    { "Bolero",      { 16, 16,  8 }, {  5,  3,  4 }, {  0,  3,  0 } },
    // --- Afrika ---
    { "Afrobeat",    { 16, 16, 16 }, {  7,  5,  3 }, {  0,  1,  4 } },
    { "Bembé",       { 12, 12, 12 }, {  7,  5,  3 }, {  0,  2,  0 } },
    { "Flamenco",    { 12, 12, 12 }, {  5,  3,  7 }, {  0,  2,  4 } },
    // --- Jazz / Swing ---
    { "Jazz 12",     { 12, 12, 12 }, {  5,  4,  3 }, {  0,  0,  0 } },
    { "Shuffle",     { 12, 12, 12 }, {  4,  2,  8 }, {  0,  0,  0 } },
    { "Waltz",       { 12, 12, 12 }, {  3,  5,  9 }, {  0,  0,  0 } },
    // --- Odd meter ---
    { "5/4",         { 10, 10, 10 }, {  3,  5,  7 }, {  0,  0,  0 } },
    { "7/8",         {  7,  7,  7 }, {  2,  3,  4 }, {  0,  0,  0 } },
    { "Aksak 9",     {  9,  9,  9 }, {  4,  5,  3 }, {  0,  0,  0 } },
    { "Polyrhythm",  { 12, 12, 12 }, {  3,  4,  5 }, {  0,  0,  0 } },
    { "World",       { 16, 12,  9 }, {  5,  7,  4 }, {  0,  3,  0 } },
    { "Wonky",       { 11, 13,  7 }, {  4,  5,  3 }, {  0,  0,  0 } },
    // --- Electronic ---
    { "Techno",      { 16, 16, 16 }, {  4,  5,  9 }, {  0,  3,  7 } },
    { "Straight",    { 16, 16, 16 }, {  4,  3,  7 }, {  0,  2,  0 } },
    { "Minimal",     { 16, 16,  8 }, {  3,  5,  4 }, {  0,  0,  0 } },
    { "Funk",        { 16, 16, 16 }, {  5,  3, 11 }, {  0,  2,  0 } },
    { "Hip-Hop",     { 16, 16, 16 }, {  3,  2,  5 }, {  0,  4,  2 } },
    { "Reggae",      { 16,  8, 16 }, {  3,  4,  5 }, {  0,  1,  0 } },
    { "Breakbeat",   { 16, 16, 16 }, {  5,  4,  9 }, {  0,  3,  1 } },
    { "DnB",         { 16, 16, 16 }, {  3,  5, 13 }, {  0,  2,  0 } },
    { "Trap",        { 16, 16, 16 }, {  4,  2, 12 }, {  0,  4,  0 } },
    { "IDM",         { 16, 12, 16 }, {  5,  7,  3 }, {  0,  0,  3 } },
    { "Dense",       { 16, 16, 16 }, {  9, 11, 13 }, {  0,  0,  0 } },
};
static const int RHYTHM_PRESET_COUNT = (int)(sizeof(RHYTHM_PRESETS) / sizeof(RHYTHM_PRESETS[0]));

static int  rhythmBrowseIdx    = 0;
static bool rhythmBrowseActive = false;

static const int RHY_X = 170;
static const int RHY_Y = 18;
static const int RHY_W = 140;
static const int RHY_H = 26;

static const int EXTCLK_X   = 175;
static const int EXTCLK_Y   = 100;
static const int EXTCLK_BOX = 14;

static void drawRhythmPresetWindow() {
    uint16_t border = rhythmBrowseActive ? ILI9341_CYAN : ILI9341_DARKGREY;
    tft.fillRect(RHY_X + 1, RHY_Y + 1, RHY_W - 2, RHY_H - 2, ILI9341_BLACK);
    tft.drawRect(RHY_X, RHY_Y, RHY_W, RHY_H, border);

    const RhythmPreset &p = RHYTHM_PRESETS[rhythmBrowseIdx];

    tft.setFont(Arial_12);
    tft.setTextColor(rhythmBrowseActive ? ILI9341_CYAN : ILI9341_LIGHTGREY);
    int nameW = (int)strlen(p.name) * 7;
    tft.setCursor(RHY_X + (RHY_W - nameW) / 2, RHY_Y + 6);
    tft.print(p.name);
}

int  getRhythmPresetCount()  { return RHYTHM_PRESET_COUNT; }
int  getRhythmBrowseIdx()    { return rhythmBrowseIdx; }
bool getRhythmBrowseActive() { return rhythmBrowseActive; }

void setRhythmBrowseIdx(int idx) {
    rhythmBrowseIdx = ((idx % RHYTHM_PRESET_COUNT) + RHYTHM_PRESET_COUNT) % RHYTHM_PRESET_COUNT;
    drawRhythmPresetWindow();
}

void setRhythmBrowseActive(bool active) {
    rhythmBrowseActive = active;
    drawRhythmPresetWindow();
}

void resetRhythmBrowseState() {
    rhythmBrowseActive = false;
}

void loadRhythmPreset(int idx) {
    if (idx < 0 || idx >= RHYTHM_PRESET_COUNT) return;
    const RhythmPreset &p = RHYTHM_PRESETS[idx];
    for (int i = 0; i < 3; i++) {
        PatLen[i] = clampVal((int)p.len[i], 1, 32);
        PatNum[i] = clampVal((int)p.hits[i], 1, PatLen[i]);
        PatRot[i] = clampVal((int)p.rot[i], -(PatLen[i] - 1), PatLen[i] - 1);
        pendingCircleRedraw[i] = true;
    }
    scheduleSaveParams();
}

// ---------------------------------------------------------------------------
// Pitch-Preset Browse (PITCH1-Screen, Enc2)
// ---------------------------------------------------------------------------
// Preset-Box links unterhalb des Rücksprungpfeils, auf gleicher Höhe wie Drw-Box.
// Nur Encoder-gesteuert (kein Touch), daher UL-Zone-Überlapp tolerierbar.
static const int PITCH_PRESET_BX = 10;
static const int PITCH_PRESET_BY = 42;   // gleiche Höhe wie Drw (PITCH_DP_BY)
static const int PITCH_PRESET_BW = 120;
static const int PITCH_PRESET_BH = 24;   // gleiche Höhe wie Drw-Checkbox (s=24)

static int  pitchPresetBrowseIdx    = 0;
static bool pitchPresetBrowseActive = false;

static void drawPitchPresetBox() {
    uint16_t border = pitchPresetBrowseActive ? ILI9341_CYAN : ILI9341_DARKGREY;
    tft.fillRect(PITCH_PRESET_BX + 1, PITCH_PRESET_BY + 1,
                 PITCH_PRESET_BW - 2, PITCH_PRESET_BH - 2, ILI9341_BLACK);
    tft.drawRect(PITCH_PRESET_BX, PITCH_PRESET_BY, PITCH_PRESET_BW, PITCH_PRESET_BH, border);
    tft.setFont(Arial_12);
    tft.setTextColor(pitchPresetBrowseActive ? ILI9341_CYAN : ILI9341_LIGHTGREY);
    const char *name = getPitchPresetName(pitchPresetBrowseIdx);
    int nameW = (int)strlen(name) * 7;
    tft.setCursor(PITCH_PRESET_BX + (PITCH_PRESET_BW - nameW) / 2, PITCH_PRESET_BY + 6);
    tft.print(name);
}

int  getPitchPresetBrowseIdx()    { return pitchPresetBrowseIdx; }
bool getPitchPresetBrowseActive() { return pitchPresetBrowseActive; }

void setPitchPresetBrowseIdx(int idx) {
    pitchPresetBrowseIdx = ((idx % PITCH_PRESET_COUNT) + PITCH_PRESET_COUNT) % PITCH_PRESET_COUNT;
    if (GUIState == PITCH1) drawPitchPresetBox();
}

void setPitchPresetBrowseActive(bool active) {
    pitchPresetBrowseActive = active;
    if (GUIState == PITCH1) drawPitchPresetBox();
}

void resetPitchPresetBrowseState() {
    pitchPresetBrowseActive = false;
}

void loadPitchPreset(int idx) {
    getPitchPresetNotes(idx, PitchNote1);
    pitchPresetBrowseIdx = ((idx % PITCH_PRESET_COUNT) + PITCH_PRESET_COUNT) % PITCH_PRESET_COUNT;
    scheduleSaveParams();
    if (GUIState == PITCH1) {
        drawPitchPresetBox();
        drawPitchBars();
    }
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
  // Rücksprungpfeil — nach oben verschoben damit Preset-Box bei y=42 Platz hat
  tft.setTextColor(color);
  tft.setFont(Arial_12);
  tft.setFont(AwesomeF100_24);
  tft.setCursor(20, 4);
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
  // Temp-Buffer verhindert src==dst-Aliasing in buildProbPattern.
  bool tmp[32];
  buildProbPattern(EPatArr[idx], tmp, len, PatNum[idx], PatProb[idx], ProbEuclidRebuild[idx]);
  for(int i = 0;     i < len; i++) EPatArr[idx][i] = tmp[i];
  for(int i = len;   i < 32;  i++) EPatArr[idx][i] = false;
  syncEPatBFromEPat(idx);
}

// Zweck: Loest die Prob-Aktion per Encoder-Button aus (identisch mit Touch-Prob-Button).
//   Generiert ein neues Zufallspattern basierend auf den aktuellen PatProb-Einstellungen.
//   Kein Effekt wenn PatProbAuto aktiv. Screen-State wird beruecksichtigt.
void triggerProbAction(int ch) {
    if (ch < 0 || ch > 2) return;
    if (PatProbAuto[ch]) return;
    applyProbToPattern(ch);
    scheduleSaveParams();
    // Flash und Redraw nur auf dem EUCLPARAM-Screen — nicht auf EUCLCIRCS
    if (GUIState == (uint16_t)(EUCLPARAM1 + ch)) {
        startProbButtonFlash(ch);
        redrawParamFromPattern(ch);
    }
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

// Zeichnet die Auto-Rotate-Auswahlbox (0=aus, 1-4=Schritte pro Zyklus).
void drawAutoRotateBox(int setIdx) {
    uint8_t step = autoRotateStep[setIdx];
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(PARAM_ARBOX_X - 1, PARAM_ARBOX_Y - 14);
    tft.print("AR");
    if (step > 0) {
        tft.drawRect(PARAM_ARBOX_X, PARAM_ARBOX_Y, PARAM_ARBOX_S, PARAM_ARBOX_S, ILI9341_GREEN);
        tft.fillRect(PARAM_ARBOX_X + 1, PARAM_ARBOX_Y + 1, PARAM_ARBOX_S - 2, PARAM_ARBOX_S - 2, ILI9341_DARKGREEN);
        tft.setTextColor(ILI9341_GREEN);
    } else {
        tft.drawRect(PARAM_ARBOX_X, PARAM_ARBOX_Y, PARAM_ARBOX_S, PARAM_ARBOX_S, ILI9341_DARKGREY);
        tft.fillRect(PARAM_ARBOX_X + 1, PARAM_ARBOX_Y + 1, PARAM_ARBOX_S - 2, PARAM_ARBOX_S - 2, ILI9341_BLACK);
        tft.setTextColor(ILI9341_DARKGREY);
    }
    tft.setCursor(PARAM_ARBOX_X + 8, PARAM_ARBOX_Y + 6);
    tft.printf("%d", step);
}

// Zweck: Zeichnet ein Patternnummer-Kaestchen (Status/Selection).
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..6.
static void drawPerfSlotBox(int idx){
  int x = PERF_BOX_XS[idx];
  bool selected   = (idx == perfSelected);
  bool encBrowse  = (idx == perfEncSlot);
  uint16_t border = ILI9341_DARKGREY;
  uint16_t fill;
  if      (selected)  fill = ILI9341_GREEN;
  else if (encBrowse) fill = ILI9341_CYAN;
  else                fill = (perfUsedMask & (1u << idx)) ? ILI9341_WHITE : ILI9341_BLACK;
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

void drawExtClockCheckbox() {
    bool checked = extClockMode;
    uint16_t boxColor = checked ? ILI9341_CYAN : ILI9341_DARKGREY;
    uint16_t fillColor = checked ? ILI9341_CYAN : ILI9341_BLACK;
    tft.drawRect(EXTCLK_X, EXTCLK_Y, EXTCLK_BOX, EXTCLK_BOX, boxColor);
    tft.fillRect(EXTCLK_X + 2, EXTCLK_Y + 2, EXTCLK_BOX - 4, EXTCLK_BOX - 4, fillColor);
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(EXTCLK_X + EXTCLK_BOX + 5, EXTCLK_Y + 1);
    tft.print("Ext Clock");
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

// Zweck: Berechnet die Y-Positionen der Mute/Solo-Boxen (single source of truth).
// Wird von drawPerformanceScreen und handlePerformance verwendet.
static void calcPerfMsY(int msY[3]) {
  int sb = PERF_BTN_Y - 15;        // seqBottom3
  msY[2] = sb - PERF_MS_H + 5;
  sb -= 30;                          // seqBottom2
  msY[1] = sb - PERF_MS_H + 5;
  sb -= 30;                          // seqBottom1
  msY[0] = sb - PERF_MS_H + 5;
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

  // Rhythmus-Preset-Fenster (oben rechts)
  rhythmBrowseActive = false;
  drawRhythmPresetWindow();

  // CV-Config-Button (rechts, unter Rhythm-Preset)
  tft.setFont(Arial_12);
  tft.drawRect(175, 50, 55, 24, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(181, 58);
  tft.print("CV Cfg");

  // Ext-Clock-Checkbox (rechte Seite, ueber Save/Del)
  drawExtClockCheckbox();

  // Sequencer-Labels und Mute/Solo-Boxen
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_LIGHTGREY);
  static const int seqCharH = 16;
  int msY[3];
  calcPerfMsY(msY);
  // seqBottom[i] = msY[i] + PERF_MS_H - 5
  drawCenteredLabel(PERF_BTN_XS[0], msY[0] + PERF_MS_H - 5 - seqCharH, PERF_BTN_W, seqCharH, "Seq1", 8, seqCharH, -5, 0);
  drawCenteredLabel(PERF_BTN_XS[0], msY[1] + PERF_MS_H - 5 - seqCharH, PERF_BTN_W, seqCharH, "Seq2", 8, seqCharH, -5, 0);
  drawCenteredLabel(PERF_BTN_XS[0], msY[2] + PERF_MS_H - 5 - seqCharH, PERF_BTN_W, seqCharH, "Seq3", 8, seqCharH, -5, 0);
  for(int i = 0; i < 3; i++){
    drawPerfMsBox(i, false, msY[i]);
    drawPerfMsBox(i, true,  msY[i]);
  }
  drawCenteredLabel(PERF_MS_X1, msY[0] - seqCharH, PERF_MS_W, seqCharH, "M", 8, seqCharH, -5, -5);
  drawCenteredLabel(PERF_MS_X2, msY[0] - seqCharH, PERF_MS_W, seqCharH, "S", 8, seqCharH, -5, -5);
}

// Zweck: Behandelt Touch-Events im Performance-Menue.
// Side Effects: wechselt GUIState und schreibt auf das TFT.
// Assumptions: mapX/mapY sind gemappt; tipPos ist gueltig.
// Return: true, wenn eine Aktion ausgefuehrt wurde.
bool handlePerformance(int mapX, int mapY, uint16_t tipPos){
  updatePerfButtonFlash();
  // CV-Config-Button (x=175, y=50, w=55, h=24) — vor UL-Prüfung
  if (hitBox(mapX, mapY, 175, 50, 55, 24, 5)) {
      GUIState = CV_CONFIG;
      drawCvConfigScreen();
      return true;
  }
  if(tipPos == UL){
    GUIState = EUCLCIRCS;
    PendingCircsRedraw = false;  // Wir zeichnen sofort selbst
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
    drawEncParamIndicators();
    drawBpmControls();
    drawBpmValue();
    drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
    drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
    drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
    resetRhythmBrowseState();
    for(int i = 0; i < 3; i++){
        displayedPatLen[i] = PatLen[i];
        pendingCircleRedraw[i] = false;
        pendingProbRegen[i] = false;
    }
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
  int msY[3];
  calcPerfMsY(msY);

  for(int row = 0; row < 3; row++){
    if(hitBox(mapX, mapY, PERF_MS_X1, msY[row], PERF_MS_W, PERF_MS_H, PERF_MS_PAD)){
      MuteSeq[row] = !MuteSeq[row];
      if(MuteSeq[row]){
        // Mute aktiviert -> alle Solos aus (gegenseitiger Ausschluss)
        SoloSeq[0] = false;
        SoloSeq[1] = false;
        SoloSeq[2] = false;
      }
      drawPerfMsBox(row, false, msY[row]);
      drawPerfMsBox(0, true, msY[0]);
      drawPerfMsBox(1, true, msY[1]);
      drawPerfMsBox(2, true, msY[2]);
      return true;
    }
    if(hitBox(mapX, mapY, PERF_MS_X2, msY[row], PERF_MS_W, PERF_MS_H, PERF_MS_PAD)){
      if(SoloSeq[row]){
        SoloSeq[row] = false;
      }else{
        // Solo aktiviert -> alle Mutes aus (gegenseitiger Ausschluss)
        MuteSeq[0] = false;
        MuteSeq[1] = false;
        MuteSeq[2] = false;
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

  // Ext-Clock-Checkbox
  if(hitBox(mapX, mapY, EXTCLK_X, EXTCLK_Y, EXTCLK_BOX + 70, EXTCLK_BOX, 6)){
    setExtClockMode(!extClockMode);
    drawExtClockCheckbox();
    return true;
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

static void drawPitchButton();   // forward decl (defined in pitch section below)

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
      stageProbPatternFromCurrent(idx);
    }else{
      syncEPatBFromEPat(idx);
    }
    drawParamButtons(PatLen[idx], PatNum[idx], PatRot[idx], PatProb[idx]);
    drawParamButtonHighlight(idx);
    drawSpeedIndicator(idx);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    if(idx >= 0 && idx <= 2){
      if(idx != 0) drawValuesButton(idx);
      drawXYButton(idx);
      drawProbAutoCheckbox(idx);
      drawProbEuclidRebuildCheckbox(idx);
      drawAutoRotateBox(idx);
      if(idx == 0) drawPitchButton();
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
    drawParamButtonHighlight(idx);
    drawSpeedIndicator(idx);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    if(idx != 0) drawValuesButton(idx);
    drawXYButton(idx);
    drawProbAutoCheckbox(idx);
    drawProbEuclidRebuildCheckbox(idx);
    drawAutoRotateBox(idx);
    if(idx == 0) drawPitchButton();
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
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(287, 10);
    tft.print(setIdx + 1);
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
    int totalW = 320 - 2 * x0;

    int idx = step % len;
    int last = lastValuesPlayIdx[setIdx];

    int y = y0 - 6;
    int r = 3;

    if(last >= 0 && last < len){
        int lastX = (x0 + (last * totalW) / len + x0 + ((last + 1) * totalW) / len) / 2;
        tft.fillCircle(lastX, y, r, ILI9341_BLACK);
    }

    int x = (x0 + (idx * totalW) / len + x0 + ((idx + 1) * totalW) / len) / 2;
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
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;

    int src = RotateValues[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
    uint8_t val = ValuesArr[setIdx][src];
    int fillH = (int)((val * (long)h) / 255L);
    bool active = RotateValues[setIdx] ? EPatArr[setIdx][src] : patternIsHit(setIdx, idx);

    // Clear full step area
    tft.fillRect(x, y0, xNext - x, h, ILI9341_BLACK);
    if(fillH > 0){
        int y = y0 + h - fillH;
        if(active){
            tft.fillRect(x, y, w, fillH, ILI9341_WHITE);
        }else{
            tft.fillRect(x, y, w, fillH, ILI9341_DARKGREY);
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
            stageProbPatternFromCurrent(idx);
        }else{
            syncEPatBFromEPat(idx);
        }
        scheduleSaveParams();
        drawProbAutoCheckbox(idx);
        return;
    }
    if(hitBox(mapX, mapY, PARAM_ERBOX_X, PARAM_ERBOX_Y, PARAM_ERBOX_S, PARAM_ERBOX_S, 8)){
        ProbEuclidRebuild[idx] = !ProbEuclidRebuild[idx];
        if(PatProbAuto[idx]){
            stageProbPatternFromCurrent(idx);
        }else{
            syncEPatBFromEPat(idx);
        }
        scheduleSaveParams();
        drawProbEuclidRebuildCheckbox(idx);
        return;
    }
    if(hitBox(mapX, mapY, PARAM_ARBOX_X, PARAM_ARBOX_Y, PARAM_ARBOX_S, PARAM_ARBOX_S, 8)){
        autoRotateStep[idx] = (autoRotateStep[idx] + 1) % 5;  // 0→1→2→3→4→0
        scheduleSaveParams();
        drawAutoRotateBox(idx);
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
        scheduleSaveParams();
        GUIState = EUCLCIRCS;
        PendingCircsRedraw = false;  // Wir zeichnen sofort selbst
        tft.fillScreen(ILI9341_BLACK);
        setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
        drawEncParamIndicators();
        drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
        drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
        drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
        drawBpmControls();
        for(int i = 0; i < 3; i++){
            displayedPatLen[i] = PatLen[i];
            pendingCircleRedraw[i] = false;
            pendingProbRegen[i] = false;
        }
        break;
      case UR:
        if(idx == 0){
          GUIState = PITCH1;
          drawPitchScreen();
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
        if(setIdx == 0){
            GUIState = PITCH1;
            drawPitchScreen();
        }else{
            GUIState = (setIdx == 1) ? EUCLPARAM2 : EUCLPARAM3;
            redrawParamFromPattern(setIdx);
        }
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
    int writeIdx = RotateValues[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
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
    int writeIdx = RotateValues[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
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
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(287, 10);
    tft.print(setIdx + 1);
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
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;

    int src = RotateGateLen[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
    uint8_t val = GateLenArr[setIdx][src];
    int fillH = (int)((val * (long)h) / 255L);
    bool active = RotateGateLen[setIdx] ? EPatArr[setIdx][src] : patternIsHit(setIdx, idx);

    // Clear full step area
    tft.fillRect(x, y0, xNext - x, h, ILI9341_BLACK);
    if(fillH > 0){
        int y = y0 + h - fillH;
        if(active){
            tft.fillRect(x, y, w, fillH, ILI9341_WHITE);
        }else{
            tft.fillRect(x, y, w, fillH, ILI9341_DARKGREY);
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
        scheduleSaveParams();
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
    int writeIdx = RotateGateLen[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
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
    int writeIdx = RotateGateLen[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    GateLenArr[setIdx][writeIdx] = (uint8_t)v;
    drawGateLenBar(setIdx, idx);
}

// Zweck: Zeichnet den XY-Pad-Screen fuer ein Pattern.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
// Berechnet die Pad-Pixelposition eines Steps (Value=X, GateLen=Y invertiert).
static void getXYDotXY(int setIdx, int stepIdx, int &dotX, int &dotY) {
    int vi = RotateValues[setIdx]  ? patternRotatedSrc(setIdx, stepIdx) : stepIdx;
    dotX = 90 + ((int)ValuesArr[setIdx][vi] * 179) / 255;
    if (setIdx == 0 && xyPadPitchMode) {
        int effIdx = foldPitchIdx(stepIdx, clampVal(PatLen[0], 1, 32), pitchFoldMode);
        int pi = pitchRotate ? patternRotatedSrc(0, effIdx) : effIdx;
        dotY = 40 + 179 - ((int)PitchNote1[pi] * 179) / 255;
    } else {
        int gi = RotateGateLen[setIdx] ? patternRotatedSrc(setIdx, stepIdx) : stepIdx;
        dotY = 40 + 179 - ((int)GateLenArr[setIdx][gi] * 179) / 255;
    }
}

static bool getXYDotIsHit(int setIdx, int stepIdx) {
    return RotateValues[setIdx]
        ? EPatArr[setIdx][patternRotatedSrc(setIdx, stepIdx)]
        : patternIsHit(setIdx, stepIdx);
}

// Loescht einen Dot (r=3) und stellt die Gitterlinien im betroffenen Bereich wieder her.
static void eraseAndRestoreXYDot(int dotX, int dotY) {
    tft.fillCircle(dotX, dotY, 3, ILI9341_BLACK);
    for (int i = 1; i < 10; i++) {
        int gx = 90 + (i * 18);
        int gy = 40 + (i * 18);
        int dx = gx - dotX; if (dx < 0) dx = -dx;
        int dy = gy - dotY; if (dy < 0) dy = -dy;
        if (dx <= 3) {
            int y0 = dotY - 3; if (y0 < 41)  y0 = 41;
            int y1 = dotY + 3; if (y1 > 218) y1 = 218;
            if (y1 >= y0) tft.drawFastVLine(gx, y0, y1 - y0 + 1, XY_GRID_COLOR);
        }
        if (dy <= 3) {
            int x0 = dotX - 3; if (x0 < 91)  x0 = 91;
            int x1 = dotX + 3; if (x1 > 268) x1 = 268;
            if (x1 >= x0) tft.drawFastHLine(x0, gy, x1 - x0 + 1, XY_GRID_COLOR);
        }
    }
}

// Loescht das Pad-Innere und zeichnet das Gitter neu (fuer vollstaendigen Dot-Refresh).
static void clearXYPadContent() {
    tft.fillRect(91, 41, 178, 178, ILI9341_BLACK);
    for (int i = 1; i < 10; i++) {
        tft.drawFastVLine(90 + i * 18, 41, 178, XY_GRID_COLOR);
        tft.drawFastHLine(91, 40 + i * 18, 178, XY_GRID_COLOR);
    }
}

// Zeichnet alle Steps als kleine Punkte (Hits=weiss, Non-Hits=dunkelgrau).
static void drawXYDots(int setIdx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    for (int i = 0; i < len; i++) {
        int dotX, dotY;
        getXYDotXY(setIdx, i, dotX, dotY);
        uint16_t col = getXYDotIsHit(setIdx, i) ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillCircle(dotX, dotY, 2, col);
    }
}

// Bewegt den gelben Playhead-Dot zum aktuellen Step (loescht alten, zeichnet neuen).
void drawXYDotPlayhead(int setIdx, unsigned int step) {
    if (setIdx < 0 || setIdx > 2) return;
    int len = clampVal(PatLen[setIdx], 1, 32);
    int idx  = (int)(step % (unsigned int)len);
    int last = lastXYDotIdx[setIdx];
    if (last >= 0 && last < len) {
        int dotX, dotY;
        getXYDotXY(setIdx, last, dotX, dotY);
        eraseAndRestoreXYDot(dotX, dotY);
        uint16_t col = getXYDotIsHit(setIdx, last) ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillCircle(dotX, dotY, 2, col);
    }
    int dotX, dotY;
    getXYDotXY(setIdx, idx, dotX, dotY);
    tft.fillCircle(dotX, dotY, 3, ILI9341_YELLOW);
    lastXYDotIdx[setIdx] = idx;
}

static void drawXYModeToggle(int setIdx) {
    if (setIdx != 0) return;
    int bx = 270, by = 5, bw = 44, bh = 24;
    if (xyPadPitchMode) {
        tft.fillRect(bx, by, bw, bh, ILI9341_DARKGREEN);
        tft.drawRect(bx, by, bw, bh, ILI9341_GREEN);
    } else {
        tft.fillRect(bx, by, bw, bh, ILI9341_BLACK);
        tft.drawRect(bx, by, bw, bh, ILI9341_DARKGREY);
    }
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(bx + 8, by + 6);
    tft.print("PV");
}

void drawXYPadScreen(int setIdx){
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);

    int x = 90;
    int y = 40;
    int w = 180;
    int h = 180;
    tft.drawRect(x, y, w, h, ILI9341_DARKGREY);

    // Raster: 10 Unterteilungen in x und y
    for (int i = 1; i < 10; i++) {
        tft.drawFastVLine(x + (i * w) / 10, y + 1, h - 2, XY_GRID_COLOR);
        tft.drawFastHLine(x + 1, y + (i * h) / 10, w - 2, XY_GRID_COLOR);
    }

    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(x + 60, y + h + 8);
    tft.print("Value");

    bool pitchMode = (setIdx == 0 && xyPadPitchMode);
    drawVerticalLabel(x - 20, y + 34, pitchMode ? "Pitch    " : "Gatelength");

    drawXYModeToggle(setIdx);

    resetXYPlayhead(setIdx);
    drawXYPlayhead(setIdx, cntCh[setIdx]);
    lastXYDotIdx[setIdx] = -1;
    drawXYDots(setIdx);
    drawXYDotPlayhead(setIdx, cntCh[setIdx]);
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

    // PV-Mode-Toggle (nur Kanal 1, oben rechts)
    if (setIdx == 0 && hitBox(mapX, mapY, 270, 5, 44, 24, 4)) {
        xyPadPitchMode = !xyPadPitchMode;
        drawXYPadScreen(setIdx);
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
    int idx = cntCh[setIdx] % len;
    int writeValIdx = RotateValues[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;

    int v = map(mapX, x, x + w - 1, 0, 255);
    int g = map(mapY, y + h - 1, y, 0, 255);
    v = clampVal(v, 0, 255);
    g = clampVal(g, 0, 255);

    ValuesArr[setIdx][writeValIdx] = (uint8_t)v;
    if (setIdx == 0 && xyPadPitchMode) {
        int writePitchIdx = pitchRotate ? patternRotatedSrc(0, idx) : idx;
        PitchNote1[writePitchIdx] = (uint8_t)g;
        scheduleSaveParams();
    } else {
        int writeGateIdx = RotateGateLen[setIdx] ? patternRotatedSrc(setIdx, idx) : idx;
        GateLenArr[setIdx][writeGateIdx] = (uint8_t)g;
    }

    lastXYDotIdx[setIdx] = -1;
    clearXYPadContent();
    drawXYDots(setIdx);
    drawXYDotPlayhead(setIdx, cntCh[setIdx]);

    outputValuesForStep(0);
}

// Basisposition der Kanalzahl auf dem EUCLCIRCS-Screen (identisch mit setMenuItems4EUCLCIRCS)
static const int CH_BX[3]       = { 20, 280, 280 };
static const int CH_BY[3]       = { 20,  20, 200 };
// Geschaetzte Breite der Kanalziffer in Arial_24 ("1" schmaler als "2"/"3")
static const int CH_DIGIT_W[3]  = { 14,  16,  16 };
// Verfuegbare Box-Breite fuer Buchstabe+Wert (ch=1,2 am rechten Rand begrenzt)
static const int CH_BOX_W[3]    = { 28,  24,  24 };

// Zweck: Zeichnet Encoder-Buchstabe rechts neben der Kanalzahl, Wert zentriert darunter.
// Layout: "1 L"  <- Kanalzahl (unveraendert) + Buchstabe (Arial_16, YELLOW)
//           "16" <- Wert (Arial_12, YELLOW, zentriert unter Buchstabe)
void drawEncParamIndicator(int ch) {
    if (ch < 0 || ch > 2) return;
    const int bx   = CH_BX[ch];
    const int by   = CH_BY[ch];
    const int lx   = bx + CH_DIGIT_W[ch];
    const int boxW = CH_BOX_W[ch];

    tft.fillRect(lx, by, boxW, 22, ILI9341_BLACK);
    tft.fillRect(bx - 2, by + 26, CH_DIGIT_W[ch] + 8, 14, ILI9341_BLACK);

    char buf[4];
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(lx, by);

    if (encParamSel[ch] == 4) {
        static const char* speedLabels[7] = { "/4", "/3", "/2", "x1", "x2", "x3", "x4" };
        int si = clampVal(chSpeedIdx[ch] + 3, 0, 6);
        tft.print("S");
        snprintf(buf, sizeof(buf), "%s", speedLabels[si]);
    } else {
        static const char *letters[4] = { "L", "H", "R", "P" };
        int val;
        switch (encParamSel[ch]) {
            case 0: val = PatLen[ch];         break;
            case 1: val = PatNum[ch];         break;
            case 2: val = PatRot[ch];         break;
            default: val = (int)PatProb[ch];  break;
        }
        tft.print(letters[encParamSel[ch]]);
        snprintf(buf, sizeof(buf), "%d", val);
    }

    int midX = bx + CH_DIGIT_W[ch] / 2;
    int estW = (int)strlen(buf) * 7;
    tft.setFont(Arial_12);
    tft.setCursor(midX - estW / 2, by + 26);
    tft.print(buf);
}

// Zweck: Zeichnet die Geschwindigkeits-Anzeige auf dem Parameter-Screen (unten links).
// Side Effects: schreibt auf das TFT.
// Assumptions: Wird nur auf einem EUCLPARAM-Screen aufgerufen; ch in 0..2.
void drawSpeedIndicator(int ch) {
    if (ch < 0 || ch > 2) return;
    static const char* speedLabels[7] = { "/4", "/3", "/2", "x1", "x2", "x3", "x4" };
    int si = clampVal(chSpeedIdx[ch] + 3, 0, 6);
    bool selected = (encParamSel[ch] == 4);
    uint16_t col  = selected ? ILI9341_RED : ILI9341_LIGHTGREY;

    int x = 10, y = 200, w = 58, h = 22;
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, ILI9341_BLACK);
    tft.drawRect(x, y, w, h, col);
    tft.setFont(Arial_12);
    tft.setTextColor(col);
    tft.setCursor(x + 4, y + 5);
    tft.printf("S: %s", speedLabels[si]);
}

void drawEncParamIndicators() {
    for (int i = 0; i < 3; i++) drawEncParamIndicator(i);
}

// Zweck: Hebt den aktuell per Encoder editierten Parameter-Button rot hervor.
// Alle anderen Buttons werden auf DARKGREY zurueckgesetzt.
// Side Effects: schreibt auf das TFT.
// Assumptions: Wird auf dem EUCLPARAM-Screen aufgerufen; ch in 0..2.
void drawParamButtonHighlight(int ch) {
    if (ch < 0 || ch > 2) return;
    for (int col = 0; col < 4; col++) {
        uint16_t c = (col == encParamSel[ch]) ? ILI9341_RED : ILI9341_DARKGREY;
        tft.drawRect(PARAM_COLX[col], PARAM_BTN_TOPY, PARAM_BTN_W, PARAM_BTN_H, c);
        tft.drawRect(PARAM_COLX[col], PARAM_BTN_BOTY, PARAM_BTN_W, PARAM_BTN_H, c);
    }
}

// Zweck: Setzt die Encoder-Browse-Hervorhebung auf dem PERFORMANCE-Screen.
// slot=-1 loescht die Hervorhebung; slot 0..6 hebt den Slot CYAN hervor.
// Side Effects: schreibt auf das TFT, aktualisiert perfEncSlot.
// Assumptions: Wird nur aufgerufen wenn GUIState == PERFORMANCE.
void setPerfEncBrowseSlot(int slot) {
    int prev = perfEncSlot;
    perfEncSlot = slot;
    if (prev >= 0 && prev < 7) drawPerfSlotBox(prev);
    if (slot >= 0 && slot < 7) drawPerfSlotBox(slot);
}

// ============================================================================
// PITCH SCREEN (Channel 1 only)
// ============================================================================

static const int PITCH_BAR_X    = 10;
static const int PITCH_BAR_Y    = 80;    // starts below UL zone (y<80) — no collision
static const int PITCH_BAR_W    = 300;
static const int PITCH_BAR_H    = 113;   // ends at y=192 (war 90/y=169)
static const int PITCH_CTRL_Y   = 196;   // 3px Abstand nach Bars (war 173)
static const int PITCH_CTRL_H   = 24;    // ends at y=219
static const int PITCH_ITVL_Y   = 220;   // halbe Hoehe am Bildschirmrand (war 200)
static const int PITCH_ITVL_H   = 20;    // halbiert (war 40), ends at y=239
static const int PITCH_ITVL_X0  = 4;
static const int PITCH_ITVL_W   = 44;
static const int PITCH_ITVL_GAP = 1;
static const uint16_t PITCH_GRID_COL = 0x2945;  // dim blue-grey for octave lines

// Pitch Hold / Rotate / Display-Mode Checkboxen
static const int PITCH_HOLD_BX = 260;
static const int PITCH_HOLD_BY = 10;
static const int PITCH_HOLD_BS = 24;
static const int PITCH_ROT_BX  = 260;
static const int PITCH_ROT_BY  = 42;
static const int PITCH_ROT_BS  = 24;
static const int PITCH_DP_BX   = 185;  // rechte Kante bündig mit V1 (x=209)
static const int PITCH_DP_BY   = 42;
static const int PITCH_DP_BS   = 24;
// Faltungs-Box (rechtsbündig mit Preset-Box, darüber)
static const int PITCH_FOLD_BX = 70;
static const int PITCH_FOLD_BY = 14;
static const int PITCH_FOLD_BW = 60;
static const int PITCH_FOLD_BH = 22;

static bool pitchDisplayMode = false;
static int lastPitchPlayIdx = -1;

// Maps a MIDI note to a Y pixel in the bar area (C2=36 at bottom, C7=96 at top).
static int pitchNoteToBarY(int midiNote) {
    int n = clampVal(midiNote, 36, 96);
    return PITCH_BAR_Y + PITCH_BAR_H - 1
           - ((n - 36) * (PITCH_BAR_H - 1)) / 60;
}

// Draws one bar (quantized height) for step idx of channel 0.
static void drawPitchBar(int idx) {
    int len = clampVal(PatLen[0], 1, 32);
    if (idx < 0 || idx >= len) return;

    int x  = PITCH_BAR_X + (idx       * PITCH_BAR_W) / len;
    int xN = PITCH_BAR_X + ((idx + 1) * PITCH_BAR_W) / len;
    int w  = xN - x - 1;
    if (w < 1) w = 1;

    int effIdx = foldPitchIdx(idx, len, pitchFoldMode);
    int src    = pitchRotate ? patternRotatedSrc(0, effIdx) : effIdx;
    bool hit   = patternIsHit(0, idx);

    tft.fillRect(x, PITCH_BAR_Y, xN - x, PITCH_BAR_H, ILI9341_BLACK);

    int bottom = PITCH_BAR_Y + PITCH_BAR_H;

    if (pitchDisplayMode) {
        // Raw PitchNote1[] value as proportional bar height for drawing
        int fillH = (int)((PitchNote1[src] * (long)PITCH_BAR_H) / 255L);
        if (fillH > 0) {
            uint16_t col = hit ? ILI9341_WHITE : 0x4208;
            tft.fillRect(x, bottom - fillH, w, fillH, col);
        }
    } else {
        int midi = quantizeToMidi(PitchNote1[src], pitchSpread, pitchScale,
                                   pitchRoot, pitchIntervalMask);
        midi = clampVal(midi + (int)pitchShift * 12, 36, 96);
        int noteY = pitchNoteToBarY(midi);
        int fillH = bottom - noteY;
        if (fillH > 0) {
            uint16_t col = hit ? ILI9341_WHITE : 0x4208;
            tft.fillRect(x, noteY, w, fillH, col);
        }
        // Restore octave grid lines that pass through this column.
        static const int PITCH_OCT[] = { 48, 60, 72, 84 };
        for (int i = 0; i < 4; i++) {
            int gy = pitchNoteToBarY(PITCH_OCT[i]);
            if (gy >= PITCH_BAR_Y && gy < bottom) {
                tft.drawFastHLine(x, gy, xN - x, PITCH_GRID_COL);
            }
        }
    }
}

void drawPitchBars() {
    int len = clampVal(PatLen[0], 1, 32);
    for (int i = 0; i < len; i++) {
        drawPitchBar(i);
    }
}

void drawPitchPlayhead(unsigned int step) {
    int len  = clampVal(PatLen[0], 1, 32);
    int idx  = (int)(step % (unsigned int)len);
    int last = lastPitchPlayIdx;
    int y    = PITCH_BAR_Y - 5;
    int r    = 3;

    if (last >= 0 && last < len) {
        int lx = PITCH_BAR_X + (last * PITCH_BAR_W) / len + (PITCH_BAR_W / len) / 2;
        tft.fillCircle(lx, y, r, ILI9341_BLACK);
    }
    int x = PITCH_BAR_X + (idx * PITCH_BAR_W) / len + (PITCH_BAR_W / len) / 2;
    tft.fillCircle(x, y, r, ILI9341_WHITE);
    lastPitchPlayIdx = idx;
}

void drawPitchHoldCheckbox() {
    int x = PITCH_HOLD_BX, y = PITCH_HOLD_BY, s = PITCH_HOLD_BS;
    tft.fillRect(x - 30, y, 29, s, ILI9341_BLACK);  // Label-Bereich leeren
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 23, y + 6);  // +3px nach links verschoben
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("HP");
    if (pitchHold) {
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

void drawPitchRotateCheckbox() {
    int x = PITCH_ROT_BX, y = PITCH_ROT_BY, s = PITCH_ROT_BS;
    tft.fillRect(x - 30, y, 29, s, ILI9341_BLACK);  // Label-Bereich leeren
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 23, y + 6);  // +3px nach links verschoben
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("PR");
    if (pitchRotate) {
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

void drawPitchDisplayModeCheckbox() {
    int x = PITCH_DP_BX, y = PITCH_DP_BY, s = PITCH_DP_BS;
    tft.fillRect(x - 28, y, 27, s, ILI9341_BLACK);  // Label-Bereich leeren
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 32, y + 6);  // +8px nach links verschoben
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("DW");
    if (pitchDisplayMode) {
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

void drawPitchControls() {
    static const int SC_X = 0,   SC_W = 124;
    static const int RT_X = 126, RT_W = 62;
    static const int SP_X = 190, SP_W = 60;
    static const int SH_X = 252, SH_W = 68;
    static const char *const ROOT_NAMES[12] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    int  boxCursor = getPitchBoxCursor();
    bool boxEdit   = getPitchBoxEditMode();
    uint16_t scBorder = (boxCursor == 0) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t rtBorder = (boxCursor == 1) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t spBorder = (boxCursor == 2) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;

    tft.drawRect(SC_X, PITCH_CTRL_Y, SC_W, PITCH_CTRL_H, scBorder);
    tft.fillRect(SC_X+1, PITCH_CTRL_Y+1, SC_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(SC_X+3, PITCH_CTRL_Y+5);
    tft.print(getScaleName(pitchScale));

    tft.drawRect(RT_X, PITCH_CTRL_Y, RT_W, PITCH_CTRL_H, rtBorder);
    tft.fillRect(RT_X+1, PITCH_CTRL_Y+1, RT_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(RT_X+3, PITCH_CTRL_Y+5);
    tft.printf("R:%s", ROOT_NAMES[pitchRoot % 12]);

    tft.drawRect(SP_X, PITCH_CTRL_Y, SP_W, PITCH_CTRL_H, spBorder);
    tft.fillRect(SP_X+1, PITCH_CTRL_Y+1, SP_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(SP_X+3, PITCH_CTRL_Y+5);
    tft.printf("Spr:%d", pitchSpread);

    tft.drawRect(SH_X, PITCH_CTRL_Y, SH_W, PITCH_CTRL_H, ILI9341_DARKGREY);
    tft.fillRect(SH_X+1, PITCH_CTRL_Y+1, SH_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(SH_X+3, PITCH_CTRL_Y+5);
    if (pitchShift > 0) tft.printf("Sft:+%d", (int)pitchShift);
    else                tft.printf("Sft:%d",  (int)pitchShift);

    for (int i = 0; i < 7; i++) {
        int bx    = PITCH_ITVL_X0 + i * (PITCH_ITVL_W + PITCH_ITVL_GAP);
        bool avail = intervalExists(pitchScale, i);
        bool act   = avail && ((pitchIntervalMask >> i) & 1);
        bool sel   = (i == getPitchItvlCursor());
        uint16_t fill   = act    ? ILI9341_CYAN   : ILI9341_BLACK;
        uint16_t border = sel    ? ILI9341_YELLOW  :
                          act    ? ILI9341_CYAN     :
                          !avail ? 0x2104           :  // sehr dunkel = nicht verfuegbar
                                   ILI9341_DARKGREY;
        uint16_t textC  = act    ? ILI9341_BLACK   :
                          !avail ? 0x4208           :  // kaum sichtbar = nicht verfuegbar
                                   ILI9341_LIGHTGREY;
        tft.drawRect(bx, PITCH_ITVL_Y, PITCH_ITVL_W, PITCH_ITVL_H, border);
        tft.fillRect(bx+1, PITCH_ITVL_Y+1, PITCH_ITVL_W-2, PITCH_ITVL_H-2, fill);
        tft.setFont(Arial_12);
        tft.setTextColor(textC);
        const char *lbl = getIntervalLabel(i);
        int lw = (int)strlen(lbl) * 7;
        tft.setCursor(bx + (PITCH_ITVL_W - lw) / 2,
                      PITCH_ITVL_Y + (PITCH_ITVL_H - 12) / 2);
        tft.print(lbl);
    }
}

static void drawPitchFoldBox() {
    static const char* foldNames[5] = { "off", "m 1/2", "r 1/2", "m 1/4", "r 1/4" };
    const char *label = foldNames[clampVal((int)pitchFoldMode, 0, 4)];
    bool active = (pitchFoldMode > 0);
    tft.fillRect(PITCH_FOLD_BX + 1, PITCH_FOLD_BY + 1,
                 PITCH_FOLD_BW - 2, PITCH_FOLD_BH - 2, ILI9341_BLACK);
    tft.drawRect(PITCH_FOLD_BX, PITCH_FOLD_BY, PITCH_FOLD_BW, PITCH_FOLD_BH,
                 active ? ILI9341_GREEN : ILI9341_DARKGREY);
    tft.setFont(Arial_12);
    tft.setTextColor(active ? ILI9341_GREEN : ILI9341_LIGHTGREY);
    int labelW = (int)strlen(label) * 7;
    tft.setCursor(PITCH_FOLD_BX + (PITCH_FOLD_BW - labelW) / 2, PITCH_FOLD_BY + 4);
    tft.print(label);
}

void drawPitchScreen() {
    tft.fillScreen(ILI9341_BLACK);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    // V1-Button wie GateLen-Button auf dem Values-Screen
    tft.setFont(Arial_12);
    tft.drawRect(159, 10, 50, 24, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(176, 18);
    tft.print("V1");
    drawPitchFoldBox();
    drawPitchPresetBox();
    drawPitchHoldCheckbox();
    drawPitchRotateCheckbox();
    drawPitchDisplayModeCheckbox();
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(287, 10);
    tft.print(1);

    tft.drawRect(PITCH_BAR_X - 1, PITCH_BAR_Y - 1,
                 PITCH_BAR_W + 2, PITCH_BAR_H + 2, ILI9341_DARKGREY);

    lastPitchPlayIdx = -1;
    drawPitchBars();
    drawPitchPlayhead(cntCh[0]);
    drawPitchControls();
}

void handlePITCH(int mapX, int mapY, uint16_t tipPos) {
    // Fold-Box vor UL-Check prüfen (liegt in UL-Zone)
    if (hitBox(mapX, mapY, PITCH_FOLD_BX, PITCH_FOLD_BY, PITCH_FOLD_BW, PITCH_FOLD_BH, 4)) {
        pitchFoldMode = (pitchFoldMode + 1) % 5;
        scheduleSaveParams();
        drawPitchFoldBox();
        drawPitchBars();
        return;
    }
    if (tipPos == UL) {
        GUIState = EUCLPARAM1;
        redrawParamFromPattern(0);
        return;
    }

    // V1-Button (identische Position wie GateLen auf dem Values-Screen)
    if (hitBox(mapX, mapY, 159, 10, 50, 24, 6)) {
        GUIState = VALUES1;
        drawValuesScreen(0);
        return;
    }

    // Pitch Hold checkbox
    if (hitBox(mapX, mapY, PITCH_HOLD_BX, PITCH_HOLD_BY, PITCH_HOLD_BS, PITCH_HOLD_BS, 8)) {
        pitchHold = !pitchHold;
        scheduleSaveParams();
        drawPitchHoldCheckbox();
        return;
    }

    // Pitch Rotate checkbox
    if (hitBox(mapX, mapY, PITCH_ROT_BX, PITCH_ROT_BY, PITCH_ROT_BS, PITCH_ROT_BS, 8)) {
        pitchRotate = !pitchRotate;
        scheduleSaveParams();
        drawPitchRotateCheckbox();
        drawPitchBars();
        return;
    }

    // Display/Play mode checkbox
    if (hitBox(mapX, mapY, PITCH_DP_BX, PITCH_DP_BY, PITCH_DP_BS, PITCH_DP_BS, 8)) {
        pitchDisplayMode = !pitchDisplayMode;
        drawPitchDisplayModeCheckbox();
        drawPitchBars();
        return;
    }

    // Interval buttons
    for (int i = 0; i < 7; i++) {
        int bx = PITCH_ITVL_X0 + i * (PITCH_ITVL_W + PITCH_ITVL_GAP);
        if (hitBox(mapX, mapY, bx, PITCH_ITVL_Y, PITCH_ITVL_W, PITCH_ITVL_H, 4)) {
            uint8_t toggled = pitchIntervalMask ^ (uint8_t)(1u << i);
            if (toggled != 0) {
                pitchIntervalMask = toggled;
                scheduleSaveParams();
                drawPitchControls();
                drawPitchBars();
            }
            return;
        }
    }

    // Scale — tap cycles forward
    if (hitBox(mapX, mapY, 0, PITCH_CTRL_Y, 124, PITCH_CTRL_H, 3)) {
        pitchScale = (uint8_t)((pitchScale + 1) % (uint8_t)SCALE_COUNT);
        scheduleSaveParams();
        drawPitchControls();
        drawPitchBars();
        return;
    }

    // Root — tap cycles forward
    if (hitBox(mapX, mapY, 126, PITCH_CTRL_Y, 62, PITCH_CTRL_H, 3)) {
        pitchRoot = (uint8_t)((pitchRoot + 1) % 12);
        scheduleSaveParams();
        drawPitchControls();
        drawPitchBars();
        return;
    }

    // Spread — cycles 1..5
    if (hitBox(mapX, mapY, 190, PITCH_CTRL_Y, 60, PITCH_CTRL_H, 3)) {
        pitchSpread = (uint8_t)(pitchSpread >= 5 ? 1 : pitchSpread + 1);
        scheduleSaveParams();
        drawPitchControls();
        drawPitchBars();
        return;
    }

    // Shift — cycles -3..+3
    if (hitBox(mapX, mapY, 252, PITCH_CTRL_Y, 68, PITCH_CTRL_H, 3)) {
        pitchShift = (int8_t)(pitchShift >= 3 ? -3 : pitchShift + 1);
        scheduleSaveParams();
        drawPitchControls();
        drawPitchBars();
        return;
    }

    // Bar area — exclude UL zone (back arrow), set raw pitch value
    if (!(mapX < 80 && mapY < 80) &&
        mapX >= PITCH_BAR_X && mapX < PITCH_BAR_X + PITCH_BAR_W &&
        mapY >= PITCH_BAR_Y && mapY < PITCH_BAR_Y + PITCH_BAR_H) {
        int len = clampVal(PatLen[0], 1, 32);
        int idx = ((mapX - PITCH_BAR_X) * len) / PITCH_BAR_W;
        if (idx >= 0 && idx < len) {
            int v = map(mapY, PITCH_BAR_Y + PITCH_BAR_H - 1, PITCH_BAR_Y, 0, 255);
            v = clampVal(v, 0, 255);
            PitchNote1[idx] = (uint8_t)v;
            scheduleSaveParams();
            drawPitchBar(idx);
        }
    }
}

void handlePITCHDrag(int mapX, int mapY) {
    if (mapX < 80 && mapY < 80) return;  // UL-Zone (Rücksprungpfeil) schützen
    if (mapX >= PITCH_BAR_X && mapX < PITCH_BAR_X + PITCH_BAR_W &&
        mapY >= PITCH_BAR_Y && mapY < PITCH_BAR_Y + PITCH_BAR_H) {
        int len = clampVal(PatLen[0], 1, 32);
        int idx = ((mapX - PITCH_BAR_X) * len) / PITCH_BAR_W;
        if (idx >= 0 && idx < len) {
            int v = map(mapY, PITCH_BAR_Y + PITCH_BAR_H - 1, PITCH_BAR_Y, 0, 255);
            v = clampVal(v, 0, 255);
            PitchNote1[idx] = (uint8_t)v;
            scheduleSaveParams();
            drawPitchBar(idx);
        }
    }
}

static void drawPitchButton() {
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.drawRect(260, 10, 50, 28, ILI9341_DARKGREY);
    tft.setCursor(272, 18);
    tft.print("Pt");
}

// ---------------------------------------------------------------------------
// CV-Config-Screen
// ---------------------------------------------------------------------------
static const int CV_CFG_ROW_Y[3] = {60, 120, 180};
static const int CV_CFG_ROW_H    = 36;
static const int CV_CFG_BTN_X    = 55;
static const int CV_CFG_BTN_W    = 180;
static const int CV_CFG_BAR_X    = 243;
static const int CV_CFG_BAR_W    = 69;
static int lastCvBarFill[3]      = {-1, -1, -1};

static void drawCvRow(int i) {
    int y = CV_CFG_ROW_Y[i];

    // Label "CV1" / "CV2" / "CV3"
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(8, y + 10);
    tft.print("CV");
    tft.print(i + 1);

    // Ziel-Button (Tippen → nächstes Target)
    uint8_t tgt    = cvTargetMap[i];
    uint16_t bcol  = (tgt != CV_TARGET_NONE) ? ILI9341_GREEN : ILI9341_DARKGREY;
    tft.fillRect(CV_CFG_BTN_X + 1, y + 1, CV_CFG_BTN_W - 2, CV_CFG_ROW_H - 2, ILI9341_BLACK);
    tft.drawRect(CV_CFG_BTN_X, y, CV_CFG_BTN_W, CV_CFG_ROW_H, bcol);
    tft.setFont(Arial_16);
    tft.setTextColor(bcol);
    const char *lbl = cvTargetLabel(tgt);
    int lblW = (int)strlen(lbl) * 9;
    tft.setCursor(CV_CFG_BTN_X + (CV_CFG_BTN_W - lblW) / 2, y + 10);
    tft.print(lbl);

    // CV-Pegel-Balken (rechts)
    int barFill = (int)((uint32_t)cvSmooth[i] * CV_CFG_BAR_W / 4095u);
    lastCvBarFill[i] = barFill;
    tft.fillRect(CV_CFG_BAR_X, y + 6, barFill, CV_CFG_ROW_H - 12, ILI9341_CYAN);
    tft.fillRect(CV_CFG_BAR_X + barFill, y + 6, CV_CFG_BAR_W - barFill, CV_CFG_ROW_H - 12, ILI9341_BLACK);
    tft.drawRect(CV_CFG_BAR_X, y + 6, CV_CFG_BAR_W, CV_CFG_ROW_H - 12, ILI9341_DARKGREY);
}

void drawCvConfigScreen() {
    tft.fillScreen(ILI9341_BLACK);
    lastCvBarFill[0] = lastCvBarFill[1] = lastCvBarFill[2] = -1;

    // Rücksprungpfeil
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setFont(AwesomeF100_24);
    tft.setCursor(10, 15);
    tft.print((char)18);

    // Titel
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(50, 18);
    tft.print("CV Config");

    for (int i = 0; i < 3; i++) drawCvRow(i);
}

void handleCvConfig(int mapX, int mapY, uint16_t tipPos) {
    if (tipPos == UL) {
        GUIState = PERFORMANCE;
        drawPerformanceScreen();
        return;
    }
    for (int i = 0; i < 3; i++) {
        int y = CV_CFG_ROW_Y[i];
        if (hitBox(mapX, mapY, CV_CFG_BTN_X, y, CV_CFG_BTN_W, CV_CFG_ROW_H, 4)) {
            cvTargetMap[i] = (uint8_t)((cvTargetMap[i] + 1) % CV_TARGET_COUNT);
            applyCvTargets();
            drawCvRow(i);
            scheduleSaveParams();
            return;
        }
    }
}

// Aktualisiert nur die CV-Pegel-Balken (wird jeden Loop-Durchlauf aufgerufen).
void tickCvConfigUi() {
    for (int i = 0; i < 3; i++) {
        int barFill = (int)((uint32_t)cvSmooth[i] * CV_CFG_BAR_W / 4095u);
        if (barFill == lastCvBarFill[i]) continue;
        lastCvBarFill[i] = barFill;
        int y = CV_CFG_ROW_Y[i];
        tft.fillRect(CV_CFG_BAR_X, y + 6, barFill, CV_CFG_ROW_H - 12, ILI9341_CYAN);
        tft.fillRect(CV_CFG_BAR_X + barFill, y + 6, CV_CFG_BAR_W - barFill, CV_CFG_ROW_H - 12, ILI9341_BLACK);
    }
}
