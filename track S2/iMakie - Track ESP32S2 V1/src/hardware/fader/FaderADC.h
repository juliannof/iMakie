#pragma once
#include <Arduino.h>

#ifdef USE_ADS1015
  #include <Wire.h>
  #include <Adafruit_ADS1X15.h>
#else
  #include "esp_adc/adc_oneshot.h"
#endif

#include "../../config.h"    // ← añadir esta línea


class FaderADC {
public:
    void     begin();
    void     update();
    void     measureRange();                              // diagnóstico de ruido en raw
    uint16_t getFaderPos() const { return _faderPos; }  // raw EMA (≈1200–6700)
    int      getRawLast()  const { return _rawLast;  }  // raw ADC sin filtrar

#ifdef USE_ADS1015
    void dumpAdsLog();  // Volcado CSV de buffer circular (timestamp,raw,pos)
#endif

private:
#ifdef USE_ADS1015
    // ADS1115 I2C ADC
    Adafruit_ADS1115 _ads;
    TwoWire _i2c = TwoWire(1);
    static volatile bool _newData;

    // Logging circular — no-bloqueante con timestamp
    struct AdsReading {
        uint32_t timestamp;
        int16_t  raw;
        uint16_t pos;
    };
    static const int ADS_LOG_SIZE = 256;
    AdsReading _adsLog[ADS_LOG_SIZE];
    int _adsLogIdx = 0;

    void _logReading(int16_t raw, uint16_t pos) {
        _adsLog[_adsLogIdx] = {millis(), raw, pos};
        _adsLogIdx = (_adsLogIdx + 1) % ADS_LOG_SIZE;
    }

    static void IRAM_ATTR _alertISR();
#else
    // ADC nativo ESP32-S2
    adc_oneshot_unit_handle_t _adcHandle = nullptr;
#endif

    // Miembros comunes a ambas implementaciones
    float    _emaValue   = 0.0f;
    uint16_t _faderPos   = 0;
    int      _rawLast    = 0;
    float    _noiseSpan  = 0.0f;
    float    _noiseWindow[NOISE_WINDOW_SIZE] = {};
    uint8_t  _noiseHead  = 0;
    bool _isTrending(float deadband) const;
};