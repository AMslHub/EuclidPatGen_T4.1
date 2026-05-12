#include <app_state.h>
#include <cv_inputs.h>
#include <encoders.h>
#include <euclid.h>
#include <gates.h>
#include <hardware_map.h>
#include <pitch.h>
#include <storage.h>
#include <ui_screens.h>
#include <ui_touch.h>

const int CX = 160;
const int CY = 120;   
 
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
ILI9341_t3n tft = ILI9341_t3n(
  TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_MOSI_PIN, TFT_SCLK_PIN, TFT_MISO_PIN
);

// ------- M O D U L G L O B A LE   V A R I A B L E N --------
// Grafikparameter
const int R1         = 115;
const int R2         = 90;
const int R3         = 65;
const int r2         = 3;
const int r3         = 8;

// GUI-Variablen
uint16_t  GUIState;
uint16_t  tipPos;
bool      stillPressed;

// dyn. Einstellungen
int PatLen[3] = { 10, 32, 16 };
int PatNum[3] = { 3, 14, 6 };
int PatRot[3] = { 1, 2, 3 };
uint8_t PatProb[3] = { 20, 20, 20 };
bool PatProbAuto[3] = { false, false, false };
bool ProbEuclidRebuild[3] = { false, false, false };

uint8_t Values1[32] = { 0 };
uint8_t Values2[32] = { 0 };
uint8_t Values3[32] = { 0 };
uint8_t *ValuesArr[3] = { Values1, Values2, Values3 };
uint8_t Ratchet1[32] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
uint8_t Ratchet2[32] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
uint8_t Ratchet3[32] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
uint8_t *RatchetArr[3] = { Ratchet1, Ratchet2, Ratchet3 };
uint8_t GateLen1[32] = { 0 };
uint8_t GateLen2[32] = { 0 };
uint8_t GateLen3[32] = { 0 };
uint8_t *GateLenArr[3] = { GateLen1, GateLen2, GateLen3 };
bool GateHold1 = false;
bool GateHold2 = false;
bool GateHold3 = false;
bool *GateHoldArr[3] = { &GateHold1, &GateHold2, &GateHold3 };
bool Hold1 = true;
bool Hold2 = true;
bool Hold3 = true;
bool *HoldArr[3] = { &Hold1, &Hold2, &Hold3 };
bool RotateValues[3]  = { false, false, false };
bool RotateGateLen[3] = { false, false, false };
bool RotateRatchet[3] = { false, false, false };
bool RotateOctave[3]  = { false, false, false };
uint32_t DurationOfOneStep = 0;

// Performance (Mute/Solo)
bool MuteSeq[3] = { false, false, false };
bool SoloSeq[3] = { false, false, false };

// Deferred save
bool PendingSave = false;
uint32_t PendingSaveAt = 0;
const uint32_t SAVE_DEBOUNCE_MS = 400;
int pendingSlotSaveSlot = -1;
int pendingSongOp  = 0;
int pendingSongNum = -1;
int pendingSlotMoveFrom = -1;
int pendingSlotMoveTo   = -1;

// Encoder: ausstehende Circle-Redraws (erst am Pattern-Ende anwenden)
bool pendingCircleRedraw[3] = { false, false, false };
// Encoder: ausstehende Prob-Neugenerierung (Pattern-Mutation + Redraw from Pattern)
bool pendingProbRegen[3]    = { false, false, false };
// Angezeigte Länge — wird nur beim echten Kreis-Redraw aktualisiert, damit
// updateEucledianCircle nie mit einer noch nicht gezeichneten Länge rechnet.
int displayedPatLen[3] = { 10, 32, 16 };

// Per-Kanal Geschwindigkeit: -3=÷4, -2=÷3, -1=÷2, 0=×1, 1=×2, 2=×3, 3=×4
int chSpeedIdx[3] = { 0, 0, 0 };
// Per-Kanal Schrittzaehler
unsigned int cntCh[3]       = { 0, 0, 0 };
uint8_t autoRotateStep[3]   = { 0, 0, 0 };  // 0=aus, 1-4=Schritte pro Zyklus

// Pitch (Kanal 1)
uint8_t PitchNote1[32]    = { 0 };
int8_t  OctaveNote1[32]   = { 0 };
uint8_t pitchSpread       = 2;
uint8_t pitchScale        = 0;
uint8_t pitchRoot         = 0;
uint8_t pitchIntervalMask = 0x07;  // Root + Terz + Quinte
int8_t  pitchShift        = 0;
bool    pitchHold         = true;
bool    pitchRotate       = true;
uint8_t pitchFoldMode     = 0;
static unsigned int cntChHold[3]  = { 0, 0, 0 };
static uint8_t  chDivPhase[3]     = { 0, 0, 0 };
static uint8_t  chSubTicksDone[3] = { 0, 0, 0 };
static uint32_t lastGlobalTickUs  = 0;

// Ratchet: Zusätzliche Sub-Hits innerhalb eines Steps (CV-gesteuert)
static uint8_t  ratchetRemain[3]   = { 0, 0, 0 };
static uint8_t  ratchetTotal[3]    = { 1, 1, 1 };  // Gesamtzahl Hits im Burst
static uint32_t ratchetNextAt[3]   = { 0, 0, 0 };
static uint32_t ratchetInterval[3] = { 0, 0, 0 };

// Swing: Verzögerter Gate-Trigger für gerade Steps (CV-gesteuert)
static bool     swingPending[3]  = { false, false, false };
static uint32_t swingFireAt[3]   = { 0, 0, 0 };
static uint32_t swingGateDur[3]  = { 0, 0, 0 };

// Performance UI refresh after load
bool PendingPerfRefresh = false;

// Deferred full-screen redraw nach Slot-Load (verhindert fillScreen() in der Tick-Schleife)
bool PendingCircsRedraw = false;
// Deferred Pitch-Controls+Bars Redraw (verhindert SPI-Block in Encoder-Handler und Tick-Schleife)
bool pendingPitchDraw = false;

// Performance touch gating
bool PerfIgnoreUntilRelease = false;
 
bool EPat1[32] = {1,0,0,1,0,0,1,0,0,0, 1,0,0,1,0,0,1,0,1,1, 0,1,1,0,1,0,1,0,1,1, 0,0};
bool EPat2[32] = {1,0,1,1,0,1,1,0,1,0, 1,0,1,1,0,0,1,0,1,1, 0,1,1,0,1,0,1,0,1,1, 0,0};
bool EPat3[32] = {1,0,0,0,1,0,0,0,0,1, 1,0,0,1,0,0,1,0,1,1, 0,1,1,0,1,0,1,0,1,1, 0,0};
bool EPatB1[32] = {0};
bool EPatB2[32] = {0};
bool EPatB3[32] = {0};

// set pointers to pattern arrays
bool *EPatArr[3] = { EPat1, EPat2, EPat3 };
bool *EPatBArr[3] = { EPatB1, EPatB2, EPatB3 };
 
const uint16_t BPM_STOP = 0;   // 0 = Timer anhalten (kein BPM)
const uint16_t BPM_MIN  = 0;   // clampVal-Untergrenze (inkl. Stop)
const uint16_t BPM_MAX  = 600;

uint16_t bpm = 120;
// Intervall in us (BPM -> Mikrosekunden pro Step)
unsigned int iTim = 0;
 
// Variablen
unsigned int cnthold = 0;
unsigned int cnt     = 0;          // Zähler, nur im Main-Loop geschrieben
volatile uint32_t pendingTicks = 0; // ISR-shared

IntervalTimer myTimer;  // Interval-Timer Objekt

// Globale Ratchet-Dämpfung: 0=flat, 255=max Decay
uint8_t ratchetDecay = 0;

// Clock-Modus: false=intern, true=extern (gespeichert im EEPROM)
volatile bool extClockMode = false;

// Externer Clock / Reset (active-low via MMBT3904-Transistor)
static volatile uint32_t lastExtClockUs    = 0;
static volatile bool     extClockReceived  = false; // ISR → Main-Loop Handshake
static volatile bool     pendingReset      = false;
static volatile uint32_t measuredPeriodUs  = 0;
static bool              extClockActive    = false; // nur im Main-Loop-Kontext

// Gate handling
const uint32_t GATE_PULSE_US = 5000; // 5 ms gate pulse
const uint8_t GatePins[3] = { GATE_OUT1_PIN, GATE_OUT2_PIN, GATE_OUT3_PIN };
volatile uint32_t gateOffAt[3] = { 0, 0, 0 };


// Zweck: Verarbeitet externen Clock-Puls (active-low, FALLING-Flanke).
// Misst Taktperiode; setzt pendingReset wenn nach einer langen Pause neu gestartet wird.
// Assumptions: Nur vom Interrupt-Kontext aufgerufen (Pin CLOCK_IN_PIN).
void clockISR() {
  if (!extClockMode) return;  // Im internen Modus externe Pulse ignorieren
  uint32_t now = micros();
  uint32_t period = now - lastExtClockUs;

  bool isRestart = false;
  if (extClockReceived) {
    uint32_t expected = measuredPeriodUs;
    // Pause laenger als 2× die letzte Periode → Neustart erkennen
    if (expected > 0) {
      isRestart = (period > expected * 2u);
    } else {
      isRestart = (period > 2000000UL);  // Fallback: >2 s = echter Stopp (war 500 ms = 120 BPM-Grenze)
    }
    if (isRestart) {
      measuredPeriodUs = 0;  // Reset: Neukalibrierung ab nächstem Puls
    } else if (period > 500) {
      measuredPeriodUs = period;
    }
  }

  lastExtClockUs   = now;
  extClockReceived = true;
  if (isRestart) {
    pendingReset = true;  // Main-Loop setzt alle Zaehler auf 0
  }
  if (pendingTicks != 0xFFFFFFFFu) pendingTicks++;
}

// Zweck: Setzt Reset-Flag bei externem Reset-Puls (active-low, FALLING-Flanke).
// Assumptions: Nur vom Interrupt-Kontext aufgerufen (Pin RESET_IN_PIN).
void resetISR() {
  pendingReset = true;
}

// Zweck: Zaehlt ausstehende Timer-Ticks fuer die Verarbeitung im Hauptloop.
// Side Effects: schreibt auf die globale Variable pendingTicks.
// Assumptions: Wird nur vom IntervalTimer-ISR-Kontext aufgerufen.
void timerISR() {
  // ISR bleibt bewusst minimal: nur Tick zaehlen, keine blockierenden Calls.
  if(pendingTicks != 0xFFFFFFFFu){
    pendingTicks++;
  }
}

// Zweck: Holt genau einen ausstehenden Timer-Tick atomar ab.
// Side Effects: verringert pendingTicks um 1.
// Assumptions: Wird nur im Hauptkontext aufgerufen.
static bool consumePendingTick(){
  noInterrupts();
  uint32_t pending = pendingTicks;
  if(pending == 0){
    interrupts();
    return false;
  }
  pendingTicks = pending - 1;
  interrupts();
  return true;
}

// Zweck: Wendet den aktuellen BPM-Wert an und aktualisiert den IntervalTimer.
// Side Effects: aktualisiert iTim, DurationOfOneStep und den Timer-Status.
// Assumptions: bpm ist ein globaler Wert im Bereich BPM_MIN..BPM_MAX.
void applyBpm(){
  bpm = clampVal((int)bpm, BPM_MIN, BPM_MAX);
  if(bpm == 0){
    myTimer.end();
    DurationOfOneStep = 0;
    noInterrupts();
    pendingTicks = 0;
    interrupts();
    return;
  }
  iTim = (unsigned int)(60.0 / (double)bpm * 1000000.0);
  if(iTim < 1){
    iTim = 1;
  }
  DurationOfOneStep = iTim;
  if(extClockMode){
    myTimer.end();  // Im ext-Modus keinen internen Timer starten
    noInterrupts();
    pendingTicks = 0;
    interrupts();
    return;
  }
  myTimer.end();
  myTimer.begin(timerISR, iTim);
}

// Zweck: Wechselt zwischen internem und externem Clock-Modus.
// Side Effects: startet/stoppt den IntervalTimer; setzt ext-Clock-Zustand zurueck.
void setExtClockMode(bool v) {
  extClockMode = v;
  noInterrupts();
  extClockReceived = false;
  measuredPeriodUs = 0;
  interrupts();
  extClockActive = false;
  if (!v) {
    applyBpm();  // Internen Timer neu starten
  } else {
    myTimer.end();
    noInterrupts();
    pendingTicks = 0;
    interrupts();
  }
  scheduleSaveParams();
}

// Zweck: Wendet EPatB auf EPat an (Sequenzgrenze).
// Side Effects: schreibt in EPatArr[idx].
// Assumptions: idx in 0..2; PatLen[idx] gueltig.
static void applyEPatBToEPat(int idx){
  if(idx < 0 || idx > 2) return;
  int len = clampVal(PatLen[idx], 1, 32);
  for(int i=0;i<len;i++){
    EPatArr[idx][i] = EPatBArr[idx][i];
  }
  for(int i=len;i<32;i++){
    EPatArr[idx][i] = false;
  }
}

// Zweck: Aktualisiert die UI fuer ein geaendertes Pattern.
// Side Effects: schreibt auf das TFT.
// Assumptions: idx in 0..2.
static void refreshUiForPatternUpdate(int idx){
  switch(GUIState){
    case EUCLCIRCS: {
      int R = (idx == 0) ? R1 : (idx == 1) ? R2 : R3;
      drawEucledianCircleFromPattern(R, PatLen[idx], PatRot[idx], EPatArr[idx]);
      break;
    }
    case EUCLPARAM1:
    case EUCLPARAM2:
    case EUCLPARAM3:
      if((idx == 0 && GUIState == EUCLPARAM1) ||
         (idx == 1 && GUIState == EUCLPARAM2) ||
         (idx == 2 && GUIState == EUCLPARAM3)){
        // Nur den Pattern-Kreis aktualisieren, um Hänger zu vermeiden.
        drawEucledianCircleFromPattern(R1, PatLen[idx], PatRot[idx], EPatArr[idx]);
      }
      break;
    default:
      break;
  }
}

// Zweck: Initialisiert Hardware, GUI-Status und startet den Takt-Timer.
// Side Effects: konfiguriert Pins, TFT/Touch, EEPROM-Parameter, Timer und globale States.
// Assumptions: Wird einmalig nach dem Start aufgerufen; Hardware ist korrekt verdrahtet.
void setup() {
  Serial.begin(115200);
  if (CrashReport) {
    uint32_t t = millis();
    while (!Serial && (millis() - t) < 2000) {}  // bis zu 2s auf USB-Host warten
    if (Serial) Serial.print(CrashReport);
  }

  // SD-Init zuerst: SD.begin() kann PLL2-Taktquellen umkonfigurieren und einen
  // kurzen Stromspike erzeugen. Passiert hier vor tft.begin(), damit das Display
  // danach in einem sauberen Zustand initialisiert wird.
  initStorage();

  pinMode(GATE_OUT1_PIN, OUTPUT);
  pinMode(GATE_OUT2_PIN, OUTPUT);
  pinMode(GATE_OUT3_PIN, OUTPUT);
  // 74HCT14 Schmitt-Trigger-Inverter: Idle-Pegel HIGH → Ausgang LOW (kein Gate)
  digitalWrite(GATE_OUT1_PIN, HIGH);
  digitalWrite(GATE_OUT2_PIN, HIGH);
  digitalWrite(GATE_OUT3_PIN, HIGH);

  initCvOutputs();

  setupEncoders();

  pinMode(CLOCK_IN_PIN, INPUT_PULLUP);
  pinMode(RESET_IN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), clockISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(RESET_IN_PIN), resetISR, FALLING);

  tft.begin(40000000); // SPI Speed 40 MHz (60 MHz führte zu DMA-Instabilität)
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3); // Screen rotation

  ts.begin();

  // 12-bit ADC fuer CV-Eingänge
  analogReadResolution(12);

  // RNG-Seed fuer Probabilistik
  randomSeed(analogRead(A0) + micros());

  initialText();
  delay(500);

  tft.fillScreen(ILI9341_BLACK);

  // Lade persistent gespeicherte Parameter (falls vorhanden)
  loadParams();
  applyBpm();

  // Zeichne Großkreis und Unterteilungspunkte (Euclid pattern 1)
  drawEucledianCircle(R1, PatLen[0], PatNum[0], PatRot[0], PatProb[0], EPatArr[0]);
  drawEucledianCircle(R2, PatLen[1], PatNum[1], PatRot[1], PatProb[1], EPatArr[1]);
  drawEucledianCircle(R3, PatLen[2], PatNum[2], PatRot[2], PatProb[2], EPatArr[2]);
  for(int i = 0; i < 3; i++) displayedPatLen[i] = PatLen[i];
  syncEPatBFromEPat(0);
  syncEPatBFromEPat(1);
  syncEPatBFromEPat(2);
  for(int i=0;i<3;i++){
    if(ProbEuclidRebuild[i] || PatProbAuto[i]){
      stageProbPatternFromCurrent(i);
    }
  }

  setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
  drawEncParamIndicators();
  drawBpmControls();

  GUIState = EUCLCIRCS;
  stillPressed = false;
}

// Zweck: Verarbeitet Touch, Timer-Ticks, GUI-Updates sowie Gate/CV-Ausgabe.
// Side Effects: schreibt auf TFT, DAC/PWM, Gate-Pins und globale Zustandsvariablen.
// Assumptions: Wird fortlaufend aufgerufen; Timer-ISR zaehlt pendingTicks.
void loop() {
  
  readCvInputs();
  applyCvTargets();

  // CV Slot-Sel: Slot laden wenn CV einen neuen Slot auswählt
  {
    static int8_t lastCvSlot = -1;
    if (cvSlotSel != lastCvSlot) {
      lastCvSlot = cvSlotSel;
      if (cvSlotSel >= 0 && (getSlotsUsedMask() & (1u << cvSlotSel))) {
        requestLoadSlot(cvSlotSel);
        resetQuickSavePointer();
        PendingPerfRefresh = true;
      }
    }
  }

  handleEncoders();

  // ----------- E X T E R N E R   C L O C K  -----------------------
  // Im ext-Modus: kein automatischer Fallback auf internen Timer —
  // wenn der externe Clock stoppt, bleibt der Sequencer stehen.
  if (extClockMode && extClockReceived) {
    if (!extClockActive) {
      extClockActive = true;
    }
    uint32_t p = measuredPeriodUs;
    if (p > 0) DurationOfOneStep = p;
  }

  // Auto-Reset nach langer Pause (>2 s) im externen Clock-Modus.
  // Stellt sicher, dass der Sequencer nach einem Stopp wieder bei Step 0 beginnt.
  if (extClockMode && extClockActive) {
    if ((uint32_t)(micros() - lastExtClockUs) > 2000000UL) {
      pendingReset   = true;
      extClockActive = false;  // Verhindert wiederholten Reset bis neuer Clock kommt
    }
  }

  // ----------- R E S E T  ------------------------------------------
  if (pendingReset) {
    noInterrupts();
    pendingReset    = false;
    pendingTicks    = 0;
    extClockReceived = false;  // Saubere Neukalibrierung ab nächstem Puls
    measuredPeriodUs = 0;
    interrupts();
    extClockActive = false;
    cnt     = 0;
    cnthold = 0;
    for (int ch = 0; ch < 3; ch++) {
        cntCh[ch]          = 0;
        cntChHold[ch]      = 0;
        chDivPhase[ch]     = 0;
        chSubTicksDone[ch] = 0;
    }
  }

  // ----------- T O U C H P A D ------------------------------------
  // DEBUG: Rohwerte fuer Touch-Kalibrierung — entfernen nach Kalibrierung
  // Ecken antippen, Min/Max in Serial Monitor ablesen, in hardware_map.h eintragen.
  // #define TOUCH_CALIBRATION_DEBUG
  #ifdef TOUCH_CALIBRATION_DEBUG
  if(ts.touched()){
    TS_Point p = ts.getPoint();
    Serial.printf("touch raw  x=%4d  y=%4d  z=%d\n", p.x, p.y, p.z);
  }
  #endif

  // Touchpad wurde gerade gedrückt (nicht gehalten)
  if(ts.touched()){
    if(stillPressed == false){
      stillPressed = true;

      // GUIState - tipPos - Actionen
      if(GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
         GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3){
        int mapX = 0;
        int mapY = 0;
        if(readTouchMapped(mapX, mapY)){
          tipPos = getTipPositionFromXY(mapX, mapY);
          if(GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3){
            int setIdx = (GUIState == VALUES1) ? 0 : (GUIState == VALUES2) ? 1 : 2;
            handleVALUES(setIdx, mapX, mapY, tipPos);
          }else{
            int setIdx = (GUIState == GATELEN1) ? 0 : (GUIState == GATELEN2) ? 1 : 2;
            handleGATELEN(setIdx, mapX, mapY, tipPos);
          }
        }
      }else{
        int mapX = 0;
        int mapY = 0;
        if(readTouchMapped(mapX, mapY)){
          // Tipp-Position bestimmen
          tipPos = getTipPositionFromXY(mapX, mapY);
          switch(GUIState){
            case EUCLCIRCS:
                switch(tipPos){
                  case P2U:
                      bpm = clampVal((int)bpm + 10, BPM_MIN, BPM_MAX);
                      applyBpm();
                      scheduleSaveParams();
                      drawBpmValue();
                      break;
                  case P2L:
                      bpm = clampVal((int)bpm - 10, BPM_MIN, BPM_MAX);
                      applyBpm();
                      scheduleSaveParams();
                      drawBpmValue();
                      break;
                  case P3U:
                      bpm = clampVal((int)bpm + 1, BPM_MIN, BPM_MAX);
                      applyBpm();
                      scheduleSaveParams();
                      drawBpmValue();
                      break;
                  case P3L:
                      bpm = clampVal((int)bpm - 1, BPM_MIN, BPM_MAX);
                      applyBpm();
                      scheduleSaveParams();
                      drawBpmValue();
                      break;
                  case UL:
                      GUIState = EUCLPARAM1; // new GUI-State
                      redrawParamFromPattern(0);
                      break;
                  case UR:
                      GUIState = EUCLPARAM2; // new GUI-State
                      redrawParamFromPattern(1);
                      break;
                  case LL:
                      GUIState = PERFORMANCE;
                      drawPerformanceScreen();
                      break;
                  case LR:
                      GUIState = EUCLPARAM3; // new GUI-State
                      redrawParamFromPattern(2);
                      break;
                  default:
                      break;          
                }
                break;
            case EUCLPARAM1:
                handleEUCLPARAM(0, mapX, mapY, tipPos);
                break;
            case EUCLPARAM2:
                handleEUCLPARAM(1, mapX, mapY, tipPos);
                break;
            case EUCLPARAM3:
                handleEUCLPARAM(2, mapX, mapY, tipPos);
                break;
            case PERFORMANCE:
                if(!PerfIgnoreUntilRelease && handlePerformance(mapX, mapY, tipPos)){
                  PerfIgnoreUntilRelease = true;
                }
                break;
            case XY1:
            case XY2:
            case XY3: {
                int xyIdx = (GUIState == XY1) ? 0 : (GUIState == XY2) ? 1 : 2;
                handleXYPAD(xyIdx, mapX, mapY, tipPos);
                break;
            }
            case PITCH1:
                handlePITCH(mapX, mapY, tipPos);
                break;
            case CV_CONFIG:
                handleCvConfig(mapX, mapY, tipPos);
                break;
            case GCONFIG:
                handleGConfig(mapX, mapY, tipPos);
                break;
            case NAV:
                handleNav(mapX, mapY);
                break;
            default:
                break;
          }
        }
      }
    }else{
      if(GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
         GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3 ||
         GUIState == PITCH1){
        int mapX = 0;
        int mapY = 0;
        if(readTouchMapped(mapX, mapY)){
          if(GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3){
            int setIdx = (GUIState == VALUES1) ? 0 : (GUIState == VALUES2) ? 1 : 2;
            handleVALUESDrag(setIdx, mapX, mapY);
          }else if(GUIState == PITCH1){
            handlePITCHDrag(mapX, mapY);
          }else{
            int setIdx = (GUIState == GATELEN1) ? 0 : (GUIState == GATELEN2) ? 1 : 2;
            handleGATELENDrag(setIdx, mapX, mapY);
          }
        }
      }
      else if(GUIState == PERFORMANCE){
        // Im Performance-Screen nur den initialen Touch verarbeiten.
      }
    }
  }       
  
  // Testen, Finger wieder vom Display genommen wurde
  if(!ts.touched() && stillPressed == true){
    stillPressed = false;
    PerfIgnoreUntilRelease = false;
    if(GUIState == XY1 || GUIState == XY2 || GUIState == XY3){
      scheduleSaveParams();
    }
  }
    

  // ----------- I N T E R V A L L T I M E R  ------------------------------
  // Wenn sich der Intervalltimer (BPM) gemeldet hat, dann folgendes durchführen
  // GUI-Aktionen hängen vom GUI-Zustand ab
  // Deferred-Masken: schwere Kreis-Redraws werden nach der Tick-Schleife ausgefuehrt
  uint8_t deferredRedrawMask = 0;   // bit → drawEucledianCircle (pendingCircleRedraw)
  uint8_t deferredProbMask   = 0;   // bit → drawEucledianCircleFromPattern (pendingProbRegen)
  int     deferredOldLen[3]  = {};
  while(consumePendingTick()){
    cnt++;
    lastGlobalTickUs = micros();

    // Per-Kanal Schrittzaehler vorrücken; chFired merkt, welche Kanaele diesen Tick feuern.
    // cntChHold wird NICHT hier gesetzt – es wird erst nach dem Zeichnen des Dots aktualisiert,
    // damit immer die zuletzt gezeichnete Position zum Loeschen verwendet wird.
    bool chFired[3] = { false, false, false };
    for (int ch = 0; ch < 3; ch++) {
        int spd = chSpeedIdx[ch];
        if (spd < 0) {
            // Division: ÷N — Kanal schreitet nur alle N globalen Ticks weiter
            int N = 1 - spd;  // spd=-1→N=2, spd=-2→N=3, spd=-3→N=4
            if (++chDivPhase[ch] >= (uint8_t)N) {
                chDivPhase[ch] = 0;
                cntCh[ch]++;
                chFired[ch] = true;
            }
        } else {
            // ×1 oder Multiplikation: einmal pro globalem Tick
            cntCh[ch]++;
            chSubTicksDone[ch] = 0;
            chFired[ch] = true;
        }
    }

    // Pending Preset-Load am Pattern-Start anwenden
    if(applyPendingLoadIfReady(cnt)){
      syncEPatBFromEPat(0);
      syncEPatBFromEPat(1);
      syncEPatBFromEPat(2);
      for(int i=0;i<3;i++){
        if(ProbEuclidRebuild[i] || PatProbAuto[i]){
          stageProbPatternFromCurrent(i);
        }
      }
      applyBpm();
      PendingCircsRedraw = true;
      PendingPerfRefresh = true;
      cnt = 0;
      cnthold = 0;
      for (int ch = 0; ch < 3; ch++) {
          cntCh[ch]          = 0;
          cntChHold[ch]      = 0;
          chDivPhase[ch]     = 0;
          chSubTicksDone[ch] = 0;
      }
    }

    // P-Flag: Sequenzgrenze -> vorbereitete Mutation uebernehmen
    bool patternUpdated[3] = { false, false, false };
    for(int i=0;i<3;i++){
      if(!PatProbAuto[i]) continue;
      int len = PatLen[i];
      if(len <= 0) continue;
      if((cntCh[i] % (unsigned int)len) == 0){
        applyEPatBToEPat(i);
        stageProbPatternFromCurrent(i);
        patternUpdated[i] = true;
      }
    }
    for(int i=0;i<3;i++){
      if(patternUpdated[i]){
        refreshUiForPatternUpdate(i);
      }
    }

    // Auto-Rotate: PatRot am Zyklusende automatisch erhoehen
    for (int ch = 0; ch < 3; ch++) {
        if (autoRotateStep[ch] == 0) continue;
        int len = clampVal(PatLen[ch], 1, 32);
        if ((cntCh[ch] % (unsigned int)len) == 0) {
            int newRot = PatRot[ch] + (int)autoRotateStep[ch];
            int maxRot = len - 1;
            while (newRot > maxRot) newRot -= len;
            PatRot[ch] = newRot;
            refreshUiForPatternUpdate(ch);
        }
    }

    // XY-Pad: Werte exakt beim Step sampeln, nur wenn Touch im Pad-Bereich
    if((GUIState == XY1 || GUIState == XY2 || GUIState == XY3) && ts.touched()){
      int mapX = 0;
      int mapY = 0;
      if(readTouchMapped(mapX, mapY)){
        int setIdx = (GUIState == XY1) ? 0 : (GUIState == XY2) ? 1 : 2;
        handleXYPADRecord(setIdx, mapX, mapY);
        scheduleSaveParams();
      }
    }

    // GUI-zustandsabhängige BPM-Timeractions
    switch(GUIState){
      case EUCLCIRCS: {
          const int Rs[3] = { R1, R2, R3 };
          for(int ch = 0; ch < 3; ch++){
              if((cntCh[ch] % (unsigned int)displayedPatLen[ch]) == 0){
                  if(pendingCircleRedraw[ch]){
                      pendingCircleRedraw[ch] = false;
                      bool lenChanged = (PatLen[ch] != displayedPatLen[ch]);
                      // Clear+Draw auf nach der Tick-Schleife verschieben (verhindert Tick-Burst)
                      deferredOldLen[ch] = displayedPatLen[ch];
                      displayedPatLen[ch] = PatLen[ch];
                      deferredRedrawMask |= (uint8_t)(1u << ch);
                      if(PatProbAuto[ch]) stageProbPatternFromCurrent(ch);
                      else                syncEPatBFromEPat(ch);
                      drawEncParamIndicator(ch);
                      if(lenChanged){
                          cntCh[ch]      = 0;
                          cntChHold[ch]  = 0;
                          chDivPhase[ch] = 0;
                      }
                  }
                  if(pendingProbRegen[ch]){
                      pendingProbRegen[ch] = false;
                      triggerProbAction(ch);  // Muster berechnen (kein SPI), Draw deferred
                      deferredOldLen[ch] = displayedPatLen[ch];
                      displayedPatLen[ch] = PatLen[ch];
                      deferredProbMask |= (uint8_t)(1u << ch);
                      drawEncParamIndicator(ch);
                  }
              }
          }
          // Nur Kanaele animieren die diesen Tick vorgerückt sind.
          // cntChHold wird NACH dem Zeichnen gesetzt → naechster Tick loescht genau dort.
          static const uint16_t CH_COLORS[3] = { ILI9341_YELLOW, ILI9341_RED, ILI9341_GREEN };
          for (int ch = 0; ch < 3; ch++) {
              if (!chFired[ch]) continue;
              // Kanal mit ausstehend em Voll-Redraw ueberspringen; Hold nachfuehren
              if ((deferredRedrawMask | deferredProbMask) & (uint8_t)(1u << ch)) {
                  cntChHold[ch] = cntCh[ch];
                  continue;
              }
              updateEucledianCircle(Rs[ch], displayedPatLen[ch], PatRot[ch], CH_COLORS[ch], EPatArr[ch], cntChHold[ch], cntCh[ch]);
              cntChHold[ch] = cntCh[ch];
          }
          break;
      }
      case VALUES1:
          drawValuesPlayhead(0, cntCh[0]);
          break;
      case VALUES2:
          drawValuesPlayhead(1, cntCh[1]);
          break;
      case VALUES3:
          drawValuesPlayhead(2, cntCh[2]);
          break;
      case GATELEN1:
          drawValuesPlayhead(0, cntCh[0]);
          break;
      case GATELEN2:
          drawValuesPlayhead(1, cntCh[1]);
          break;
      case GATELEN3:
          drawValuesPlayhead(2, cntCh[2]);
          break;
      case XY1:
          drawXYPlayhead(0, cntCh[0]);
          drawXYDotPlayhead(0, cntCh[0]);
          break;
      case XY2:
          drawXYPlayhead(1, cntCh[1]);
          drawXYDotPlayhead(1, cntCh[1]);
          break;
      case XY3:
          drawXYPlayhead(2, cntCh[2]);
          drawXYDotPlayhead(2, cntCh[2]);
          break;
      case PITCH1:
          tickPitchUi();
          if (chFired[0]) drawPitchPlayhead(cntCh[0]);
          break;
      case EUCLPARAM1:
      case EUCLPARAM2:
      case EUCLPARAM3:
          break;
      default:
         break;
    }

    // Swing-Maske: Kanaele, deren Gate verzögert feuert → DAC-Wert jetzt einfrieren
    uint8_t swingMask = 0;
    if (cvSwingPct > 0) {
        for (int ch = 0; ch < 3; ch++) {
            if (!chFired[ch] || !isSeqActive(ch)) continue;
            int len = PatLen[ch];
            if (len <= 0) continue;
            int idx    = (int)(cntCh[ch] % (unsigned int)len);
            int effRot = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
            if (EPatArr[ch][euclidRotatedSrc(idx, len, effRot)] && ((cntCh[ch] % 2u) == 0u)) {
                swingMask |= (uint8_t)(1u << ch);
            }
        }
    }
    outputValuesForStep(cnt, swingMask);

    // Gate-Trigger: Swing und Ratchet beruecksichtigen.
    // Nur Kanaele triggern die diesen Tick tatsaechlich vorgerueckt sind.
    for (int ch = 0; ch < 3; ch++) {
        if (!chFired[ch]) continue;
        if (!isSeqActive(ch)) { ratchetRemain[ch] = 0; swingPending[ch] = false; continue; }
        int len = PatLen[ch];
        if (len <= 0) continue;
        int idx    = (int)(cntCh[ch] % (unsigned int)len);
        int effRot = clampVal(PatRot[ch] + (int)cvPatRotOffset[ch], -(len - 1), len - 1);
        if (!EPatArr[ch][euclidRotatedSrc(idx, len, effRot)]) { ratchetRemain[ch] = 0; continue; }

        // Swing gilt fuer jeden 2. Step (0-indiziert: cntCh gerade → Step 2, 4, 6…)
        bool applySwing = (cvSwingPct > 0) && ((cntCh[ch] % 2u) == 0u);
        if (applySwing) {
            uint32_t swingDelay = (uint32_t)cvSwingPct * DurationOfOneStep / 100u;
            swingPending[ch]  = true;
            swingFireAt[ch]   = lastGlobalTickUs + swingDelay;
            swingGateDur[ch]  = gateLenForStep(ch, cntCh[ch]);
            ratchetRemain[ch] = 0;
        } else {
            swingPending[ch] = false;
            {
                int rIdx = RotateRatchet[ch] ? euclidRotatedSrc(idx, len, effRot) : idx;
                uint8_t perStepR = RatchetArr[ch][rIdx];
                uint8_t effR = (cvRatchetCount[ch] > perStepR) ? cvRatchetCount[ch] : perStepR;
                digitalWrite(GatePins[ch], LOW);
                gateOffAt[ch] = micros() + (effR > 1 ? GATE_PULSE_US : gateLenForStep(ch, cntCh[ch]));
                if (effR > 1 && DurationOfOneStep > 0) {
                    ratchetTotal[ch]    = effR;
                    ratchetRemain[ch]   = effR - 1;
                    ratchetInterval[ch] = DurationOfOneStep / (uint32_t)effR;
                    ratchetNextAt[ch]   = lastGlobalTickUs + ratchetInterval[ch];
                } else {
                    ratchetTotal[ch]  = 1;
                    ratchetRemain[ch] = 0;
                }
            }
        }
    }

    cnthold = cnt;
    // Auf Nicht-EUCLCIRCS Screens cntChHold nachfuehren (wird auf EUCLCIRCS bereits
    // direkt nach dem Zeichnen gesetzt; hier nur fuer Screen-Wechsel-Konsistenz).
    if (GUIState != EUCLCIRCS) {
        for (int ch = 0; ch < 3; ch++) cntChHold[ch] = cntCh[ch];
    }
  }

  // Deferred Kreis-Redraws nach der Tick-Schleife.
  // clearEucledianCircle + drawEucledianCircle brauchen ~30 ms — ausserhalb der
  // Tick-Schleife koennen waehrend dieser Zeit neue Ticks akkumulieren und werden
  // im naechsten loop()-Durchlauf glatt abgearbeitet, statt als Burst.
  if ((deferredRedrawMask | deferredProbMask) && GUIState == EUCLCIRCS) {
    const int Rs[3] = { R1, R2, R3 };
    for (int ch = 0; ch < 3; ch++) {
      if (deferredRedrawMask & (uint8_t)(1u << ch)) {
        if (PatLen[ch] != deferredOldLen[ch]) {
          // Length changed: arc-restore clears old positions, then draws new
          rebuildPattern(PatLen[ch], PatNum[ch], PatProb[ch], EPatArr[ch]);
          redrawEucledianCircleLenChange(Rs[ch], deferredOldLen[ch], PatLen[ch], PatRot[ch], EPatArr[ch]);
        } else {
          // Same length: only redraw changed dot positions (no full ring drawCircle)
          int len = PatLen[ch];
          bool oldPat[32];
          for (int i = 0; i < len; i++) oldPat[i] = EPatArr[ch][i];
          rebuildPattern(len, PatNum[ch], PatProb[ch], EPatArr[ch]);
          for (int i = 0; i < len; i++) {
            int src = euclidRotatedSrc(i, len, PatRot[ch]);
            if (EPatArr[ch][src] == oldPat[src]) continue;
            double ang = 2.0 * M_PI / len * i;
            int ox = (int)(CX + Rs[ch] * sin(ang) + 0.5);
            int oy = (int)(CY - Rs[ch] * cos(ang) + 0.5);
            clearAndRestoreRingArc(Rs[ch], ang, ox, oy);
            if (EPatArr[ch][src]) tft.fillCircle(ox, oy, r2+2, ILI9341_WHITE);
            else                  tft.drawCircle(ox, oy, r2, ILI9341_WHITE);
          }
        }
      } else if (deferredProbMask & (uint8_t)(1u << ch)) {
        // Same-len prob redraw: no redundant outer clear, no full ring drawCircle
        drawEucledianCircleFromPatternFast(Rs[ch], PatLen[ch], PatRot[ch], EPatArr[ch]);
      } else {
        continue;
      }
    }
  }

  // Deferred Pitch-Redraw: nach der Tick-Schleife, damit keine Ticks akkumulieren.
  // Synchrones Rendern (21ms) ist mit schnellem EEPROM-Save wieder unproblematisch.
  // Playhead wird direkt nach dem Render neu gesetzt → kein visueller "Stopp".
  if (pendingPitchDraw && GUIState == PITCH1) {
    pendingPitchDraw = false;
    drawPitchControls();
    drawPitchBars();
    drawPitchPlayhead(cntCh[0]);
  }

  // Sub-Ticks fuer ×N Kanaele (zwischen globalen Ticks, basiert auf micros())
  {
    uint32_t nowUs = micros();
    for (int ch = 0; ch < 3; ch++) {
        int spd = chSpeedIdx[ch];
        if (spd <= 0) continue;  // ÷N oder ×1: keine Sub-Ticks
        int N = spd + 1;  // ×2→N=2, ×3→N=3, ×4→N=4
        uint32_t period = (extClockActive && measuredPeriodUs > 0) ? measuredPeriodUs : iTim;
        if (period == 0) continue;
        uint32_t subInterval = period / (uint32_t)N;
        for (int sub = 1; sub < N; sub++) {
            if (chSubTicksDone[ch] >= (uint8_t)sub) continue;
            uint32_t fireAt = lastGlobalTickUs + (uint32_t)sub * subInterval;
            if ((int32_t)(nowUs - fireAt) >= 0) {
                chSubTicksDone[ch] = (uint8_t)sub;
                cntCh[ch]++;
                triggerGateForCh(ch);
                outputValuesForStep(cnt);
                // UI-Animation fuer Sub-Tick: Playhead Schritt fuer Schritt bewegen
                if (GUIState == EUCLCIRCS) {
                    static const uint16_t SUB_COLORS[3] = { ILI9341_YELLOW, ILI9341_RED, ILI9341_GREEN };
                    const int Rs[3] = { R1, R2, R3 };
                    updateEucledianCircle(Rs[ch], displayedPatLen[ch], PatRot[ch], SUB_COLORS[ch], EPatArr[ch], cntChHold[ch], cntCh[ch]);
                    cntChHold[ch] = cntCh[ch];
                } else {
                    // Welcher Kanal wird gerade auf dem Screen angezeigt?
                    int screenCh = -1;
                    switch (GUIState) {
                        case VALUES1: case GATELEN1: case XY1: screenCh = 0; break;
                        case VALUES2: case GATELEN2: case XY2: screenCh = 1; break;
                        case VALUES3: case GATELEN3: case XY3: screenCh = 2; break;
                        default: break;
                    }
                    if (screenCh == ch) {
                        if (GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
                            GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3) {
                            drawValuesPlayhead(ch, cntCh[ch]);
                        } else if (GUIState == PITCH1 && ch == 0) {
                            drawPitchPlayhead(cntCh[0]);
                        } else {
                            drawXYPlayhead(ch, cntCh[ch]);
                            drawXYDotPlayhead(ch, cntCh[ch]);
                        }
                    }
                }
            }
        }
    }
  }

  // Gate pulses off timing (non-blocking)
  uint32_t now = micros();
  for(int i=0;i<3;i++){
    uint32_t offAt = gateOffAt[i];
    if(offAt != 0 && (int32_t)(now - offAt) >= 0){
      digitalWrite(GatePins[i], HIGH); // 74HCT14: HIGH → Inverter-Ausgang LOW = Gate aus
      gateOffAt[i] = 0;
    }
  }

  // Ratchet Sub-Hits (zeitgesteuert, unabhaengig von Ticks)
  {
    uint32_t nowUs = micros();
    for (int ch = 0; ch < 3; ch++) {
        if (ratchetRemain[ch] > 0 && (int32_t)(nowUs - ratchetNextAt[ch]) >= 0) {
            ratchetRemain[ch]--;
            ratchetNextAt[ch] += ratchetInterval[ch];
            // ratchetIdx: Burst-Position dieses Sub-Hits (1..N-1)
            int ratchetIdx = (int)ratchetTotal[ch] - 1 - (int)ratchetRemain[ch];
            if (isSeqActive(ch)) {
                outputRatchetValue(ch, ratchetIdx, (int)ratchetTotal[ch]);
                digitalWrite(GatePins[ch], LOW);
                gateOffAt[ch] = micros() + GATE_PULSE_US;
            }
        }
    }
    // Swing: verzoegerte Gate-Trigger feuern + DAC-Wert nachholen
    for (int ch = 0; ch < 3; ch++) {
        if (swingPending[ch] && (int32_t)(nowUs - swingFireAt[ch]) >= 0) {
            swingPending[ch] = false;
            if (isSeqActive(ch)) {
                outputRatchetValue(ch, 0, 1);  // deferred new CV/Pitch value
                digitalWrite(GatePins[ch], LOW);
                gateOffAt[ch] = micros() + swingGateDur[ch];
            }
        }
    }
  }

  // Deferred save (debounce).
  // Schreibt nur wenn kein Tick aussteht (pt==0); akkumulierte Ticks danach verwerfen.
  // EEPROM.put() schreibt nur geänderte Bytes → typisch <5ms für einzelne Parameteränderung.
  if(PendingSave){
    uint32_t nowMs = millis();
    if((int32_t)(nowMs - PendingSaveAt) >= 0){
      noInterrupts();
      uint32_t pt = pendingTicks;
      interrupts();
      if(pt == 0){
        PendingSave = false;
        saveParams();
        noInterrupts(); pendingTicks = 0; interrupts();
      } else {
        PendingSaveAt = nowMs + 5;
      }
    }
  }

  // Deferred Slot-Save: LittleFS-Write vom Encoder-Handler in den Main-Loop verlagert.
  // Kein bpm==0-Guard: Nutzer hat explizit gespeichert → im nächsten tick-freien Moment ausführen.
  // Ticks, die während des LittleFS-Writes akkumulieren (ISR wird intern wieder freigegeben),
  // werden danach verworfen, damit kein Gate-Burst folgt.
  if (pendingSlotSaveSlot >= 0) {
    noInterrupts();
    uint32_t pt = pendingTicks;
    interrupts();
    if (pt == 0) {
      int slot = pendingSlotSaveSlot;
      pendingSlotSaveSlot = -1;
      saveParamsSlot(slot);
      noInterrupts();
      pendingTicks = 0;
      interrupts();
    }
  }

  // Deferred Slot-Move: P&P-Operation (SD-Datei kopieren + alte löschen).
  if (pendingSlotMoveFrom >= 0) {
    int from = pendingSlotMoveFrom;
    int to   = pendingSlotMoveTo;
    pendingSlotMoveFrom = -1;
    pendingSlotMoveTo   = -1;
    moveParamsSlot(from, to);
    noInterrupts(); pendingTicks = 0; interrupts();
    if (GUIState == PERFORMANCE) refreshPerfSlotState();
  }

  // Deferred Song-Op: Song-Save/Load/Delete in den Main-Loop verlagert (SD-Blocking).
  // Ticks die während der Operation akkumulieren werden danach verworfen.
  if (pendingSongOp > 0) {
    int op  = pendingSongOp;
    int num = pendingSongNum;
    pendingSongOp  = 0;
    pendingSongNum = -1;
    if      (op == 1) saveSong(num);
    else if (op == 2) loadSong(num);
    else if (op == 3) deleteSong(num);
    noInterrupts(); pendingTicks = 0; interrupts();
    if (GUIState == GCONFIG) refreshGConfigSongSelector();
  }

  // Sofort-Load wenn Sequencer gestoppt: applyPendingLoadIfReady läuft sonst nur in der
  // Tick-Schleife, die bei bpm==0 nie ausgeführt wird.
  if (bpm == 0 && applyPendingLoadIfReady(0, true)) {
    syncEPatBFromEPat(0);
    syncEPatBFromEPat(1);
    syncEPatBFromEPat(2);
    for(int i=0;i<3;i++){
      if(ProbEuclidRebuild[i] || PatProbAuto[i]){
        stageProbPatternFromCurrent(i);
      }
    }
    applyBpm();
    PendingCircsRedraw = true;
    PendingPerfRefresh = true;
    cnt = 0;
    cnthold = 0;
    for (int ch = 0; ch < 3; ch++) {
      cntCh[ch]          = 0;
      cntChHold[ch]      = 0;
      chDivPhase[ch]     = 0;
      chSubTicksDone[ch] = 0;
    }
  }

  // Performance-Button-Flash und Sequencer-Playhead aktualisieren
  if(GUIState == PERFORMANCE){
    tickPerformanceUi();
  }
  if(GUIState == CV_CONFIG){
    tickCvConfigUi();
  }
  tickProbButtonFlash();
  if (tickSaveToast()) {
    navigateToScreen(GUIState);  // Screen nach Toast-Ablauf neu zeichnen
  }

  // Vollbild-Redraw nach Slot-Load (ausserhalb der Tick-Schleife, keine Tick-Stauung)
  if(PendingCircsRedraw){
    PendingCircsRedraw = false;
    if(GUIState == EUCLCIRCS){
      tft.fillScreen(ILI9341_BLACK);
      setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
      drawEncParamIndicators();
      drawBpmControls();
      drawBpmValue();
      drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
      drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
      drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
      for(int i = 0; i < 3; i++) displayedPatLen[i] = PatLen[i];
    }
  }

  // Active-Slot Kennzeichnung aktualisieren (falls ein Load passiert ist)
  if(PendingPerfRefresh){
    if(GUIState == PERFORMANCE){
      drawPerformanceScreen();
    }
    PendingPerfRefresh = false;
  }

  // USB-Stack bedienen + WDOG3 kicken (verhindert Watchdog-Reset bei langem loop())
  yield();
}
