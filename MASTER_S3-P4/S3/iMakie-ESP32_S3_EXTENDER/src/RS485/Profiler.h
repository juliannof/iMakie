#pragma once
#include <Arduino.h>

// Profiler sin overhead — mide tiempos de operación RS485 con tracking per-slave
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

    struct SlaveStats {
        uint32_t txCount;
        uint32_t rxCount;
        uint32_t timeoutCount;
        uint32_t crcErrors;
        uint32_t idMismatchCount;
        uint32_t sumRxWait;
        uint32_t maxRxWait;
        uint32_t minRxWait;
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

    void recordCrcError(uint8_t slave_id) {
        if (slave_id < 1 || slave_id > 8) return;
        _slaveStats[slave_id].crcErrors++;
    }

    void recordIdMismatch(uint8_t expected_id, uint8_t received_id) {
        if (expected_id < 1 || expected_id > 8) return;
        _slaveStats[expected_id].idMismatchCount++;
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
    void reportIfNeeded(uint32_t cycleNumber, uint32_t reportInterval = 100, bool verbose = false) {
        if (cycleNumber % reportInterval == 0) {
            _reportStats(cycleNumber, verbose);
        }
    }

private:
    Metrics _current = {};
    uint32_t _txStart_us = 0;
    uint32_t _rxWaitStart_us = 0;
    uint32_t _gapStart_us = 0;

    // Stats acumuladas (global)
    uint32_t _totalSampleCount = 0;
    uint32_t _maxRxWaitTime = 0;
    uint32_t _minRxWaitTime = UINT32_MAX;
    uint32_t _sumRxWaitTime = 0;
    uint32_t _timeoutCount = 0;

    // Stats per-slave (8 slaves máximo)
    SlaveStats _slaveStats[9] = {};  // índice 1-8

    void _recordMetric() {
        uint8_t id = _current.slave_id;
        if (id < 1 || id > 8) return;

        _totalSampleCount++;
        _slaveStats[id].txCount++;

        if (_current.timeout) {
            _timeoutCount++;
            _slaveStats[id].timeoutCount++;
        } else {
            _slaveStats[id].rxCount++;
            _slaveStats[id].sumRxWait += _current.rxWaitTime_us;
            if (_current.rxWaitTime_us > _slaveStats[id].maxRxWait)
                _slaveStats[id].maxRxWait = _current.rxWaitTime_us;
            if (_current.rxWaitTime_us < _slaveStats[id].minRxWait)
                _slaveStats[id].minRxWait = _current.rxWaitTime_us;
        }

        _sumRxWaitTime += _current.rxWaitTime_us;
        if (_current.rxWaitTime_us > _maxRxWaitTime)
            _maxRxWaitTime = _current.rxWaitTime_us;
        if (_current.rxWaitTime_us < _minRxWaitTime)
            _minRxWaitTime = _current.rxWaitTime_us;
    }

    void _reportStats(uint32_t cycleNum, bool verbose = false) {
        if (_totalSampleCount == 0) return;

        uint32_t avgRxWait = _sumRxWaitTime / _totalSampleCount;
        float timeoutRate = (float)_timeoutCount / _totalSampleCount * 100.0f;

        // Reporte global (siempre)
        log_i("[PROF] Ciclo %u: RX_WAIT avg=%uµs min=%uµs max=%uµs TO:%.1f%% (%u/%u)",
              cycleNum, avgRxWait, _minRxWaitTime, _maxRxWaitTime,
              timeoutRate, _timeoutCount, _totalSampleCount);

        // Reporte per-slave (modo verbose)
        if (verbose) {
            for (uint8_t id = 1; id <= 8; id++) {
                if (_slaveStats[id].txCount == 0) continue;
                uint32_t slaveAvg = _slaveStats[id].rxCount > 0
                    ? _slaveStats[id].sumRxWait / _slaveStats[id].rxCount
                    : 0;
                float slaveTo = (float)_slaveStats[id].timeoutCount / _slaveStats[id].txCount * 100.0f;
                log_i("[PROF]   Slave %d: RX=%u TO=%u CRC=%u ID_MM=%u avg=%uµs min=%uµs max=%uµs (TO:%.0f%%)",
                      id, _slaveStats[id].rxCount, _slaveStats[id].timeoutCount,
                      _slaveStats[id].crcErrors, _slaveStats[id].idMismatchCount,
                      slaveAvg, _slaveStats[id].minRxWait, _slaveStats[id].maxRxWait, slaveTo);
            }
        }

        // Reset para siguientes ciclos
        _totalSampleCount = 0;
        _maxRxWaitTime = 0;
        _minRxWaitTime = UINT32_MAX;
        _sumRxWaitTime = 0;
        _timeoutCount = 0;
        for (uint8_t i = 0; i < 9; i++) {
            _slaveStats[i] = {};
        }
    }
};

extern RS485Profiler rs485prof;
