#include <app_state.h>
#include <cv_inputs.h>
#include <euclid.h>
#include <gates.h>
#include <hardware_map.h>
#include <storage.h>
#include <ui_screens.h>
#include <ui_touch.h>

const int CX = 160;
const int CY = 120;   
 
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
ILI9341_t3 tft = ILI9341_t3(
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
bool RotateValues[3] = { false, false, false };
bool RotateGateLen[3] = { false, false, false };
uint32_t DurationOfOneStep = 0;  

// Performance (Mute/Solo)
bool MuteSeq[3] = { false, false, false };
bool SoloSeq[3] = { false, false, false };

// Deferred save
bool PendingSave = false;
uint32_t PendingSaveAt = 0;
const uint32_t SAVE_DEBOUNCE_MS = 400;

// Performance UI refresh after load
bool PendingPerfRefresh = false;

// Deferred full-screen redraw nach Slot-Load (verhindert fillScreen() in der Tick-Schleife)
bool PendingCircsRedraw = false;

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

// Externer Clock / Reset (active-low via MMBT3904-Transistor)
static volatile uint32_t lastExtClockUs    = 0;
static volatile bool     extClockReceived  = false; // ISR → Main-Loop Handshake
static volatile bool     pendingReset      = false;
static volatile uint32_t measuredPeriodUs  = 0;
static bool              extClockActive    = false; // nur im Main-Loop-Kontext
static const uint32_t    EXT_CLOCK_TIMEOUT_US = 3000000UL; // 3s ohne Puls → interner Timer

// Gate handling
const uint32_t GATE_PULSE_US = 5000; // 5 ms gate pulse
const uint8_t GatePins[3] = { GATE_OUT1_PIN, GATE_OUT2_PIN, GATE_OUT3_PIN };
volatile uint32_t gateOffAt[3] = { 0, 0, 0 };


// Zweck: Verarbeitet externen Clock-Puls (active-low, FALLING-Flanke).
// Misst Taktperiode fuer Gate-Laengen-Berechnung; zaehlt pendingTick.
// Assumptions: Nur vom Interrupt-Kontext aufgerufen (Pin CLOCK_IN_PIN).
void clockISR() {
  uint32_t now = micros();
  uint32_t period = now - lastExtClockUs;
  if (extClockReceived && period > 500 && period < EXT_CLOCK_TIMEOUT_US) {
    measuredPeriodUs = period;
  }
  lastExtClockUs   = now;
  extClockReceived = true;
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
  myTimer.end();
  myTimer.begin(timerISR, iTim);
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
  pinMode(GATE_OUT1_PIN, OUTPUT);
  pinMode(GATE_OUT2_PIN, OUTPUT);
  pinMode(GATE_OUT3_PIN, OUTPUT);
  // 74HCT14 Schmitt-Trigger-Inverter: Idle-Pegel HIGH → Ausgang LOW (kein Gate)
  digitalWrite(GATE_OUT1_PIN, HIGH);
  digitalWrite(GATE_OUT2_PIN, HIGH);
  digitalWrite(GATE_OUT3_PIN, HIGH);

  initCvOutputs();

  pinMode(CLOCK_IN_PIN, INPUT_PULLUP);
  pinMode(RESET_IN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), clockISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(RESET_IN_PIN), resetISR, FALLING);

  tft.begin();
  ts.begin();
  tft.setClock(60000000); // SPI Speed
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3); // Screen rotation

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
  syncEPatBFromEPat(0);
  syncEPatBFromEPat(1);
  syncEPatBFromEPat(2);
  for(int i=0;i<3;i++){
    if(ProbEuclidRebuild[i] || PatProbAuto[i]){
      stageProbPatternFromCurrent(i);
    }
  }

  setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
  drawBpmControls();

  GUIState = EUCLCIRCS;
  stillPressed = false;
}

// Zweck: Verarbeitet Touch, Timer-Ticks, GUI-Updates sowie Gate/CV-Ausgabe.
// Side Effects: schreibt auf TFT, DAC/PWM, Gate-Pins und globale Zustandsvariablen.
// Assumptions: Wird fortlaufend aufgerufen; Timer-ISR zaehlt pendingTicks.
void loop() {
  
  readCvInputs();

  // ----------- E X T E R N E R   C L O C K  -----------------------
  if (extClockReceived) {
    if (!extClockActive) {
      extClockActive = true;
      myTimer.end(); // internen Timer anhalten
    }
    uint32_t p = measuredPeriodUs;
    if (p > 0) DurationOfOneStep = p;

    // Timeout: kein Puls seit >3s → zurück zum internen Timer
    if ((uint32_t)(micros() - lastExtClockUs) > EXT_CLOCK_TIMEOUT_US) {
      noInterrupts();
      extClockReceived = false;
      measuredPeriodUs = 0;
      interrupts();
      extClockActive = false;
      applyBpm();
    }
  }

  // ----------- R E S E T  ------------------------------------------
  if (pendingReset) {
    noInterrupts();
    pendingReset = false;
    pendingTicks = 0;
    interrupts();
    cnt     = 0;
    cnthold = 0;
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
                if(tipPos == UL){
                  GUIState = EUCLPARAM1;
                  redrawParam(0);
                }
                break;
            case XY2:
                if(tipPos == UL){
                  GUIState = EUCLPARAM2;
                  redrawParam(1);
                }
                break;
            case XY3:
                if(tipPos == UL){
                  GUIState = EUCLPARAM3;
                  redrawParam(2);
                }
                break;
            default:
                break;
          }
        }
      }
    }else{
      if(GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3 ||
         GUIState == GATELEN1 || GUIState == GATELEN2 || GUIState == GATELEN3){
        int mapX = 0;
        int mapY = 0;
        if(readTouchMapped(mapX, mapY)){
          if(GUIState == VALUES1 || GUIState == VALUES2 || GUIState == VALUES3){
            int setIdx = (GUIState == VALUES1) ? 0 : (GUIState == VALUES2) ? 1 : 2;
            handleVALUESDrag(setIdx, mapX, mapY);
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
  while(consumePendingTick()){
    cnt++; 

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
      // Redraw nicht hier – fillScreen() blockiert ~50ms und laesst Ticks aufstauen.
      // Wird nach der Tick-Schleife per PendingCircsRedraw ausgefuehrt.
      PendingCircsRedraw = true;
      PendingPerfRefresh = true;
      // Lesezeiger nach Load auf Schritt 1 (Index 0) zuruecksetzen
      cnt = 0;
      cnthold = 0;
    }

    // P-Flag: Sequenzgrenze -> vorbereitete Mutation uebernehmen und naechste erzeugen.
    bool patternUpdated[3] = { false, false, false };
    for(int i=0;i<3;i++){
      if(!PatProbAuto[i]) continue;
      int len = PatLen[i];
      if(len <= 0) continue;
      if((cnt % (unsigned int)len) == 0){
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

    // XY-Pad: exakt beim Step sampeln, nur wenn Touch aktiv ist
    if((GUIState == XY1 || GUIState == XY2 || GUIState == XY3) && ts.touched()){
      int mapX = 0;
      int mapY = 0;
      if(readTouchMapped(mapX, mapY)){
        uint16_t pos = getTipPositionFromXY(mapX, mapY);
        int setIdx = (GUIState == XY1) ? 0 : (GUIState == XY2) ? 1 : 2;
        handleXYPAD(setIdx, mapX, mapY, pos);
        scheduleSaveParams();
      }
    }

    // Wahrscheinlichkeits-Autoupdates deaktiviert

    // GUI-zustandsabhängige BPM-Timeractions
    switch(GUIState){
      case EUCLCIRCS:
          // Eucledian-Circle-Grafik-Update (Laufpunkt in allen Circles um eins weitersetzen)
          updateEucledianCircle(R1, PatLen[0], PatRot[0], ILI9341_YELLOW, EPatArr[0]);
          updateEucledianCircle(R2, PatLen[1], PatRot[1], ILI9341_RED, EPatArr[1]);
          updateEucledianCircle(R3, PatLen[2], PatRot[2], ILI9341_GREEN, EPatArr[2]);
          break;
      case VALUES1:
          drawValuesPlayhead(0, cnt);
          break;
      case VALUES2:
          drawValuesPlayhead(1, cnt);
          break;
      case VALUES3:
          drawValuesPlayhead(2, cnt);
          break;
      case GATELEN1:
          drawValuesPlayhead(0, cnt);
          break;
      case GATELEN2:
          drawValuesPlayhead(1, cnt);
          break;
      case GATELEN3:
          drawValuesPlayhead(2, cnt);
          break;
      case XY1:
          drawXYPlayhead(0, cnt);
          break;
      case XY2:
          drawXYPlayhead(1, cnt);
          break;
      case XY3:
          drawXYPlayhead(2, cnt);
          break;
      case EUCLPARAM1:
      case EUCLPARAM2:
      case EUCLPARAM3:
          // Parameter-Menüs werden nur bei Benutzeraktion aktualisiert (verhindert Flackern)
          break; 
      default:
         break;
    }

    outputValuesForStep(cnt);
    // Hinweis: Falls DAC-Settling vor dem Gate-Trigger nötig ist, den Offset
    // in GATE_PULSE_US einrechnen statt hier blockierend zu warten.
    triggerGates();

    cnthold = cnt; 
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

  // Deferred save (debounce)
  if(PendingSave){
    uint32_t nowMs = millis();
    if((int32_t)(nowMs - PendingSaveAt) >= 0){
      PendingSave = false;
      saveParams();
    }
  }

  // Performance-Button-Flash und Sequencer-Playhead aktualisieren
  if(GUIState == PERFORMANCE){
    tickPerformanceUi();
  }
  tickProbButtonFlash();

  // Vollbild-Redraw nach Slot-Load (ausserhalb der Tick-Schleife, keine Tick-Stauung)
  if(PendingCircsRedraw){
    PendingCircsRedraw = false;
    if(GUIState == EUCLCIRCS){
      tft.fillScreen(ILI9341_BLACK);
      setMenuItems4EUCLCIRCS(ILI9341_LIGHTGREY);
      drawBpmControls();
      drawBpmValue();
      drawEucledianCircleFromPattern(R1, PatLen[0], PatRot[0], EPatArr[0]);
      drawEucledianCircleFromPattern(R2, PatLen[1], PatRot[1], EPatArr[1]);
      drawEucledianCircleFromPattern(R3, PatLen[2], PatRot[2], EPatArr[2]);
    }
  }

  // Active-Slot Kennzeichnung aktualisieren (falls ein Load passiert ist)
  if(PendingPerfRefresh){
    if(GUIState == PERFORMANCE){
      drawPerformanceScreen();
    }
    PendingPerfRefresh = false;
  }
    
}
