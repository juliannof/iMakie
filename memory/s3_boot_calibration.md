---
name: s3_boot_calibration
description: S3 escanea y calibra slaves secuencialmente en boot
metadata:
  type: project
---

## Arquitectura de Boot Calibration S3

**Estado:** En diseño (2026-05-13)

**Requisito:** S3 debe detectar y calibrar todos sus slaves (1-8) secuencialmente en boot, sin bloquear.

### Secuencia de Calibración (Ya Implementada)

1. **S3 envía FLAG_CALIB** en MasterPacket.flags (bit 4)
   - Implementado: RS485.cpp línea 133-136
   
2. **S2 calibra Motor** → envía min/max con flags CALIB_SENDING|CALIB_IS_MIN
   - Implementado: S2 RS485Handler.cpp (confirmado por usuario)
   
3. **S3 captura min/max** → almacena calibratedMin/Max
   - Implementado: RS485.cpp línea 208-215
   - Usa para mapeo: setFaderTarget() línea 350-357 (mapea 0-14848 → rango real)
   
4. **S3 marca calibrated=true** → espera target actual
   - Implementado: RS485.cpp línea 285 (actualmente bypass)

### Lo Que Falta

**Disparar calibración automática en boot:** no hay código que ordene `setCalibrate(id)` para cada slave.

Actualmente:
- setup() inicializa sistemas
- NO espera a que slaves calibren
- Todos se marcan como `calibrated=true` (bypass línea 285)
- Sistema funciona pero SIN validación de hardware real

### Decisión de Ubicación

**Opción elegida:** Core 1 (rs485.runTask), fuera del time-critical path
- No bloquea timing RS485 (SEND/WAIT_RESP/GAP)
- Check en `_nextSlave()` o `GAP`: si hay slave sin calibrar → dispara FLAG_CALIB
- Secuencial: una calibración a la vez

**Por qué NO en setup():**
- Si 8 slaves × 3-5s/cada = 30-40s de boot bloqueado (inaceptable)

**Por qué NOT Core 0 (taskCore0):**
- Core 0 maneja MIDI (tiempo sensible)
- Core 1 ya maneja RS485 (natural)

### Implementación Pendiente

```cpp
// En RS485.h ChannelData:
bool     calibrating   = false;   // flag de estado durante calibración
uint8_t  calibratingId = 0;       // slave actual en calibración

// En RS485.cpp runTask(), BusState::GAP:
if (!_calibrating) {
    for (uint8_t id = 1; id <= _numSlaves; id++) {
        if (!_ch[id].calibrated) {
            log_i("[RS485] Iniciando calibración Slave %d", id);
            _ch[id].calibrate = true;
            _calibrating = true;
            _calibratingId = id;
            break;  // Una a la vez
        }
    }
}
```

### Validación

- [ ] Implementar automático en Core1
- [ ] Test: S3 boot → escanea 1 slave → completa → escanea siguiente
- [ ] Verificar no afecta timing RS485
- [ ] Log progress: `[BOOT] Slave 1: MIN=44 MAX=26448 ✓`
