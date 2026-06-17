#include <ui_screens.h>

#include <ili9341_t3n_font_Arial.h>
#include <font_AwesomeF100.h>

#include <euclid.h>
#include <storage.h>
#include <gates.h>
#include <pitch.h>
#include <encoders.h>
#include <cv_inputs.h>
#include <midi_out.h>

// Zwei-Phasen-Draw: Phase 1 hat fillScreen bereits ausgeführt → überspringen.
static bool s_skipNextFill_top = false;
static inline void fillScreenIfNeeded() {
    if (!s_skipNextFill_top) tft.fillScreen(ILI9341_BLACK);
    s_skipNextFill_top = false;
}

static int lastValuesPlayIdx[3]    = { -1, -1, -1 };
static int dragLockIdx = -1;  // Gesperrter Balken-Index für Drag-Hysterese (-1 = nicht aktiv)
// Betriebsart: false=Einzelschritt (X-Lock), true=Malen (horizontales Gleiten)
static bool barPaintMode = false;
bool getBarPaintMode()        { return barPaintMode; }
void setBarPaintMode(bool v)  {
    barPaintMode = v;
    drawBarPaintModeIndicator();
}
static int gateLenEditMode[3] = { 0, 0, 0 };  // 0=GateLen, 1=HoldStep, 2=MuteStep
static void drawBarPaintModeIndicator();  // forward
static void drawTransposeIndicator();     // forward
static void drawGateLenModeButtons(int setIdx);  // forward
static void drawRotateHoldStepCheckbox(int setIdx);  // forward
static void drawRotateMuteStepCheckbox(int setIdx);  // forward
static int  valuesEditMode[3]      = { 0, 0, 0 };  // 0=values, 1=ratchet, 2=octave, 3=iv
static bool valStepEditActive[3]   = {false, false, false};
static int  valStepEditCursor[3]   = {0, 0, 0};

// Condition Preset Mode
static bool condPresetMode = false;
static int  condPresetCh   = -1;
static int  condPresetIdx  = 0;
static const int COND_PRESET_COUNT = 18;
static const char* const condPresetNames[COND_PRESET_COUNT] = {
    "4-Bar",   "ACC23",   "Accent",  "Alternate","Bounce",  "Chaos",
    "Fill",    "Ghost",   "Groove",  "Heavy",    "HUMAN234","Humanize",
    "Punchy",  "Stutter", "Swell",   "Thin Out", "All Odd", "All Even"
};
static int lastXYPlayIdx[3]    = { -1, -1, -1 };
static int lastXYDotIdx[3]     = { -1, -1, -1 };
static int lastYellowPxX[3]   = { -1, -1, -1 };
static int lastYellowPxY[3]   = { -1, -1, -1 };

// Verwendet effRotSel (PatRot+PatRotSel+cvOffset) — für angekreuzte Schichten (folgen R+r).
static inline int layerRotatedSrc(int ch, int idx) {
    int len = PatLen[ch];
    if (len <= 0) return idx;
    int effRotSel = clampVal(PatRot[ch] + PatRotSel[ch] + (int)cvPatRotOffset[ch],
                             -(len - 1), len - 1);
    return euclidRotatedSrc(idx, len, effRotSel);
}

// Verwendet effRot (PatRot+cvOffset) — für nicht-angekreuzte Schichten (folgen nur R).
static inline int layerBaseSrc(int ch, int idx) {
    int len = PatLen[ch];
    if (len <= 0) return idx;
    int effRot = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
    return euclidRotatedSrc(idx, len, effRot);
}
static const uint16_t XY_GRID_COLOR = 0x2104;  // dunkles Grau fuer XY-Raster
static const uint16_t BAR_GRID_COL  = 0x630C;  // mittleres Grau (~RGB 99,97,99) fuer 4-Schritt-Raster

// Zeichnet vertikale Gitterlinien an jeder 4. Step-Grenze (idx=4,8,12,...).
// x0=10, totalW=300 entspricht dem gemeinsamen Koordinatensystem aller Bar-Screens.
static void drawStepGridLines(int y0, int h, int len) {
    for (int gi = 4; gi < len; gi += 4) {
        tft.drawFastVLine(10 + (gi * 300) / len, y0, h, BAR_GRID_COL);
    }
}
static int  xyPadPitchMode = 0;  // 0=GateLen, 1=Pitch 1oct, 2=Pitch 3oct, 3=Pitch 5oct
static uint16_t keyBgCache[180];  // Hintergrundfarbe pro Y-Pixel (y=40..219)
static bool     xyKeyboardMode = false;  // true wenn Klaviatur-BG aktiv

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
static const int PERF_BOX_W      = 40;   // 8 Slots × 40 px = 320 px
static const int PERF_BOX_H      = 24;
static const int PERF_BOX_ROW1_Y = 184;  // Slots 0-7
static const int PERF_BOX_ROW2_Y = 210;  // Slots 8-15
static const int PERF_BTN_Y = 150;
static const int PERF_BTN_W = 55;
static const int PERF_BTN_H = 30;
static const int PERF_BTN_XS[4] = {0, 113, 172, 238};
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
static uint16_t perfUsedMask = 0;
static int perfEncSlot = -1;   // Encoder-Browse-Hervorhebung (-1 = inaktiv)
static int perfPickSlot = -1;  // P&P: gepickter Quell-Slot (-1 = kein Pick aktiv)
static bool perfButtonFlash[4] = { false, false, false, false };
static uint32_t perfButtonFlashUntil[4] = { 0, 0, 0, 0 };
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

static const int EXTCLK_X   = 170;
static const int EXTCLK_Y   = 115;
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
static int  ivInversionIdx          = 0;  // IV-Inversions-Selector: 0=Grundstellung

static void drawPitchPresetBox() {
    uint16_t border = pitchPresetBrowseActive ? ILI9341_CYAN : ILI9341_DARKGREY;
    tft.fillRect(PITCH_PRESET_BX + 1, PITCH_PRESET_BY + 1,
                 PITCH_PRESET_BW - 2, PITCH_PRESET_BH - 2, ILI9341_BLACK);
    tft.drawRect(PITCH_PRESET_BX, PITCH_PRESET_BY, PITCH_PRESET_BW, PITCH_PRESET_BH, border);
    tft.setFont(Arial_12);
    uint8_t cat = getPitchPresetCategory(pitchPresetBrowseIdx);
    uint16_t nameColor;
    if      (cat == 2) nameColor = ILI9341_GREEN;
    else if (cat == 1) nameColor = ILI9341_YELLOW;
    else if (pitchPresetBrowseActive) nameColor = ILI9341_CYAN;
    else               nameColor = ILI9341_LIGHTGREY;
    tft.setTextColor(nameColor);
    const char *name = getPitchPresetName(pitchPresetBrowseIdx);
    // Exakte Breite messen; wenn zu breit → kleinere Schrift
    int nameW = (int)tft.measureTextWidth((const uint8_t*)name, strlen(name));
    if (nameW > PITCH_PRESET_BW - 6) {
        tft.setFont(Arial_10);
        nameW = (int)tft.measureTextWidth((const uint8_t*)name, strlen(name));
        tft.setCursor(PITCH_PRESET_BX + (PITCH_PRESET_BW - nameW) / 2, PITCH_PRESET_BY + 7);
    } else {
        tft.setCursor(PITCH_PRESET_BX + (PITCH_PRESET_BW - nameW) / 2, PITCH_PRESET_BY + 6);
    }
    tft.print(name);
}

int  getPitchPresetBrowseIdx()    { return pitchPresetBrowseIdx; }
bool getPitchPresetBrowseActive() { return pitchPresetBrowseActive; }

void setPitchPresetBrowseIdx(int idx) {
    int N = PITCH_PRESET_COUNT + 1;  // +1 für "Random"
    pitchPresetBrowseIdx = ((idx % N) + N) % N;
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
    if (getPitchPresetCategory(idx) == 2) {
        // Note-Effekt: Undo möglich wenn gleicher Effekt nochmal gewählt
        if (idx == lastNoteEffectIdx) {
            // Undo: Zustand vor dem letzten Effekt wiederherstellen
            memcpy(PitchNote1, undoPitchNotes, 32);
            memcpy(lastNonEchoPitchNotes, undoPitchNotes, 32);
            lastNoteEffectIdx = -1;
        } else {
            // Neuer Effekt: Undo-Buffer füllen, Effekt anwenden, sofort backen
            memcpy(undoPitchNotes, PitchNote1, 32);
            memcpy(lastNonEchoPitchNotes, PitchNote1, 32);
            getPitchPresetNotes(idx, PitchNote1);
            memcpy(lastNonEchoPitchNotes, PitchNote1, 32);
            lastNoteEffectIdx = idx;
        }
    } else {
        // Normales Preset: direkt laden, Undo-Kette zurücksetzen
        getPitchPresetNotes(idx, PitchNote1);
        memcpy(lastNonEchoPitchNotes, PitchNote1, 32);
        lastNoteEffectIdx = -1;
    }
    for (int i = 0; i < 32; i++) OctaveNote1[i] = 0;
    ivInversionIdx = 0;
    pitchNotesFrozen = false;
    transposeOffset  = 0;
    int N = PITCH_PRESET_COUNT + 1;
    pitchPresetBrowseIdx = ((idx % N) + N) % N;
    scheduleSaveParams();
    if (GUIState == PITCH1) {
        drawPitchPresetBox();
        pendingPitchDraw = true;
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

  tft.setFont(Arial_24);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(8, 6);
  tft.print("EuclidPatGen T4.1");

  tft.setFont(Arial_12);
  tft.setTextColor(ILI9341_DARKGREY);
  tft.setCursor(8, 36);
  tft.print("v2.0  (c) 2026 AMsl");

  tft.setFont(Arial_12);
  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(8, 58);  tft.print("3-Kanal Euklid-Sequencer");
  tft.setCursor(8, 74);  tft.print("Pitch-CV 1V/Oct \x7C 25 Skalen \x7C Fold");
  tft.setCursor(8, 90);  tft.print("XY-Pad \x7C Ratchet \x7C Oktav per Step");
  tft.setCursor(8, 106); tft.print("31 Rhythmus- / 16+ Pitch-Presets");
  tft.setCursor(8, 122); tft.print("Performance: Mute / Solo / 7 Slots");
  tft.setCursor(8, 138); tft.print("Song-Sequencer: 64 Schritte / Loop");
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(8, 154); tft.print("COND: 16 Presets \x7C +V*2/1.5/1.3/1.25");
  tft.setCursor(8, 170); tft.print("COND: +/-2Oct \x7C Mute \x7C Acc \x7C Gate \x7C R");
  tft.setCursor(8, 186); tft.print("CV: Cmp1/2/3/A \x7C PatK \x7C Morph \x7C Rot");
  tft.setCursor(8, 202); tft.print("Values: Enc1=Shift \x7C Enc2=Expand/Cmp");

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
// Assumptions: idx in 0..15.
static void drawPerfSlotBox(int idx){
  int col = idx % 8;
  int x   = col * PERF_BOX_W;
  int y   = (idx < 8) ? PERF_BOX_ROW1_Y : PERF_BOX_ROW2_Y;
  bool picked     = (idx == perfPickSlot);
  bool selected   = (idx == perfSelected);
  bool encBrowse  = (idx == perfEncSlot) && (cvSlotSel < 0);
  bool cvCtrl     = (cvSlotSel >= 0) && (idx == (int)cvSlotSel);
  uint16_t border = cvCtrl ? ILI9341_CYAN : (picked ? ILI9341_ORANGE : ILI9341_DARKGREY);
  uint16_t fill;
  if      (picked)    fill = ILI9341_ORANGE;
  else if (selected)  fill = ILI9341_GREEN;
  else if (cvCtrl)    fill = 0x0410;
  else if (encBrowse) fill = ILI9341_CYAN;
  else                fill = (perfUsedMask & (uint16_t)(1u << idx)) ? ILI9341_WHITE : ILI9341_BLACK;
  tft.fillRect(x + 1, y + 1, PERF_BOX_W - 2, PERF_BOX_H - 2, fill);
  tft.drawRect(x, y, PERF_BOX_W, PERF_BOX_H, border);
  if(idx == perfActive){
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_BLACK);
    drawCenteredLabel(x, y, PERF_BOX_W, PERF_BOX_H, "A", 8, 16, -4, 0);
  }
}

// CV-Lock-Indikator: im freien Bereich zwischen Load (x=0..55) und Save (x=113..168)
static void drawCvSlotIndicator() {
  tft.fillRect(57, PERF_BTN_Y + 2, 54, PERF_BTN_H - 4, ILI9341_BLACK);
  if (cvSlotSel >= 0) {
    tft.setFont(Arial_10);
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(59, PERF_BTN_Y + 10);
    tft.print("CV\x10Slot");
  }
}

// Zweck: Zeichnet einen Performance-Button (Load/Save/Del/P&P) mit optionalem Flash.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..3.
static void drawPerfButton(int idx){
  int x = PERF_BTN_XS[idx];
  uint16_t border;
  if      (idx == 1 || idx == 2)              border = ILI9341_RED;
  else if (idx == 3 && perfPickSlot >= 0)     border = ILI9341_ORANGE;  // P&P im Pick-Modus
  else                                         border = ILI9341_WHITE;
  uint16_t fill = perfButtonFlash[idx] ? ILI9341_LIGHTGREY : ILI9341_BLACK;
  tft.drawRect(x, PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, border);
  tft.drawRect(x + 1, PERF_BTN_Y + 1, PERF_BTN_W - 2, PERF_BTN_H - 2, border);
  tft.drawRect(x + 2, PERF_BTN_Y + 2, PERF_BTN_W - 4, PERF_BTN_H - 4, border);
  tft.drawRect(x + 3, PERF_BTN_Y + 3, PERF_BTN_W - 6, PERF_BTN_H - 6, border);
  tft.fillRect(x + 4, PERF_BTN_Y + 4, PERF_BTN_W - 8, PERF_BTN_H - 8, fill);
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_LIGHTGREY);
  const char *labels[4] = { "Load", "Save", "Del", "P&P" };
  drawCenteredLabel(x, PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, labels[idx], 8, 16, -8, -1);
}

// Zweck: Startet nicht-blockierenden Flash fuer einen Button.
// Side Effects: setzt Timer-Status.
// Assumptions: idx in 0..3.
static void startPerfButtonFlash(int idx){
  if (idx < 0 || idx > 3) return;
  perfButtonFlash[idx] = true;
  perfButtonFlashUntil[idx] = millis() + 80;
  drawPerfButton(idx);
}

// Zweck: Aktualisiert Button-Flash (nicht-blockierend).
// Side Effects: schreibt auf das TFT.
static void updatePerfButtonFlash(){
  uint32_t now = millis();
  for(int i=0;i<4;i++){
    if(perfButtonFlash[i] && (int32_t)(now - perfButtonFlashUntil[i]) >= 0){
      perfButtonFlash[i] = false;
      drawPerfButton(i);
    }
  }
}

bool getPerfPickActive() { return perfPickSlot >= 0; }

void cancelPerfPick() {
    if (perfPickSlot < 0) return;
    int was = perfPickSlot;
    perfPickSlot = -1;
    drawPerfSlotBox(was);
    drawPerfButton(3);
}

void executePerfPickPlace(int dst) {
    if (perfPickSlot < 0 || dst < 0 || dst >= 16) return;
    int src = perfPickSlot;
    perfPickSlot = -1;
    if (src != dst) {
        // Kanal-Maske: gemutete Kanäle NICHT kopieren (bit i=1 → Kanal i übernehmen)
        uint8_t mask = (uint8_t)(
            (!MuteSeq[0] ? 0x01u : 0u) |
            (!MuteSeq[1] ? 0x02u : 0u) |
            (!MuteSeq[2] ? 0x04u : 0u));
        if (mask == 0) mask = 0x07;  // alle gemutet → kompletter Copy als Fallback
        perfUsedMask = (uint16_t)(perfUsedMask | (1u << dst));
        // Quelle bleibt erhalten (kein Löschen aus perfUsedMask)
        pendingSlotMoveFrom = src;
        pendingSlotMoveTo   = dst;
        pendingSlotCopyMask = mask;
    }
    drawPerfSlotBox(src);
    drawPerfSlotBox(dst);
    drawPerfButton(3);
}

void refreshPerfSlotState() {
    perfUsedMask = getSlotsUsedMask();
    perfActive   = getActiveSlot();
    for (int i = 0; i < 16; i++) drawPerfSlotBox(i);
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
  static int8_t lastCvSlotTick = -2;
  if (cvSlotSel != lastCvSlotTick) {
    int8_t prev = lastCvSlotTick;
    lastCvSlotTick = cvSlotSel;
    if (prev >= 0 && prev < 16) drawPerfSlotBox(prev);
    if (cvSlotSel >= 0 && cvSlotSel < 16) drawPerfSlotBox(cvSlotSel);
    drawCvSlotIndicator();
  }
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
  fillScreenIfNeeded();
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
  for(int i=0;i<16;i++){
    drawPerfSlotBox(i);
  }
  drawCvSlotIndicator();

  // Load/Save/Del/P&P Buttons
  for(int i=0;i<4;i++){
    perfButtonFlash[i] = false;
    perfButtonFlashUntil[i] = 0;
    drawPerfButton(i);
  }

  // Rhythmus-Preset-Fenster (oben rechts)
  rhythmBrowseActive = false;
  drawRhythmPresetWindow();

  // CV-Config-Button und G.Config-Button (nebeneinander, y=50)
  tft.setFont(Arial_12);
  tft.fillRect(EXTCLK_X + 1, 51, 70, 22, 0x4208);
  tft.drawRect(EXTCLK_X, 50, 72, 24, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(EXTCLK_X + 7, 58);
  tft.print("CV Cfg");
  tft.fillRect(249, 51, 68, 22, 0x4208);
  tft.drawRect(248, 50, 70, 24, ILI9341_DARKGREY);
  tft.setCursor(255, 58);
  tft.print("global");
  // Song-Button (unter CV Cfg und global, gleiche Breite zusammen)
  tft.fillRect(171, 79, 146, 22, songPlaying ? 0x03E0u : 0x4208u);
  tft.drawRect(170, 78, 148, 24, songPlaying ? ILI9341_GREEN : ILI9341_DARKGREY);
  tft.setCursor(225, 86);
  tft.print("Song");

  // Ext-Clock-Checkbox (rechte Seite, unter Song-Button)
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
  // CV-Config-Button (x=EXTCLK_X, y=50, w=72, h=24) — vor UL-Prüfung
  if (hitBox(mapX, mapY, EXTCLK_X, 50, 72, 24, 5)) {
      requestNavigateTo(CV_CONFIG);
      return true;
  }
  // G.Config-Button (x=248, y=50, w=70, h=24)
  if (hitBox(mapX, mapY, 248, 50, 70, 24, 5)) {
      requestNavigateTo(GCONFIG);
      return true;
  }
  // Song-Button (x=170, y=78, w=148, h=24)
  if (hitBox(mapX, mapY, 170, 78, 148, 24, 5)) {
      if (getPerfPickActive()) cancelPerfPick();
      requestNavigateTo(SONG);
      return true;
  }
  if(tipPos == UL){
    requestNavigateTo(EUCLCIRCS);
    return true;
  }

  // Buttons: Load / Save / Del — vor Slot-Boxen prüfen (geometrische Überlappung)
  if(hitBox(mapX, mapY, PERF_BTN_XS[0], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(0);
    if(perfSelected >= 0 && (perfUsedMask & (1u << perfSelected))){
      requestLoadSlot(perfSelected);
      resetQuickSavePointer();
      int was = perfSelected;
      perfSelected = -1;
      drawPerfSlotBox(was);
    }
    return true;
  }
  if(hitBox(mapX, mapY, PERF_BTN_XS[1], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(1);
    if(perfSelected >= 0){
      // Deferred: SD-Write in den Main-Loop verlagern (nicht im Touch-Handler blockieren)
      pendingSlotSaveSlot = perfSelected;
      perfUsedMask = (uint16_t)(perfUsedMask | (1u << perfSelected));
      int was = perfSelected;
      perfSelected = -1;
      drawPerfSlotBox(was);
    }
    return true;
  }
  if(hitBox(mapX, mapY, PERF_BTN_XS[2], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(2);
    if(perfSelected >= 0){
      if(deleteParamsSlot(perfSelected)){
        perfUsedMask = (uint16_t)(perfUsedMask & ~(1u << perfSelected));
        int was = perfSelected;
        perfSelected = -1;
        drawPerfSlotBox(was);
      }
    }
    return true;
  }
  // P&P: Pick-Modus starten/beenden
  if(hitBox(mapX, mapY, PERF_BTN_XS[3], PERF_BTN_Y, PERF_BTN_W, PERF_BTN_H, 6)){
    startPerfButtonFlash(3);
    if(perfPickSlot >= 0){
      // Zweiter Druck: Pick-Modus abbrechen
      int was = perfPickSlot;
      perfPickSlot = -1;
      drawPerfSlotBox(was);
      drawPerfButton(3);
    } else if(perfSelected >= 0 && (perfUsedMask & (1u << perfSelected))){
      // Erster Druck: Pick-Modus aktivieren
      perfPickSlot = perfSelected;
      perfSelected = -1;
      drawPerfSlotBox(perfPickSlot);
      drawPerfButton(3);
    }
    return true;
  }

  // Pattern-Boxen (2 Reihen à 8 Slots)
  for(int i=0;i<16;i++){
    int bx = (i % 8) * PERF_BOX_W;
    int by = (i < 8) ? PERF_BOX_ROW1_Y : PERF_BOX_ROW2_Y;
    if(hitBox(mapX, mapY, bx, by, PERF_BOX_W, PERF_BOX_H, 4)){
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

static void drawPitchButton();          // forward decl (defined in pitch section below)
static void drawPitchV1CordButtons();   // forward decl (defined in pitch section below)
// Chord-Seq forward decls
void drawChordSeqTitleBar();
void drawChordSeqCell(int page, int col);

// Redraw helper for parameter menus (zeigt Kreis mit R1)
// Zweck: Zeichnet das Parameter-Menue fuer das gewaehlte Pattern neu.
// Side Effects: schreibt auf das TFT und aktualisiert PatLen/PatNum/PatRot.
// Assumptions: idx in 0..2; Arrays sind gueltig.
void redrawParam(int idx){
    if(idx<0 || idx>2) return;

    fillScreenIfNeeded();

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

    fillScreenIfNeeded();

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
  tft.fillRect(261, 11, 48, 28, 0x4208);
  tft.drawRect(260, 10, 50, 30, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_WHITE);
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
  tft.fillRect(261, 201, 48, 28, 0x4208);
  tft.drawRect(260, 200, 50, 30, ILI9341_DARKGREY);
  tft.setTextColor(ILI9341_WHITE);
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

static void drawAbToggleButton() {
    static const int x = 150, y = 10, w = 50, h = 24;
    uint16_t col = abEditMode ? 0xFD20 : ILI9341_DARKGREY;
    tft.fillRect(x, y, w, h, ILI9341_BLACK);
    tft.drawRect(x, y, w, h, col);
    tft.setFont(Arial_10);
    tft.setCursor(x + 9, y + 7);
    tft.setTextColor(abEditMode ? ILI9341_DARKGREY : ILI9341_WHITE);
    tft.print("A");
    tft.setTextColor(ILI9341_DARKGREY);
    tft.print("/");
    tft.setTextColor(abEditMode ? 0xFD20 : ILI9341_DARKGREY);
    tft.print("B");
}

static void drawValuesEditButtons(int setIdx) {
    tft.fillRect(0, 42, 285, 28, ILI9341_BLACK);
    int mode = valuesEditMode[setIdx];

    // Rat toggle button: x=10, y=42, w=40, h=24 (flush with bar area)
    bool ratOn = (mode == 1);
    tft.drawRect(10, 42, 40, 24, ratOn ? ILI9341_CYAN : ILI9341_DARKGREY);
    tft.setFont(Arial_12);
    tft.setTextColor(ratOn ? ILI9341_CYAN : ILI9341_LIGHTGREY);
    tft.setCursor(20, 48);
    tft.print("Rat");

    // RR checkbox (RotateRatchet): x=54, y=43, s=18
    tft.drawRect(54, 43, 18, 18, ILI9341_DARKGREY);
    if (RotateRatchet[setIdx]) {
        tft.drawLine(58, 53, 61, 57, ILI9341_GREEN);
        tft.drawLine(61, 57, 68, 46, ILI9341_GREEN);
    }

    if (setIdx == 0) {
        // Oct toggle button: x=80, y=42, w=40, h=24
        bool octOn = (mode == 2);
        tft.drawRect(80, 42, 40, 24, octOn ? ILI9341_MAGENTA : ILI9341_DARKGREY);
        tft.setTextColor(octOn ? ILI9341_MAGENTA : ILI9341_LIGHTGREY);
        tft.setCursor(90, 48);
        tft.print("Oct");

        // RO checkbox (RotateOctave): x=124, y=43, s=18
        tft.drawRect(124, 43, 18, 18, ILI9341_DARKGREY);
        if (RotateOctave[setIdx]) {
            tft.drawLine(128, 53, 131, 57, ILI9341_GREEN);
            tft.drawLine(131, 57, 138, 46, ILI9341_GREEN);
        }

        // IV toggle button: x=150, y=42, w=40, h=24
        bool ivOn = (mode == 3);
        tft.drawRect(150, 42, 40, 24, ivOn ? ILI9341_GREEN : ILI9341_DARKGREY);
        tft.setTextColor(ivOn ? ILI9341_GREEN : ILI9341_LIGHTGREY);
        tft.setCursor(163, 48);
        tft.print("IV");

        // RI checkbox (RotateIvStep): x=194, y=43, s=18
        tft.drawRect(194, 43, 18, 18, ILI9341_DARKGREY);
        if (RotateIvStep) {
            tft.drawLine(198, 53, 201, 57, ILI9341_GREEN);
            tft.drawLine(201, 57, 208, 46, ILI9341_GREEN);
        }
    }

    // RV checkbox (RotateValues): x=260, y=42, s=24 (rechtsbündig mit Hold)
    tft.drawRect(260, 42, 24, 24, ILI9341_DARKGREY);
    tft.setFont(Arial_12);
    tft.setCursor(236, 48);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("RV");
    if (RotateValues[setIdx]) {
        tft.drawLine(264, 54, 270, 60, ILI9341_GREEN);
        tft.drawLine(270, 60, 280, 48, ILI9341_GREEN);
    }
}

void drawRatchetBar(int setIdx, int idx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10, y0 = 240 - 5 - 160, h = 160;
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;
    int src = RotateRatchet[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    uint8_t rval = (uint8_t)clampVal((int)RatchetArr[setIdx][src], 1, 4);
    int fillH = (rval * h) / 4;  // 1→40, 2→80, 3→120, 4→160
    bool active = patternIsHit(setIdx, idx);
    int emptyH = h - fillH;
    if (emptyH > 0) tft.fillRect(x, y0,          xNext - x, emptyH, ILI9341_BLACK);
    if (fillH > 0) {
        int y = y0 + emptyH;
        tft.fillRect(x, y, w, fillH, active ? ILI9341_YELLOW : ILI9341_DARKGREY);
        if (w >= 10 && fillH >= 12) {
            tft.setFont(Arial_12);
            tft.setTextColor(ILI9341_BLACK);
            tft.setCursor(x + (w - 7) / 2, y + 2);
            tft.print(rval);
        }
    }
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_CYAN);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
}

void drawRatchetBars(int setIdx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    for (int i = 0; i < len; i++) drawRatchetBar(setIdx, i);
    drawStepGridLines(240 - 5 - 160, 160, len);
}

void drawOctaveBar(int setIdx, int idx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10, y0 = 240 - 5 - 160, h = 160;
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;
    int osrc = RotateOctave[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    int8_t octVal = OctaveNote1[osrc];
    bool active = patternIsHit(setIdx, idx);
    tft.fillRect(x, y0, xNext - x, h, ILI9341_BLACK);
    int cy = y0 + h / 2;
    tft.drawFastHLine(x, cy, xNext - x, ILI9341_DARKGREY);
    if (octVal != 0) {
        int barH = abs(octVal) * (h / 2) / 3;
        uint16_t col = active ? ILI9341_MAGENTA : 0x4208;
        if (octVal > 0) tft.fillRect(x, cy - barH, w, barH, col);
        else            tft.fillRect(x, cy + 1, w, barH, col);
    }
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_CYAN);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
}

void drawOctaveBars(int setIdx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    for (int i = 0; i < len; i++) drawOctaveBar(setIdx, i);
    drawStepGridLines(240 - 5 - 160, 160, len);
}

void drawIvStepBar(int setIdx, int idx) {
    if (setIdx != 0) return;
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10, y0 = 240 - 5 - 160, h = 160;
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;
    int src = RotateIvStep ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    uint8_t ivVal = IvStep1[src];
    bool active = patternIsHit(setIdx, idx);
    int fillH = ivVal > 0 ? ((int)ivVal * h) / 7 : 0;
    int emptyH = h - fillH;
    if (emptyH > 0) tft.fillRect(x, y0,          xNext - x, emptyH, ILI9341_BLACK);
    if (ivVal > 0) {
        int y = y0 + emptyH;
        tft.fillRect(x, y, w, fillH, active ? ILI9341_GREEN : ILI9341_DARKGREY);
        if (w >= 10 && fillH >= 12) {
            tft.setFont(Arial_12);
            tft.setTextColor(ILI9341_BLACK);
            tft.setCursor(x + (w - 7) / 2, y + 2);
            tft.print(ivVal);
        }
    }
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_CYAN);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
}

void drawIvStepBars(int setIdx) {
    if (setIdx != 0) return;
    int len = clampVal(PatLen[setIdx], 1, 32);
    for (int i = 0; i < len; i++) drawIvStepBar(setIdx, i);
    drawStepGridLines(240 - 5 - 160, 160, len);
}

// Zweck: Baut den Values-Screen fuer ein Pattern auf.
// Side Effects: schreibt auf das TFT und setzt Playhead-Status.
// Assumptions: setIdx in 0..2.
void drawValuesScreen(int setIdx){
    fillScreenIfNeeded();
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    drawHoldCheckbox(setIdx);
    drawRotateValuesCheckbox(setIdx);
    drawGateLenButton();
    drawAbToggleButton();
    drawBarPaintModeIndicator();
    drawValuesEditButtons(setIdx);
    // Rahmen um den Values-Bereich
    {
      int x0 = 10;
      int y0 = 240 - 5 - 160;
      int h  = 160;
      tft.drawRect(x0 - 1, y0 - 1, 320 - 2 * x0 + 2, h + 2, ILI9341_DARKGREY);
    }
    if      (valuesEditMode[setIdx] == 1) drawRatchetBars(setIdx);
    else if (valuesEditMode[setIdx] == 2) drawOctaveBars(setIdx);
    else if (valuesEditMode[setIdx] == 3) drawIvStepBars(setIdx);
    else                                  drawValuesBars(setIdx);
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
    drawStepGridLines(240 - 5 - 160, 160, len);
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

    int src = RotateValues[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    uint8_t val = abEditMode ? ValuesBArr[setIdx][src] : ValuesArr[setIdx][src];
    int fillH = (int)((val * (long)h) / 255L);
    bool active = patternIsHit(setIdx, idx);
    uint16_t hitCol  = abEditMode ? 0xFD20 : ILI9341_WHITE;
    uint16_t missCol = abEditMode ? 0x8400 : ILI9341_DARKGREY;

    int emptyH = h - fillH;
    if (emptyH > 0) tft.fillRect(x, y0,          xNext - x, emptyH, ILI9341_BLACK);
    if (fillH  > 0) tft.fillRect(x, y0 + emptyH, w,         fillH,  active ? hitCol : missCol);
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_CYAN);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
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
    tft.setCursor(x - 15, y + 6);
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
    drawValuesEditButtons(setIdx);
}

// Zweck: Zeichnet den Button zum GateLen-Screen.
// Side Effects: schreibt auf das TFT.
// Assumptions: TFT ist initialisiert.
void drawGateLenButton(){
    int x = 80;
    int y = 10;
    int w = 50;
    int h = 24;
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, 0x4208);
    tft.drawRect(x, y, w, h, ILI9341_DARKGREY);
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_WHITE);
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
        requestNavigateTo((idx == 0) ? XY1 : (idx == 1) ? XY2 : XY3);
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
        requestNavigateTo(EUCLCIRCS);
        break;
      case UR:
        if(idx == 0){
          requestNavigateTo(PITCH1);
        }else if(idx == 1){
          requestNavigateTo(VALUES2);
        }else if(idx == 2){
          requestNavigateTo(VALUES3);
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
    dragLockIdx = -1;
    if(tipPos == UL){
        requestNavigateTo(setIdx == 0 ? PITCH1 : (setIdx == 1) ? EUCLPARAM2 : EUCLPARAM3);
        return;
    }
    if(hitBox(mapX, mapY, 150, 10, 50, 24, 6)){
        abEditMode = !abEditMode;
        drawAbToggleButton();
        if (valuesEditMode[setIdx] == 0) drawValuesBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 54, 43, 18, 18, 6)){
        RotateRatchet[setIdx] = !RotateRatchet[setIdx];
        scheduleSaveParams();
        drawValuesEditButtons(setIdx);
        if (valuesEditMode[setIdx] == 1) drawRatchetBars(setIdx);
        return;
    }
    if(setIdx == 0 && hitBox(mapX, mapY, 124, 43, 18, 18, 6)){
        RotateOctave[setIdx] = !RotateOctave[setIdx];
        scheduleSaveParams();
        drawValuesEditButtons(setIdx);
        if (valuesEditMode[setIdx] == 2) drawOctaveBars(setIdx);
        return;
    }
    if(setIdx == 0 && hitBox(mapX, mapY, 194, 43, 18, 18, 6)){
        RotateIvStep = !RotateIvStep;
        scheduleSaveParams();
        drawValuesEditButtons(setIdx);
        if (valuesEditMode[setIdx] == 3) drawIvStepBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 42, 24, 24, 8)){
        RotateValues[setIdx] = !RotateValues[setIdx];
        scheduleSaveParams();
        drawValuesEditButtons(setIdx);
        if (valuesEditMode[setIdx] == 0) drawValuesBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 260, 10, 24, 24, 8)){
        // Hold: Werte werden nur bei Hits uebernommen.
        *HoldArr[setIdx] = !(*HoldArr[setIdx]);
        scheduleSaveParams();
        drawHoldCheckbox(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 80, 10, 50, 24, 6)){
        requestNavigateTo((setIdx == 0) ? GATELEN1 : (setIdx == 1) ? GATELEN2 : GATELEN3);
        return;
    }
    if(hitBox(mapX, mapY, 10, 42, 40, 24, 6)){
        valuesEditMode[setIdx] = (valuesEditMode[setIdx] == 1) ? 0 : 1;
        drawValuesEditButtons(setIdx);
        if      (valuesEditMode[setIdx] == 1) drawRatchetBars(setIdx);
        else if (valuesEditMode[setIdx] == 2) drawOctaveBars(setIdx);
        else if (valuesEditMode[setIdx] == 3) drawIvStepBars(setIdx);
        else                                  drawValuesBars(setIdx);
        return;
    }
    if(setIdx == 0 && hitBox(mapX, mapY, 80, 42, 40, 24, 6)){
        valuesEditMode[setIdx] = (valuesEditMode[setIdx] == 2) ? 0 : 2;
        drawValuesEditButtons(setIdx);
        if      (valuesEditMode[setIdx] == 1) drawRatchetBars(setIdx);
        else if (valuesEditMode[setIdx] == 2) drawOctaveBars(setIdx);
        else if (valuesEditMode[setIdx] == 3) drawIvStepBars(setIdx);
        else                                  drawValuesBars(setIdx);
        return;
    }
    if(setIdx == 0 && hitBox(mapX, mapY, 150, 42, 40, 24, 6)){
        valuesEditMode[setIdx] = (valuesEditMode[setIdx] == 3) ? 0 : 3;
        drawValuesEditButtons(setIdx);
        if      (valuesEditMode[setIdx] == 1) drawRatchetBars(setIdx);
        else if (valuesEditMode[setIdx] == 2) drawOctaveBars(setIdx);
        else if (valuesEditMode[setIdx] == 3) drawIvStepBars(setIdx);
        else                                  drawValuesBars(setIdx);
        return;
    }

    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int totalW = 320 - 2 * x0;  // = 300

    if(mapX < x0 || mapX >= (x0 + totalW) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    int idx = clampVal(((mapX - x0) * len + len - 1) / totalW, 0, len - 1);
    dragLockIdx = idx;
    if (valuesEditMode[setIdx] == 1) {
        int writeIdx = RotateRatchet[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = clampVal(1 + (y0 + h - mapY) * 4 / h, 1, 4);
        RatchetArr[setIdx][writeIdx] = (uint8_t)v;
        scheduleSaveParams();
        drawRatchetBar(setIdx, idx);
    } else if (valuesEditMode[setIdx] == 2 && setIdx == 0) {
        int writeIdx = RotateOctave[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = clampVal(3 - (mapY - y0) * 7 / h, -3, 3);
        OctaveNote1[writeIdx] = (int8_t)v;
        scheduleSaveParams();
        drawOctaveBar(setIdx, idx);
    } else if (valuesEditMode[setIdx] == 3 && setIdx == 0) {
        int writeIdx = RotateIvStep ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = clampVal((y0 + h - mapY) * 8 / h, 0, 7);
        IvStep1[writeIdx] = (uint8_t)v;
        scheduleSaveParams();
        drawIvStepBar(setIdx, idx);
    } else {
        int writeIdx = RotateValues[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = map(mapY, y0 + h, y0, 0, 255);
        v = clampVal(v, 0, 255);
        uint8_t *arr = abEditMode ? ValuesBArr[setIdx] : ValuesArr[setIdx];
        arr[writeIdx] = (uint8_t)v;
        scheduleSaveParams();
        drawValuesBar(setIdx, idx);
    }
}

// Zweck: Behandelt Drag-Events im Values-Screen.
// Side Effects: veraendert Values, schreibt auf TFT, speichert EEPROM.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt.
void handleVALUESDrag(int setIdx, int mapX, int mapY){
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int totalW = 320 - 2 * x0;  // = 300

    if(mapX < x0 || mapX >= (x0 + totalW) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    // Betriebsart: X-Lock (Einzelschritt) oder freies Gleiten (Malen)
    int idx = (!barPaintMode && dragLockIdx >= 0 && dragLockIdx < len)
            ? dragLockIdx
            : clampVal(((mapX - x0) * len + len - 1) / totalW, 0, len - 1);
    if (valuesEditMode[setIdx] == 1) {
        int writeIdx = RotateRatchet[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = clampVal(1 + (y0 + h - mapY) * 4 / h, 1, 4);
        RatchetArr[setIdx][writeIdx] = (uint8_t)v;
        scheduleSaveParams();
        drawRatchetBar(setIdx, idx);
    } else if (valuesEditMode[setIdx] == 2 && setIdx == 0) {
        int writeIdx = RotateOctave[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = clampVal(3 - (mapY - y0) * 7 / h, -3, 3);
        OctaveNote1[writeIdx] = (int8_t)v;
        scheduleSaveParams();
        drawOctaveBar(setIdx, idx);
    } else if (valuesEditMode[setIdx] == 3 && setIdx == 0) {
        int writeIdx = RotateIvStep ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = clampVal((y0 + h - mapY) * 8 / h, 0, 7);
        IvStep1[writeIdx] = (uint8_t)v;
        scheduleSaveParams();
        drawIvStepBar(setIdx, idx);
    } else {
        int writeIdx = RotateValues[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        int v = map(mapY, y0 + h, y0, 0, 255);
        v = clampVal(v, 0, 255);
        uint8_t *arr = abEditMode ? ValuesBArr[setIdx] : ValuesArr[setIdx];
        arr[writeIdx] = (uint8_t)v;
        scheduleSaveParams();
        drawValuesBar(setIdx, idx);
    }
}

// Zweck: Baut den GateLen-Screen fuer ein Pattern auf.
// Side Effects: schreibt auf das TFT und setzt Playhead-Status.
// Assumptions: setIdx in 0..2.
void drawGateLenScreen(int setIdx){
    fillScreenIfNeeded();
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    drawGateHoldCheckbox(setIdx);
    drawRotateGateLenCheckbox(setIdx);
    drawGateLenModeButtons(setIdx);
    drawAbToggleButton();
    drawBarPaintModeIndicator();
    drawCondButton(setIdx);
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
    if (gateLenEditMode[setIdx] == 1) { drawHoldStepBars(setIdx); return; }
    if (gateLenEditMode[setIdx] == 2) { drawMuteStepBars(setIdx); return; }
    int len = clampVal(PatLen[setIdx], 1, 32);
    for(int i=0;i<len;i++){
        drawGateLenBar(setIdx, i);
    }
    drawStepGridLines(240 - 5 - 160, 160, len);
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

    int src = RotateGateLen[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    uint8_t val = abEditMode ? GateLenBArr[setIdx][src] : GateLenArr[setIdx][src];
    int fillH = (int)((val * (long)h) / 255L);
    bool active = patternIsHit(setIdx, idx);
    uint16_t hitCol  = abEditMode ? 0xFD20 : ILI9341_WHITE;
    uint16_t missCol = abEditMode ? 0x8400 : ILI9341_DARKGREY;

    int emptyH = h - fillH;
    if (emptyH > 0) tft.fillRect(x, y0,          xNext - x, emptyH, ILI9341_BLACK);
    if (fillH  > 0) tft.fillRect(x, y0 + emptyH, w,         fillH,  active ? hitCol : missCol);
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_CYAN);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
}

// Per-Step Hold Bar: CYAN wenn Hold an, leer wenn aus
void drawHoldStepBar(int setIdx, int idx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10, y0 = 240 - 5 - 160, h = 160;
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;
    int src = RotateHoldStep[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    bool holdOn = (bool)HoldStepArr[setIdx][src];
    bool active = patternIsHit(setIdx, idx);
    tft.fillRect(x, y0, xNext - x, h, ILI9341_BLACK);
    if (holdOn) {
        int fillH = h * 3 / 4;
        tft.fillRect(x, y0 + (h - fillH), w, fillH,
                     active ? ILI9341_CYAN : 0x0318);  // Cyan / Dunkelcyan
    }
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_YELLOW);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
}
void drawHoldStepBars(int setIdx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    for (int i = 0; i < len; i++) drawHoldStepBar(setIdx, i);
    drawStepGridLines(240 - 5 - 160, 160, len);
}

// Per-Step Mute Bar: ROT wenn Mute an, leer wenn aus
void drawMuteStepBar(int setIdx, int idx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10, y0 = 240 - 5 - 160, h = 160;
    int totalW = 320 - 2 * x0;
    int x     = x0 + (idx       * totalW) / len;
    int xNext = x0 + ((idx + 1) * totalW) / len;
    int w     = xNext - x - 1;
    int src = RotateMuteStep[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    bool muteOn = (bool)MuteStepArr[setIdx][src];
    bool active = patternIsHit(setIdx, idx);
    tft.fillRect(x, y0, xNext - x, h, ILI9341_BLACK);
    if (muteOn) {
        int fillH = h * 3 / 4;
        tft.fillRect(x, y0 + (h - fillH), w, fillH,
                     active ? ILI9341_RED : 0x3800);  // Rot / Dunkelrot
    }
    if (valStepEditActive[setIdx] && idx == valStepEditCursor[setIdx])
        tft.drawRect(x, y0, w, h, ILI9341_YELLOW);
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, y0, h, BAR_GRID_COL);
}
void drawMuteStepBars(int setIdx) {
    int len = clampVal(PatLen[setIdx], 1, 32);
    for (int i = 0; i < len; i++) drawMuteStepBar(setIdx, i);
    drawStepGridLines(240 - 5 - 160, 160, len);
}

// Mode-Buttons und Rotate-Checkboxen für Hold/Mute auf dem GateLen-Screen
static void drawRotateHoldStepCheckbox(int setIdx) {
    int x = 54, y = 43, s = 18;
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    if (RotateHoldStep[setIdx]) {
        tft.drawLine(x+3, y+9, x+7, y+14, ILI9341_CYAN);
        tft.drawLine(x+7, y+14, x+14, y+4, ILI9341_CYAN);
    }
}
static void drawRotateMuteStepCheckbox(int setIdx) {
    int x = 124, y = 43, s = 18;
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    if (RotateMuteStep[setIdx]) {
        tft.drawLine(x+3, y+9, x+7, y+14, ILI9341_RED);
        tft.drawLine(x+7, y+14, x+14, y+4, ILI9341_RED);
    }
}
static void drawGateLenModeButtons(int setIdx) {
    // Hold-Button (Position wie Ratchet auf Values-Screen)
    {
        int x = 10, y = 42, w = 40, h = 24;
        bool active = (gateLenEditMode[setIdx] == 1);
        tft.fillRect(x, y, w, h, active ? 0x0318 : ILI9341_BLACK);
        tft.drawRect(x, y, w, h, active ? ILI9341_CYAN : ILI9341_DARKGREY);
        tft.setFont(Arial_12);
        tft.setTextColor(active ? ILI9341_CYAN : ILI9341_LIGHTGREY);
        tft.setCursor(x + 5, y + 6);
        tft.print("HLD");
    }
    drawRotateHoldStepCheckbox(setIdx);
    // Mute-Button (Position wie Octave auf Values-Screen)
    {
        int x = 80, y = 42, w = 40, h = 24;
        bool active = (gateLenEditMode[setIdx] == 2);
        tft.fillRect(x, y, w, h, active ? 0x3800 : ILI9341_BLACK);
        tft.drawRect(x, y, w, h, active ? ILI9341_RED : ILI9341_DARKGREY);
        tft.setFont(Arial_12);
        tft.setTextColor(active ? ILI9341_RED : ILI9341_LIGHTGREY);
        tft.setCursor(x + 4, y + 6);
        tft.print("MUT");
    }
    drawRotateMuteStepCheckbox(setIdx);
}

// Betriebsart-Indikator: "S◄" (Einzelschritt) oder "►P" (Malen), oben rechts im Screen
static void drawBarPaintModeIndicator() {
    // Nur auf VALUES- und GATELEN-Screens anzeigen
    if (GUIState != VALUES1 && GUIState != VALUES2 && GUIState != VALUES3 &&
        GUIState != GATELEN1 && GUIState != GATELEN2 && GUIState != GATELEN3) return;
    int x = 212, y = 10, w = 46, h = 24;
    tft.fillRect(x, y, w, h, ILI9341_BLACK);
    tft.setFont(Arial_10);
    // Linke Seite: "S" (Einzel-Schritt)
    tft.setTextColor(barPaintMode ? ILI9341_DARKGREY : ILI9341_WHITE);
    tft.setCursor(x + 2, y + 7);
    tft.print("S");
    // Trennlinie
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(x + 14, y + 7);
    tft.print("|");
    // Rechte Seite: "P" (Malen)
    tft.setTextColor(barPaintMode ? ILI9341_WHITE : ILI9341_DARKGREY);
    tft.setCursor(x + 24, y + 7);
    tft.print("P");
    // Aktiver Modus: Rahmen
    if (barPaintMode)
        tft.drawRect(x + 20, y, w - 20, h, ILI9341_WHITE);
    else
        tft.drawRect(x, y, 20, h, ILI9341_WHITE);
}

// Zweck: Zeichnet die GateHold-Checkbox.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
void drawGateHoldCheckbox(int setIdx){
    int x = 268;
    int y = 10;
    int s = 24;

    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_10);
    tft.setCursor(x - 24, y + 7);
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
    int x = 268;
    int y = 42;
    int s = 24;

    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_10);
    tft.setCursor(x - 28, y + 7);
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
    dragLockIdx = -1;
    if(tipPos == UL){
        requestNavigateTo((setIdx == 0) ? VALUES1 : (setIdx == 1) ? VALUES2 : VALUES3);
        return;
    }
    if(hitBox(mapX, mapY, 80, 10, 52, 24, 6)){
        requestNavigateTo((setIdx == 0) ? COND1 : (setIdx == 1) ? COND2 : COND3);
        return;
    }
    if(hitBox(mapX, mapY, 150, 10, 50, 24, 6)){
        abEditMode = !abEditMode;
        drawAbToggleButton();
        drawGateLenBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 268, 42, 24, 24, 8)){
        RotateGateLen[setIdx] = !RotateGateLen[setIdx];
        scheduleSaveParams();
        drawRotateGateLenCheckbox(setIdx);
        drawGateLenBars(setIdx);
        return;
    }
    // Hold-Mode-Button
    if(hitBox(mapX, mapY, 10, 42, 40, 24, 6)){
        gateLenEditMode[setIdx] = (gateLenEditMode[setIdx] == 1) ? 0 : 1;
        drawGateLenModeButtons(setIdx);
        drawGateLenBars(setIdx);
        return;
    }
    // Rotate-HoldStep-Checkbox
    if(hitBox(mapX, mapY, 54, 43, 18, 18, 6)){
        RotateHoldStep[setIdx] = !RotateHoldStep[setIdx];
        scheduleSaveParams();
        drawRotateHoldStepCheckbox(setIdx);
        if (gateLenEditMode[setIdx] == 1) drawHoldStepBars(setIdx);
        return;
    }
    // Mute-Mode-Button
    if(hitBox(mapX, mapY, 80, 42, 40, 24, 6)){
        gateLenEditMode[setIdx] = (gateLenEditMode[setIdx] == 2) ? 0 : 2;
        drawGateLenModeButtons(setIdx);
        drawGateLenBars(setIdx);
        return;
    }
    // Rotate-MuteStep-Checkbox
    if(hitBox(mapX, mapY, 124, 43, 18, 18, 6)){
        RotateMuteStep[setIdx] = !RotateMuteStep[setIdx];
        scheduleSaveParams();
        drawRotateMuteStepCheckbox(setIdx);
        if (gateLenEditMode[setIdx] == 2) drawMuteStepBars(setIdx);
        return;
    }
    if(hitBox(mapX, mapY, 268, 10, 24, 24, 8)){
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
    int totalW = 320 - 2 * x0;  // = 300

    if(mapX < x0 || mapX >= (x0 + totalW) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    int idx = clampVal(((mapX - x0) * len + len - 1) / totalW, 0, len - 1);
    dragLockIdx = idx;

    if (gateLenEditMode[setIdx] == 1) {
        // Hold-Modus: Toggle HoldStep
        int src = RotateHoldStep[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        HoldStepArr[setIdx][src] = HoldStepArr[setIdx][src] ? 0 : 1;
        scheduleSaveParams();
        drawHoldStepBar(setIdx, idx);
        return;
    }
    if (gateLenEditMode[setIdx] == 2) {
        // Mute-Modus: Toggle MuteStep
        int src = RotateMuteStep[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
        MuteStepArr[setIdx][src] = MuteStepArr[setIdx][src] ? 0 : 1;
        scheduleSaveParams();
        drawMuteStepBar(setIdx, idx);
        return;
    }

    int writeIdx = RotateGateLen[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    uint8_t *glArr = abEditMode ? GateLenBArr[setIdx] : GateLenArr[setIdx];
    glArr[writeIdx] = (uint8_t)v;
    drawGateLenBar(setIdx, idx);
}

// Zweck: Behandelt Drag-Events im GateLen-Screen.
// Side Effects: veraendert GateLen, schreibt auf TFT.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt.
void handleGATELENDrag(int setIdx, int mapX, int mapY){
    // In Hold/Mute-Modi kein Drag (Boolean toggle nur per Tap)
    if (gateLenEditMode[setIdx] != 0) return;

    int len = clampVal(PatLen[setIdx], 1, 32);
    int x0 = 10;
    int y0 = 240 - 5 - 160;
    int h  = 160;
    int totalW = 320 - 2 * x0;  // = 300

    if(mapX < x0 || mapX >= (x0 + totalW) || mapY < y0 || mapY >= (y0 + h)){
        return;
    }

    // Betriebsart: X-Lock (Einzelschritt) oder freies Gleiten (Malen)
    int idx = (!barPaintMode && dragLockIdx >= 0 && dragLockIdx < len)
            ? dragLockIdx
            : clampVal(((mapX - x0) * len + len - 1) / totalW, 0, len - 1);
    int writeIdx = RotateGateLen[setIdx] ? layerRotatedSrc(setIdx, idx) : layerBaseSrc(setIdx, idx);
    int v = map(mapY, y0 + h, y0, 0, 255);
    v = clampVal(v, 0, 255);
    uint8_t *glArr = abEditMode ? GateLenBArr[setIdx] : GateLenArr[setIdx];
    glArr[writeIdx] = (uint8_t)v;
    drawGateLenBar(setIdx, idx);
}

static int calcPvMaxRaw();  // forward

// Zweck: Zeichnet den XY-Pad-Screen fuer ein Pattern.
// Side Effects: schreibt auf das TFT.
// Assumptions: setIdx in 0..2.
// Berechnet die Pad-Pixelposition eines Steps (Value=X, GateLen=Y invertiert).
static void getXYDotXY(int setIdx, int stepIdx, int &dotX, int &dotY) {
    int vi = RotateValues[setIdx]  ? layerRotatedSrc(setIdx, stepIdx) : layerBaseSrc(setIdx, stepIdx);
    uint8_t *xyVals = abEditMode ? ValuesBArr[setIdx] : ValuesArr[setIdx];
    dotX = 90 + ((int)xyVals[vi] * 179) / 255;
    if (setIdx == 0 && xyPadPitchMode == 4) {
        // RO-Modus: X=Ratchet-Zellmitte, Y=Oktave-Zellmitte
        int rsrc = RotateRatchet[0] ? layerRotatedSrc(0, stepIdx) : layerBaseSrc(0, stepIdx);
        int osrc = RotateOctave[0]  ? layerRotatedSrc(0, stepIdx) : layerBaseSrc(0, stepIdx);
        int rat = clampVal((int)RatchetArr[0][rsrc], 1, 4);
        int oct = clampVal((int)OctaveNote1[osrc], -3, 3);
        dotX = 90 + (rat - 1) * 45 + 22;
        int ri = 3 - oct;  // Zeilenindex: 0=oben(+3)..6=unten(-3)
        dotY = 40 + (2 * ri + 1) * 90 / 7;
    } else if (setIdx == 0 && xyPadPitchMode > 0) {
        // Dot zeigt tatsächlich klingende Tonhöhe (inkl. pitchShift + OctaveNote1)
        int mr = calcPvMaxRaw();
        int pitchSrc = pitchRotate ? layerRotatedSrc(0, stepIdx) : layerBaseSrc(0, stepIdx);
        int octSrc   = RotateOctave[0] ? layerRotatedSrc(0, stepIdx) : layerBaseSrc(0, stepIdx);
        int shiftSteps = (int)pitchShift + (int)OctaveNote1[octSrc];
        int shiftComp  = shiftSteps * mr / (int)pitchSpread;
        int displayVal = clampVal((int)PitchNote1[pitchSrc] + shiftComp, 0, mr);
        int scaledY = (displayVal * 179) / mr;
        if (scaledY > 179) scaledY = 179;
        dotY = 40 + 179 - scaledY;
    } else {
        int gi = RotateGateLen[setIdx] ? layerRotatedSrc(setIdx, stepIdx) : layerBaseSrc(setIdx, stepIdx);
        uint8_t *xyGL = abEditMode ? GateLenBArr[setIdx] : GateLenArr[setIdx];
        dotY = 40 + 179 - ((int)xyGL[gi] * 179) / 255;
    }
}

static bool getXYDotIsHit(int setIdx, int stepIdx) {
    return patternIsHit(setIdx, stepIdx);
}

// Loescht einen Dot (r=3) und stellt den Hintergrund im betroffenen Bereich wieder her.
static void eraseAndRestoreXYDot(int dotX, int dotY) {
    if (xyKeyboardMode) {
        // Klaviatur-Hintergrund: Scanlines aus Cache wiederherstellen
        static const int halfW[7] = { 0, 2, 2, 3, 2, 2, 0 };  // Halbbreite fuer dy=-3..+3
        for (int i = 0; i < 7; i++) {
            int y = dotY - 3 + i;
            if (y < 40 || y > 219) continue;
            int hw = halfW[i];
            int x0 = dotX - hw, x1 = dotX + hw;
            if (x0 < 91)  x0 = 91;
            if (x1 > 268) x1 = 268;
            if (x0 > x1) continue;
            tft.drawFastHLine(x0, y, x1 - x0 + 1, keyBgCache[y - 40]);
        }
        // Vertikale Gitterlinien im betroffenen Bereich wiederherstellen
        for (int gi = 1; gi < 10; gi++) {
            int gx = 90 + gi * 18;
            int dx = gx - dotX; if (dx < 0) dx = -dx;
            if (dx <= 3) {
                int y0 = dotY - 3; if (y0 < 41)  y0 = 41;
                int y1 = dotY + 3; if (y1 > 218) y1 = 218;
                if (y1 >= y0) tft.drawFastVLine(gx, y0, y1 - y0 + 1, XY_GRID_COLOR);
            }
        }
        return;
    }
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

static void buildKeyboardBgCache();  // forward
static void drawKeyboardBg();        // forward

// Loescht das Pad-Innere und zeichnet den Hintergrund neu (fuer vollstaendigen Dot-Refresh).
static void clearXYPadContent(int setIdx = 0) {
    tft.fillRect(91, 41, 178, 178, ILI9341_BLACK);
    if (setIdx == 0 && xyPadPitchMode == 4) {
        for (int i = 1; i <= 3; i++)
            tft.drawFastVLine(90 + i * 45, 41, 178, XY_GRID_COLOR);
        for (int i = 1; i <= 6; i++)
            tft.drawFastHLine(91, 40 + (i * 180) / 7, 178, XY_GRID_COLOR);
    } else if (setIdx == 0 && xyPadPitchMode > 0) {
        buildKeyboardBgCache();
        drawKeyboardBg();
    } else {
        xyKeyboardMode = false;
        for (int i = 1; i < 10; i++) {
            tft.drawFastVLine(90 + i * 18, 41, 178, XY_GRID_COLOR);
            tft.drawFastHLine(91, 40 + i * 18, 178, XY_GRID_COLOR);
        }
    }
}

// Stellt RO-Gitterlinien (4×7) nach Dot-Loeschung wieder her.
static void eraseAndRestoreXYDotRO(int dotX, int dotY) {
    tft.fillCircle(dotX, dotY, 3, ILI9341_BLACK);
    for (int i = 1; i <= 3; i++) {
        int gx = 90 + i * 45;
        int dx = gx - dotX; if (dx < 0) dx = -dx;
        if (dx <= 3) {
            int y0 = dotY - 3; if (y0 < 41)  y0 = 41;
            int y1 = dotY + 3; if (y1 > 218) y1 = 218;
            if (y1 >= y0) tft.drawFastVLine(gx, y0, y1 - y0 + 1, XY_GRID_COLOR);
        }
    }
    for (int i = 1; i <= 6; i++) {
        int gy = 40 + (i * 180) / 7;
        int dy = gy - dotY; if (dy < 0) dy = -dy;
        if (dy <= 3) {
            int x0 = dotX - 3; if (x0 < 91)  x0 = 91;
            int x1 = dotX + 3; if (x1 > 268) x1 = 268;
            if (x1 >= x0) tft.drawFastHLine(x0, gy, x1 - x0 + 1, XY_GRID_COLOR);
        }
    }
}

// Berechnet das effektive Raw-Maximum fuer den aktuellen PV-Modus.
// PV1/PV3/PV5 zeigen 1/3/5 Oktaven — unabhaengig von pitchSpread.
// Formel: mr = min(255, pvOcts * 255 / pitchSpread)
static int calcPvMaxRaw() {
    static const int pvOcts[] = { 0, 1, 3, 5 };  // index = xyPadPitchMode
    int pvO = pvOcts[xyPadPitchMode];
    int sp  = (pitchSpread > 0) ? (int)pitchSpread : 1;
    int mr  = pvO * 255 / sp;
    return (mr > 255) ? 255 : (mr < 1 ? 1 : mr);
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
        // Gespeicherte Pixel-Position verwenden (nicht neu berechnen), damit Drag-Updates
        // zwischen Ticks den Lösch-Ort nicht verschieben → keine Gelb-Artefakte.
        int dotX = lastYellowPxX[setIdx];
        int dotY = lastYellowPxY[setIdx];
        if (setIdx == 0 && xyPadPitchMode == 4)
            eraseAndRestoreXYDotRO(dotX, dotY);
        else
            eraseAndRestoreXYDot(dotX, dotY);
        uint16_t col = getXYDotIsHit(setIdx, last) ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillCircle(dotX, dotY, 2, col);
    }
    int dotX, dotY;
    getXYDotXY(setIdx, idx, dotX, dotY);
    tft.fillCircle(dotX, dotY, 3, ILI9341_YELLOW);
    lastXYDotIdx[setIdx]  = idx;
    lastYellowPxX[setIdx] = dotX;
    lastYellowPxY[setIdx] = dotY;
}

static void drawXYModeToggle(int setIdx) {
    if (setIdx != 0) return;
    int bx = 270, by = 5, bw = 44, bh = 24;
    static const char* labels[5] = { "PV", "PV1", "PV3", "PV5", "RO" };
    bool roMode = (xyPadPitchMode == 4);
    bool active = (xyPadPitchMode > 0);
    uint16_t borderCol = roMode ? ILI9341_MAGENTA : (active ? ILI9341_GREEN : ILI9341_DARKGREY);
    tft.fillRect(bx, by, bw, bh, (roMode || active) ? ILI9341_BLACK : ILI9341_BLACK);
    tft.drawRect(bx, by, bw, bh, borderCol);
    tft.setFont(Arial_12);
    tft.setTextColor(active ? borderCol : ILI9341_DARKGREY);
    const char *lbl = labels[xyPadPitchMode];
    int lw = (int)strlen(lbl) * 7;
    tft.setCursor(bx + (bw - lw) / 2, by + 6);
    tft.print(lbl);
}

// Baut den Klaviatur-Hintergrund-Cache auf (ein Farbwert pro Y-Pixel 40..219).
// Weisstöne → ILI9341_LIGHTGREY, Halbtöne → 0x4208, Trennlinie → BLACK/GREEN.
static void buildKeyboardBgCache() {
    xyKeyboardMode = (xyPadPitchMode > 0 && xyPadPitchMode < 4);
    if (!xyKeyboardMode) return;
    for (int i = 0; i < 180; i++) keyBgCache[i] = ILI9341_BLACK;

    int noteList[60];
    int noteCount = buildNoteList(pitchSpread, pitchScale, pitchRoot,
                                  pitchIntervalMask, noteList);
    if (noteCount == 0) return;

    int mr = calcPvMaxRaw();

    // Iteriere Y von oben (hohe Tonhöhe) nach unten (tiefe Tonhöhe)
    int prevK = -1;
    for (int y = 40; y <= 219; y++) {
        int idx = y - 40;
        int raw = (219 - y) * mr / 179;
        if (raw < 0)   raw = 0;
        if (raw > 255) raw = 255;

        int k = (raw * noteCount) / 256;
        if (k < 0)          k = 0;
        if (k >= noteCount) k = noteCount - 1;

        // Übergang: von Note prevK (oben) zu Note k (unten) → Trennlinie
        bool isBoundary = (prevK >= 0 && k < prevK);
        if (isBoundary) {
            bool isRoot = ((noteList[prevK] % 12) == (pitchRoot % 12));
            keyBgCache[idx] = isRoot ? ILI9341_GREEN : ILI9341_BLACK;
        } else {
            int nc = noteList[k] % 12;
            bool isBlackKey = (nc==1||nc==3||nc==6||nc==8||nc==10);
            keyBgCache[idx] = isBlackKey ? 0x4208 : ILI9341_LIGHTGREY;
        }
        prevK = k;
    }
}

// Zeichnet den Klaviatur-Hintergrund aus dem Cache (Run-Length-optimiert).
// Zeichnet danach vertikale Gitterlinien für den Value-Bezug darüber.
static void drawKeyboardBg() {
    if (!xyKeyboardMode) return;
    // Streifen mit Run-Length-Encoding zeichnen (spart SPI-Transaktionen)
    int runStart = 0;
    uint16_t runColor = keyBgCache[0];
    for (int idx = 1; idx <= 180; idx++) {
        uint16_t c = (idx < 180) ? keyBgCache[idx] : (uint16_t)(~runColor);
        if (c != runColor) {
            tft.fillRect(91, 40 + runStart, 178, idx - runStart, runColor);
            runStart = idx;
            runColor = c;
        }
    }
    // Vertikale Gitterlinien (Value-Bezug) oben drüber
    for (int i = 1; i < 10; i++)
        tft.drawFastVLine(90 + i * 18, 41, 178, XY_GRID_COLOR);
}

void drawXYPadScreen(int setIdx){
    fillScreenIfNeeded();
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);

    int x = 90;
    int y = 40;
    int w = 180;
    int h = 180;
    bool roMode = (setIdx == 0 && xyPadPitchMode == 4);
    tft.drawRect(x, y, w, h, roMode ? ILI9341_MAGENTA : ILI9341_DARKGREY);

    if (roMode) {
        // 4 Ratchet-Spalten × 7 Oktave-Zeilen
        for (int i = 1; i <= 3; i++)
            tft.drawFastVLine(x + i * 45, y + 1, h - 2, XY_GRID_COLOR);
        for (int i = 1; i <= 6; i++)
            tft.drawFastHLine(x + 1, y + (i * h) / 7, w - 2, XY_GRID_COLOR);
        // Spaltenbeschriftung (Ratchet 1-4) oberhalb des Pads
        tft.setFont(Arial_12);
        tft.setTextColor(ILI9341_DARKGREY);
        for (int r = 0; r < 4; r++)
            tft.setCursor(x + r * 45 + 19, y - 12), tft.print(r + 1);
        // Zeilenbeschriftung (Oktave +3..-3) links
        static const char* octLbls[7] = {"+3","+2","+1"," 0","-1","-2","-3"};
        for (int ri = 0; ri < 7; ri++) {
            int rowCy = y + (2 * ri + 1) * 90 / 7;
            tft.setCursor(68, rowCy - 6);
            tft.print(octLbls[ri]);
        }
    } else if (setIdx == 0 && xyPadPitchMode > 0) {
        // Klaviatur-Hintergrund fuer PV1/PV3/PV5
        buildKeyboardBgCache();
        drawKeyboardBg();
    } else {
        // 10×10 Raster (GateLen oder Ch2/Ch3)
        xyKeyboardMode = false;
        for (int i = 1; i < 10; i++) {
            tft.drawFastVLine(x + (i * w) / 10, y + 1, h - 2, XY_GRID_COLOR);
            tft.drawFastHLine(x + 1, y + (i * h) / 10, w - 2, XY_GRID_COLOR);
        }
    }

    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(roMode ? x + 52 : x + 60, y + h + 8);
    tft.print(roMode ? "Ratchet" : "Value");

    static const char* yLabels[5] = { "Gatelength", "1Oct Pitch", "3Oct Pitch", "5Oct Pitch", "Octave    " };
    int yLabelIdx = (setIdx == 0 && xyPadPitchMode > 0) ? xyPadPitchMode : 0;
    drawVerticalLabel(x - 20, y + 34, yLabels[yLabelIdx]);

    drawXYModeToggle(setIdx);
    if (setIdx == 0) drawAbToggleButton();

    resetXYPlayhead(setIdx);
    drawXYPlayhead(setIdx, cntCh[setIdx]);
    lastXYDotIdx[setIdx]  = -1;
    lastYellowPxX[setIdx] = -1;
    lastYellowPxY[setIdx] = -1;
    drawXYDots(setIdx);
    drawXYDotPlayhead(setIdx, cntCh[setIdx]);
  }

// Zweck: Behandelt Touch-Events im XY-Pad-Screen.
// Side Effects: wechselt GUIState und schreibt auf das TFT.
// Assumptions: setIdx in 0..2; mapX/mapY sind gemappt; tipPos ist gueltig.
// Behandelt Button-Taps auf dem XY-Screen (sofort beim Erst-Touch aufrufen).
// Gibt true zurück wenn ein Button getroffen wurde (kein Wert-Schreiben nötig).
bool handleXYPAD(int setIdx, int mapX, int mapY, uint16_t tipPos){
    if(tipPos == UL){
        requestNavigateTo((setIdx == 0) ? EUCLPARAM1 : (setIdx == 1) ? EUCLPARAM2 : EUCLPARAM3);
        return true;
    }
    // PV-Mode-Toggle (nur Kanal 1, oben rechts): 0→1→2→3→0
    if (setIdx == 0 && hitBox(mapX, mapY, 270, 5, 44, 24, 8)) {
        xyPadPitchMode = (xyPadPitchMode + 1) % 5;
        requestNavigateTo((setIdx == 0) ? XY1 : (setIdx == 1) ? XY2 : XY3);
        return true;
    }
    // A/B-Toggle (Kanal 1): Vollbild-Redraw damit Dots in korrekter Farbe/Position
    if (setIdx == 0 && hitBox(mapX, mapY, 150, 10, 50, 24, 8)) {
        abEditMode = !abEditMode;
        requestNavigateTo(XY1);
        return true;
    }
    return false;
}

// Schreibt Werte tick-synchron (aufrufen wenn Sequencer-Tick + Touch aktiv).
void handleXYPADRecord(int setIdx, int mapX, int mapY, bool drawDot){
    int x = 90;
    int y = 40;
    int w = 180;
    int h = 180;
    if(mapX < x || mapX >= (x + w) || mapY < y || mapY >= (y + h)){
        return;
    }

    int len = clampVal(PatLen[setIdx], 1, 32);
    int idx = cntCh[setIdx] % len;

    // Dot-Position VOR der Wert-Änderung merken (für gezieltes Löschen)
    int oldDotX, oldDotY;
    getXYDotXY(setIdx, idx, oldDotX, oldDotY);

    // Basis-Rotation (ohne PatRotSel) und Selektion-Rotation — müssen mit
    // outputValuesForStep/gateLenForStep/getXYDotXY übereinstimmen.
    // Auch wenn das Rotate-Flag nicht gesetzt ist, wird die Basis-Rotation (effRot)
    // immer angewendet, damit Schreib- und Leseindex identisch sind.
    int effRot    = clampVal(PatRot[setIdx] + (int)cvPatRotOffset[setIdx], -(len - 1), len - 1);
    int effRotSel = clampVal(PatRot[setIdx] + PatRotSel[setIdx] + (int)cvPatRotOffset[setIdx],
                             -(len - 1), len - 1);
    if (setIdx == 0 && xyPadPitchMode == 4) {
        // RO-Modus: X→Ratchet (1-4), Y→Oktave (-3..+3)
        int rat = (mapX - x) * 4 / w + 1;
        rat = clampVal(rat, 1, 4);
        int oct = (y + h - 1 - mapY) * 7 / h - 3;  // unten=-3, oben=+3
        oct = clampVal(oct, -3, 3);
        int rWriteIdx = RotateRatchet[0] ? euclidRotatedSrc(idx, len, effRotSel)
                                         : euclidRotatedSrc(idx, len, effRot);
        int oWriteIdx = RotateOctave[0]  ? euclidRotatedSrc(idx, len, effRotSel)
                                         : euclidRotatedSrc(idx, len, effRot);
        RatchetArr[0][rWriteIdx] = (uint8_t)rat;
        OctaveNote1[oWriteIdx] = (int8_t)oct;
    } else {
        int writeValIdx = RotateValues[setIdx] ? euclidRotatedSrc(idx, len, effRotSel)
                                               : euclidRotatedSrc(idx, len, effRot);
        int v = map(mapX, x, x + w - 1, 0, 255);
        v = clampVal(v, 0, 255);
        uint8_t *xyValArr = abEditMode ? ValuesBArr[setIdx] : ValuesArr[setIdx];
        xyValArr[writeValIdx] = (uint8_t)v;
        if (setIdx == 0 && xyPadPitchMode > 0) {
            int mr = calcPvMaxRaw();
            int g = map(mapY, y + h - 1, y, 0, mr);
            g = clampVal(g, 0, mr);
            // pitchShift + OctaveNote1 kompensieren: Y-Position = tatsächlich klingende Tonhöhe
            {
                int octSrc = RotateOctave[0] ? euclidRotatedSrc(idx, len, effRotSel)
                                             : euclidRotatedSrc(idx, len, effRot);
                int shiftSteps = (int)pitchShift + (int)OctaveNote1[octSrc];
                int shiftComp  = shiftSteps * mr / (int)pitchSpread;
                g = clampVal(g - shiftComp, 0, mr);
            }
            // Y-Position auf nächste Skalennote quantisieren
            {
                int noteList[60];
                int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot,
                                       pitchIntervalMask, noteList);
                if (nc > 0) {
                    int noteIdx = clampVal((int)g * nc / 256, 0, nc - 1);
                    g = clampVal((noteIdx * 256 + 128) / nc, 0, 255);
                }
            }
            int pitchWriteIdx = pitchRotate ? euclidRotatedSrc(idx, len, effRotSel)
                                            : euclidRotatedSrc(idx, len, effRot);
            PitchNote1[pitchWriteIdx] = (uint8_t)g;
        } else {
            int g = map(mapY, y + h - 1, y, 0, 255);
            g = clampVal(g, 0, 255);
            int writeGateIdx = RotateGateLen[setIdx] ? euclidRotatedSrc(idx, len, effRotSel)
                                                     : euclidRotatedSrc(idx, len, effRot);
            uint8_t *xyGLArr = abEditMode ? GateLenBArr[setIdx] : GateLenArr[setIdx];
            xyGLArr[writeGateIdx] = (uint8_t)g;
        }
    }

    // DAC-Update: in Pitch-Modi (xyPadPitchMode>0, ch0) KEIN sofortiger Update —
    // Pitch-CV wird ausschließlich über die Tick-Schleife ausgegeben (pitchHold greift dort).
    // In reinen Values/GateLen-Modi (xyPadPitchMode==0 oder ch1/ch2) live aktualisieren.
    if (xyPadPitchMode == 0 || setIdx != 0) {
        outputValuesForStep(0);
    }

    if (drawDot) {
        // Neue Dot-Position NACH der Wert-Änderung berechnen
        int newDotX, newDotY;
        getXYDotXY(setIdx, idx, newDotX, newDotY);

        // Gezieltes Dot-Update: nur geänderten Step neu zeichnen (~2ms statt ~25ms).
        // drawXYDotPlayhead im switch-case kümmert sich um den gelben Playhead-Dot.
        if (setIdx == 0 && xyPadPitchMode == 4)
            eraseAndRestoreXYDotRO(oldDotX, oldDotY);
        else
            eraseAndRestoreXYDot(oldDotX, oldDotY);
        uint16_t col = getXYDotIsHit(setIdx, idx) ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillCircle(newDotX, newDotY, 2, col);
    }
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

    if (encParamSel[ch] == 5) {
        static const char* speedLabels[7] = { "/4", "/3", "/2", "x1", "x2", "x3", "x4" };
        int si = clampVal(chSpeedIdx[ch] + 3, 0, 6);
        tft.print("S");
        snprintf(buf, sizeof(buf), "%s", speedLabels[si]);
    } else {
        static const char *letters[5] = { "L", "H", "R", "r", "P" };
        int val;
        switch (encParamSel[ch]) {
            case 0: val = PatLen[ch];          break;
            case 1: val = PatNum[ch];          break;
            case 2: val = PatRot[ch];          break;
            case 3: val = PatRotSel[ch];       break;
            default: val = (int)PatProb[ch];   break;
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
    bool selected = (encParamSel[ch] == 5);
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
    // Columns 0..3 map to enc values L=0, H=1, R=2, P=4 (r=3 has no dedicated button)
    static const int COL_TO_ENC[4] = { 0, 1, 2, 4 };
    for (int col = 0; col < 4; col++) {
        uint16_t c = (COL_TO_ENC[col] == encParamSel[ch]) ? ILI9341_RED : ILI9341_DARKGREY;
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
    if (prev >= 0 && prev < 16) drawPerfSlotBox(prev);
    if (slot >= 0 && slot < 16) drawPerfSlotBox(slot);
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

// Pitch Hold / Rotate / Display-Mode Checkboxen — wie auf main
static const int PITCH_HOLD_BX = 260;
static const int PITCH_HOLD_BY = 10;
static const int PITCH_HOLD_BS = 24;
static const int PITCH_ROT_BX  = 260;
static const int PITCH_ROT_BY  = 42;
static const int PITCH_ROT_BS  = 24;
static const int PITCH_DP_BX   = 185;  // eigene X-Position (nicht überlagert mit Rotate)
static const int PITCH_DP_BY   = 42;
static const int PITCH_DP_BS   = 24;
// V1- und CORD-Button (nebeneinander, Y 10–33) — etwas weiter links als zuvor
static const int PITCH_V1_BX   = 100;
static const int PITCH_V1_BY   = 10;
static const int PITCH_V1_BW   = 44;
static const int PITCH_V1_BH   = 24;
static const int PITCH_CORD_BX = 148;
static const int PITCH_CORD_BY = 10;
static const int PITCH_CORD_BW = 44;
static const int PITCH_CORD_BH = 24;
// Faltungs-Box (rechtsbündig mit Preset-Box, darüber)
static const int PITCH_FOLD_BX = 70;
static const int PITCH_FOLD_BY = 14;
static const int PITCH_FOLD_BW = 60;
static const int PITCH_FOLD_BH = 22;

static bool pitchDisplayMode  = false;
static bool pitchChordMode    = false;
static int  pitchChordIdx     = 0;
static bool stepEditActive      = false;
static int  stepEditCursor      = 0;
static bool stepEditChromatic   = false;
static bool stepLabelShowOctave = false;
static int  lastPitchPlayIdx  = -1;

static const int PITCH_NLB_X  = PITCH_BAR_X + 2;
static const int PITCH_NLB_Y  = PITCH_BAR_Y + 2;
static const int PITCH_NLB_W  = 80;
static const int PITCH_NLB_H  = 16;

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

    uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
    int effIdx = foldPitchIdx(idx, len, effFold);
    int src    = pitchRotate ? layerRotatedSrc(0, effIdx) : effIdx;
    bool hit   = patternIsHit(0, idx);

    int bottom = PITCH_BAR_Y + PITCH_BAR_H;
    bool isCursor = stepEditActive && (idx == stepEditCursor);

    if (pitchDisplayMode) {
        int fillH  = (int)((PitchNote1[src] * (long)PITCH_BAR_H) / 255L);
        int emptyH = PITCH_BAR_H - fillH;
        if (emptyH > 0) tft.fillRect(x, PITCH_BAR_Y, xN - x, emptyH, ILI9341_BLACK);
        if (fillH > 0) {
            int midiQ = quantizeToMidi(PitchNote1[src], pitchSpread, pitchScale,
                                       pitchRoot, pitchIntervalMask);
            bool isRoot = ((midiQ % 12) == (int)pitchRoot);
            uint16_t col = hit ? (isCursor ? ILI9341_YELLOW : (isRoot ? ILI9341_ORANGE : ILI9341_WHITE))
                               : (isCursor ? 0x8400          : (isRoot ? 0x6200         : 0x4208));
            tft.fillRect(x, bottom - fillH, w, fillH, col);
        }
    } else {
        int midi = pitchNotesFrozen
            ? frozenMidi[src]
            : quantizeToMidi(PitchNote1[src], pitchSpread, pitchScale,
                             pitchRoot, pitchIntervalMask);
        midi = clampVal(midi + ((int)pitchShift + (int)OctaveNote1[src]) * 12, 36, 96);
        bool isRoot = ((midi % 12) == (int)pitchRoot);
        int noteY  = pitchNoteToBarY(midi);
        int fillH  = bottom - noteY;
        int emptyH = PITCH_BAR_H - fillH;  // = noteY - PITCH_BAR_Y
        if (emptyH > 0) tft.fillRect(x, PITCH_BAR_Y, xN - x, emptyH, ILI9341_BLACK);
        if (fillH > 0) {
            uint16_t col = hit ? (isCursor ? ILI9341_YELLOW : (isRoot ? ILI9341_ORANGE : ILI9341_WHITE))
                               : (isCursor ? 0x8400          : (isRoot ? 0x6200         : 0x4208));
            tft.fillRect(x, noteY, w, fillH, col);
        }
        // Restore octave grid lines across the full bar height.
        static const int PITCH_OCT[] = { 48, 60, 72, 84 };
        for (int i = 0; i < 4; i++) {
            int gy = pitchNoteToBarY(PITCH_OCT[i]);
            if (gy >= PITCH_BAR_Y && gy < bottom) {
                tft.drawFastHLine(x, gy, xN - x, PITCH_GRID_COL);
            }
        }
    }
    if (idx > 0 && idx % 4 == 0) tft.drawFastVLine(x, PITCH_BAR_Y, PITCH_BAR_H, BAR_GRID_COL);
}

void drawPitchBars() {
    int len = clampVal(PatLen[0], 1, 32);
    for (int i = 0; i < len; i++) {
        drawPitchBar(i);
    }
    drawStepGridLines(PITCH_BAR_Y, PITCH_BAR_H, len);
    drawTransposeIndicator();
}

void drawPitchBarRange(int from, int to) {
    int len = clampVal(PatLen[0], 1, 32);
    for (int i = from; i < to && i < len; i++) {
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
    tft.fillRect(x - 26, y, 25, s, ILI9341_BLACK);  // Label-Bereich leeren
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 20, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("HP");
    if (pitchHold) {
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

void drawPitchRotateCheckbox() {
    int x = PITCH_ROT_BX, y = PITCH_ROT_BY, s = PITCH_ROT_BS;
    tft.fillRect(x - 26, y, 25, s, ILI9341_BLACK);  // Label-Bereich leeren
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 20, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("PR");
    if (pitchRotate) {
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

void drawPitchDisplayModeCheckbox() {
    int x = PITCH_DP_BX, y = PITCH_DP_BY, s = PITCH_DP_BS;
    tft.fillRect(x - 26, y, 25, s, ILI9341_BLACK);  // Label-Bereich leeren
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x+1, y+1, s-2, s-2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(x - 20, y + 6);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("DW");
    if (pitchDisplayMode) {
        tft.drawLine(x+4, y+12, x+10, y+18, ILI9341_GREEN);
        tft.drawLine(x+10, y+18, x+20, y+6, ILI9341_GREEN);
    }
}

void drawPitchControls() {
    static const int SC_X = 0,   SC_W = 100;
    static const int RT_X = 102, RT_W = 38;
    static const int SP_X = 142, SP_W = 36;
    static const int IV_X = 180, IV_W = 28;
    static const int AI_X = 210, AI_W = 28;
    static const int SH_X = 240, SH_W = 38;
    static const int TP_X = 280, TP_W = 40;
    static const char *const ROOT_NAMES[12] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    int  boxCursor = getPitchBoxCursor();
    bool boxEdit   = getPitchBoxEditMode();
    uint16_t scBorder = (boxCursor == 0) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t rtBorder = (boxCursor == 1) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t spBorder = (boxCursor == 2) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t ivBorder = (boxCursor == 3) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t aiBorder = (boxCursor == 4) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;
    uint16_t tpBorder = (boxCursor == 5) ? (boxEdit ? ILI9341_YELLOW : ILI9341_RED) : ILI9341_DARKGREY;

    tft.drawRect(SC_X, PITCH_CTRL_Y, SC_W, PITCH_CTRL_H, scBorder);
    tft.fillRect(SC_X+1, PITCH_CTRL_Y+1, SC_W-2, PITCH_CTRL_H-2,
                 pitchChordMode ? 0x000F : ILI9341_BLACK);  // dark blue in chord mode
    tft.setFont(Arial_12);
    tft.setTextColor(pitchChordMode ? ILI9341_CYAN : ILI9341_LIGHTGREY);
    tft.setCursor(SC_X+3, PITCH_CTRL_Y+5);
    if (pitchChordMode) tft.print(getChordName(pitchChordIdx));
    else                tft.print(getScaleName(pitchScale));

    tft.drawRect(RT_X, PITCH_CTRL_Y, RT_W, PITCH_CTRL_H, rtBorder);
    tft.fillRect(RT_X+1, PITCH_CTRL_Y+1, RT_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(RT_X+3, PITCH_CTRL_Y+5);
    tft.print(ROOT_NAMES[pitchRoot % 12]);

    tft.drawRect(SP_X, PITCH_CTRL_Y, SP_W, PITCH_CTRL_H, spBorder);
    tft.fillRect(SP_X+1, PITCH_CTRL_Y+1, SP_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(SP_X+3, PITCH_CTRL_Y+5);
    tft.printf("Sp%d", pitchSpread);

    tft.drawRect(IV_X, PITCH_CTRL_Y, IV_W, PITCH_CTRL_H, ivBorder);
    tft.fillRect(IV_X+1, PITCH_CTRL_Y+1, IV_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(IV_X+4, PITCH_CTRL_Y+5);
    tft.print("IV");

    tft.drawRect(AI_X, PITCH_CTRL_Y, AI_W, PITCH_CTRL_H, aiBorder);
    tft.fillRect(AI_X+1, PITCH_CTRL_Y+1, AI_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(AI_X+4, PITCH_CTRL_Y+5);
    tft.print("AI");

    tft.drawRect(SH_X, PITCH_CTRL_Y, SH_W, PITCH_CTRL_H, ILI9341_DARKGREY);
    tft.fillRect(SH_X+1, PITCH_CTRL_Y+1, SH_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(SH_X+3, PITCH_CTRL_Y+5);
    if (pitchShift >= 0) tft.printf("Sf+%d", (int)pitchShift);
    else                 tft.printf("Sf%d",  (int)pitchShift);

    tft.drawRect(TP_X, PITCH_CTRL_Y, TP_W, PITCH_CTRL_H, tpBorder);
    tft.fillRect(TP_X+1, PITCH_CTRL_Y+1, TP_W-2, PITCH_CTRL_H-2, ILI9341_BLACK);
    tft.setCursor(TP_X+8, PITCH_CTRL_Y+5);
    tft.print("Tp");

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
    static const char* foldNames[13] = {
        "off",
        "m H1", "r H1", "m Q1", "r Q1",
        "m H2", "r H2",
        "m Q2", "m Q3", "m Q4",
        "r Q2", "r Q3", "r Q4"
    };
    bool cvActive = (cvPitchFold >= 0);
    uint8_t effFold = cvActive ? (uint8_t)cvPitchFold : pitchFoldMode;
    const char *label = foldNames[clampVal((int)effFold, 0, 12)];
    uint16_t col;
    if (cvActive)             col = ILI9341_CYAN;
    else if (pitchFoldMode > 0) col = ILI9341_GREEN;
    else                        col = ILI9341_DARKGREY;
    tft.fillRect(PITCH_FOLD_BX + 1, PITCH_FOLD_BY + 1,
                 PITCH_FOLD_BW - 2, PITCH_FOLD_BH - 2, ILI9341_BLACK);
    tft.drawRect(PITCH_FOLD_BX, PITCH_FOLD_BY, PITCH_FOLD_BW, PITCH_FOLD_BH, col);
    tft.setFont(Arial_12);
    tft.setTextColor(col == ILI9341_DARKGREY ? ILI9341_LIGHTGREY : col);
    int labelW = (int)strlen(label) * 7;
    tft.setCursor(PITCH_FOLD_BX + (PITCH_FOLD_BW - labelW) / 2, PITCH_FOLD_BY + 4);
    tft.print(label);
}

void tickPitchUi() {
    static int8_t lastCvPitchFoldTick = -2;
    if (cvPitchFold != lastCvPitchFoldTick) {
        lastCvPitchFoldTick = cvPitchFold;
        drawPitchFoldBox();
        pendingPitchDraw = true;  // drawPitchBars deferred: don't block tick loop
    }
}

static void drawPitchStepNoteLabel() {
    static const char* const NOTE_NAMES[12] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    int len = clampVal(PatLen[0], 1, 32);
    uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
    int effIdx = foldPitchIdx(stepEditCursor, len, effFold);
    int src    = pitchRotate ? layerRotatedSrc(0, effIdx) : effIdx;
    int octSrc = RotateOctave[0] ? layerRotatedSrc(0, stepEditCursor) : stepEditCursor;
    int midi   = quantizeToMidi(PitchNote1[src], pitchSpread, pitchScale,
                                pitchRoot, pitchIntervalMask);
    midi = clampVal(midi + ((int)pitchShift + (int)OctaveNote1[octSrc]) * 12, 0, 127);
    tft.fillRect(PITCH_NLB_X, PITCH_NLB_Y, PITCH_NLB_W, PITCH_NLB_H, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(PITCH_NLB_X + 2, PITCH_NLB_Y + 3);
    if (stepLabelShowOctave) {
        int octSrcOct = RotateOctave[0] ? layerRotatedSrc(0, stepEditCursor) : stepEditCursor;
        tft.setTextColor(ILI9341_GREEN);
        tft.printf("S%d:o%+d", stepEditCursor + 1, (int)OctaveNote1[octSrcOct]);
    } else {
        tft.setTextColor(stepEditChromatic ? ILI9341_CYAN : ILI9341_YELLOW);
        tft.printf("S%d:%s%d", stepEditCursor + 1,
                   NOTE_NAMES[midi % 12], midi / 12 - 1);
    }
}

bool getPitchStepEditActive()  { return stepEditActive; }
int  getPitchStepEditCursor()  { return stepEditCursor; }

void togglePitchStepEdit() {
    stepEditActive = !stepEditActive;
    if (stepEditActive) {
        int len = clampVal(PatLen[0], 1, 32);
        stepEditCursor = clampVal(stepEditCursor, 0, len - 1);
        drawPitchStepNoteLabel();
    } else {
        // Label liegt in der Balkenbox — betroffene Balken neu zeichnen
        stepLabelShowOctave = false;
        int len = clampVal(PatLen[0], 1, 32);
        int lastBar = ((PITCH_NLB_X + PITCH_NLB_W - PITCH_BAR_X) * len) / PITCH_BAR_W;
        for (int i = 0; i <= lastBar && i < len; i++) drawPitchBar(i);
    }
    drawPitchBar(stepEditCursor);
}

void movePitchStepCursor(int delta) {
    if (!stepEditActive) return;
    int len  = clampVal(PatLen[0], 1, 32);
    int prev = stepEditCursor;
    stepEditCursor = ((stepEditCursor + delta) % len + len) % len;
    if (stepEditCursor == prev) return;
    stepLabelShowOctave = false;
    drawPitchBar(prev);
    drawPitchBar(stepEditCursor);
    drawPitchStepNoteLabel();
}

void adjustPitchStepNote(int delta) {
    if (!stepEditActive) return;
    int len = clampVal(PatLen[0], 1, 32);
    uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
    int effIdx = foldPitchIdx(stepEditCursor, len, effFold);
    int src    = pitchRotate ? layerRotatedSrc(0, effIdx) : effIdx;
    if (stepEditChromatic) {
        int totalSt = (int)pitchSpread * 12;
        int st_old  = ((int)PitchNote1[src] * totalSt) / 256;
        int st_new  = clampVal(st_old + delta, 0, totalSt - 1);
        if (st_new != st_old)
            PitchNote1[src] = (uint8_t)((st_new * 256 + 128) / totalSt);
        else if (delta > 0) PitchNote1[src] = 255;
        else if (delta < 0) PitchNote1[src] = 0;
    } else {
        int noteList[60];
        int nc = buildNoteList(pitchSpread, pitchScale, pitchRoot,
                               pitchIntervalMask, noteList);
        if (nc == 0) return;
        int k_old = clampVal(((int)PitchNote1[src] * nc) / 256, 0, nc - 1);
        int k_new = clampVal(k_old + delta, 0, nc - 1);
        if (k_new != k_old)
            PitchNote1[src] = (uint8_t)clampVal((k_new * 256 + 128) / nc, 0, 255);
    }
    scheduleSaveParams();
    stepLabelShowOctave = false;
    drawPitchBar(stepEditCursor);
    drawPitchStepNoteLabel();
}

void adjustPitchStepOctave(int delta) {
    if (!stepEditActive) return;
    int octSrc = RotateOctave[0] ? layerRotatedSrc(0, stepEditCursor) : stepEditCursor;
    OctaveNote1[octSrc] = (int8_t)clampVal((int)OctaveNote1[octSrc] + delta, -3, 3);
    scheduleSaveParams();
    stepLabelShowOctave = true;
    drawPitchStepNoteLabel();
}

void togglePitchStepChromatic() {
    if (!stepEditActive) return;
    stepEditChromatic = !stepEditChromatic;
    drawPitchStepNoteLabel();
}

bool getPitchChordMode() { return pitchChordMode; }

void flashPitchBars() {
    // Flackern entfernt — kein visuelles Feedback mehr beim VLP-Umschalten
}

static int findBestChordMatch(uint8_t scaleIdx, uint8_t iMask) {
    int bestIdx = 0, bestScore = -1;
    for (int i = 0; i < CHORD_COUNT; i++) {
        uint8_t cs, cm;
        getChordPreset(i, cs, cm);
        if (cs == scaleIdx && cm == iMask) return i;  // exakter Treffer
        int score = (cs == scaleIdx ? 50 : 0);
        uint8_t both = cm & iMask;
        for (int b = 0; b < 7; b++) if (both & (1u << b)) score++;
        if (score > bestScore) { bestScore = score; bestIdx = i; }
    }
    return bestIdx;
}

void togglePitchChordMode() {
    pitchChordMode = !pitchChordMode;
    if (pitchChordMode) {
        pitchChordIdx = findBestChordMatch(pitchScale, pitchIntervalMask);
        // scale+mask bleiben unverändert — Chord-Name ist nur Label für aktuellen Zustand
    }
    pendingPitchDraw = true;
}

void movePitchChordIdx(int delta) {
    pitchChordIdx = ((pitchChordIdx + delta) % CHORD_COUNT + CHORD_COUNT) % CHORD_COUNT;
    uint8_t sc, mask;
    getChordPreset(pitchChordIdx, sc, mask);
    pitchScale = sc; pitchIntervalMask = mask;
    scheduleSaveParams();
    pendingPitchDraw = true;
}

// IV: Inversions-Selector — wählt Inversion 0..N-1, setzt alle Hit-Steps gleichzeitig.
// Noten mit Rang < ivIdx kommen 1 Oktave höher (OctaveNote1=1), Rest bleibt (OctaveNote1=0).
void invertPitchSequence(int delta) {
    int len = clampVal(PatLen[0], 1, 32);

    // Basis-MIDIs aller Hit-Steps sammeln (bei OctaveNote1=0)
    int hitIdx[32];
    int baseMidi[32];
    int hitCount = 0;
    for (int i = 0; i < len; i++) {
        if (!patternIsHit(0, i)) continue;
        hitIdx[hitCount]   = i;
        baseMidi[hitCount] = quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                                            pitchRoot, pitchIntervalMask);
        hitCount++;
    }
    if (hitCount == 0) return;

    // Einzigartige Basis-MIDIs sortiert sammeln
    int unique[32];
    int N = 0;
    for (int i = 0; i < hitCount; i++) {
        bool found = false;
        for (int j = 0; j < N; j++) if (unique[j] == baseMidi[i]) { found = true; break; }
        if (!found) unique[N++] = baseMidi[i];
    }
    for (int i = 0; i < N - 1; i++)
        for (int j = i + 1; j < N; j++)
            if (unique[j] < unique[i]) { int t = unique[i]; unique[i] = unique[j]; unique[j] = t; }

    if (N <= 1) return;  // nur ein Ton → keine Inversion möglich

    ivInversionIdx = ((ivInversionIdx + delta) % N + N) % N;

    // OctaveNote1 aller Hit-Steps setzen: Rang < ivIdx → +1 Oktave, sonst 0
    for (int s = 0; s < hitCount; s++) {
        int rank = 0;
        for (int j = 0; j < N; j++) if (unique[j] == baseMidi[s]) { rank = j; break; }
        OctaveNote1[hitIdx[s]] = (int8_t)(rank < ivInversionIdx ? 1 : 0);
    }

    scheduleSaveParams();
    pendingPitchDraw = true;
}

// AI: Alle Steps auf dem tiefsten (dir>0) bzw. höchsten (dir<0) Niveau gleichzeitig
// um ±1 Oktave verschieben — nicht Note für Note, sondern alle auf einmal.
void aInvPitchSequence(int dir) {
    int len = clampVal(PatLen[0], 1, 32);
    if (dir > 0) {
        // Tiefsten MIDI-Wert unter allen Hit-Steps ermitteln
        int minMidi = 9999;
        for (int i = 0; i < len; i++) {
            if (!patternIsHit(0, i)) continue;
            int midi = quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                                      pitchRoot, pitchIntervalMask)
                       + (int)OctaveNote1[i] * 12;
            if (midi < minMidi) minMidi = midi;
        }
        if (minMidi == 9999) return;
        // Alle Hit-Steps auf diesem Niveau anheben
        bool changed = false;
        for (int i = 0; i < len; i++) {
            if (!patternIsHit(0, i)) continue;
            int midi = quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                                      pitchRoot, pitchIntervalMask)
                       + (int)OctaveNote1[i] * 12;
            if (midi == minMidi && (int)OctaveNote1[i] < 3) {
                OctaveNote1[i]++;
                changed = true;
            }
        }
        if (!changed) return;
    } else {
        // Höchsten MIDI-Wert unter allen Hit-Steps ermitteln
        int maxMidi = -9999;
        for (int i = 0; i < len; i++) {
            if (!patternIsHit(0, i)) continue;
            int midi = quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                                      pitchRoot, pitchIntervalMask)
                       + (int)OctaveNote1[i] * 12;
            if (midi > maxMidi) maxMidi = midi;
        }
        if (maxMidi == -9999) return;
        // Alle Hit-Steps auf diesem Niveau absenken
        bool changed = false;
        for (int i = 0; i < len; i++) {
            if (!patternIsHit(0, i)) continue;
            int midi = quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                                      pitchRoot, pitchIntervalMask)
                       + (int)OctaveNote1[i] * 12;
            if (midi == maxMidi && (int)OctaveNote1[i] > -3) {
                OctaveNote1[i]--;
                changed = true;
            }
        }
        if (!changed) return;
    }
    scheduleSaveParams();
    pendingPitchDraw = true;
}

// Transponiert alle Steps um delta Skalenstufen (alle Töne der Scale, unabhängig von IntervalMask).
// Arbeitet relativ zu frozenMidiBase[], damit Zurückdrehen das Original wiederherstellt.
void transposePitchSequence(int delta) {
    int len = clampVal(PatLen[0], 1, 32);
    // Immer 5 Oktaven für Transpose-Bereich — unabhängig vom eingestellten Spread
    int fullNoteList[60];
    int fullCount = buildNoteList(5, pitchScale, pitchRoot, 0x7F, fullNoteList);
    if (fullCount < 2) return;

    // Beim ersten Transpose-Aufruf: Basis-Snapshot anlegen
    if (!pitchNotesFrozen) {
        for (int i = 0; i < len; i++) {
            frozenMidiBase[i] = quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                                               pitchRoot, pitchIntervalMask);
        }
        transposeOffset  = 0;
        pitchNotesFrozen = true;
    }

    // Neuen Offset berechnen und prüfen ob er für alle Steps gültig ist
    int newOffset = transposeOffset + delta;

    // Für jeden Step: Basis-Position in fullNoteList finden, dann Offset anwenden
    // Wenn auch nur ein Step aus der Liste fällt, Offset nicht übernehmen
    for (int i = 0; i < len; i++) {
        int baseIdx = 0;
        int bestDist = 127;
        for (int k = 0; k < fullCount; k++) {
            int d = abs(fullNoteList[k] - frozenMidiBase[i]);
            if (d < bestDist) { bestDist = d; baseIdx = k; }
        }
        int newIdx = baseIdx + newOffset;
        if (newIdx < 0 || newIdx >= fullCount) return;  // Offset würde einen Step aus der Liste schieben
    }

    // Offset ist gültig — anwenden
    transposeOffset = newOffset;
    for (int i = 0; i < len; i++) {
        int baseIdx = 0;
        int bestDist = 127;
        for (int k = 0; k < fullCount; k++) {
            int d = abs(fullNoteList[k] - frozenMidiBase[i]);
            if (d < bestDist) { bestDist = d; baseIdx = k; }
        }
        frozenMidi[i] = fullNoteList[baseIdx + transposeOffset];
    }
    if (GUIState == PITCH1) drawTransposeIndicator();
    scheduleSaveParams();
    pendingPitchDraw = true;
}

// Zeigt Transpose-Stufennummer oben links wenn Freeze aktiv, löscht sonst den Bereich.
static void drawTransposeIndicator() {
    // Oben links innerhalb der Bar-Area (überlagert leerern schwarzen Bereich über den Balken)
    const int TX = PITCH_BAR_X + 4;
    const int TY = PITCH_BAR_Y + 4;
    tft.fillRect(TX, TY, 50, 20, ILI9341_BLACK);
    if (!pitchNotesFrozen) return;
    int fullNoteList[60];
    int fullCount = buildNoteList(5, pitchScale, pitchRoot, 0x7F, fullNoteList);
    if (fullCount < 1) return;
    int step = ((transposeOffset % fullCount) + fullCount) % fullCount + 1;
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(TX + 2, TY + 2);
    tft.print(step);
}

void drawPitchScreen() {
    fillScreenIfNeeded();
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    drawPitchV1CordButtons();
    drawPitchFoldBox();
    drawPitchPresetBox();
    drawPitchHoldCheckbox();
    drawPitchRotateCheckbox();
    drawPitchDisplayModeCheckbox();
    if (stepEditActive) drawPitchStepNoteLabel();
    drawTransposeIndicator();

    tft.drawRect(PITCH_BAR_X - 1, PITCH_BAR_Y - 1,
                 PITCH_BAR_W + 2, PITCH_BAR_H + 2,
                 pitchNotesFrozen ? ILI9341_CYAN : ILI9341_DARKGREY);

    lastPitchPlayIdx = -1;
    drawPitchBars();
    drawPitchPlayhead(cntCh[0]);
    drawPitchControls();
  }

// Hilfsfunktion: Rotation von Kanal ch in alle betroffenen Arrays einfrieren,
// PatRot[ch] auf 0 setzen. Kein Redraw — Aufrufer ist zuständig.
static void flattenChannelRotation(int ch) {
    int len    = clampVal(PatLen[ch], 1, 32);
    int rot    = PatRot[ch];
    int rotSel = PatRotSel[ch];
    if (rot == 0 && rotSel == 0) return;

    bool    etmp[32];
    uint8_t vtmp[32];
    // Combined rotation for EPat: R + r applied together
    int combined = clampVal(rot + rotSel, -(len - 1), len - 1);

    // EPat immer rotieren (Global R bestimmt die Muster-Verschiebung)
    if (rot != 0) {
        for (int i = 0; i < len; i++) etmp[i] = EPatArr[ch][euclidRotatedSrc(i, len, rot)];
        for (int i = 0; i < len; i++) EPatArr[ch][i] = etmp[i];
        syncEPatBFromEPat(ch);
    }

    // Selective-Rot-Arrays mit kombinierter Rotation einfrieren
    if (RotateValues[ch]) {
        for (int i = 0; i < len; i++) vtmp[i] = ValuesArr[ch][euclidRotatedSrc(i, len, combined)];
        for (int i = 0; i < len; i++) ValuesArr[ch][i] = vtmp[i];
    }
    if (RotateGateLen[ch]) {
        for (int i = 0; i < len; i++) vtmp[i] = GateLenArr[ch][euclidRotatedSrc(i, len, combined)];
        for (int i = 0; i < len; i++) GateLenArr[ch][i] = vtmp[i];
    }
    if (RotateRatchet[ch]) {
        for (int i = 0; i < len; i++) vtmp[i] = RatchetArr[ch][euclidRotatedSrc(i, len, combined)];
        for (int i = 0; i < len; i++) RatchetArr[ch][i] = vtmp[i];
    }
    if (ch == 0) {
        if (RotateOctave[0]) {
            int8_t otmp[32];
            for (int i = 0; i < len; i++) otmp[i] = OctaveNote1[euclidRotatedSrc(i, len, combined)];
            for (int i = 0; i < len; i++) OctaveNote1[i] = otmp[i];
        }
        if (pitchRotate) {
            for (int i = 0; i < len; i++) vtmp[i] = PitchNote1[euclidRotatedSrc(i, len, combined)];
            for (int i = 0; i < len; i++) PitchNote1[i] = vtmp[i];
        }
    }
    PatRot[ch]    = 0;
    PatRotSel[ch] = 0;
    pendingCircleRedraw[ch] = true;
}

// Enc1-Long-Press auf PITCH1:
// 1. Rotation + Fold aller Kanäle einfrieren (PatRot=0, pitchFoldMode=0)
// 2. Aktuell klingende Töne (frozenMidi falls aktiv, sonst quantisiert) in PitchNote1[] backen
// 3. Freeze aufheben — Töne sind jetzt fest in PitchNote1[], kein Freeze-Modus nötig
// 4. Grüner Rahmen als visuelles Feedback
void applyAllTransforms() {
    int len = clampVal(PatLen[0], 1, 32);

    for (int ch = 0; ch < 3; ch++)
        flattenChannelRotation(ch);

    uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
    if (effFold != 0) {
        uint8_t tmp[32];
        for (int i = 0; i < len; i++)
            tmp[i] = PitchNote1[foldPitchIdx(i, len, effFold)];
        for (int i = 0; i < len; i++)
            PitchNote1[i] = tmp[i];
        pitchFoldMode = 0;
    }

    // Aktuell klingende MIDI-Noten als neue PitchNote1-Rohwerte backen.
    // Wichtig: noteList mit spread=5 + 0x7F bauen (identisch zu Transpose),
    // damit Töne außerhalb des aktuellen Spreads exakt gefunden werden.
    int bakeNoteList[60];
    int bakeNc = buildNoteList(5, pitchScale, pitchRoot, 0x7F, bakeNoteList);
    for (int i = 0; i < len; i++) {
        int midi = pitchNotesFrozen
            ? frozenMidi[i]
            : quantizeToMidi(PitchNote1[i], pitchSpread, pitchScale,
                             pitchRoot, pitchIntervalMask);
        if (bakeNc > 0) {
            int bestIdx = 0, bestDist = 127;
            for (int k = 0; k < bakeNc; k++) {
                int d = abs(bakeNoteList[k] - midi);
                if (d < bestDist) { bestDist = d; bestIdx = k; }
            }
            PitchNote1[i] = (uint8_t)clampVal((bestIdx * 256 + 128) / bakeNc, 0, 255);
        }
        OctaveNote1[i] = 0;
    }
    // Nach dem Bake: spread=5 + 0x7F setzen damit Rohwerte korrekt interpretiert werden
    pitchSpread       = 5;
    pitchIntervalMask = 0x7F;
    pitchNotesFrozen  = false;
    transposeOffset   = 0;
    memcpy(lastNonEchoPitchNotes, PitchNote1, 32);
    memcpy(undoPitchNotes, PitchNote1, 32);
    lastNoteEffectIdx = -1;

    scheduleSaveParams();
    // Grüner Rahmen als Feedback, dann zurück zu normal
    tft.drawRect(PITCH_BAR_X - 1, PITCH_BAR_Y - 1,
                 PITCH_BAR_W + 2, PITCH_BAR_H + 2, ILI9341_GREEN);
    delayMicroseconds(120000);
    tft.drawRect(PITCH_BAR_X - 1, PITCH_BAR_Y - 1,
                 PITCH_BAR_W + 2, PITCH_BAR_H + 2, ILI9341_DARKGREY);
    drawPitchBars();
    drawPitchControls();
}

void handlePITCH(int mapX, int mapY, uint16_t tipPos) {
    // Fold-Box vor UL-Check prüfen (liegt in UL-Zone)
    if (hitBox(mapX, mapY, PITCH_FOLD_BX, PITCH_FOLD_BY, PITCH_FOLD_BW, PITCH_FOLD_BH, 4)) {
        pitchFoldMode = (pitchFoldMode + 1) % 13;
        scheduleSaveParams();
        drawPitchFoldBox();
        pendingPitchDraw = true;
        return;
    }
    if (tipPos == UL) {
        requestNavigateTo(EUCLPARAM1);
        return;
    }

    // V1-Button
    if (hitBox(mapX, mapY, PITCH_V1_BX, PITCH_V1_BY, PITCH_V1_BW, PITCH_V1_BH, 6)) {
        requestNavigateTo(VALUES1);
        return;
    }
    // CORD-Button → Akkord-Sequencer
    if (hitBox(mapX, mapY, PITCH_CORD_BX, PITCH_CORD_BY, PITCH_CORD_BW, PITCH_CORD_BH, 6)) {
        requestNavigateTo(CHORD_SEQ);
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
        pendingPitchDraw = true;
        return;
    }

    // Display/Play mode checkbox
    if (hitBox(mapX, mapY, PITCH_DP_BX, PITCH_DP_BY, PITCH_DP_BS, PITCH_DP_BS, 8)) {
        pitchDisplayMode = !pitchDisplayMode;
        drawPitchDisplayModeCheckbox();
        pendingPitchDraw = true;
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
                pendingPitchDraw = true;
            }
            return;
        }
    }

    // Scale/Chord box — tap cycles forward
    if (hitBox(mapX, mapY, 0, PITCH_CTRL_Y, 100, PITCH_CTRL_H, 3)) {
        if (pitchChordMode) {
            movePitchChordIdx(1);
        } else {
            pitchScale = (uint8_t)((pitchScale + 1) % (uint8_t)SCALE_COUNT);
            scheduleSaveParams();
            pendingPitchDraw = true;
        }
        return;
    }

    // Root — tap cycles forward
    if (hitBox(mapX, mapY, 102, PITCH_CTRL_Y, 38, PITCH_CTRL_H, 3)) {
        pitchRoot = (uint8_t)((pitchRoot + 1) % 12);
        scheduleSaveParams();
        pendingPitchDraw = true;
        return;
    }

    // Spread — cycles 1..5
    if (hitBox(mapX, mapY, 142, PITCH_CTRL_Y, 36, PITCH_CTRL_H, 3)) {
        uint8_t oldSpread = pitchSpread;
        pitchSpread = (uint8_t)(pitchSpread >= 5 ? 1 : pitchSpread + 1);
        if (pitchSpread != oldSpread) {
            pitchNotesFrozen = false;
            transposeOffset  = 0;
        }
        scheduleSaveParams();
        pendingPitchDraw = true;
        return;
    }

    // IV — tap: wrap lowest note +1 oct
    if (hitBox(mapX, mapY, 180, PITCH_CTRL_Y, 28, PITCH_CTRL_H, 3)) {
        invertPitchSequence(1);
        return;
    }

    // AI — tap: raise truly lowest MIDI note +1 oct
    if (hitBox(mapX, mapY, 210, PITCH_CTRL_Y, 28, PITCH_CTRL_H, 3)) {
        aInvPitchSequence(1);
        return;
    }

    // Shift — cycles -3..+3
    if (hitBox(mapX, mapY, 240, PITCH_CTRL_Y, 38, PITCH_CTRL_H, 3)) {
        pitchShift = (int8_t)(pitchShift >= 3 ? -3 : pitchShift + 1);
        scheduleSaveParams();
        pendingPitchDraw = true;
        return;
    }

    // Tp — tap: transpose +1 Skalenstufe
    if (hitBox(mapX, mapY, 280, PITCH_CTRL_Y, 40, PITCH_CTRL_H, 3)) {
        transposePitchSequence(1);
        return;
    }

    // Bar area — exclude UL zone (back arrow), set raw pitch value
    dragLockIdx = -1;  // Reset für folgenden Drag
    if (!(mapX < 80 && mapY < 80) &&
        mapX >= PITCH_BAR_X && mapX < PITCH_BAR_X + PITCH_BAR_W &&
        mapY >= PITCH_BAR_Y && mapY < PITCH_BAR_Y + PITCH_BAR_H) {
        int len = clampVal(PatLen[0], 1, 32);
        int idx = clampVal(((mapX - PITCH_BAR_X) * len + len - 1) / PITCH_BAR_W, 0, len - 1);
        dragLockIdx = idx;  // Balken für folgenden Drag sperren
        int v = map(mapY, PITCH_BAR_Y + PITCH_BAR_H - 1, PITCH_BAR_Y, 0, 255);
        v -= (int)pitchShift * 255 / (int)pitchSpread;
        v = clampVal(v, 0, 255);
        uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
        int effIdx = foldPitchIdx(idx, len, effFold);
        int src = pitchRotate ? layerRotatedSrc(0, effIdx) : effIdx;
        pitchNotesFrozen = false;
        transposeOffset  = 0;
        PitchNote1[src] = (uint8_t)v;
        scheduleSaveParams();
        drawPitchBar(idx);
    }
}

void handlePITCHDrag(int mapX, int mapY) {
    if (mapX < 80 && mapY < 80) return;  // UL-Zone (Rücksprungpfeil) schützen
    if (mapX >= PITCH_BAR_X && mapX < PITCH_BAR_X + PITCH_BAR_W &&
        mapY >= PITCH_BAR_Y && mapY < PITCH_BAR_Y + PITCH_BAR_H) {
        int len = clampVal(PatLen[0], 1, 32);
        // X-Lock: beim ersten Touch gewählter Balken bleibt für den gesamten Drag gesperrt.
        int idx = (dragLockIdx >= 0 && dragLockIdx < len)
                ? dragLockIdx
                : clampVal(((mapX - PITCH_BAR_X) * len + len - 1) / PITCH_BAR_W, 0, len - 1);
        {
            int v = map(mapY, PITCH_BAR_Y + PITCH_BAR_H - 1, PITCH_BAR_Y, 0, 255);
            v -= (int)pitchShift * 255 / (int)pitchSpread;
            v = clampVal(v, 0, 255);
            uint8_t effFold = (cvPitchFold >= 0) ? (uint8_t)cvPitchFold : pitchFoldMode;
            int effIdx = foldPitchIdx(idx, len, effFold);
            int src = pitchRotate ? layerRotatedSrc(0, effIdx) : effIdx;
            pitchNotesFrozen = false;
            PitchNote1[src] = (uint8_t)v;
            scheduleSaveParams();
            drawPitchBar(idx);
        }
    }
}

// "Pt"-Button auf EUCLPARAM1 (Kanal 0) — navigiert zu PITCH1
static void drawPitchButton() {
    tft.setFont(Arial_12);
    tft.fillRect(261, 11, 48, 26, 0x4208);
    tft.drawRect(260, 10, 50, 28, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(272, 18);
    tft.print("Pt");
}

// V1- und CORD-Button auf dem PITCH1-Screen
static void drawPitchV1CordButtons() {
    tft.setFont(Arial_12);
    // V1-Button
    tft.fillRect(PITCH_V1_BX+1, PITCH_V1_BY+1, PITCH_V1_BW-2, PITCH_V1_BH-2, 0x4208);
    tft.drawRect(PITCH_V1_BX,   PITCH_V1_BY,   PITCH_V1_BW,   PITCH_V1_BH,   ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(PITCH_V1_BX + 16, PITCH_V1_BY + 6);
    tft.print("V1");
    // CORD-Button
    tft.fillRect(PITCH_CORD_BX+1, PITCH_CORD_BY+1, PITCH_CORD_BW-2, PITCH_CORD_BH-2, 0x0008);
    tft.drawRect(PITCH_CORD_BX,   PITCH_CORD_BY,   PITCH_CORD_BW,   PITCH_CORD_BH,   ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(PITCH_CORD_BX + 11, PITCH_CORD_BY + 6);
    tft.print("Crd");
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
    fillScreenIfNeeded();
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
        requestNavigateTo(PERFORMANCE);
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
        int barFill = (int)((uint32_t)cvSlow[i] * CV_CFG_BAR_W / 4095u);
        if (barFill == lastCvBarFill[i]) continue;
        lastCvBarFill[i] = barFill;
        int y = CV_CFG_ROW_Y[i];
        tft.fillRect(CV_CFG_BAR_X, y + 6, barFill, CV_CFG_ROW_H - 12, ILI9341_CYAN);
        tft.fillRect(CV_CFG_BAR_X + barFill, y + 6, CV_CFG_BAR_W - barFill, CV_CFG_ROW_H - 12, ILI9341_BLACK);
    }
}

// ---------------------------------------------------------------------------
// Global Config Screen
// ---------------------------------------------------------------------------
static const int GC_DECAY_SLD_X  = 15;
static const int GC_DECAY_SLD_Y  = 70;
static const int GC_DECAY_SLD_W  = 250;
static const int GC_DECAY_SLD_H  = 18;
static const int GC_SONG_SEL_Y   = 180;
static const int GC_SONG_BTN_Y   = 212;
static const int GC_SONG_BTN_W   = 90;
static const int GC_SONG_BTN_H   = 28;
static int  gcSongNum        = 0;
static bool gcSongUsedCached = false;  // cached used-bit; read once per navigation/op

static void drawGcDecaySlider() {
    int fill = (int)((uint32_t)ratchetDecay * GC_DECAY_SLD_W / 255u);
    tft.fillRect(GC_DECAY_SLD_X, GC_DECAY_SLD_Y,     fill,                   GC_DECAY_SLD_H, ILI9341_CYAN);
    tft.fillRect(GC_DECAY_SLD_X + fill, GC_DECAY_SLD_Y, GC_DECAY_SLD_W - fill, GC_DECAY_SLD_H, ILI9341_DARKGREY);
    tft.drawRect(GC_DECAY_SLD_X - 1, GC_DECAY_SLD_Y - 1, GC_DECAY_SLD_W + 2, GC_DECAY_SLD_H + 2, ILI9341_LIGHTGREY);
    // Percent label right of slider
    tft.setFont(Arial_12);
    tft.fillRect(GC_DECAY_SLD_X + GC_DECAY_SLD_W + 5, GC_DECAY_SLD_Y - 2, 44, GC_DECAY_SLD_H + 4, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(GC_DECAY_SLD_X + GC_DECAY_SLD_W + 7, GC_DECAY_SLD_Y + 3);
    tft.printf("%3d%%", (int)((uint32_t)ratchetDecay * 100u / 255u));
}

// Zeichnet den Song-Selektor; verwendet gcSongUsedCached (kein SD-Zugriff hier).
static void drawGcSongSelector() {
    // Song number display box
    tft.fillRect(120, GC_SONG_SEL_Y, 80, 26, ILI9341_BLACK);
    tft.drawRect(119, GC_SONG_SEL_Y - 1, 82, 28, ILI9341_DARKGREY);
    tft.setFont(Arial_16);
    tft.setTextColor(gcSongUsedCached ? ILI9341_GREEN : ILI9341_LIGHTGREY);
    tft.setCursor(135, GC_SONG_SEL_Y + 5);
    tft.printf("%02d", gcSongNum);
    // Arrows
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_WHITE);
    tft.fillRect(90, GC_SONG_SEL_Y, 25, 26, 0x4208);
    tft.drawRect(89, GC_SONG_SEL_Y - 1, 27, 28, ILI9341_DARKGREY);
    tft.setCursor(95, GC_SONG_SEL_Y + 5);
    tft.print("<");
    tft.fillRect(206, GC_SONG_SEL_Y, 25, 26, 0x4208);
    tft.drawRect(205, GC_SONG_SEL_Y - 1, 27, 28, ILI9341_DARKGREY);
    tft.setCursor(210, GC_SONG_SEL_Y + 5);
    tft.print(">");
}

// Öffentlich: vom Main-Loop nach Song-Op aufgerufen. Liest einmal von SD, dann
// kein weiterer SD-Zugriff bis zur nächsten Navigation zum GCONFIG-Screen.
void refreshGConfigSongSelector() {
    gcSongUsedCached = getSongUsedBit(gcSongNum);
    drawGcSongSelector();
}

static uint32_t gcSaveFlashUntil = 0;

static void drawGcSaveButton(bool flashing) {
    uint16_t fill = flashing ? ILI9341_GREEN : ILI9341_BLACK;
    uint16_t text = flashing ? ILI9341_BLACK : ILI9341_GREEN;
    tft.fillRect(11, GC_SONG_BTN_Y + 1, GC_SONG_BTN_W - 2, GC_SONG_BTN_H - 2, fill);
    tft.drawRect(10, GC_SONG_BTN_Y, GC_SONG_BTN_W, GC_SONG_BTN_H, ILI9341_GREEN);
    tft.setFont(Arial_16);
    tft.setTextColor(text);
    tft.setCursor(10 + (GC_SONG_BTN_W - 4 * 9) / 2, GC_SONG_BTN_Y + 7);
    tft.print("Save");
}

static void drawGcSongButtons() {
    // Save | Load | Del
    static const char* lbls[3] = { "Save", "Load", "Del" };
    static const uint16_t bcs[3] = { ILI9341_GREEN, ILI9341_WHITE, ILI9341_RED };
    int xs[3] = { 10, 115, 220 };
    for (int i = 0; i < 3; i++) {
        tft.fillRect(xs[i] + 1, GC_SONG_BTN_Y + 1, GC_SONG_BTN_W - 2, GC_SONG_BTN_H - 2, ILI9341_BLACK);
        tft.drawRect(xs[i], GC_SONG_BTN_Y, GC_SONG_BTN_W, GC_SONG_BTN_H, bcs[i]);
        tft.setFont(Arial_16);
        tft.setTextColor(bcs[i]);
        int tx = xs[i] + (GC_SONG_BTN_W - (int)strlen(lbls[i]) * 9) / 2;
        tft.setCursor(tx, GC_SONG_BTN_Y + 7);
        tft.print(lbls[i]);
    }
}

void triggerGConfigSaveFlash() {
    gcSaveFlashUntil = millis() + 300;
    drawGcSaveButton(true);
}

void tickGConfigUi() {
    if (gcSaveFlashUntil != 0 && (int32_t)(millis() - gcSaveFlashUntil) >= 0) {
        gcSaveFlashUntil = 0;
        drawGcSaveButton(false);
    }
}

void drawGConfigScreen() {
    gcSongNum        = activeSongNum;               // zuletzt benutzten Song vorauswählen
    gcSongUsedCached = getSongUsedBit(gcSongNum);  // einmalige SD-Abfrage beim Screen-Aufbau
    fillScreenIfNeeded();

    // Back arrow
    tft.setFont(AwesomeF100_24);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(10, 15);
    tft.print((char)18);

    // Title
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(50, 18);
    tft.print("Global Config");

    // CV Cfg link button
    tft.setFont(Arial_12);
    tft.fillRect(221, 11, 88, 22, 0x4208);
    tft.drawRect(220, 10, 90, 24, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(227, 18);
    tft.print("CV Config");

    // Ratchet Decay section
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(15, 52);
    tft.print("Ratchet Decay:");
    drawGcDecaySlider();

    // Autosave-Toggle (3 Modi: 0=Aus, 1=Ständig, 2=Bei Clock-Stop)
    {
        int cx = 15, cy = 105, cs = 20;
        static const uint16_t modeColor[3] = { ILI9341_ORANGE, ILI9341_GREEN, ILI9341_CYAN };
        static const char* modeLabel[3] = { "Aus (Live)", "Ein (Sound-Suche)", "Bei Clock-Stop" };
        tft.drawRect(cx, cy, cs, cs, ILI9341_DARKGREY);
        tft.fillRect(cx+1, cy+1, cs-2, cs-2, ILI9341_BLACK);
        tft.setFont(Arial_12);
        tft.setTextColor(ILI9341_LIGHTGREY);
        tft.setCursor(cx + cs + 6, cy + 5);
        tft.print("Autosave");
        if (autosaveMode == 1) {
            tft.drawLine(cx+3, cy+10, cx+8, cy+16, ILI9341_GREEN);
            tft.drawLine(cx+8, cy+16, cx+17, cy+4, ILI9341_GREEN);
        } else if (autosaveMode == 2) {
            tft.drawLine(cx+3, cy+4, cx+17, cy+16, ILI9341_CYAN);
            tft.drawLine(cx+17, cy+4, cx+3, cy+16, ILI9341_CYAN);
        }
        tft.setFont(Arial_10);
        tft.setTextColor(modeColor[autosaveMode]);
        tft.setCursor(cx + cs + 70, cy + 6);
        tft.print(modeLabel[autosaveMode]);
    }

    // Pin 7 Mode Toggle (0=Reset-Puls, 1=Run/Stop-Pegel) + manueller Reset-Button
    {
        int cx = 15, cy = 130, cs = 20;
        static const uint16_t modeColor[2] = { ILI9341_ORANGE, ILI9341_CYAN };
        static const char* modeLabel[2] = { "Reset-Puls", "Run/Stop" };
        tft.drawRect(cx, cy, cs, cs, ILI9341_DARKGREY);
        tft.fillRect(cx+1, cy+1, cs-2, cs-2, ILI9341_BLACK);
        tft.setFont(Arial_12);
        tft.setTextColor(ILI9341_LIGHTGREY);
        tft.setCursor(cx + cs + 6, cy + 5);
        tft.print("Pin 7:");
        if (pin7Mode == 1) {
            tft.drawLine(cx+3, cy+10, cx+8, cy+16, ILI9341_CYAN);
            tft.drawLine(cx+8, cy+16, cx+17, cy+4, ILI9341_CYAN);
        }
        tft.setFont(Arial_10);
        tft.setTextColor(modeColor[pin7Mode]);
        tft.setCursor(cx + cs + 70, cy + 6);
        tft.print(modeLabel[pin7Mode]);
        // Manueller Reset-Button (nur in Mode 1 sichtbar)
        if (pin7Mode == 1) {
            tft.fillRect(248, cy, 62, cs, 0x4208);
            tft.drawRect(247, cy - 1, 64, cs + 2, ILI9341_DARKGREY);
            tft.setFont(Arial_10);
            tft.setTextColor(ILI9341_WHITE);
            tft.setCursor(252, cy + 5);
            tft.print("RESET");
        } else {
            tft.fillRect(247, cy - 1, 65, cs + 2, ILI9341_BLACK);
        }
    }

    // Song Memory section
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(15, 155);
    tft.print("Song Memory:");
    drawGcSongSelector();
    drawGcSongButtons();
}

void handleGConfig(int mapX, int mapY, uint16_t tipPos) {
    if (tipPos == UL) {
        requestNavigateTo(PERFORMANCE);
        return;
    }
    // CV Config button (x=220, y=10, w=90, h=24)
    if (hitBox(mapX, mapY, 220, 10, 90, 24, 5)) {
        requestNavigateTo(CV_CONFIG);
        return;
    }
    // Autosave-Toggle (x=15, y=105, w=230, h=24) — 3 Modi: 0→1→2→0
    if (hitBox(mapX, mapY, 15, 105, 230, 24, 4)) {
        autosaveMode = (autosaveMode + 1) % 3;
        saveParams();  // Sofort speichern, damit Einstellung Neustart überlebt
        drawGConfigScreen();
        return;
    }
    // Pin 7 Mode Toggle (x=15, y=130, w=230, h=24) — 0=Reset-Puls, 1=Run/Stop
    if (hitBox(mapX, mapY, 15, 130, 230, 24, 4)) {
        pin7Mode = (pin7Mode == 0) ? 1 : 0;
        applyPin7Mode();
        saveParams();
        drawGConfigScreen();
        return;
    }
    // Manueller Reset-Button (nur in Mode 1 aktiv, x=247, y=129, w=65, h=22)
    if (pin7Mode == 1 && hitBox(mapX, mapY, 247, 129, 65, 22, 4)) {
        triggerManualReset();
        return;
    }
    // Decay slider (full width touch)
    if (hitBox(mapX, mapY, GC_DECAY_SLD_X - 1, GC_DECAY_SLD_Y - 8, GC_DECAY_SLD_W + 2, GC_DECAY_SLD_H + 16, 2)) {
        int v = ((mapX - GC_DECAY_SLD_X) * 255 + GC_DECAY_SLD_W / 2) / GC_DECAY_SLD_W;
        ratchetDecay = (uint8_t)clampVal(v, 0, 255);
        drawGcDecaySlider();
        scheduleSaveParams();
        return;
    }
    // Song selector arrows: used-bit für neuen Song aus SD lesen (nur hier, nicht im Main-Loop)
    if (hitBox(mapX, mapY, 89, GC_SONG_SEL_Y - 1, 27, 28, 4)) {
        gcSongNum = (gcSongNum > 0) ? gcSongNum - 1 : 99;
        gcSongUsedCached = getSongUsedBit(gcSongNum);  // SD-Read hier ok: keine Timer-Kritikalität
        drawGcSongSelector();
        return;
    }
    if (hitBox(mapX, mapY, 205, GC_SONG_SEL_Y - 1, 27, 28, 4)) {
        gcSongNum = (gcSongNum < 99) ? gcSongNum + 1 : 0;
        gcSongUsedCached = getSongUsedBit(gcSongNum);
        drawGcSongSelector();
        return;
    }
    // Song buttons: Save | Load | Del — DEFER zum Main-Loop (SD-Blocking!)
    int xs[3] = { 10, 115, 220 };
    for (int i = 0; i < 3; i++) {
        if (hitBox(mapX, mapY, xs[i], GC_SONG_BTN_Y, GC_SONG_BTN_W, GC_SONG_BTN_H, 4)) {
            pendingSongOp  = i + 1;  // 1=save, 2=load, 3=delete
            pendingSongNum = gcSongNum;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Song Screen
// ---------------------------------------------------------------------------
static const int SONG_KEY_W   = 80;
static const int SONG_KEY_H   = 30;
static const int SONG_KEY_Y0  = 89;
static const int SONG_KEY_GAP = 1;
static const int SONG_SEQ_Y   = 40;
static const int SONG_SEQ_H   = 28;
static const int SONG_MUTE_Y  = 69;   // M1/M2/M3 Arm-Streifen
static const int SONG_MUTE_H  = 18;
static const int SONG_BTN_Y   = SONG_KEY_Y0 + 4 * (SONG_KEY_H + SONG_KEY_GAP);  // 89+124=213
static const int SONG_BTN_H   = 26;
static uint16_t songUsedMask  = 0;
static uint8_t  songArmMask   = 0;  // bit0=M1 arm, bit1=M2 arm, bit2=M3 arm
static int      songCursor    = 0;  // Caret-Position: 0..songLen (zwischen Zeichen)
static int      songViewStart = 0;  // erster sichtbarer Index

static void drawSongKey(int digit) {
    int col = digit % 4;
    int row = digit / 4;
    int x   = col * SONG_KEY_W;
    int y   = SONG_KEY_Y0 + row * (SONG_KEY_H + SONG_KEY_GAP);
    bool used    = (songUsedMask & (uint16_t)(1u << digit)) != 0;
    bool playing = songPlaying && ((songSeq[songLoadedPos] & 0x0F) == (uint8_t)digit);
    uint16_t fill   = playing ? 0x000Fu :
                      used    ? 0x2104u :
                                0x0821u;
    uint16_t border = playing ? ILI9341_CYAN :
                      used    ? ILI9341_DARKGREY :
                                0x2104u;
    uint16_t txtcol = playing ? ILI9341_CYAN :
                      used    ? ILI9341_WHITE :
                                0x4208u;
    tft.fillRect(x + 1, y + 1, SONG_KEY_W - 2, SONG_KEY_H - 2, fill);
    tft.drawRect(x, y, SONG_KEY_W, SONG_KEY_H, border);
    tft.setFont(Arial_16);
    tft.setTextColor(txtcol);
    char buf[2] = { (char)(digit < 10 ? '0' + digit : 'a' + digit - 10), 0 };
    tft.setCursor(x + 32, y + 7);
    tft.print(buf);
}

static void drawSongBottomButtons() {
    static const struct { const char* lbl; int x; } btns[4] = {
        { "BS",   0   },
        { "CLR",  80  },
        { "PLAY", 160 },
        { "STP",  240 },
    };
    static const uint16_t BASE_COLS[4] = {
        ILI9341_YELLOW, ILI9341_RED, ILI9341_GREEN, ILI9341_DARKGREY
    };
    for (int i = 0; i < 4; i++) {
        uint16_t col = BASE_COLS[i];
        if (i == 2 && songPlaying) col = ILI9341_CYAN;
        if (i == 3 && songHalted)  col = ILI9341_CYAN;
        tft.fillRect(btns[i].x + 1, SONG_BTN_Y + 1, 78, SONG_BTN_H - 2, ILI9341_BLACK);
        tft.drawRect(btns[i].x, SONG_BTN_Y, 80, SONG_BTN_H, col);
        tft.setFont(Arial_16);
        tft.setTextColor(col);
        int tw = (int)strlen(btns[i].lbl) * 9;
        tft.setCursor(btns[i].x + (80 - tw) / 2, SONG_BTN_Y + 5);
        tft.print(btns[i].lbl);
    }
}

static const uint16_t SONG_CH_COLS[3] = { ILI9341_YELLOW, ILI9341_RED, ILI9341_GREEN };
static const int SONG_MUTE_XS[3] = { 0, 107, 214 };
static const int SONG_MUTE_WS[3] = { 107, 107, 106 };

static void drawSongMuteArm() {
    // Im Spielmodus: aktuellen Step-Mute anzeigen; sonst: Arm-Zustand zeigen
    uint8_t displayMask = songArmMask;
    if (songPlaying && songLen > 0) {
        displayMask = (songSeq[songLoadedPos] >> 4) & 0x07;
    }
    for (int ch = 0; ch < 3; ch++) {
        bool muted = (displayMask >> ch) & 1;
        uint16_t col = muted ? SONG_CH_COLS[ch] : 0x2104u;
        tft.fillRect(SONG_MUTE_XS[ch] + 1, SONG_MUTE_Y + 1, SONG_MUTE_WS[ch] - 2, SONG_MUTE_H - 2, ILI9341_BLACK);
        tft.drawRect(SONG_MUTE_XS[ch], SONG_MUTE_Y, SONG_MUTE_WS[ch], SONG_MUTE_H, col);
        tft.setFont(Arial_16);
        tft.setTextColor(col);
        const char* lbl = (ch == 0) ? "M1" : (ch == 1) ? "M2" : "M3";
        int tw = (int)strlen(lbl) * 9;
        tft.setCursor(SONG_MUTE_XS[ch] + (SONG_MUTE_WS[ch] - tw) / 2, SONG_MUTE_Y + 2);
        tft.print(lbl);
    }
}

static const int SONG_VIS     = 17;   // sichtbare Einträge (je 2 Zeichen für ◄/► reserviert)
static const int SONG_SEQ_X0  = 18;   // Start der Zeichen-Area (nach ◄-Zone)
static const int SONG_STEP_W  = 16;   // Pixel pro Eintrag

// Hält songViewStart im gültigen Bereich und den Cursor im sichtbaren Fenster.
static void clampSongView() {
    int maxStart = (int)songLen - SONG_VIS + 1;
    if (maxStart < 0) maxStart = 0;
    if (songViewStart > maxStart) songViewStart = maxStart;
    if (songViewStart < 0) songViewStart = 0;
    // Cursor ins Fenster ziehen
    if (songCursor < songViewStart) songViewStart = songCursor;
    if (songCursor > songViewStart + SONG_VIS - 1) songViewStart = songCursor - SONG_VIS + 1;
    if (songViewStart < 0) songViewStart = 0;
}

static void drawSongSequence() {
    tft.fillRect(1, SONG_SEQ_Y + 1, 318, SONG_SEQ_H - 2, ILI9341_BLACK);
    tft.drawRect(0, SONG_SEQ_Y, 320, SONG_SEQ_H, ILI9341_DARKGREY);

    // Im Play-Modus: Fenster um spielende Position zentrieren
    if (songPlaying) {
        songViewStart = (int)songLoadedPos - SONG_VIS / 2;
        int maxStart = (int)songLen - SONG_VIS + 1;
        if (maxStart < 0) maxStart = 0;
        if (songViewStart > maxStart) songViewStart = maxStart;
        if (songViewStart < 0) songViewStart = 0;
    }
    // Bereichs-Clamp (kein Cursor-Constraint hier — clampSongView nur bei Cursor-Bewegung)

    bool hasLeft  = (songViewStart > 0);
    bool hasRight = (songViewStart + SONG_VIS <= (int)songLen);

    // ◄ Scroll-Indikator
    tft.setTextColor(hasLeft ? ILI9341_DARKGREY : ILI9341_BLACK);
    tft.setFont(Arial_16);
    tft.setCursor(2, SONG_SEQ_Y + 4);
    tft.print("<");

    // ► Scroll-Indikator
    tft.setTextColor(hasRight ? ILI9341_DARKGREY : ILI9341_BLACK);
    tft.setCursor(306, SONG_SEQ_Y + 4);
    tft.print(">");

    int cx = SONG_SEQ_X0;
    int cy = SONG_SEQ_Y + 4;

    for (int vi = 0; vi < SONG_VIS; vi++) {
        int i = songViewStart + vi;
        if (i < (int)songLen) {
            uint8_t raw  = songSeq[i];
            uint8_t slot = raw & 0x0F;
            uint8_t mute = (raw >> 4) & 0x07;
            bool invalid = !(songUsedMask & (uint16_t)(1u << slot));
            char c = (slot < 10) ? ('0' + slot) : ('a' + slot - 10);

            bool playing = songPlaying && ((int)songLoadedPos == i);
            if (playing) tft.fillRect(cx - 1, SONG_SEQ_Y + 2, 14, SONG_SEQ_H - 4, 0x000Fu);

            bool caretHere = !songPlaying && (songCursor == i + 1);
            if (caretHere) tft.fillRect(cx + 12, SONG_SEQ_Y + 18, 2, 7, ILI9341_YELLOW);

            tft.setTextColor(playing ? ILI9341_CYAN : invalid ? ILI9341_RED : ILI9341_WHITE);
            tft.setFont(Arial_16);
            tft.setCursor(cx, cy);
            tft.print(c);

            for (int ch = 0; ch < 3; ch++) {
                bool muted = (mute >> ch) & 1;
                tft.fillRect(cx + 1 + ch * 4, SONG_SEQ_Y + 21, 3, 3,
                             muted ? (uint16_t)0x2104u : SONG_CH_COLS[ch]);
            }
            cx += SONG_STEP_W;
        } else if (i == (int)songLen && songLen < 64) {
            bool caretHere = !songPlaying && (songCursor == (int)songLen);
            if (caretHere) tft.fillRect(cx - 2, SONG_SEQ_Y + 18, 2, 7, ILI9341_YELLOW);
            tft.setTextColor(ILI9341_DARKGREY);
            tft.setFont(Arial_16);
            tft.setCursor(cx, cy);
            tft.print("_");
            cx += SONG_STEP_W;
        }
    }
}

void moveSongCursor(int delta) {
    if (songPlaying) return;
    songCursor += delta;
    if (songCursor < 0) songCursor = 0;
    if (songCursor > (int)songLen) songCursor = (int)songLen;
    clampSongView();
    drawSongSequence();
}

void tickSongUi() {
    drawSongSequence();
    drawSongMuteArm();
    drawSongBottomButtons();
    for (int i = 0; i < 16; i++) drawSongKey(i);
}

static const int SONG_LOOP_X = 284, SONG_LOOP_Y = 10, SONG_LOOP_S = 20;

static void drawSongLoopCheckbox() {
    tft.drawRect(SONG_LOOP_X, SONG_LOOP_Y, SONG_LOOP_S, SONG_LOOP_S, ILI9341_DARKGREY);
    tft.fillRect(SONG_LOOP_X + 1, SONG_LOOP_Y + 1, SONG_LOOP_S - 2, SONG_LOOP_S - 2, ILI9341_BLACK);
    tft.setFont(Arial_12);
    tft.setCursor(SONG_LOOP_X - 42, SONG_LOOP_Y + 4);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("Loop");
    if (songLoop) {
        tft.drawLine(SONG_LOOP_X + 3, SONG_LOOP_Y + 10, SONG_LOOP_X + 8, SONG_LOOP_Y + 16, ILI9341_GREEN);
        tft.drawLine(SONG_LOOP_X + 8, SONG_LOOP_Y + 16, SONG_LOOP_X + 17, SONG_LOOP_Y + 4, ILI9341_GREEN);
    }
}

void drawSongScreen() {
    resetSongPlayback();   // Stop + Lookahead-Flags löschen; songPos=0
    songHalted = true;     // Tick-Schleife pausieren bis PLAY gedrückt wird
    songUsedMask = getSlotsUsedMask();
    songCursor   = (int)songLen;  // Cursor ans Ende → Append-Modus
    fillScreenIfNeeded();
    tft.setFont(AwesomeF100_24);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(10, 15);
    tft.print((char)18);
    tft.setFont(Arial_16);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(50, 18);
    tft.print("Song Sequencer");
    drawSongLoopCheckbox();
    drawSongSequence();
    drawSongMuteArm();
    for (int i = 0; i < 16; i++) drawSongKey(i);
    drawSongBottomButtons();
}

void handleSong(int mapX, int mapY, uint16_t tipPos) {
    if (tipPos == UL) {
        songPlaying = false;
        songHalted  = false;
        for (int ch = 0; ch < 3; ch++) MuteSeq[ch] = false;
        requestNavigateTo(PERFORMANCE);
        return;
    }
    // Loop-Checkbox (oben rechts, immer togglebar)
    if (hitBox(mapX, mapY, SONG_LOOP_X - 45, SONG_LOOP_Y, 45 + SONG_LOOP_S, SONG_LOOP_S, 4)) {
        songLoop = !songLoop;
        drawSongLoopCheckbox();
        return;
    }
    if (!songPlaying) {
        // Tap auf Sequenz-Zeile → Cursor setzen (rechts vom getippten Zeichen)
        if (hitBox(mapX, mapY, 0, SONG_SEQ_Y, 320, SONG_SEQ_H, 2)) {
            int col = (mapX - SONG_SEQ_X0) / SONG_STEP_W;
            if (col < 0) col = 0;
            songCursor = songViewStart + col + 1;
            if (songCursor > (int)songLen) songCursor = (int)songLen;
            if (songCursor < 0) songCursor = 0;
            clampSongView();
            drawSongSequence();
            return;
        }
        // M-Arm Toggle
        for (int ch = 0; ch < 3; ch++) {
            if (hitBox(mapX, mapY, SONG_MUTE_XS[ch], SONG_MUTE_Y, SONG_MUTE_WS[ch], SONG_MUTE_H, 4)) {
                songArmMask ^= (uint8_t)(1u << ch);
                drawSongMuteArm();
                return;
            }
        }
        // Hex-Tasten 0-f: Einfügen an Cursor-Position
        for (int i = 0; i < 16; i++) {
            if (!(songUsedMask & (uint16_t)(1u << i))) continue;
            int col = i % 4;
            int row = i / 4;
            int x   = col * SONG_KEY_W;
            int y   = SONG_KEY_Y0 + row * (SONG_KEY_H + SONG_KEY_GAP);
            if (hitBox(mapX, mapY, x, y, SONG_KEY_W, SONG_KEY_H, 4)) {
                if (songLen < 64) {
                    uint8_t entry = (uint8_t)((songArmMask << 4) | (uint8_t)i);
                    memmove(&songSeq[songCursor + 1], &songSeq[songCursor],
                            (size_t)((int)songLen - songCursor));
                    songSeq[songCursor] = entry;
                    songLen++;
                    songCursor++;
                    drawSongSequence();
                    scheduleSaveParams();
                }
                return;
            }
        }
        // BS: löscht links vom Cursor
        if (hitBox(mapX, mapY, 0, SONG_BTN_Y, 80, SONG_BTN_H, 4)) {
            if (songCursor > 0 && songLen > 0) {
                songCursor--;
                memmove(&songSeq[songCursor], &songSeq[songCursor + 1],
                        (size_t)((int)songLen - songCursor - 1));
                songLen--;
                drawSongSequence();
                scheduleSaveParams();
            }
            return;
        }
        // CLR
        if (hitBox(mapX, mapY, 80, SONG_BTN_Y, 80, SONG_BTN_H, 4)) {
            songLen       = 0;
            songCursor    = 0;
            songViewStart = 0;
            songHalted    = false;
            songArmMask   = 0;
            drawSongSequence();
            drawSongMuteArm();
            drawSongBottomButtons();
            scheduleSaveParams();
            return;
        }
    }
    // PLAY
    if (hitBox(mapX, mapY, 160, SONG_BTN_Y, 80, SONG_BTN_H, 4)) {
        if (!songPlaying && songLen > 0) {
            songHalted    = false;
            songPlaying   = true;
            songPos       = 0;
            songLoadedPos = 0;
            songCursor    = 0;
            // cnt auf PatLen-1 setzen: erste Load feuert sofort im ersten Tick
            cnt    = (PatLen[0] > 0) ? (unsigned int)(PatLen[0] - 1) : 0u;
            cnthold = 0;
            for (int ch = 0; ch < 3; ch++) { cntCh[ch] = 0; cycleCount[ch] = 1; }
            requestLoadSlot((int)(songSeq[0] & 0x0F));
        }
        drawSongBottomButtons();
        drawSongSequence();
        for (int i = 0; i < 16; i++) drawSongKey(i);
        return;
    }
    // STOP
    if (hitBox(mapX, mapY, 240, SONG_BTN_Y, 80, SONG_BTN_H, 4)) {
        if (songPlaying) {
            pendingSongHalt = true;
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// navigateToScreen: setzt GUIState und zeichnet den Ziel-Screen komplett neu.
// ---------------------------------------------------------------------------
// navFromState hier vorab deklariert, damit navigateToScreen(NAV) darauf zugreifen kann.
static uint16_t navFromState_early = EUCLCIRCS;

void requestNavigateTo(uint16_t target) {
    pendingNavTarget = target;
}

void markPreFilled() { s_skipNextFill_top = true; }
void setNavOpenedFrom(uint16_t state) { navFromState_early = state; }

void navigateToScreen(uint16_t target) {
    // Screen-Wechsel ist ein sicherer Moment für Flash-Write:
    // fillScreen() + Neuzeichnen dauert 50-100ms sowieso → 4-8ms Flash fällt nicht auf.
    if (PendingSave && bpm == 0) {
        PendingSave = false;
        saveParams();
    }
    // Song-Screen verlassen: Playback stoppen, normal weiterlaufen
    if (GUIState == SONG && target != SONG) {
        resetSongPlayback();   // setzt songHalted=false, löscht alle Song-Flags
    }
    GUIState = target;
    switch (target) {
        case EUCLCIRCS:
            fillScreenIfNeeded();
            setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
            drawEncParamIndicators();
            drawBpmControls();
            drawBpmValue();
            drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
            drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
            drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
            for (int i = 0; i < 3; i++) displayedPatLen[i] = PatLen[i];
            break;
        case PERFORMANCE:  drawPerformanceScreen(); break;
        case PITCH1:       drawPitchScreen();        break;
        case CV_CONFIG:    drawCvConfigScreen();     break;
        case GCONFIG:      drawGConfigScreen();      break;
        case SONG:         drawSongScreen();         break;
        case EUCLPARAM1:   redrawParamFromPattern(0); break;
        case EUCLPARAM2:   redrawParamFromPattern(1); break;
        case EUCLPARAM3:   redrawParamFromPattern(2); break;
        case VALUES1:      drawValuesScreen(0);      break;
        case VALUES2:      drawValuesScreen(1);      break;
        case VALUES3:      drawValuesScreen(2);      break;
        case GATELEN1:     drawGateLenScreen(0);     break;
        case GATELEN2:     drawGateLenScreen(1);     break;
        case GATELEN3:     drawGateLenScreen(2);     break;
        case XY1:          drawXYPadScreen(0);       break;
        case XY2:          drawXYPadScreen(1);       break;
        case XY3:          drawXYPadScreen(2);       break;
        case NAV:          drawNavScreen(navFromState_early); break;
        case COND1:        drawCondScreen(0); break;
        case COND2:        drawCondScreen(1); break;
        case COND3:        drawCondScreen(2); break;
        case CHORD_SEQ:    drawChordSeqScreen();  break;
        case CHORD_DEF:    drawChordDefScreen();  break;
        default: break;
    }
  }

// ---------------------------------------------------------------------------
// Navigation-Übersicht: 4×4-Grid aller Screens.
// fromState = Screen von dem aus NAV geöffnet wurde (wird hervorgehoben).
// ---------------------------------------------------------------------------
static const int NAV_COLS = 5, NAV_ROWS = 4;
static const int NAV_TW   = 64, NAV_TH  = 60;

static const uint16_t NAV_STATE[NAV_ROWS][NAV_COLS] = {
    { EUCLCIRCS,  PERFORMANCE, PITCH1,   CV_CONFIG, SONG  },
    { EUCLPARAM1, VALUES1,     GATELEN1, XY1,       COND1 },
    { EUCLPARAM2, VALUES2,     GATELEN2, XY2,       COND2 },
    { EUCLPARAM3, VALUES3,     GATELEN3, XY3,       COND3 },
};

static const char* const NAV_L1[NAV_ROWS][NAV_COLS] = {
    { "",    "",    "",    "",    ""    },
    { "Ch1", "Ch1", "Ch1", "Ch1", "Ch1" },
    { "Ch2", "Ch2", "Ch2", "Ch2", "Ch2" },
    { "Ch3", "Ch3", "Ch3", "Ch3", "Ch3" },
};

static const char* const NAV_L2[NAV_ROWS][NAV_COLS] = {
    { "Circles", "Perform", "Pitch",  "CV Cfg", "Song" },
    { "Param",   "Values",  "Gate",   "XY",     "Cond" },
    { "Param",   "Values",  "Gate",   "XY",     "Cond" },
    { "Param",   "Values",  "Gate",   "XY",     "Cond" },
};

// Hintergrundfarben pro Zeile (global / Ch1 / Ch2 / Ch3)
static const uint16_t NAV_BG[NAV_ROWS] = { 0x1088, 0x6180, 0x4000, 0x0180 };
// Highlight-Farben: Cyan / Gelb / Rot / Grün
static const uint16_t NAV_HL[NAV_ROWS] = {
    ILI9341_CYAN, ILI9341_YELLOW, ILI9341_RED, ILI9341_GREEN
};

// Cursor-Zustand: welches Tile ist per Encoder markiert, von wo wurde NAV geöffnet
static int      navCursor    = 0;
static uint16_t navFromState = EUCLCIRCS;

// isFrom: Tile des vorherigen Screens (farbiger Rahmen)
// isCursor: aktuell per Encoder ausgewähltes Tile (weißer Außenrahmen)
static void drawNavTile(int row, int col, bool isFrom, bool isCursor) {
    int x = col * NAV_TW;
    int y = row * NAV_TH;
    tft.fillRect(x, y, NAV_TW, NAV_TH, NAV_BG[row]);
    tft.drawRect(x, y, NAV_TW, NAV_TH, isCursor ? ILI9341_WHITE : ILI9341_BLACK);
    if (isFrom) {
        tft.drawRect(x + 2, y + 2, NAV_TW - 4, NAV_TH - 4, NAV_HL[row]);
        tft.drawRect(x + 3, y + 3, NAV_TW - 6, NAV_TH - 6, NAV_HL[row]);
    }
    tft.setTextColor(isFrom ? NAV_HL[row] : ILI9341_WHITE);
    if (NAV_L1[row][col][0] != '\0') {
        tft.setFont(Arial_10);
        tft.setCursor(x + 6, y + 10);
        tft.print(NAV_L1[row][col]);
        tft.setFont(Arial_12);
        tft.setCursor(x + 6, y + 30);
        tft.print(NAV_L2[row][col]);
    } else {
        tft.setFont(Arial_12);
        tft.setCursor(x + 6, y + 22);
        tft.print(NAV_L2[row][col]);
    }
}

void drawNavScreen(uint16_t fromState) {
    navFromState = fromState;
    // Cursor startet auf dem Tile des aktuellen Screens
    navCursor = 0;
    for (int r = 0; r < NAV_ROWS; r++)
        for (int c = 0; c < NAV_COLS; c++)
            if (NAV_STATE[r][c] == fromState) navCursor = r * NAV_COLS + c;
    for (int r = 0; r < NAV_ROWS; r++)
        for (int c = 0; c < NAV_COLS; c++)
            drawNavTile(r, c, NAV_STATE[r][c] == fromState, (r * NAV_COLS + c) == navCursor);
}

// Enc3-Drehung auf NAV: Cursor verschieben, nur die zwei betroffenen Tiles neu zeichnen.
void moveNavCursor(int delta) {
    int oldCursor = navCursor;
    navCursor = ((navCursor + delta) % 20 + 20) % 20;
    if (navCursor == oldCursor) return;
    int r1 = oldCursor / NAV_COLS, c1 = oldCursor % NAV_COLS;
    int r2 = navCursor  / NAV_COLS, c2 = navCursor  % NAV_COLS;
    drawNavTile(r1, c1, NAV_STATE[r1][c1] == navFromState, false);
    drawNavTile(r2, c2, NAV_STATE[r2][c2] == navFromState, true);
}

// Gibt den GUIState des aktuell markierten Tiles zurück (für Enc3-Kurzdruck).
uint16_t getNavCursorState() {
    return NAV_STATE[navCursor / NAV_COLS][navCursor % NAV_COLS];
}

void handleNav(int mapX, int mapY) {
    int col = mapX / NAV_TW;
    int row = mapY / NAV_TH;
    if (col < 0 || col >= NAV_COLS || row < 0 || row >= NAV_ROWS) return;
    requestNavigateTo(NAV_STATE[row][col]);
}

// Quick-Save Toast: kleines Overlay in Bildschirmmitte, verschwindet nach 1.2 s.
static uint32_t saveToastUntilMs = 0;
static const int TOAST_X = 60, TOAST_Y = 100, TOAST_W = 200, TOAST_H = 38;

void showSaveToast(int slot) {
    tft.fillRoundRect(TOAST_X, TOAST_Y, TOAST_W, TOAST_H, 6, ILI9341_DARKGREEN);
    tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREEN);
    tft.setFont(Arial_12);
    if (slot < 0) {
        tft.setCursor(TOAST_X + 18, TOAST_Y + 12);
        tft.print("Alle Slots belegt!");
    } else {
        tft.setCursor(TOAST_X + 12, TOAST_Y + 12);
        tft.printf("Gespeichert \x10 Slot %d", slot + 1);
    }
    saveToastUntilMs = millis() + 1200;
}

bool tickSaveToast() {
    if (saveToastUntilMs == 0) return false;
    if ((int32_t)(millis() - saveToastUntilMs) >= 0) {
        saveToastUntilMs = 0;
        return true;  // Aufrufer soll Screen neu zeichnen
    }
    return false;
}

// ---------------------------------------------------------------------------
// Conditional Actions Screen
// ---------------------------------------------------------------------------

// Grid layout constants
static const int COND_LBL_W  = 24;   // left label column width
static const int COND_COL_W  = 37;   // pitch per column (incl. 1px gap right)
static const int COND_CELL_W = 36;   // usable cell width
static const int COND_ROW0_Y = 105;  // step number row (title strip = 0..39, empty = 40..104)
static const int COND_ROW0_H = 27;
static const int COND_ROW1_Y = 134;  // hit indicator row (2px gap)
static const int COND_ROW1_H = 27;
static const int COND_ROW2_Y = 163;  // condition row (2px gap)
static const int COND_ROW2_H = 27;
static const int COND_ROW3_Y = 192;  // action row (2px gap)
static const int COND_ROW3_H = 27;

// Playhead state per channel (step last drawn, to erase it)
static int condPhStep[3] = { -1, -1, -1 };

static const char* condTypeLabel(uint8_t t) {
    static const char* const lbl[] = { "---", "ODD", "EVN", "M:3", "M:4", "P25", "P50", "P75" };
    return (t < COND_TYPE_COUNT) ? lbl[t] : "---";
}

static const char* condActionLabel(uint8_t a) {
    static const char* const lbl[] = {
        "---", "MUT", "ACC", "G.S", "G.M", "G.L", "TIE",
        "R:2", "R:3", "R:4",
        "+1","+2","+3","+4","+5","+6","+7","+8","+9","+10","+11","+12",
        "-1","-2","-3","-4","-5","-6","-7","-8","-9","-10","-11","-12",
        "+V2", "+V/2", "+V/3", "+V/4", "+2O", "-2O",
        "+S1","+S2","+S3","+S4","-S1","-S2","-S3","-S4"
    };
    return (a < COND_ACT_COUNT) ? lbl[a] : "---";
}

static uint16_t condActionDisplayColor(uint8_t a) {
    if (a == COND_ACT_NONE)   return 0x4208;
    if (a == COND_ACT_MUTE)   return 0x7800;   // dark red
    if (a == COND_ACT_ACCENT) return ILI9341_YELLOW;
    if (a >= COND_ACT_GATE_S  && a <= COND_ACT_GATE_TIE) return ILI9341_CYAN;
    if (a >= COND_ACT_R2      && a <= COND_ACT_R4)       return ILI9341_MAGENTA;
    if (a >= COND_ACT_T_PLUS_1  && a <= COND_ACT_T_PLUS_12)  return ILI9341_GREEN;
    if (a >= COND_ACT_T_MINUS_1 && a <= COND_ACT_T_MINUS_12) return 0xFD20;  // orange
    if (a >= COND_ACT_VAL_X2   && a <= COND_ACT_VAL_X1_25)  return ILI9341_WHITE;  // value mul
    if (a == COND_ACT_T_PLUS_24)  return ILI9341_GREEN;
    if (a == COND_ACT_T_MINUS_24) return 0xFD20;  // orange, matches -transpose
    if (a >= COND_ACT_SC_PLUS_1  && a <= COND_ACT_SC_PLUS_4)  return 0x87E0;  // light green
    if (a >= COND_ACT_SC_MINUS_1 && a <= COND_ACT_SC_MINUS_4) return 0xFBE0;  // light orange
    return ILI9341_LIGHTGREY;
}

// Draws one column (all 4 rows) for given page+col.
void drawCondCell(int setIdx, int page, int col) {
    int stepIdx  = page * 8 + col;
    int cursor   = getCondStepCursor(setIdx);
    bool isCursor = (cursor == stepIdx);
    int len      = clampVal(PatLen[setIdx], 1, 32);
    bool active  = (stepIdx < len);
    int x        = COND_LBL_W + col * COND_COL_W;

    // Clear full column height to remove any cursor-rect remnants in row gaps / bottom margin
    tft.fillRect(x, COND_ROW0_Y, COND_CELL_W, 240 - COND_ROW0_Y, ILI9341_BLACK);

    // Row 0: step number
    {
        uint16_t bg = isCursor ? 0x2945 : 0x1082;
        tft.fillRect(x, COND_ROW0_Y, COND_CELL_W, COND_ROW0_H, bg);
        tft.setFont(Arial_10);
        tft.setTextColor(active ? ILI9341_WHITE : ILI9341_DARKGREY);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", clampVal(stepIdx + 1, 1, 32));
        int xOff = (int)strlen(buf) <= 1 ? 14 : 10;
        tft.setCursor(x + xOff, COND_ROW0_Y + 8);
        tft.print(buf);
    }

    // Row 1: hit indicator
    {
        tft.fillRect(x, COND_ROW1_Y, COND_CELL_W, COND_ROW1_H, ILI9341_BLACK);
        int cx = x + COND_CELL_W / 2;
        int cy = COND_ROW1_Y + COND_ROW1_H / 2;
        if (active) {
            bool hit = patternIsHit(setIdx, stepIdx);
            if (hit)  tft.fillCircle(cx, cy, 8, ILI9341_WHITE);
            else      tft.drawCircle(cx, cy, 7, ILI9341_DARKGREY);
        }
    }

    // Row 2: condition type
    {
        uint8_t ct = active ? condTypeArr[setIdx][stepIdx] : 0;
        static const uint16_t ctColors[COND_TYPE_COUNT] = {
            0x4208, 0xFFFF, 0xFFFF, 0xAFE5, 0xAFE5, 0xFEA0, 0xFEA0, 0xFEA0
        };
        uint16_t bg = (active && ct != COND_NONE) ? 0x0841 : ILI9341_BLACK;
        tft.fillRect(x, COND_ROW2_Y, COND_CELL_W, COND_ROW2_H, bg);
        if (active) {
            const char* lbl = condTypeLabel(ct);
            uint16_t fc = ctColors[ct < COND_TYPE_COUNT ? ct : 0];
            tft.setFont(Arial_10);
            tft.setTextColor(fc);
            int xOff = (int)strlen(lbl) <= 2 ? 13 : 8;
            tft.setCursor(x + xOff, COND_ROW2_Y + COND_ROW2_H / 2 - 6);
            tft.print(lbl);
        }
    }

    // Row 3: action
    {
        uint8_t ca = active ? condActionArr[setIdx][stepIdx] : 0;
        uint16_t fc = condActionDisplayColor(ca);
        uint16_t bg = (active && ca != COND_ACT_NONE) ? 0x0841 : ILI9341_BLACK;
        tft.fillRect(x, COND_ROW3_Y, COND_CELL_W, COND_ROW3_H, bg);
        if (active) {
            const char* lbl = condActionLabel(ca);
            tft.setFont(Arial_10);
            tft.setTextColor(fc);
            int xOff = (int)strlen(lbl) <= 2 ? 13 : (int)strlen(lbl) <= 3 ? 8 : 3;
            tft.setCursor(x + xOff, COND_ROW3_Y + COND_ROW3_H / 2 - 6);
            tft.print(lbl);
        }
    }

    // Cursor border: yellow rectangle spanning all rows
    if (isCursor) {
        tft.drawRect(x, COND_ROW0_Y, COND_CELL_W,
                     240 - COND_ROW0_Y, ILI9341_YELLOW);
    }
}

// Draws the left row label column.
static void drawCondLabels() {  // only used internally
    tft.setFont(Arial_10);
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(2, COND_ROW1_Y + 8);  tft.print("P");
    tft.setCursor(0, COND_ROW2_Y + COND_ROW2_H / 2 - 6);  tft.print("C");
    tft.setCursor(0, COND_ROW3_Y + COND_ROW3_H / 2 - 6);  tft.print("A");
}

static void drawCondClearButton() {
    int x = 10, y = 46, w = 54, h = 26;
    tft.drawRect(x, y, w, h, 0x4A49);
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, 0x2104);
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(x + 9, y + 7);
    tft.print("CLR");
}

static void drawCondPresetWidget(int ch) {
    int x = 72, y = 46, w = 186, h = 26;
    tft.fillRect(x, y, w, h, ILI9341_BLACK);
    if (condPresetMode && condPresetCh == ch) {
        tft.drawRect(x, y, w, h, ILI9341_CYAN);
        tft.setFont(Arial_12);
        tft.setTextColor(ILI9341_WHITE);
        tft.setCursor(x + 4, y + 7);
        tft.print("<");
        tft.setCursor(x + w - 13, y + 7);
        tft.print(">");
        tft.setTextColor(ILI9341_CYAN);
        tft.setCursor(x + 18, y + 7);
        tft.print(condPresetNames[condPresetIdx]);
    } else {
        tft.setFont(Arial_10);
        tft.setTextColor(0x2945);
        tft.setCursor(x + 4, y + 8);
        tft.print("Enc2: RND preset");
    }
}

void applyCondPreset(int ch, int preset) {
    int len = clampVal(PatLen[ch], 1, 32);
    memset(condTypeArr[ch],   COND_NONE,     32 * sizeof(condTypeArr[ch][0]));
    memset(condActionArr[ch], COND_ACT_NONE, 32 * sizeof(condActionArr[ch][0]));

    // Collect hit steps
    int hits[32];
    int hitCount = 0;
    for (int i = 0; i < len; i++) {
        if (patternIsHit(ch, i)) hits[hitCount++] = i;
    }
    if (hitCount == 0) { scheduleSaveParams(); return; }

    randomSeed(micros());

    switch (preset) {
    case 0: {  // 4-Bar: last ¼ of hits get MOD4+R2 (ratchet fill every 4 cycles)
        int fillCount = (hitCount + 3) / 4;
        if (fillCount < 1) fillCount = 1;
        for (int i = hitCount - fillCount; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_MOD4;
            condActionArr[ch][hits[i]] = COND_ACT_R2;
        }
        break;
    }
    case 1: {  // ACC23: odd positions (1,3,5...) P50+V/2, even positions (2,4,6...) P50+V/3
        for (int i = 0; i < len; i++) {
            if (i % 2 == 0) {  // positions 1,3,5,... (1-based)
                condTypeArr[ch][i]   = COND_P50;
                condActionArr[ch][i] = COND_ACT_VAL_X1_5;
            } else {           // positions 2,4,6,... (1-based)
                condTypeArr[ch][i]   = COND_P50;
                condActionArr[ch][i] = COND_ACT_VAL_X1_33;
            }
        }
        break;
    }
    case 2: {  // Accent: first hit always, ~30% of others probabilistic
        condTypeArr[ch][hits[0]]   = COND_NONE;
        condActionArr[ch][hits[0]] = COND_ACT_ACCENT;
        static const uint8_t ap[] = { COND_P50, COND_P75 };
        for (int i = 1; i < hitCount; i++) {
            if (random(10) < 3) {
                condTypeArr[ch][hits[i]]   = ap[random(2)];
                condActionArr[ch][hits[i]] = COND_ACT_ACCENT;
            }
        }
        break;
    }
    case 3: {  // Alternate: all hits ODD+MUTE → plays only on even cycles
        for (int i = 0; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_ODD;
            condActionArr[ch][hits[i]] = COND_ACT_MUTE;
        }
        break;
    }
    case 4: {  // Bounce: alternating hits ODD+T+12 / EVEN+T-12 (octave ping-pong)
        for (int i = 0; i < hitCount; i++) {
            if (i % 2 == 0) {
                condTypeArr[ch][hits[i]]   = COND_ODD;
                condActionArr[ch][hits[i]] = COND_ACT_T_PLUS_12;
            } else {
                condTypeArr[ch][hits[i]]   = COND_EVEN;
                condActionArr[ch][hits[i]] = COND_ACT_T_MINUS_12;
            }
        }
        break;
    }
    case 5: {  // Chaos: weighted random mix
        static const uint8_t muteProbs[]   = { COND_P25, COND_P50, COND_P50 };
        static const uint8_t accentProbs[] = { COND_P50, COND_P75, COND_ODD, COND_EVEN };
        static const uint8_t gateProbs[]   = { COND_ODD, COND_EVEN, COND_P50 };
        for (int i = 0; i < hitCount; i++) {
            int r = (int)random(100);
            if (r < 25) {
                condTypeArr[ch][hits[i]]   = muteProbs[random(3)];
                condActionArr[ch][hits[i]] = COND_ACT_MUTE;
            } else if (r < 50) {
                condTypeArr[ch][hits[i]]   = accentProbs[random(4)];
                condActionArr[ch][hits[i]] = COND_ACT_ACCENT;
            } else if (r < 65) {
                condTypeArr[ch][hits[i]]   = gateProbs[random(3)];
                condActionArr[ch][hits[i]] = random(2) ? COND_ACT_GATE_S : COND_ACT_GATE_L;
            } else if (r < 80) {
                condTypeArr[ch][hits[i]]   = gateProbs[random(3)];
                condActionArr[ch][hits[i]] = COND_ACT_R2;
            }
            // 20% chance: stays NONE
        }
        break;
    }
    case 6: {  // Fill: last ~33% of hits get ODD+R2
        int fillCount = (hitCount + 2) / 3;
        if (fillCount < 1) fillCount = 1;
        for (int i = hitCount - fillCount; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_ODD;
            condActionArr[ch][hits[i]] = COND_ACT_R2;
        }
        break;
    }
    case 7: {  // Ghost: ~35% of non-first hits get P25+ACCENT (whisper accents)
        for (int i = 1; i < hitCount; i++) {
            if (random(10) < 4) {
                condTypeArr[ch][hits[i]]   = COND_P25;
                condActionArr[ch][hits[i]] = COND_ACT_ACCENT;
            }
        }
        break;
    }
    case 8: {  // Groove: alternating hits get EVEN+GATE_L / ODD+GATE_S
        for (int i = 0; i < hitCount; i++) {
            if (i % 2 == 0) {
                condTypeArr[ch][hits[i]]   = COND_EVEN;
                condActionArr[ch][hits[i]] = COND_ACT_GATE_L;
            } else {
                condTypeArr[ch][hits[i]]   = COND_ODD;
                condActionArr[ch][hits[i]] = COND_ACT_GATE_S;
            }
        }
        break;
    }
    case 9: {  // Heavy: ~60% of hits get P50+MUTE (aggressive dropout)
        for (int i = 0; i < hitCount; i++) {
            if (random(10) < 6) {
                condTypeArr[ch][hits[i]]   = COND_P50;
                condActionArr[ch][hits[i]] = COND_ACT_MUTE;
            }
        }
        break;
    }
    case 10: {  // HUMAN234: all hits P75 + random +V/2 / +V/3 / +V/4
        static const uint8_t valActs[] = { COND_ACT_VAL_X1_5, COND_ACT_VAL_X1_33, COND_ACT_VAL_X1_25 };
        for (int i = 0; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_P75;
            condActionArr[ch][hits[i]] = valActs[random(3)];
        }
        break;
    }
    case 11: {  // Humanize: 40% of hits get P75+ACCENT or P25+MUTE (live dynamics)
        for (int i = 0; i < hitCount; i++) {
            int r = (int)random(10);
            if (r < 2) {
                condTypeArr[ch][hits[i]]   = COND_P25;
                condActionArr[ch][hits[i]] = COND_ACT_MUTE;
            } else if (r < 4) {
                condTypeArr[ch][hits[i]]   = COND_P75;
                condActionArr[ch][hits[i]] = COND_ACT_ACCENT;
            }
        }
        break;
    }
    case 12: {  // Punchy: all hits ODD+ACCENT (strong every odd cycle)
        for (int i = 0; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_ODD;
            condActionArr[ch][hits[i]] = COND_ACT_ACCENT;
        }
        break;
    }
    case 13: {  // Stutter: ~20% of hits get P25+R3 (rare triple ratchets)
        for (int i = 0; i < hitCount; i++) {
            if (random(10) < 2) {
                condTypeArr[ch][hits[i]]   = COND_P25;
                condActionArr[ch][hits[i]] = COND_ACT_R3;
            }
        }
        break;
    }
    case 14: {  // Swell: first hit GATE_TIE, all others GATE_S
        condTypeArr[ch][hits[0]]   = COND_NONE;
        condActionArr[ch][hits[0]] = COND_ACT_GATE_TIE;
        for (int i = 1; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_NONE;
            condActionArr[ch][hits[i]] = COND_ACT_GATE_S;
        }
        break;
    }
    case 15: {  // Thin Out: ~35% of hits get probabilistic MUTE
        static const uint8_t probs[] = { COND_P25, COND_P50, COND_P50, COND_P75 };
        for (int i = 0; i < hitCount; i++) {
            if (random(10) < 4) {
                condTypeArr[ch][hits[i]]   = probs[random(4)];
                condActionArr[ch][hits[i]] = COND_ACT_MUTE;
            }
        }
        break;
    }
    case 16: {  // All Odd: alle Hits COND_ODD ohne Action (Template für manuelle Action-Zuweisung)
        for (int i = 0; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_ODD;
            condActionArr[ch][hits[i]] = COND_ACT_NONE;
        }
        break;
    }
    case 17: {  // All Even: alle Hits COND_EVEN ohne Action
        for (int i = 0; i < hitCount; i++) {
            condTypeArr[ch][hits[i]]   = COND_EVEN;
            condActionArr[ch][hits[i]] = COND_ACT_NONE;
        }
        break;
    }
    default: break;
    }

    scheduleSaveParams();
}

// Public API for encoders.cpp
bool getCondPresetMode()          { return condPresetMode; }
int  getCondPresetCh()            { return condPresetCh; }

void condPresetModeToggle(int ch) {
    if (condPresetMode && condPresetCh == ch) {
        // Second press → apply
        applyCondPreset(ch, condPresetIdx);
        condPresetMode = false;
        condPresetCh   = -1;
        drawCondTitle(ch);
        // Redraw visible page after applying
        int cursor = getCondStepCursor(ch);
        int page   = cursor / 8;
        for (int c = 0; c < 8; c++) drawCondCell(ch, page, c);
    } else {
        condPresetMode = true;
        condPresetCh   = ch;
        drawCondPresetWidget(ch);
    }
}

void condPresetModeCancel() {
    if (!condPresetMode) return;
    int ch = condPresetCh;
    condPresetMode = false;
    condPresetCh   = -1;
    if (ch >= 0 && ch < 3) drawCondPresetWidget(ch);
}

void condPresetModeRotate(int ch, int delta) {
    if (!condPresetMode || condPresetCh != ch) return;
    condPresetIdx = ((condPresetIdx + delta) % COND_PRESET_COUNT + COND_PRESET_COUNT) % COND_PRESET_COUNT;
    drawCondPresetWidget(ch);
}

static void drawRotateCondCheckbox(int setIdx) {
    int x = 268, y = 50, s = 20;
    tft.drawRect(x, y, s, s, ILI9341_DARKGREY);
    tft.fillRect(x + 1, y + 1, s - 2, s - 2, ILI9341_BLACK);
    tft.setFont(Arial_10);
    tft.setCursor(x - 22, y + 5);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.print("RC");
    if (RotateCond[setIdx]) {
        tft.drawLine(x + 3, y + 10, x + 8, y + 15, ILI9341_GREEN);
        tft.drawLine(x + 8, y + 15, x + 17, y + 5, ILI9341_GREEN);
    }
}

// Draws the title row (page indicator + channel label) and RC checkbox.
void drawCondTitle(int setIdx) {
    tft.fillRect(0, 0, 320, COND_ROW0_Y, ILI9341_BLACK);
    setMenuItems4EUCLPARAM(ILI9341_LIGHTGREY);
    int cursor = getCondStepCursor(setIdx);
    int page   = cursor / 8;
    int pages  = (clampVal(PatLen[setIdx], 1, 32) + 7) / 8;
    tft.setFont(Arial_10);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(70, 14);
    tft.printf("COND Ch%d", setIdx + 1);
    if (pages > 1) {
        tft.printf("  p.%d/%d", page + 1, pages);
    }
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(248, 14);
    tft.printf("C%lu", (unsigned long)cycleCount[setIdx]);
    drawCondClearButton();
    drawCondPresetWidget(setIdx);
    drawRotateCondCheckbox(setIdx);
}

void drawCondScreen(int setIdx) {
    condPresetMode = false;  // cancel any active preset selection on screen entry
    condPresetCh   = -1;
    fillScreenIfNeeded();
    condPhStep[setIdx] = -1;  // invalidate playhead
    drawCondTitle(setIdx);
    drawCondLabels();
    int cursor = getCondStepCursor(setIdx);
    int page   = cursor / 8;
    for (int c = 0; c < 8; c++) {
        drawCondCell(setIdx, page, c);
    }
}

// Draws the small "COND" navigation button on the GateLen screen at (80,10,52,24).
void drawCondButton(int setIdx) {
    (void)setIdx;
    int x = 80, y = 10, w = 52, h = 24;
    tft.drawRect(x, y, w, h, 0x4A49);
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, 0x2104);
    tft.setFont(Arial_12);
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(x + 13, y + 6);
    tft.print("CND");
}

void handleCond(int setIdx, int mapX, int mapY, uint16_t tipPos) {
    if (tipPos == UL) {
        requestNavigateTo((setIdx == 0) ? GATELEN1 : (setIdx == 1) ? GATELEN2 : GATELEN3);
        return;
    }
    if (hitBox(mapX, mapY, 268, 50, 20, 20, 8)) {
        RotateCond[setIdx] = !RotateCond[setIdx];
        scheduleSaveParams();
        drawRotateCondCheckbox(setIdx);
        return;
    }
    // CLR: alle Conditions des aktuellen Kanals auf NONE zurücksetzen
    if (hitBox(mapX, mapY, 10, 46, 54, 26, 8)) {
        memset(condTypeArr[setIdx],   COND_NONE,     32 * sizeof(condTypeArr[setIdx][0]));
        memset(condActionArr[setIdx], COND_ACT_NONE, 32 * sizeof(condActionArr[setIdx][0]));
        scheduleSaveParams();
        int cursor = getCondStepCursor(setIdx);
        int page   = cursor / 8;
        for (int c = 0; c < 8; c++) drawCondCell(setIdx, page, c);
        return;
    }
}

// Highlights the step number cell for the current playing step (same page only).
void drawCondPlayhead(int setIdx, unsigned int step) {
    // Refresh cycle counter in title strip whenever value changes
    static uint32_t lastCycle[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    if (cycleCount[setIdx] != lastCycle[setIdx]) {
        lastCycle[setIdx] = cycleCount[setIdx];
        tft.fillRect(244, 4, 76, 26, ILI9341_BLACK);
        tft.setFont(Arial_10);
        tft.setTextColor(ILI9341_DARKGREY);
        tft.setCursor(248, 14);
        tft.printf("C%lu", (unsigned long)cycleCount[setIdx]);
    }

    int len    = clampVal(PatLen[setIdx], 1, 32);
    int cursor = getCondStepCursor(setIdx);
    int page   = cursor / 8;
    int curStep = (int)(step % (unsigned int)len);
    int prevStep = condPhStep[setIdx];

    // Erase previous playhead (if on current page)
    if (prevStep >= 0 && prevStep / 8 == page) {
        drawCondCell(setIdx, page, prevStep % 8);
    }

    condPhStep[setIdx] = curStep;

    // Draw new playhead (if on current page)
    if (curStep / 8 == page) {
        int col       = curStep % 8;
        int x         = COND_LBL_W + col * COND_COL_W;
        bool isCursor = (curStep == cursor);
        bool stepOk   = (curStep < len);
        uint16_t bg   = isCursor ? 0x2945 : 0x1082;
        tft.fillRect(x, COND_ROW0_Y, COND_CELL_W, COND_ROW0_H, bg);
        tft.fillCircle(x + COND_CELL_W / 2, COND_ROW0_Y + 4, 3, ILI9341_GREEN);
        tft.setFont(Arial_10);
        tft.setTextColor(stepOk ? ILI9341_WHITE : ILI9341_DARKGREY);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", curStep + 1);
        int xOff = (int)strlen(buf) <= 1 ? 14 : 10;
        tft.setCursor(x + xOff, COND_ROW0_Y + 12);
        tft.print(buf);
        if (isCursor) tft.drawRect(x, COND_ROW0_Y, COND_CELL_W, 240 - COND_ROW0_Y, ILI9341_YELLOW);
    }
}

// Briefly flashes the title bar red as VLP feedback.
void flashCondBars(int setIdx) {
    tft.fillRect(0, 0, 320, 40, ILI9341_RED);
    delay(40);
    drawCondTitle(setIdx);
    discardPendingTicks();
}

// ---------------------------------------------------------------------------
// Values/GateLen/Ratchet/Octave/IvStep Step-Edit
// ---------------------------------------------------------------------------

static void redrawValBar(int ch, int idx) {
    if (GUIState == (uint16_t)(GATELEN1 + ch)) {
        if      (gateLenEditMode[ch] == 1) drawHoldStepBar(ch, idx);
        else if (gateLenEditMode[ch] == 2) drawMuteStepBar(ch, idx);
        else                               drawGateLenBar(ch, idx);
    } else {
        int mode = valuesEditMode[ch];
        if      (mode == 1) drawRatchetBar(ch, idx);
        else if (mode == 2) drawOctaveBar(ch, idx);
        else if (mode == 3) drawIvStepBar(ch, idx);
        else                drawValuesBar(ch, idx);
    }
}

bool getValStepEditActive(int ch) { return (ch >= 0 && ch < 3) ? valStepEditActive[ch] : false; }
int  getValStepEditCursor(int ch) { return (ch >= 0 && ch < 3) ? valStepEditCursor[ch] : 0; }

void toggleValStepEdit(int ch) {
    if (ch < 0 || ch > 2) return;
    valStepEditActive[ch] = !valStepEditActive[ch];
    if (valStepEditActive[ch]) {
        int len = clampVal(PatLen[ch], 1, 32);
        valStepEditCursor[ch] = clampVal(valStepEditCursor[ch], 0, len - 1);
    }
    redrawValBar(ch, valStepEditCursor[ch]);
}

void moveValStepCursor(int ch, int delta) {
    if (ch < 0 || ch > 2 || !valStepEditActive[ch]) return;
    int len  = clampVal(PatLen[ch], 1, 32);
    int prev = valStepEditCursor[ch];
    valStepEditCursor[ch] = ((valStepEditCursor[ch] + delta) % len + len) % len;
    if (valStepEditCursor[ch] == prev) return;
    redrawValBar(ch, prev);
    redrawValBar(ch, valStepEditCursor[ch]);
}

void adjustValStep(int ch, int delta) {
    if (ch < 0 || ch > 2 || !valStepEditActive[ch]) return;
    int idx        = valStepEditCursor[ch];
    bool isGateLen = (GUIState == (uint16_t)(GATELEN1 + ch));
    int  mode      = isGateLen ? -1 : valuesEditMode[ch];

    // GateLen-Screen: Hold/Mute-Modi haben eigene Behandlung
    if (isGateLen && gateLenEditMode[ch] == 1) {
        int src = RotateHoldStep[ch] ? layerRotatedSrc(ch, idx) : layerBaseSrc(ch, idx);
        HoldStepArr[ch][src] = HoldStepArr[ch][src] ? 0 : 1;  // Toggle
    } else if (isGateLen && gateLenEditMode[ch] == 2) {
        int src = RotateMuteStep[ch] ? layerRotatedSrc(ch, idx) : layerBaseSrc(ch, idx);
        MuteStepArr[ch][src] = MuteStepArr[ch][src] ? 0 : 1;  // Toggle
    } else if (isGateLen || mode == 0) {
        bool useRotate = isGateLen ? RotateGateLen[ch] : RotateValues[ch];
        int src = useRotate ? layerRotatedSrc(ch, idx) : layerBaseSrc(ch, idx);
        uint8_t *arr = isGateLen ? (abEditMode ? GateLenBArr[ch] : GateLenArr[ch])
                                 : (abEditMode ? ValuesBArr[ch]  : ValuesArr[ch]);
        arr[src] = (uint8_t)clampVal((int)arr[src] + delta * 4, 0, 255);
    } else if (mode == 1) {
        int src = RotateRatchet[ch] ? layerRotatedSrc(ch, idx) : layerBaseSrc(ch, idx);
        RatchetArr[ch][src] = (uint8_t)clampVal((int)RatchetArr[ch][src] + delta, 1, 4);
    } else if (mode == 2) {
        int src = RotateOctave[ch] ? layerRotatedSrc(ch, idx) : layerBaseSrc(ch, idx);
        OctaveNote1[src] = (int8_t)clampVal((int)OctaveNote1[src] + delta, -3, 3);
    } else if (mode == 3) {
        int src = RotateIvStep ? layerRotatedSrc(ch, idx) : layerBaseSrc(ch, idx);
        IvStep1[src] = (uint8_t)clampVal((int)IvStep1[src] + delta, 0, 7);
    }

    scheduleSaveParams();
    redrawValBar(ch, idx);
}

void shiftValues(int ch, int amount) {
    int len = clampVal(PatLen[ch], 1, 32);
    uint8_t* arr = abEditMode ? ValuesBArr[ch] : ValuesArr[ch];
    for (int i = 0; i < len; i++) {
        int v = (int)arr[i] + amount;
        arr[i] = (uint8_t)clampVal(v, 0, 255);
    }
    drawValuesBars(ch);
    scheduleSaveParams();
}

void scaleValues(int ch, float mf) {
    int len = clampVal(PatLen[ch], 1, 32);
    uint8_t* arr = abEditMode ? ValuesBArr[ch] : ValuesArr[ch];
    float sum = 0.0f;
    for (int i = 0; i < len; i++) sum += (float)arr[i];
    float mw = sum / (float)len;
    for (int i = 0; i < len; i++) {
        float v = ((float)arr[i] - mw) * mf + mw;
        int vi = (int)(v + 0.5f);
        arr[i] = (uint8_t)clampVal(vi, 0, 255);
    }
    drawValuesBars(ch);
    scheduleSaveParams();
}

void flashValBars(int ch) {
    (void)ch;
    int x0 = 10, y0 = 240 - 5 - 160, h = 160, totalW = 320 - 2 * x0;
    for (int i = 0; i < 2; i++) {
        tft.drawRect(x0 - 1, y0 - 1, totalW + 2, h + 2, ILI9341_ORANGE);
        delay(25);
        tft.drawRect(x0 - 1, y0 - 1, totalW + 2, h + 2, ILI9341_DARKGREY);
        delay(15);
    }
    discardPendingTicks();
}

// ---------------------------------------------------------------------------
// Chord-Sequencer Screen
// ---------------------------------------------------------------------------
static const int CS_LBL_W  = 24;   // linke Label-Spalte
static const int CS_COL_W  = 37;   // Breite pro Slot-Spalte (incl. 1px Gap)
static const int CS_CELL_W = 36;   // nutzbare Zellbreite

static const int CS_ROW0_Y = 70;   // Slot-Nummer
static const int CS_ROW0_H = 24;
static const int CS_ROW1_Y = 94;   // Mute
static const int CS_ROW1_H = 24;
static const int CS_ROW2_Y = 118;  // Legato
static const int CS_ROW2_H = 24;
static const int CS_ROW3_Y = 142;  // Val
static const int CS_ROW3_H = 24;
static const int CS_ROW4_Y = 166;  // AkkNr
static const int CS_ROW4_H = 24;

// Carry-Forward-Auflösung: gibt den effektiv gültigen Wert an Step i zurück
uint8_t csResolveAkkNr(int i) {
    for (int j = i; j >= 0; j--)
        if (chordSlots[j].akkNr != CHORD_SLOT_EMPTY) return chordSlots[j].akkNr;
    return 0;
}
static uint8_t csResolveLeg(int i) {
    for (int j = i; j >= 0; j--)
        if (chordSlots[j].leg != CHORD_SLOT_EMPTY) return chordSlots[j].leg;
    return 0;
}
uint8_t csResolveValNr(int i) {
    for (int j = i; j >= 0; j--)
        if (chordSlots[j].val != CHORD_SLOT_EMPTY) return chordSlots[j].val;
    return 50;
}

void drawChordSeqTitleBar() {
    tft.fillRect(0, 0, 320, 40, ILI9341_BLACK);
    // Rücksprungpfeil
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setFont(AwesomeF100_24);
    tft.setCursor(20, 4);
    tft.print((char)18);
    tft.setFont(Arial_12);  // Font zurücksetzen nach Pfeil
    // Div
    bool divSel = (chordFieldCursor == 4);
    tft.setTextColor(divSel ? ILI9341_YELLOW : ILI9341_LIGHTGREY);
    tft.setCursor(90, 4);
    tft.printf("Div:%d", (int)chordDiv);
    // Len
    bool lenSel = (chordFieldCursor == 5);
    tft.setTextColor(lenSel ? ILI9341_YELLOW : ILI9341_LIGHTGREY);
    tft.setCursor(155, 4);
    tft.printf("Len:%d", (int)chordLen);
    // LIVE-Button
    uint16_t liveFill   = chordLive ? 0x0600 : 0x0841;
    uint16_t liveText   = chordLive ? ILI9341_GREEN : ILI9341_DARKGREY;
    tft.fillRect(261, 5, 54, 22, liveFill);
    tft.drawRect(260, 4, 56, 24, chordLive ? ILI9341_GREEN : ILI9341_DARKGREY);
    tft.setTextColor(liveText);
    tft.setCursor(270, 10);
    tft.print("LIVE");
}

static void drawChordSeqFillButtons() {
    tft.fillRect(0, 40, 320, 30, ILI9341_BLACK);
    struct { int x; int w; const char *lbl; } btns[] = {
        { 0,   80, "Leg=1"  },
        { 84,  80, "Val=50" },
        { 168, 80, "CLR"    },
    };
    for (auto &b : btns) {
        tft.drawRect(b.x, 42, b.w, 22, ILI9341_DARKGREY);
        tft.fillRect(b.x+1, 43, b.w-2, 20, 0x0841);
        tft.setFont(Arial_12);
        tft.setTextColor(ILI9341_LIGHTGREY);
        int tw = (int)strlen(b.lbl) * 7;
        tft.setCursor(b.x + (b.w - tw) / 2, 48);
        tft.print(b.lbl);
    }
}

static void drawChordSeqLabels() {
    tft.setFont(Arial_10);
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(2, CS_ROW0_Y + 7);  tft.print("#");
    tft.setCursor(0, CS_ROW1_Y + 7);  tft.print("M");
    tft.setCursor(0, CS_ROW2_Y + 7);  tft.print("L");
    tft.setCursor(0, CS_ROW3_Y + 7);  tft.print("V");
    tft.setCursor(0, CS_ROW4_Y + 7);  tft.print("A");
}

void drawChordSeqCell(int page, int col) {
    int slotIdx   = page * 8 + col;
    bool isCursor = (slotIdx == chordStepCursor);
    bool active   = (slotIdx < (int)chordLen);
    int x         = CS_LBL_W + col * CS_COL_W;

    tft.fillRect(x, CS_ROW0_Y, CS_CELL_W, 240 - CS_ROW0_Y, ILI9341_BLACK);

    // Row 0: Slot-Nummer — cyan wenn gerade spielend
    {
        bool playing = (chordPlayPos >= 0 && slotIdx == chordPlayPos);
        uint16_t bg  = playing  ? 0x0410 :   // dunkles Cyan-Grün
                       isCursor ? 0x2945 : 0x1082;
        uint16_t fg  = playing  ? ILI9341_CYAN :
                       active   ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillRect(x, CS_ROW0_Y, CS_CELL_W, CS_ROW0_H, bg);
        tft.setFont(Arial_10);
        tft.setTextColor(fg);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", slotIdx + 1);
        int xOff = (int)strlen(buf) == 1 ? 14 : 10;
        tft.setCursor(x + xOff, CS_ROW0_Y + 7);
        tft.print(buf);
    }

    if (!active) {
        if (isCursor)
            tft.drawRect(x, CS_ROW0_Y, CS_CELL_W, 240 - CS_ROW0_Y, ILI9341_YELLOW);
        return;
    }

    // Row 1: Mute
    {
        bool m = chordSlots[slotIdx].mute;
        uint16_t bg = m ? 0x6000 : ILI9341_BLACK;
        tft.fillRect(x, CS_ROW1_Y, CS_CELL_W, CS_ROW1_H, bg);
        tft.setFont(Arial_10);
        tft.setTextColor(m ? ILI9341_RED : ILI9341_DARKGREY);
        tft.setCursor(x + 13, CS_ROW1_Y + 7);
        tft.print(m ? "M" : "-");
    }

    // Row 2: Legato (carry-forward: grau wenn geerbt)
    {
        bool own  = (chordSlots[slotIdx].leg != CHORD_SLOT_EMPTY);
        uint8_t v = own ? chordSlots[slotIdx].leg : csResolveLeg(slotIdx);
        uint16_t fg = own ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillRect(x, CS_ROW2_Y, CS_CELL_W, CS_ROW2_H, ILI9341_BLACK);
        tft.setFont(Arial_10);
        tft.setTextColor(fg);
        tft.setCursor(x + 13, CS_ROW2_Y + 7);
        tft.print(v ? "1" : "0");
    }

    // Row 3: Val (carry-forward: grau wenn geerbt)
    {
        bool own  = (chordSlots[slotIdx].val != CHORD_SLOT_EMPTY);
        uint8_t v = own ? chordSlots[slotIdx].val : csResolveValNr(slotIdx);
        uint16_t fg = own ? ILI9341_WHITE : ILI9341_DARKGREY;
        tft.fillRect(x, CS_ROW3_Y, CS_CELL_W, CS_ROW3_H, ILI9341_BLACK);
        tft.setFont(Arial_10);
        tft.setTextColor(fg);
        char buf[5]; snprintf(buf, sizeof(buf), "%d", (int)v);
        int xOff = (int)strlen(buf) == 2 ? 10 : 6;
        tft.setCursor(x + xOff, CS_ROW3_Y + 7);
        tft.print(buf);
    }

    // Row 4: AkkNr (carry-forward: grau wenn geerbt)
    {
        bool own  = (chordSlots[slotIdx].akkNr != CHORD_SLOT_EMPTY);
        uint8_t v = own ? chordSlots[slotIdx].akkNr : csResolveAkkNr(slotIdx);
        uint16_t bg = own ? 0x0841 : ILI9341_BLACK;
        uint16_t fg = own ? ILI9341_CYAN : ILI9341_DARKGREY;
        tft.fillRect(x, CS_ROW4_Y, CS_CELL_W, CS_ROW4_H, bg);
        tft.setFont(Arial_10);
        tft.setTextColor(fg);
        tft.setCursor(x + 14, CS_ROW4_Y + 7);
        tft.print((int)v);
    }

    // Cursor-Rahmen: umschließt alle Rows
    if (isCursor) {
        // Aktives Feld hervorheben: Field→Row-Mapping (Field 4/5 = Div/Len → nur Titelzeile, kein Row-Highlight)
        // Field: 0=AkkNr(Row4), 1=Mute(Row1), 2=Leg(Row2), 3=Val(Row3)
        static const int fieldToRowY[] = { CS_ROW4_Y, CS_ROW1_Y, CS_ROW2_Y, CS_ROW3_Y };
        if (chordFieldCursor < 4) {
            tft.drawRect(x, fieldToRowY[chordFieldCursor], CS_CELL_W, CS_ROW0_H, ILI9341_YELLOW);
        }
        tft.drawRect(x, CS_ROW0_Y, CS_CELL_W, 240 - CS_ROW0_Y, ILI9341_YELLOW);
    }
}

// ---------------------------------------------------------------------------
// Chord-Definitions-Screen
// Layout: 320×240, Titelzeile Y=0..35, Parameter Y=38..239
//   Rückpfeil X=4, Tabs X=44..268 (8×28px), SAVE-Button X=272..319
//   Zeile 1 Spread  Y=38..75   (5 Buttons à 46px, X=74..304)
//   Zeile 2 Inv     Y=85..122  (4 Buttons à 58px, X=74..306)
//   Zeile 3 Oct     Y=132..169 (5 Buttons à 46px, X=74..304)
//   Zeile 4 Tone    Y=179..239 (7 Buttons à 34px, X=74..312)
// ---------------------------------------------------------------------------
static const int CD_TAB_Y  = 4;
static const int CD_TAB_H  = 28;
static const int CD_TAB_W  = 28;
static const int CD_TAB_X0 = 44;   // 44 + 8*28 = 268; SAVE-Button ab 272

// Tone-Zeile: 7 Töne (1,3,5,7,9,11,13), je 34px breit ab X=74 → 74+7*34=312 ≤ 320
static const int CD_TONE_COUNT = 7;
static const int CD_TONE_X0    = 74;
static const int CD_TONE_W     = 34;
static const int CD_TONE_Y     = 179;
static const int CD_TONE_H     = 38;
static const char *CD_TONE_LBL[7] = { "1","3","5","7","9","11","13" };

void drawChordDefSaveButton() {
    bool saved = chordDefs[chordDefCursor].saved;
    uint16_t fill   = saved ? 0x0320 : 0x0841;
    uint16_t border = saved ? ILI9341_GREEN : 0xC618;
    uint16_t fg     = saved ? ILI9341_GREEN : ILI9341_WHITE;
    tft.fillRect(273, 5, 45, 22, fill);
    tft.drawRect(272, 4, 47, 24, border);
    tft.setFont(Arial_12);
    tft.setTextColor(fg);
    tft.setCursor(278, 10);
    tft.print(saved ? "SAVD" : "SAVE");
}

void drawChordDefTabs() {
    for (int i = 0; i < CHORD_DEF_COUNT; i++) {
        int x    = CD_TAB_X0 + i * CD_TAB_W;
        bool sel  = (i == chordDefCursor);
        bool play = (chordPlayPos >= 0 && csResolveAkkNr(chordPlayPos) == (uint8_t)i);
        bool used = chordDefs[i].saved;
        // Farbe: spielend=grün, selektiert=gelb, belegt=weiß, leer=dunkelgrau
        uint16_t border = play ? ILI9341_GREEN :
                          sel  ? ILI9341_YELLOW :
                          used ? 0xC618 : ILI9341_DARKGREY;
        uint16_t fill   = sel  ? 0x2945 :
                          used ? 0x1082 : ILI9341_BLACK;
        uint16_t fg     = play ? ILI9341_GREEN :
                          sel  ? ILI9341_WHITE :
                          used ? ILI9341_LIGHTGREY : 0x4208;
        tft.fillRect(x+1, CD_TAB_Y+1, CD_TAB_W-2, CD_TAB_H-2, fill);
        tft.drawRect(x,   CD_TAB_Y,   CD_TAB_W,   CD_TAB_H,   border);
        tft.setFont(Arial_12);
        tft.setTextColor(fg);
        tft.setCursor(x + 9, CD_TAB_Y + 8);
        tft.print(i);
    }
}

// Hilfsfunktion: eine Parameter-Zeile zeichnen (Radio-Buttons: genau ein Wert aktiv)
static void drawCDRow(bool sel, int y, int h, const char *label,
                      int btnCount, int btnW, int btnX0,
                      int activeIdx,
                      const char * const lbls[], int lblXOff[]) {
    tft.setFont(Arial_12);
    tft.setTextColor(sel ? ILI9341_YELLOW : ILI9341_LIGHTGREY);
    tft.setCursor(4, y + (h - 14) / 2);
    tft.print(label);
    for (int v = 0; v < btnCount; v++) {
        int bx  = btnX0 + v * btnW;
        bool act = (v == activeIdx);
        // Aktiv: heller Hintergrund (0x07FF = Cyan-tinted), deutlicher Rahmen
        uint16_t bg  = act ? 0x07BF : ILI9341_BLACK;
        uint16_t brdr = act ? ILI9341_CYAN :
                        (sel ? ILI9341_YELLOW : 0x4208);
        uint16_t fg  = act ? ILI9341_BLACK : (sel ? ILI9341_WHITE : 0x8410);
        tft.fillRect(bx+1,   y+1,     btnW-2, h-2, bg);
        tft.drawRect(bx,     y,       btnW,   h,   brdr);
        tft.setTextColor(fg);
        int xOff = lblXOff ? lblXOff[v] : (btnW - (int)strlen(lbls[v]) * 7) / 2;
        tft.setCursor(bx + xOff, y + (h - 14) / 2);
        tft.print(lbls[v]);
    }
}

void drawChordDefParams() {
    tft.fillRect(0, 36, 320, 204, ILI9341_BLACK);
    ChordDef &d = chordDefs[chordDefCursor];

    // Zeile 1: Spread (5 Radio-Buttons)
    {
        const char *lbl[] = { "1","2","3","4","5" };
        drawCDRow(chordDefField == 0, 38, 34, "Sprd:", 5, 46, 74, d.spread - 1, lbl, nullptr);
    }

    // Zeile 2: Inversion (4 Radio-Buttons)
    {
        const char *lbl[] = { "0","1","2","3" };
        drawCDRow(chordDefField == 1, 84, 34, "Inv:", 4, 58, 74, d.inv, lbl, nullptr);
    }

    // Zeile 3: Oktave (5 Radio-Buttons)
    {
        const char *lbl[] = { "-2","-1","0","+1","+2" };
        drawCDRow(chordDefField == 2, 130, 34, "Oct:", 5, 46, 74, d.oct + 2, lbl, nullptr);
    }

    // Zeile 4: Töne (7 Toggle-Buttons, unabhängig schaltbar)
    {
        bool sel = (chordDefField == 3);
        tft.setFont(Arial_12);
        tft.setTextColor(sel ? ILI9341_YELLOW : ILI9341_LIGHTGREY);
        tft.setCursor(4, CD_TONE_Y + (CD_TONE_H - 14) / 2);
        tft.print("Tone:");
        for (int b = 0; b < CD_TONE_COUNT; b++) {
            int bx  = CD_TONE_X0 + b * CD_TONE_W;
            bool act     = (d.toneMask >> b) & 1;
            bool curHere = sel && (b == chordToneCursor);
            uint16_t bg   = act ? 0x07BF : ILI9341_BLACK;
            // Cursor-Position: dicker weißer/gelber Rahmen; aktiv=Cyan, Cursor=Weiß, beides=helles Cyan
            uint16_t brdr = curHere ? ILI9341_WHITE :
                            act     ? ILI9341_CYAN  :
                            sel     ? ILI9341_YELLOW : 0x4208;
            uint16_t fg   = act ? ILI9341_BLACK : (sel ? ILI9341_WHITE : 0x8410);
            tft.fillRect(bx+1, CD_TONE_Y+1, CD_TONE_W-2, CD_TONE_H-2, bg);
            tft.drawRect(bx,   CD_TONE_Y,   CD_TONE_W,   CD_TONE_H,   brdr);
            // Cursor-Highlighting: zweiter Rahmen 1px innen für Sichtbarkeit
            if (curHere) tft.drawRect(bx+2, CD_TONE_Y+2, CD_TONE_W-4, CD_TONE_H-4, ILI9341_WHITE);
            tft.setTextColor(fg);
            int xOff = (b < 5) ? (CD_TONE_W - 8) / 2 : (CD_TONE_W - 14) / 2;
            tft.setCursor(bx + xOff, CD_TONE_Y + (CD_TONE_H - 14) / 2);
            tft.print(CD_TONE_LBL[b]);
        }
    }
}

void drawChordDefScreen() {
    fillScreenIfNeeded();
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setFont(AwesomeF100_24);
    tft.setCursor(4, 4);
    tft.print((char)18);
    tft.setFont(Arial_12);  // Font zurücksetzen nach Pfeil
    drawChordDefSaveButton();
    drawChordDefTabs();
    drawChordDefParams();
}

void handleChordDef(int mapX, int mapY, uint16_t tipPos) {
    if (tipPos == UL) {
        requestNavigateTo(CHORD_SEQ);
        return;
    }
    // SAVE-Button oben rechts
    if (mapX >= 272 && mapY >= 4 && mapY < 28) {
        chordDefs[chordDefCursor].saved = true;
        scheduleSaveParams();
        drawChordDefSaveButton();
        drawChordDefTabs();  // Tab-Farbe aktualisieren
        return;
    }
    // Tab antippen → Akkord wechseln (nur belegte Tabs erlaubt, außer leerem Slot direkt rechts)
    if (mapY >= CD_TAB_Y && mapY < CD_TAB_Y + CD_TAB_H) {
        for (int i = 0; i < CHORD_DEF_COUNT; i++) {
            int x = CD_TAB_X0 + i * CD_TAB_W;
            if (mapX >= x && mapX < x + CD_TAB_W) {
                // Ersten leeren Slot nach dem letzten belegten darf man anwählen (zum Anlegen)
                bool canSelect = chordDefs[i].saved ||
                                 (i == 0) ||
                                 (i > 0 && chordDefs[i-1].saved);
                if (canSelect) {
                    chordDefCursor = i;
                    if (chordLive) midiOutLiveChord();
                    drawChordDefTabs();
                    drawChordDefParams();
                    drawChordDefSaveButton();
                }
                return;
            }
        }
    }
    // Spread-Buttons
    if (mapY >= 38 && mapY < 72) {
        for (int v = 1; v <= 5; v++) {
            int bx = 74 + (v-1) * 46;
            if (mapX >= bx && mapX < bx + 46) {
                chordDefs[chordDefCursor].spread = v;
                chordDefField = 0;
                if (chordLive) midiOutLiveChord();
                drawChordDefParams();
                return;
            }
        }
    }
    // Inv-Buttons
    if (mapY >= 84 && mapY < 118) {
        for (int v = 0; v <= 3; v++) {
            int bx = 74 + v * 58;
            if (mapX >= bx && mapX < bx + 58) {
                chordDefs[chordDefCursor].inv = v;
                chordDefField = 1;
                if (chordLive) midiOutLiveChord();
                drawChordDefParams();
                return;
            }
        }
    }
    // Oct-Buttons
    if (mapY >= 130 && mapY < 164) {
        for (int v = 0; v < 5; v++) {
            int bx = 74 + v * 46;
            if (mapX >= bx && mapX < bx + 46) {
                chordDefs[chordDefCursor].oct = (int8_t)(v - 2);
                chordDefField = 2;
                if (chordLive) midiOutLiveChord();
                drawChordDefParams();
                return;
            }
        }
    }
    // Tone-Toggle-Buttons (7 Töne)
    if (mapY >= CD_TONE_Y && mapY < CD_TONE_Y + CD_TONE_H) {
        for (int b = 0; b < CD_TONE_COUNT; b++) {
            int bx = CD_TONE_X0 + b * CD_TONE_W;
            if (mapX >= bx && mapX < bx + CD_TONE_W) {
                uint8_t toggled = chordDefs[chordDefCursor].toneMask ^ (uint8_t)(1u << b);
                if (toggled != 0) chordDefs[chordDefCursor].toneMask = toggled;
                chordDefField = 3;
                if (chordLive) midiOutLiveChord();
                drawChordDefParams();
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
void drawChordSeqScreen() {
    fillScreenIfNeeded();
    drawChordSeqTitleBar();
    drawChordSeqFillButtons();
    drawChordSeqLabels();
    int page = chordStepCursor / 8;
    for (int c = 0; c < 8; c++) drawChordSeqCell(page, c);
}

void handleChordSeq(int mapX, int mapY, uint16_t tipPos) {
    if (tipPos == UL) {
        requestNavigateTo(PITCH1);
        return;
    }
    // Fill-Buttons
    if (mapY >= 42 && mapY < 64) {
        int len = (int)chordLen;
        if (mapX < 80) {
            // Leg=1 für alle aktiven Slots
            for (int i = 0; i < len; i++) chordSlots[i].leg = 1;
        } else if (mapX < 164) {
            // Val=50 für alle aktiven Slots
            for (int i = 0; i < len; i++) chordSlots[i].val = 50;
        } else if (mapX < 248) {
            // CLR: alle Slots auf Empty zurück (außer Slot 0 AkkNr)
            for (int i = 0; i < CHORD_SLOT_COUNT; i++)
                chordSlots[i] = { CHORD_SLOT_EMPTY, false, CHORD_SLOT_EMPTY, CHORD_SLOT_EMPTY };
            chordSlots[0].akkNr = 0;
            chordSlots[0].val   = 50;
            chordSlots[0].leg   = 0;
        }
        int page = chordStepCursor / 8;
        for (int c = 0; c < 8; c++) drawChordSeqCell(page, c);
        return;
    }
    // LIVE-Button: toggle; Einschalten öffnet Chord-Def, Ausschalten bleibt auf dieser Seite
    if (mapX >= 260 && mapY >= 4 && mapY < 28) {
        chordLive = !chordLive;
        if (chordLive) {
            requestNavigateTo(CHORD_DEF);
        } else {
            drawChordSeqTitleBar();
        }
        return;
    }
}
