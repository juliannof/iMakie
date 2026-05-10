#include "FaderADC.h"
#include "../../config.h"

volatile bool FaderADC::_newData = false;

void IRAM_ATTR FaderADC::_alertISR() {
    _newData = true;
}

void FaderADC::begin() {
    _i2c.begin(ADS_SDA_PIN, ADS_SCL_PIN);
    _i2c.setClock(100000);

    if (!_ads.begin(ADS_I2C_ADDR, &_i2c)) {
        log_e("[ADC] ADS1115 not found at 0x%02X", ADS_I2C_ADDR);
        return;
    }

    _ads.setGain(GAIN_ONE);
    _ads.setDataRate(RATE_ADS1115_860SPS);

    pinMode(ADS_ALERT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ADS_ALERT_PIN),
                    FaderADC::_alertISR, FALLING);

    _ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);

    for (int i = 0; i < 10; i++) {
        if (_newData) {
            _newData = false;
            int raw = _ads.getLastConversionResults();
            if (raw < 0) raw = 0;
            _rawLast = raw;
            _emaValue = (float)raw;
            for (int i = 0; i < NOISE_WINDOW_SIZE; i++) _noiseWindow[i] = (float)raw;
            _adsLogIdx = 0;
            log_i("[ADC] ADS1115 OK  GAIN_ONE  860SPS  ALERT=IO%d  seed=%d", ADS_ALERT_PIN, _rawLast);
            return;  // Éxito
        }
        delay(10);
    }

    // Timeout: ISR no respondió
    log_e("[ADC] ADS1115 ISR timeout — no data en 100ms");
}

void FaderADC::update() {
    if (!_newData) return;
    _newData = false;

    int16_t adcRaw = _ads.getLastConversionResults();
    if (adcRaw < 0) adcRaw = 0;

    // Validar rango esperado (0–27000)
    if (adcRaw > MOTOR_ADC_MAX) {
        log_w("[ADC] Valor fuera de rango: %d (máx %d)", adcRaw, MOTOR_ADC_MAX);
        return;  // Descartar lectura inválida
    }

    _faderPos = (uint16_t)adcRaw;
    _rawLast  = (int)adcRaw;

    _logReading(adcRaw, _faderPos);
}

void FaderADC::measureRange() {
    // ⚠️ ADVERTENCIA: Esta función es BLOQUEANTE 5 segundos
    // S2 es single-core → RS485 se congela, Motor pierde sincronización
    //
    // Impacto:
    // - SAT menu no responde durante 5s
    // - RS485 no envía respuesta → Master timeout (~20ms poll cycle)
    // - Master marca slave como NO_CALIBRATED tras 3 reintentos
    // - Motor aborta calibración en progreso
    //
    // Usar SOLO en diagnóstico cuando sea necesario conocer rango ADS real.
    // NO llamar durante operación normal o calibración activa.
    //
    // TODO: Implementar versión no-bloqueante con máquina de estados en SAT menu

    int minVal = 32767, maxVal = 0;

    log_i("[ADC] measureRange — BLOQUEANTE 5s: mueve fader extremo a extremo");
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
    log_i("[ADC] RANGE  min=%d  max=%d  span=%d", minVal, maxVal, maxVal - minVal);
}

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