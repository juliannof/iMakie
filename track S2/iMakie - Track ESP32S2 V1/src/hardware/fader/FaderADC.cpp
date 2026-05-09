#include "FaderADC.h"
#include "../../config.h"

// ============================================================
// ADS1115 ISR + Flag (bajo guardia USE_ADS1015)
// ============================================================
#ifdef USE_ADS1015
volatile bool FaderADC::_newData = false;

void IRAM_ATTR FaderADC::_alertISR() {
    _newData = true;
}
#endif

// ============================================================
// ADC Nativo (solo compilación sin ADS1015)
// ============================================================
#define FADER_ADC_UNIT    ADC_UNIT_1
#define FADER_ADC_CHANNEL ADC_CHANNEL_9   // GPIO10 en ESP32-S2
#define FADER_ADC_ATTEN   ADC_ATTEN_DB_11

void FaderADC::begin() {
#ifdef USE_ADS1015
    // ─── ADS1115 I2C Initialization ───
    _i2c.begin(ADS_SDA_PIN, ADS_SCL_PIN);
    _i2c.setClock(100000);

    if (!_ads.begin(ADS_I2C_ADDR, &_i2c)) {
        log_e("[ADC] ADS1115 not found at 0x%02X", ADS_I2C_ADDR);
        return;
    }

    _ads.setGain(GAIN_ONE);                    // ±4.096V — safe for 3.3V
    _ads.setDataRate(RATE_ADS1115_860SPS);    // 860 SPS continuous

    pinMode(ADS_ALERT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ADS_ALERT_PIN),
                    FaderADC::_alertISR, RISING);

    _ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);

    // Seed inicial
    int raw = 0;
    for (int i = 0; i < 10; i++) {
        if (_newData) {
            _newData = false;
            raw = _ads.getLastConversionResults();
            if (raw < 0) raw = 0;
            break;
        }
        delay(10);
    }
    _rawLast  = raw;
    _emaValue = (float)raw;
    for (int i = 0; i < NOISE_WINDOW_SIZE; i++) _noiseWindow[i] = (float)raw;
    _adsLogIdx = 0;

    log_i("[ADC] ADS1115 OK  GAIN_ONE  860SPS  ALERT=IO%d  seed=%d", ADS_ALERT_PIN, _rawLast);

#else
    // ─── ADC Nativo (ESP32-S2) ───
    adc_oneshot_unit_init_cfg_t unitCfg = {
        .unit_id  = FADER_ADC_UNIT,
        .clk_src  = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitCfg, &_adcHandle));

    adc_oneshot_chan_cfg_t chanCfg = {
        .atten    = FADER_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_13,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(_adcHandle, FADER_ADC_CHANNEL, &chanCfg));

    int raw = 0;
    adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
    _rawLast  = raw;
    _emaValue = (float)raw;

    for (int i = 0; i < NOISE_WINDOW_SIZE; i++) _noiseWindow[i] = (float)raw;

    log_i("[ADC] seed=%d raw", _rawLast);
#endif
}

void FaderADC::update() {
#ifdef USE_ADS1015
    // ─── ADS1115: Ultra-lean — ISR-driven, no polling ───
    if (!_newData) return;  // No new data → return immediately (0µs cost)
    _newData = false;

    int16_t adcRaw = _ads.getLastConversionResults();
    if (adcRaw < 0) adcRaw = 0;

    _faderPos = (uint16_t)adcRaw;  // Raw directo, sin escalado FP — máxima velocidad
    _rawLast  = (int)adcRaw;

    _logReading(adcRaw, _faderPos);  // No-bloqueante: copia a buffer circular

#else
    // ─── ADC Nativo: Oversampling + EMA ───
    // 1. Oversampling — 3 grupos × 8 muestras
    auto readGroup = [&]() -> float {
        uint32_t s = 0;
        for (int i = 0; i < 8; i++) {
            int r = 0;
            adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &r);
            s += r;
        }
        return (float)s / 8.0f;
    };

    float a = readGroup();
    float b = readGroup();
    float c = readGroup();

    // 2. Mediana de los 3 grupos
    float median;
    if ((a <= b && b <= c) || (c <= b && b <= a)) median = b;
    else if ((b <= a && a <= c) || (c <= a && a <= b)) median = a;
    else median = c;

    // 3. Ventana deslizante — ruido local
    _noiseWindow[_noiseHead] = median;
    _noiseHead = (_noiseHead + 1) % NOISE_WINDOW_SIZE;

    float nMin = 8191.0f, nMax = 0.0f;
    for (int i = 0; i < NOISE_WINDOW_SIZE; i++) {
        if (_noiseWindow[i] < nMin) nMin = _noiseWindow[i];
        if (_noiseWindow[i] > nMax) nMax = _noiseWindow[i];
    }
    _noiseSpan = nMax - nMin;

    // 4. Detección de movimiento: umbral clásico OR tendencia direccional
    float diff        = fabsf(median - _emaValue);
    float threshMove  = _noiseSpan * NOISE_K_MOVE;
    float threshMicro = _noiseSpan * NOISE_K_MICRO;

    bool moving   = (diff > threshMove);
    bool trending = _isTrending(20.0f);

    if (moving || trending) {
        _emaValue += FADER_EMA_ALPHA_FAST * (median - _emaValue);
    } else if (diff > threshMicro) {
        _emaValue += (FADER_EMA_ALPHA_FAST * 0.05f) * (median - _emaValue);
    }

    _rawLast  = (int)median;
    _faderPos = (uint16_t)(_emaValue + 0.5f);

    // 5. Log solo cuando hay actividad
    if (diff > threshMicro || trending) {
        log_v("[ADC] med=%.0f ema=%d span=%.0f diff=%.0f tMov=%.0f tr=%d",
              median, _faderPos, _noiseSpan, diff, threshMove, (int)trending);
    }
#endif
}

void FaderADC::measureRange() {
#ifdef USE_ADS1015
    // ─── ADS1115: Captura 5 segundos del buffer logging ───
    int minVal = 32767, maxVal = 0;

    log_i("[ADC] measureRange — mueve el fader de extremo a extremo (5s)...");
    uint32_t t0 = millis();
    while (millis() - t0 < 5000) {
        if (_newData) {
            _newData = false;
            int16_t raw = _ads.getLastConversionResults();
            if (raw < 0) raw = 0;
            if (raw < minVal) minVal = raw;
            if (raw > maxVal) maxVal = raw;
        }
    }
    log_i("[ADC] RANGE  min=%d  max=%d  span=%d  (ADS1115 16-bit raw)",
          minVal, maxVal, maxVal - minVal);

#else
    // ─── ADC Nativo: 100 muestras con EMA ───
    int minVal = 8191, maxVal = 0;
    float ema  = (float)_rawLast;

    for (int i = 0; i < 100; i++) {
        int raw = 0;
        adc_oneshot_read(_adcHandle, FADER_ADC_CHANNEL, &raw);
        ema += FADER_EMA_ALPHA_FAST * ((float)raw - ema);
        int pos = (int)ema;
        if (pos < minVal) minVal = pos;
        if (pos > maxVal) maxVal = pos;
        delay(10);
    }
    log_i("[ADC] RANGE  min=%d  max=%d  span=%d  (raw EMA, 13-bit)",
          minVal, maxVal, maxVal - minVal);
#endif
}

#ifdef USE_ADS1015
void FaderADC::dumpAdsLog() {
    log_i("[ADC] Dump circular buffer (256 muestras): timestamp,raw,pos");
    for (int i = 0; i < ADS_LOG_SIZE; i++) {
        Serial.printf("%u,%d,%d\n",
            _adsLog[i].timestamp,
            _adsLog[i].raw,
            _adsLog[i].pos);
    }
    log_i("[ADC] Dump complete");
}
#endif

bool FaderADC::_isTrending(float deadband) const {
    // Compara primera mitad de la ventana contra segunda
    // Si el desplazamiento neto es consistente → movimiento real
    float oldest = _noiseWindow[_noiseHead];
    float newest = _noiseWindow[(_noiseHead + NOISE_WINDOW_SIZE - 1) % NOISE_WINDOW_SIZE];
    
    // Contar cuántas muestras están por encima/debajo de la más antigua
    uint8_t up = 0, down = 0;
    for (uint8_t i = 1; i < NOISE_WINDOW_SIZE; i++) {
        float s = _noiseWindow[(_noiseHead + i) % NOISE_WINDOW_SIZE];
        if (s > oldest + deadband) up++;
        else if (s < oldest - deadband) down++;
    }
    return (up >= 5 || down >= 5);
}