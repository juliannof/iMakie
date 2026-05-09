#pragma once
#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#include "../../config.h"    // ← añadir esta línea


class FaderADC {
public:
    void     begin();
    void     update();
    void     measureRange();
    void     dumpAdsLog();
    uint16_t getFaderPos() const { return _faderPos; }
    int      getRawLast()  const { return _rawLast;  }

private:
    Adafruit_ADS1115 _ads;
    TwoWire _i2c = TwoWire(1);
    static volatile bool _newData;

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

    float    _emaValue   = 0.0f;
    uint16_t _faderPos   = 0;
    int      _rawLast    = 0;
    float    _noiseSpan  = 0.0f;
    float    _noiseWindow[NOISE_WINDOW_SIZE] = {};
    uint8_t  _noiseHead  = 0;
    bool _isTrending(float deadband) const;
};