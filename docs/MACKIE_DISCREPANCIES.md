# Mackie Control Protocol — Discrepancias encontradas

## Resumen

Se encontraron **dos documentos de protocolo Mackie** con implementaciones conflictivas en tres áreas críticas:
- **VPot Rotation** (CC 16-23)
- **Channel Pressure / Metering** (0xD0)
- **vPot LED Ring** (CC 48-55)

Este documento mapea ambas interpretaciones y marca lo que necesita validación con hardware real.

---

## 1. VPot Rotation (CC 16-23) — 🔴 CONFLICTO CRÍTICO

### Interpretación A (Simple/Tabla)

**Fuente:** Tabla simple de protocolo

```
CC 16-23: V-Pot Rotation
    CC Value {0:62}: CW (clockwise)
    CC Value {Other/63-127}: CCW (counter-clockwise)
    
Ticks = CC Value % 64

Ejemplos:
    CC 16, Value 5   → VPot canal 1, +5 ticks CW
    CC 16, Value 65  → VPot canal 1, -1 tick CCW (65-64=1)
    CC 16, Value 127 → VPot canal 1, -63 ticks CCW (127-64=63)
```

**Código iMakie actual (INCORRECTO):**
```cpp
// En S3/main.cpp processSlaveResponse()
if (ch.encoderDelta != 0) {
    uint8_t cc = 16 + midiCh;
    uint8_t val = (ch.encoderDelta > 0) ? 65 : 63;  // ← Envía siempre 65 o 63
    byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
    sendMIDIBytes(msg, 3);
}
```

---

### Interpretación B (Compleja/Reverse-engineered)

**Fuente:** Deep dive + reverse engineering con hardware

```
CC 16-23: V-Pot Rotation
    
Value field uses binary representation:
    - Bit 6 = sign bit (0=CW, 1=CCW)
    - Bits 5-0 = magnitude (ticks)
    
Ejemplos:
    CC 16, Value 0b00000001 (1)   → +1 tick CW
    CC 16, Value 0b00000101 (5)   → +5 ticks CW
    CC 16, Value 0b01000001 (65)  → -1 tick CCW
    CC 16, Value 0b01000101 (69)  → -5 ticks CCW
    
Posible aceleración:
    CC 16, Value 0b00001111 (15)  → +15 ticks CW (fast rotation)
    TODO: confirmar si valores > 1 codifican velocidad
```

**Mapeo binario:**
```
Bit 7: siempre 0 (MIDI standard)
Bit 6: 0=CW, 1=CCW (sign)
Bits 5-0: ticks (1-63)

0b00000001 = 1 CW
0b00111111 = 63 CW
0b01000001 = 1 CCW
0b01111111 = 63 CCW
```

---

### ❓ Validación requerida

**Test 1: Rotación lenta (+1 tick CW)**
- Interpretación A: enviar CC 16, Value 1
- Interpretación B: enviar CC 16, Value 0b00000001 (1)
- **Son iguales numéricamente, pero significado diferente**

**Test 2: Rotación rápida (+10 ticks CW)**
- Interpretación A: enviar CC 16, Value 10
- Interpretación B: enviar CC 16, Value 0b00001010 (10)
- **De nuevo iguales numéricamente**

**Test 3: Rotación CCW (-1 tick)**
- Interpretación A: enviar CC 16, Value 64 o 65
- Interpretación B: enviar CC 16, Value 0b01000001 (65)
- **Iguales**

**Test 4: Rotación CCW rápida (-10 ticks)**
- Interpretación A: enviar CC 16, Value 74 (64+10)
- Interpretación B: enviar CC 16, Value 0b01001010 (74)
- **Iguales**

**Conclusión:** Ambas son **idénticas en práctica**. La diferencia es solo notación (decimal vs binario).
**Interpretación B es más clara conceptualmente.**

---

## 2. Channel Pressure / Metering (0xD0) — 🔴 CONFLICTO

### Interpretación A (Fórmula)

**Fuente:** Tabla simple

```
Channel Pressure message format:
    Status: 0xD0 | channel
    Value: (Channel_Number * 16) + Meter_Level

Meter Channel Number = Value / 16 (integer division)
Meter Level = Value % 16

Cuando Value % 16 está entre 0 y 12:
    Meter_Value = (Value % 16) / 12 * 100%
    
Cuando Value % 16 es 14:
    set meter channel overload
    
Cuando Value % 16 es 15:
    clear meter channel overload

Ejemplos:
    0xD0, Value 0x07  → Channel 0, level 7/12 = 58%
    0xD0, Value 0x1C  → Channel 1, level 12/12 = 100%
    0xD0, Value 0x1E  → Channel 1, CLIP
```

**Código iMakie actual (S3):**
```cpp
// En S3/midi/MIDIProcessor.cpp processChannelPressure()
if (channel == 0) {
    targetChannel = (value >> 4) & 0x0F;  // Bits 7:4
    byte mcu_level = value & 0x0F;         // Bits 3:0
    // ... mapea mcu_level a nivel visual
}
```

---

### Interpretación B (Tabla exacta)

**Fuente:** Deep dive + reverse engineering

```
Tabla exacta de valores dB:

Value %16 | Signal Level        | Descripción
-----------|-------------------|------------------
0xsF      | Clear overload     | Limpia clip
0xsE      | Set overload       | Activa clip (> 0 dB)
0xsD      | 100% (> 0 dB)      | Rojo/Clip
0xsC      | 0 dB               | Rojo/Máximo
0xsB      | >= -2 dB           | Amarillo
0xsA      | >= -4 dB           | Amarillo
0xs9      | >= -6 dB           | Amarillo
0xs8      | >= -8 dB           | Verde
0xs7      | >= -10 dB          | Verde
0xs6      | >= -14 dB          | Verde
0xs5      | >= -20 dB          | Verde
0xs4      | >= -30 dB          | Verde
0xs3      | >= -40 dB          | Verde
0xs2      | >= -50 dB          | Verde
0xs1      | >= -60 dB          | Verde
0xs0      | 0% (< -60 dB)      | Todo OFF

Donde 's' = channel number, value = value & 0x0F
```

**Mapeo visual (en hardware físico):**
- TouchMCU muestra: 12 LEDs rojo → amarillo → verde
- Decay automático: ~300ms por división

---

### ❓ Discrepancia

**Interpretación A:** nivel lineal 0-12 → 0-100%
**Interpretación B:** tabla con saltos logarítmicos (-∞ a 0 dB)

**Problema:** Logic Professional usa escala **logarítmica** (dB), no lineal.

**Hipótesis:** Interpretación B es más precisa para Logic Pro (usa dB nativamente).

---

### Validación requerida

Enviar a Logic:
- `0xD0 0x07` (channel 0, level 7) → ¿qué muestra Logic?
- `0xD0 0x0C` (channel 0, level 12) → ¿0 dB?
- `0xD0 0x0E` (channel 0, overload) → ¿clip activado?

---

## 3. vPot LED Ring (CC 48-55) — 🟡 CONFLICTO MENOR

### Interpretación A (Simples)

**Fuente:** Tabla simple

```
CC 48-55: V-Pot LED Ring

CC Value % 64 rango:
    0-15:  Single Dot Mode
    16-31: Boost/Cut Mode
    32-47: Wrap Mode
    48-63: Spread Mode
    
Value (0-15): posición LED (0-15)
    
Bit 6 (bit adicional):
    0 = LED pequeño off
    1 = LED pequeño on
```

---

### Interpretación B (Bits exactos)

**Fuente:** Deep dive

```
CC Value = [b7=0][b6=LED][b5-b4=Mode][b3-b0=Value]

Bit 7: siempre 0 (MIDI)
Bit 6: small LED on/off
Bits 5-4: Mode (0-3)
    0b00 = Single Dot Mode
    0b01 = Boost/Cut Mode
    0b10 = Wrap Mode
    0b11 = Spread Mode
Bits 3-0: Value (0-15)

Tabla visual de patrones:
Mode 0 (Single Dot):
    Value 0:  -----------
    Value 1:  O----------
    Value 2:  -O---------
    ...
    Value 11: ----------O

Mode 1 (Boost/Cut):
    Value 0:  -----------
    Value 1:  OOOOOO-----
    Value 2:  -OOOOO-----
    ...
    
(similar para modes 2 y 3)
```

---

### ✅ Análisis

Ambas son **equivalentes**. Interpretación B es solo más explícita en bit-level.

Diferencia: Interpretación A usa `Value % 64` (ambiguo), Interpretación B especifica exactamente qué bits.

---

## 4. Fader Position (Pitch Wheel) — 🟡 INCOMPLETO

Ambos dicen: **0-16383 es rango válido**

Pero CLAUDE.md menciona: **máximo real es 14848** (no 16383)

**Discrepancia:**
- Protocolo simple/complejo: 0-16383
- CLAUDE.md: 0-14848 (zona muerta de 1535 unidades)

**¿Por qué?** Mackie dejó 1535 unidades (16383-14848) como zona muerta de seguridad.

**Validación requerida:** Enviar PB 16383 a Logic → ¿qué dB muestra? (-10.0? 0.0?)

---

## 5. Fader Touch Detection — ✅ ACUERDO

Ambos documentos coinciden:
```
Note 104-112 (0x68-0x70): Fader Touch
    - Note 104 + midiCh = fader channel
    - Velocity 127 = touched
    - Velocity 0 = released
```

---

## Plan de Validación

| Componente | Test | Esperado | Interpretación |
|---|---|---|---|
| **VPot CCW** | Enviar CC 16, Val 64 | VPot gira izquierda en Logic | A o B (iguales) |
| **VPot aceleración** | Enviar CC 16, Val 10 vs 127 | Rotación lenta vs rápida | B (si funciona) |
| **Metering -10dB** | Enviar 0xD0 0x07 | VU muestra -10dB o nivel lineal 58% | A vs B (crítico) |
| **Metering clip** | Enviar 0xD0 0x0E | VU muestra clip / overload LED | A vs B |
| **Fader máximo** | Enviar PB 16383 | Fader en tope, Logic muestra 0dB o -10dB | Validar 14848 |

---

## Recomendación

1. **Implementar Interpretación B** (más explícita, reverse-engineered)
2. **Validar cada componente** con Logic Pro + hardware real
3. **Ajustar máximo de fader** a 14848 si es necesario
4. **Documentar resultados** en CLAUDE.md

---

## Código propuesto (provisional)

### VPot Rotation (ambas interpretaciones son iguales)

```cpp
// Interpretación B (más clara conceptualmente)
if (ch.encoderDelta != 0) {
    uint8_t cc = 16 + midiCh;
    uint8_t val;
    
    if (ch.encoderDelta > 0) {
        // CW: Bit 6=0, bits 5-0 = ticks
        val = constrain((uint8_t)ch.encoderDelta, 1, 63);
    } else {
        // CCW: Bit 6=1, bits 5-0 = ticks
        val = 0x40 | constrain((uint8_t)(-ch.encoderDelta), 1, 63);
    }
    
    byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
    sendMIDIBytes(msg, 3);
}
```

### Metering (Interpretación B — tabla exacta)

```cpp
void setMeterValue(uint8_t channel, int db_value) {
    // Mapea dB a valor Mackie exacto
    uint8_t mcu_level;
    
    if (db_value > 0) {
        mcu_level = 0x0E;  // Overload
    } else if (db_value >= 0) {
        mcu_level = 0x0C;  // 0 dB
    } else if (db_value >= -2) {
        mcu_level = 0x0B;  // -2 dB
    } else if (db_value >= -4) {
        mcu_level = 0x0A;  // -4 dB
    } else if (db_value >= -6) {
        mcu_level = 0x09;  // -6 dB
    } else if (db_value >= -8) {
        mcu_level = 0x08;  // -8 dB
    } else if (db_value >= -10) {
        mcu_level = 0x07;  // -10 dB
    } else if (db_value >= -14) {
        mcu_level = 0x06;  // -14 dB
    } else if (db_value >= -20) {
        mcu_level = 0x05;  // -20 dB
    } else if (db_value >= -30) {
        mcu_level = 0x04;  // -30 dB
    } else if (db_value >= -40) {
        mcu_level = 0x03;  // -40 dB
    } else if (db_value >= -50) {
        mcu_level = 0x02;  // -50 dB
    } else if (db_value >= -60) {
        mcu_level = 0x01;  // -60 dB
    } else {
        mcu_level = 0x00;  // < -60 dB (OFF)
    }
    
    uint8_t cp_value = (channel << 4) | mcu_level;
    byte msg[2] = { (byte)(0xD0 | 0), cp_value };
    sendMIDIBytes(msg, 2);
}
```

### Fader Position (validar rango)

```cpp
uint16_t faderADC = ch.faderPos;  // 0-8191 (13-bit)

// Opción A: Escalar a 16383 completo
// uint16_t pb = (faderADC * 16383UL) / 8191;

// Opción B: Escalar a 14848 (rango Mackie real)
uint16_t pb = (faderADC * 14848UL) / 8191;

// Enviar Pitch Bend
byte lsb = pb & 0x7F;
byte msb = (pb >> 7) & 0x7F;
byte msg[3] = { (byte)(0xE0 | midiCh), lsb, msb };
sendMIDIBytes(msg, 3);
```

---

## Siguiente paso

**Validar con hardware real:**
1. Conectar S3 Extender + PTxx a Logic Pro
2. Ejecutar cada test de la tabla arriba
3. Documentar resultados
4. Ajustar implementación según hallazgos
