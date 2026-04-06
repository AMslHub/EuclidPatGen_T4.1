#include <cv_inputs.h>
#include <hardware_map.h>

uint16_t cvRaw[3] = { 0, 0, 0 };

static const uint8_t CV_PINS[3] = { CV_IN_1_PIN, CV_IN_2_PIN, CV_IN_3_PIN };

void readCvInputs() {
    for (int i = 0; i < 3; i++) {
        // Invertierung kompensieren: Schaltung gibt 3.3V bei Vin=0V aus
        cvRaw[i] = 4095 - (uint16_t)analogRead(CV_PINS[i]);
    }
    // TODO: cvRaw[0..2] auf Zielparameter mappen (PatNum, PatLen, PatRot, ...)
}
