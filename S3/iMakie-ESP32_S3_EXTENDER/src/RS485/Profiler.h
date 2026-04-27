#pragma once
#include <Arduino.h>

// Profiler sin overhead — mide tiempos de operación RS485
class RS485Profiler {
public:
    struct Metrics {
        uint32_t txTime_us;      // tiempo de envío paquete master
        uint32_t rxWaitTime_us;  // tiempo esperando respuesta (timeout o éxito)
        uint32_t gapTime_us;     // tiempo en estado GAP
        uint32_t totalCycleTime_us;
        bool timeout;
        uint8_t slave_id;
    };

    static RS485Profiler& instance() {
        static RS485Profiler prof;
        return prof;
    }

    void markTxStart(uint8_t id) {
        _txStart_us = micros();
        _current.slave_id = id;
        _current.timeout = false;
    }

    void markRxWaitStart() {
        _rxWaitStart_us = micros();
    }

    void markRxSuccess() {
        _current.rxWaitTime_us = micros() - _rxWaitStart_us;
        _current.timeout = false;
        _recordMetric();
    }

    void markTimeout() {
        _current.rxWaitTime_us = micros() - _rxWaitStart_us;
        _current.timeout = true;
        _recordMetric();
    }

    void markGapStart() {
        _gapStart_us = micros();
    }

    void markGapEnd() {
        _current.gapTime_us = micros() - _gapStart_us;
    }

    void markCycleEnd() {
        _current.totalCycleTime_us = micros() - _txStart_us;
    }

    // Reporta cada N ciclos (no bloquea)
    void reportIfNeeded(uint32_t cycleNumber, uint32_t reportInterval = 100) {
        if (cycleNumber % reportInterval == 0) {
            _reportStats(cycleNumber);
        }
    }

private:
    Metrics _current = {};
    uint32_t _txStart_us = 0;
    uint32_t _rxWaitStart_us = 0;
    uint32_t _gapStart_us = 0;

    // Stats acumuladas
    uint32_t _totalSampleCount = 0;
    uint32_t _maxRxWaitTime = 0;
    uint32_t _minRxWaitTime = UINT32_MAX;
    uint32_t _sumRxWaitTime = 0;
    uint32_t _timeoutCount = 0;

    void _recordMetric() {
        _totalSampleCount++;
        if (_current.timeout) {
            _timeoutCount++;
        }
        _sumRxWaitTime += _current.rxWaitTime_us;
        if (_current.rxWaitTime_us > _maxRxWaitTime)
            _maxRxWaitTime = _current.rxWaitTime_us;
        if (_current.rxWaitTime_us < _minRxWaitTime)
            _minRxWaitTime = _current.rxWaitTime_us;
    }

    void _reportStats(uint32_t cycleNum) {
        if (_totalSampleCount == 0) return;

        uint32_t avgRxWait = _sumRxWaitTime / _totalSampleCount;
        float timeoutRate = (float)_timeoutCount / _totalSampleCount * 100.0f;

        log_i("[PROF] Ciclo %u: RX_WAIT avg=%uµs min=%uµs max=%uµs TO:%.1f%% (%u/%u)",
              cycleNum, avgRxWait, _minRxWaitTime, _maxRxWaitTime,
              timeoutRate, _timeoutCount, _totalSampleCount);

        // Reset para siguientes 100 ciclos
        _totalSampleCount = 0;
        _maxRxWaitTime = 0;
        _minRxWaitTime = UINT32_MAX;
        _sumRxWaitTime = 0;
        _timeoutCount = 0;
    }
};

extern RS485Profiler rs485prof;
