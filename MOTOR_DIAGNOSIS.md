# Motor Diagnosis — S2 Track (2026-05-10 15:20)

## Problema
Motor no se mueve durante calibración. Específicamente: **no baja**.

## Síntomas Observados
1. Test mode comienza a los 3s
2. startCalib() dispara correctamente (una sola vez, fix aplicado)
3. Calibración ejecuta pero:
   - KICK_UP, GOING_UP, SETTLE_UP: ADC sube (posiblemente)
   - KICK_DOWN, GOING_DOWN, SETTLE_DOWN: ADC NO baja
   - Resultado: `top=3984, bot=3984` → ERROR (rango inválido)

## Hipótesis
**Motor solo se mueve en una dirección (probablemente UP).**

Posibles causas:
1. **PWM no llega a IN2** (DOWN control)
   - Cable suelto o desconectado
   - Soldadura mala en GPIO16
   - Pin no configurado correctamente
   
2. **DRV8833 falla en una dirección**
   - Uno de los MOSFET internos abierto/muerto
   - Alimentación de un lado solo
   
3. **Inverted logic aún incorrecta**
   - Aunque se invirtió `_hwUp/_hwDown`, quizás necesita re-invertir
   - O hay otra capa de inversión (cable físicamente al revés)

4. **Motor mecánicamente trabado hacia arriba**
   - Resorte o carga empujando hacia arriba
   - Fricción direccional (improbable)

## Investigación Requerida (Próxima sesión)

### 1. Test de PWM Individual
```cpp
// Agregar función de test directo en SAT menu o main.cpp
void testMotorPWM() {
    // Test UP
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    digitalWrite(MOTOR_EN, HIGH);
    
    analogWrite(MOTOR_IN1, 100);
    analogWrite(MOTOR_IN2, 0);
    delay(2000);
    
    // Observar: ¿motor sube?
    analogWrite(MOTOR_IN1, 0);
    delay(1000);
    
    // Test DOWN
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 100);
    delay(2000);
    
    // Observar: ¿motor baja?
    digitalWrite(MOTOR_EN, LOW);
}
```

### 2. Medir con Osciloscopio
- **EN (GPIO14):** ¿Sube a HIGH durante KICK_UP/DOWN?
- **IN1 (GPIO18):** ¿PWM 20kHz en UP? ¿Voltaje adecuado?
- **IN2 (GPIO16):** ¿PWM 20kHz en DOWN? ¿Voltaje adecuado?

### 3. Verificar Voltajes DRV8833
- OUT1 (motor +): ¿Cambia de HIGH/LOW según IN1?
- OUT2 (motor -): ¿Cambia de HIGH/LOW según IN2?

### 4. Verificar Físicamente
- Cables motor: ¿conectados ambos?
- Cables GPIO: ¿soldadura buena IN1/IN2?
- ¿Motor responde a jala manual? (comprobar que no está mecánicamente trabado)

## Código Actual (Funcionando)
- ✅ Motor::init() — configure GPIO14, 18, 16 como OUTPUT, EN=LOW
- ✅ Motor safety — EN=LOW en setup() ANTES de todo
- ✅ Test mode — calibración disparada una sola vez
- ✅ _hwUp/_hwDown — IN1 para UP, IN2 para DOWN (invertido previamente)

## Próximos Pasos
1. **Osciloscopio:** confirmar PWM en IN1/IN2
2. **Test directo:** function testMotorPWM() en SAT o main
3. **Físico:** verificar motor responde manualmente
4. **Diagnóstico:** determinar si es software (inversión) o hardware (driver/cables)

## Commits Relevantes (Hoy)
- `534021d` — Auditoría Motor/Fader completa
- `ceed039` — Safety: Motor EN=LOW en setup()
- `afc62ac` — Test mode: calibración + movimiento automático
- `10ce193` — Fix: startCalib() una sola vez

---

**Estado:** 🔴 BLOQUEADO — Motor no calibra (rango inválido)  
**Próxima sesión:** Debugging físico con osciloscopio o test directo
