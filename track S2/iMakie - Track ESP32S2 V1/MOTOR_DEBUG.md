# Motor Debug — ESP32-S2 Track Control

**Sesión:** 2026-05-10 21:58  
**Objetivo:** Diagnosticar por qué el motor no responde a comandos de bajar

---

## HALLAZGOS CONFIRMADOS

### 1. Cables del motor están invertidos en hardware
- **Evidencia:** Ambas direcciones (_hwUp e _hwDown) producen el mismo comportamiento inicialmente
- **Root cause:** Conexión física en DRV8833 está invertida
- **Solución implementada:** Invertir lógica en software (_hwUp usa IN2, _hwDown usa IN1)

### 2. PWM_MIN (65) es INSUFICIENTE
- **Prueba:** testUpDown() alternando UP/DOWN cada 3-10 segundos con PWM_MIN=65
- **Resultado:** Motor NO se mueve en ninguna dirección
- **ADC:** Fluctúa alrededor de 404-407 sin cambios significativos
- **Conclusión:** Motor requiere PWM > 65 para vencer inercia/fricción

### 3. Motor SÍ responde a PWM mayor (previamente con PWM_MAX=130)
- **Evidencia:** En prueba anterior, fader bajó de ~26400 a valores bajos cuando se aplicó comando (aunque configuración estaba confusa)
- **Implicación:** Motor funciona con PWM suficiente, pero tiene un "dead band"

---

## PROBLEMA ACTUAL

**Solo se logró movimiento con PWM_MAX=130 (49.6% duty), pero:**
- Ambas direcciones (_hwUp e _hwDown) parecieron hacer lo mismo inicialmente
- No hay confirmación clara de que **DOWN funciona**
- Posibilidad: **IN1 no está conectada** en el DRV8833

---

## CÓDIGO ACTUAL

### Motor.cpp (_hwUp / _hwDown)
```cpp
static void _hwUp(uint8_t pwm) {
    log_d("[HW] UP - EN=HIGH IN1=0 IN2=%d (cables invertidos)", pwm);
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
}

static void _hwDown(uint8_t pwm) {
    log_d("[HW] DOWN - EN=HIGH IN1=%d IN2=0 (cables invertidos)", pwm);
    digitalWrite(MOTOR_EN, HIGH);
    analogWrite(MOTOR_IN1, pwm);
    analogWrite(MOTOR_IN2, 0);
}
```

### Motor.cpp (testUpDown)
```cpp
void testUpDown() {
    static uint32_t testStartTime = 0;
    static int testPhase = 0;

    if (testStartTime == 0) {
        testStartTime = millis();
    }

    uint32_t elapsed = millis() - testStartTime;
    uint32_t cycleTime = elapsed % 20000;  // 20s: 10s UP + 10s DOWN

    if (cycleTime < 10000) {
        _hwUp(PWM_MIN);  // PWM_MIN = 65
        if (testPhase != 0) {
            testPhase = 0;
            log_i("[TEST] UP — PWM_MIN=%d en IN2", PWM_MIN);
        }
    } else {
        _hwDown(PWM_MIN);
        if (testPhase != 1) {
            testPhase = 1;
            log_i("[TEST] DOWN — PWM_MIN=%d en IN1", PWM_MIN);
        }
    }
}
```

---

## PROXIMOS PASOS

### 1. Verificar si IN1 está conectada
- Cambiar testUpDown() a usar PWM_MAX (130)
- Observar si DOWN mueve el fader
- Si DOWN no mueve pero UP sí → **IN1 está suelta o no conectada**

### 2. Si IN1 está bien
- Encontrar PWM umbral exacto (entre 65 y 130)
- Determinar si es un problema de dead band normal o hardware dañado

### 3. Si solo UP funciona
- Revisar conexión física de IN1 en DRV8833
- Posible sustitución de DRV8833 si está dañado

---

## CONFIGURACIÓN

- **Motor Driver:** DRV8833 H-bridge
- **EN Pin:** GPIO14 (nSLEEP, activo alto)
- **IN1 Pin:** GPIO18
- **IN2 Pin:** GPIO16
- **PWM Freq:** 20 kHz, 8-bit (0-255)
- **PWM_MIN:** 65 (insuficiente)
- **PWM_MAX:** 130 (anteriormente suficiente, no confirmado)

---

## LOG REPRESENTATIVO (PWM_MIN=65, sin movimiento)

```
[TEST] DOWN — PWM_MIN=65 en IN1
ADC=405 (sin cambio)
...
[TEST] UP — PWM_MIN=65 en IN2
ADC=405 (sin cambio)
...
(cuando usuario mueve fader manualmente)
ADC=408 → ADC=26 (cambio manual, no del motor)
```

---

## NOTAS

- FaderTouch::isTouched() eliminado temporalmente (causaba _hwBrake ejecutándose continuamente)
- testUpDown() cronometrado para permitir observación en osciloscopio y cambios manuales de cables
- Ambas funciones _hwUp/_hwDown tienen logs en debug level (no inundan serial)
