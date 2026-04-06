#include <gates.h>
#include <hardware_map.h>
#include <euclid.h>

namespace {

constexpr uint16_t MCP4822_CMD_DAC_A = 0x0000;
constexpr uint16_t MCP4822_CMD_DAC_B = 0x8000;
// Bit13=1 → 1× interner Gain → Vout_max = 2.048 V (MCP4822 an 3.3V, kein SPI-Levelshifter nötig).
// Externer TL074 (±12V) verstärkt auf 0–6.6V: Gain = 3.22 (R1=10k, R2=22k).
constexpr uint16_t MCP4822_CMD_GAIN1X = 0x2000;
constexpr uint16_t MCP4822_CMD_ACTIVE = 0x1000;
constexpr uint32_t MCP4822_SPI_HZ = 10000000UL;

SPISettings mcp4822SpiSettings(MCP4822_SPI_HZ, MSBFIRST, SPI_MODE0);

uint16_t scale8To12(uint8_t v) {
    return (uint16_t)(((uint32_t)v * 4095u + 127u) / 255u);
}

void writeMcp4822Channel(uint8_t csPin, bool channelB, uint16_t value12) {
    uint16_t cmd = channelB ? MCP4822_CMD_DAC_B : MCP4822_CMD_DAC_A;
    cmd = (uint16_t)(cmd | MCP4822_CMD_GAIN1X | MCP4822_CMD_ACTIVE | (value12 & 0x0FFFu));
    digitalWriteFast(csPin, LOW);
    SPI.transfer16(cmd);
    digitalWriteFast(csPin, HIGH);
}

void writeCvOutputsRaw(uint16_t out1, uint16_t out2, uint16_t out3) {
    SPI.beginTransaction(mcp4822SpiSettings);
    writeMcp4822Channel(MCP4822_CS_VALUE_PIN, false, out1);
    writeMcp4822Channel(MCP4822_CS_VALUE_PIN, true, out2);
    writeMcp4822Channel(MCP4822_CS_PITCH_PIN, false, out3);
    writeMcp4822Channel(MCP4822_CS_PITCH_PIN, true, 0);
    SPI.endTransaction();
}

} // namespace

// Zweck: Initialisiert SPI0 und beide MCP4822 fuer die CV-Ausgabe.
// Side Effects: konfiguriert CS Pins und setzt alle DAC-Kanaele auf 0 V.
// Assumptions: Die MCP4822 haengen an SPI0 (11/12/13) und reagieren auf CS=9/8. LDAC liegt auf GND.
void initCvOutputs() {
    SPI.begin();
    pinMode(MCP4822_CS_VALUE_PIN, OUTPUT);
    pinMode(MCP4822_CS_PITCH_PIN, OUTPUT);
    digitalWriteFast(MCP4822_CS_VALUE_PIN, HIGH);
    digitalWriteFast(MCP4822_CS_PITCH_PIN, HIGH);
    writeCvOutputsRaw(0, 0, 0);
}

// Zweck: Prueft, ob eine Spur aktuell aktiv ist (Mute/Solo).
// Side Effects: keine.
// Assumptions: MuteSeq/SoloSeq sind gesetzt.
static inline bool isSeqActive(int ch) {
    bool anySolo = SoloSeq[0] || SoloSeq[1] || SoloSeq[2];
    if (anySolo) {
        return SoloSeq[ch] && !MuteSeq[ch];
    }
    return !MuteSeq[ch];
}

// Zweck: Liefert die Dauer bis zum naechsten Hit im selben Pattern (in us).
// Side Effects: keine.
// Assumptions: DurationOfOneStep > 0 oder GATE_PULSE_US ist gesetzt.
static uint32_t durationToNextHit(int ch, unsigned int step) {
    int len = PatLen[ch];
    if (len <= 0) {
        return DurationOfOneStep > 0 ? DurationOfOneStep : GATE_PULSE_US;
    }
    int idx = step % len;
    for (int i = 1; i <= len; i++) {
        int nidx = (idx + i) % len;
        if (patternIsHit(ch, nidx)) {
            uint32_t d = (uint32_t)i * DurationOfOneStep;
            return (d > 0) ? d : GATE_PULSE_US;
        }
    }
    return DurationOfOneStep > 0 ? DurationOfOneStep : GATE_PULSE_US;
}

// Zweck: Berechnet die Gate-Laenge fuer einen Step.
// Side Effects: keine.
// Assumptions: GateHold/RotateGateLen sind konsistent mit Arrays; PatLen[ch] > 0.
uint32_t gateLenForStep(int ch, unsigned int step) {
    int len = PatLen[ch];
    if (len <= 0) {
        return GATE_PULSE_US;
    }
    if (!(*GateHoldArr[ch])) {
        return GATE_PULSE_US;
    }
    int idx = step % len;
    int src = RotateGateLen[ch] ? patternRotatedSrc(ch, idx) : idx;
    uint8_t v = GateLenArr[ch][src];
    if (v == 0) {
        return GATE_PULSE_US;
    }
    uint32_t maxLen = durationToNextHit(ch, step);
    if (v == 255) {  // GateHold ist hier bereits gesichert (siehe Zeile oben)
        return maxLen;
    }
    return (uint32_t)(GATE_PULSE_US + ((maxLen - GATE_PULSE_US) * (uint32_t)v) / 255U);
}

// Zweck: Triggert Gate-Ausgaenge, wenn der aktuelle Step ein Hit ist.
// Side Effects: schreibt auf Gate-Pins und gateOffAt.
// Assumptions: cnt ist aktuell; GatePins sind als OUTPUT konfiguriert.
void triggerGates() {
    for (int i = 0; i < 3; i++) {
        if (!isSeqActive(i)) continue;
        int len = PatLen[i];
        if (len <= 0) continue;
        int idx = cnt % len;
        if (patternIsHit(i, idx)) {
            digitalWrite(GatePins[i], LOW); // 74HCT14: LOW → Inverter-Ausgang HIGH = Gate an
            gateOffAt[i] = micros() + gateLenForStep(i, cnt);
        }
    }
}

// Zweck: Gibt CV-Werte fuer den aktuellen Step aus.
// Side Effects: schreibt auf die externen MCP4822 DACs.
// Assumptions: PatLen[ch] > 0; initCvOutputs() wurde im setup() aufgerufen.
void outputValuesForStep(unsigned int step) {
    static uint8_t lastOut[3] = { 0, 0, 0 };

    for (int ch = 0; ch < 3; ch++) {
        if (!isSeqActive(ch)) {
            lastOut[ch] = 0;
            continue;
        }
        int len = PatLen[ch];
        if (len <= 0) {
            lastOut[ch] = 0;
            continue;
        }

        int idx = step % len;
        bool hit = patternIsHit(ch, idx);
        int src = RotateValues[ch] ? patternRotatedSrc(ch, idx) : idx;
        uint8_t v = ValuesArr[ch][src];

        if (*HoldArr[ch]) {
            if (hit) {
                lastOut[ch] = v;
            }
        } else {
            lastOut[ch] = v;
        }
    }

    writeCvOutputsRaw(
        scale8To12(lastOut[0]),
        scale8To12(lastOut[1]),
        scale8To12(lastOut[2])
    );
}
