# Motor Calibración — Debug GOING_UP/DOWN Bloqueadas (2026-05-11 20:30)

## Estado Actual
Motor calibración se detiene en GOING_UP y GOING_DOWN. KICK_UP y KICK_DOWN funcionan perfectamente.

## Síntomas
```
KICK_UP:  30 → 26028 cuentas en 450ms ✓ Transiciona a GOING_UP
GOING_UP: 26471 cuentas → "Sin movimiento" error (300ms después)
          Motor NO se mueve
          
KICK_DOWN: 26465 → 977 cuentas en 424ms ✓ Transiciona a GOING_DOWN  
GOING_DOWN: 27 cuentas → "MOTOR BLOQUEADO" (237ms después)
            Motor NO se mueve
```

## Análisis Raíz

### Código GOING_UP (Motor.cpp línea 82-91)
```cpp
case CalibPhase::GOING_UP: {
    if (now < _motor_calibMinDetect) break;

    uint8_t pwmGoing = (pos < 26000) ? _pwm_max : _pwm_min;
    if (_motor_currentPWM != pwmGoing) {  // ← PROBLEMA AQUÍ
        if (pos < 26000) _hwUp(_pwm_max);
        else _hwUp(_pwm_min);
        _motor_currentPWM = pwmGoing;
    }
    // ... resto de código detección movimiento
}
```

### El Problema Exacto

**Cuando transiciona a GOING_UP:**
1. Entra con `pos = 26028` (ya > 26000)
2. Calcula `pwmGoing = _pwm_min` (135)
3. **Pero `_motor_currentPWM` ya ES 135** (establecido en última iteración de KICK_UP)
4. Condición `_motor_currentPWM != pwmGoing` es FALSA
5. El bloque if **NO entra**
6. **`_hwUp()` NUNCA se ejecuta**
7. Motor entra en GOING_UP **sin recibir comando PWM**
8. Se queda donde está → detecta "sin movimiento" después de 500ms

**Mismo problema en GOING_DOWN.**

### Por Qué KICK Funciona
KICK_UP/DOWN usan `_pwm_max` (165), que es diferente al anterior PWM:
- Al entrar en KICK_UP: `_motor_currentPWM = 0` (anterior)
- `pwmGoing = _pwm_max` (165)
- `0 != 165` → if entra → `_hwUp()` se ejecuta ✓

## Soluciones Propuestas

### Opción A: Eliminar el if (SIMPLE)
```cpp
uint8_t pwmTarget = (pos < 26000) ? _pwm_max : _pwm_min;
_hwUp(pwmTarget);  // ← Ejecuta SIEMPRE, cada iteración
_motor_currentPWM = pwmTarget;
```
**Pro:** Simple, robusto
**Con:** Slight overhead (llamadas repetidas, pero sin daño)

### Opción B: Detectar Transición de Fase
```cpp
static CalibPhase lastPhase = CalibPhase::IDLE;
bool phaseChanged = (lastPhase != CalibPhase::GOING_UP);
lastPhase = CalibPhase::GOING_UP;

uint8_t pwmTarget = (pos < 26000) ? _pwm_max : _pwm_min;
if (_motor_currentPWM != pwmTarget || phaseChanged) {
    _hwUp(pwmTarget);
    _motor_currentPWM = pwmTarget;
}
```
**Pro:** Solo ejecuta cuando cambia PWM o fase
**Con:** Más complejo, requiere variable estática por fase

### Opción C: Revisar Init _motor_currentPWM
Asegurar que `_motor_currentPWM` se resetea a 0 al entrar en GOING_UP:
```cpp
case CalibPhase::GOING_UP: {
    if (now < _motor_calibMinDetect) break;
    
    _motor_currentPWM = 0;  // ← Force reset
    uint8_t pwmTarget = (pos < 26000) ? _pwm_max : _pwm_min;
    if (_motor_currentPWM != pwmTarget) {  // Ahora siempre entra
        _hwUp(pwmTarget);
        _motor_currentPWM = pwmTarget;
    }
```
**Pro:** Garantiza entrada en if
**Con:** Artificial, no trata la raíz

## Recomendación
**Opción A (Eliminar if)** — Es la más robusta y simple. Motor debe recibir comando PWM cada iteración mientras está en fase activa, no solo cuando cambia.

## Tareas Mañana
1. Implementar Opción A en GOING_UP y GOING_DOWN
2. Test calibración completa
3. Verificar que SETTLE_UP y SETTLE_DOWN funcionan
4. Commit y documentar solución final

## Context Técnico NVS
- PWM_MIN en NVS: 135 (válido, 53% duty)
- PWM_MAX en NVS: 165 (válido)
- Fallback config.h: PWM_MIN=100, PWM_MAX=160
- initPWM() correcto — carga desde NVS con fallback a config.h

## Commits Relacionados
- e166b06: KICK phase basada en posición (26000/1000)
- 0ec46ee: GOING phases con 70% PWM (REVERTIDO)
- 212eaf1: initPWM() fallback a config.h
