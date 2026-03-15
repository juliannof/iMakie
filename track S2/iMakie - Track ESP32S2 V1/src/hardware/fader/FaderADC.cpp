#include "FaderADC.h"



void FaderADC::begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(FADER_POT_PIN, ADC_0db);
    delay(30);                 // estabilizar antes del seed
    _emaValue = (float)analogRead(FADER_POT_PIN);  // seed correcto
}

void FaderADC::update() {
    int raw = analogRead(FADER_POT_PIN);
    _emaValue += FADER_EMA_ALPHA * (raw - _emaValue);
    int filtered = (int)_emaValue;

    // Clamp al rango físico real y normalizar a 13-bit (0–8191)
    filtered = constrain(filtered, FADER_ADC_MIN, FADER_ADC_MAX);
    _faderPos = map(filtered, FADER_ADC_MIN, FADER_ADC_MAX, 0, 8191);

    static uint32_t _lastPrint = 0;
    if (millis() - _lastPrint >= 100) {
        _lastPrint = millis();
        //Serial.printf("[FADER] raw=%5d  ema=%5d  pos=%5d\n", raw, filtered, _faderPos);
    }
}