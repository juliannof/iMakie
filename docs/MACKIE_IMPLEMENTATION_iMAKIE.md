# Mackie Control Implementation en iMakie

## Arquitectura: Logic ↔ S3 Extender ↔ PTxx Slaves

```
Logic Pro (macOS)
    │ USB-MIDI
    ▼
S3 Extender
    │ RS485 bus B
    ▼
8× PTxx (ESP32-S2 slaves, IDs 1-8)
```

**Mapeo de canales:**
- Logic channel 1-8 → S3 slave 1-8
- MIDI channel 0 (1 en Logic UI) = fader canal 1, encoder VPot 1, etc.

---

## VPot (Virtual Potentiometer / Parámetro de equipo)

### Flujo: Logic → PTxx

**1. Logic envía posición VPot via CC (Logic → S3)**

```
CC 48-55: V-Pot LED Ring (posición del anillo visual)
    CC 48 = Canal 1 VPot
    CC 49 = Canal 2 VPot
    ... CC 55 = Canal 8 VPot
    
Formato del valor CC:
    CC Value = [bit7: mode_select] + [bits 6-4: mode_id] + [bits 3-0: position]
    
    bit 6-4 = modo (0-7):
        0 = Single Dot Mode (valor actual -7 a +7, mapeado a posición LED 0-14)
        1 = Boost/Cut Mode
        2 = Wrap Mode
        3 = Spread Mode
        
    bit 3-0 = posición LED (0-15):
        En Single Dot Mode: posición -7 a +7 se mapea a LED 0-14 (15 LEDs)
```

**2. S3 Extender recibe CC y lo mapea a RS485**

En `MIDIProcessor::processControlChange()`:
```cpp
if (controller >= 48 && controller <= 55) {
    uint8_t channel = controller - 47;  // canal 1-8
    rs485.setVPotValue(channel, value);  // Envía raw CC value a slave
}
```

**3. PTxx recibe vpotValue en MasterPacket**

En `RS485Handler::onMasterData()`:
```cpp
setVPotRaw(pkt.vpotValue);
```

Internamente, S2 interpreta el valor para actualizar la posición del VPot ring display.

---

### Flujo: PTxx → Logic

**1. Usuario rota encoder físico en PTxx**

El encoder genera deltas:
```
Encoder::getCount() → acumulado desde último reset
Dirección: +1 = CW (derecha), -1 = CCW (izquierda)
```

**2. PTxx envía encoderDelta en SlavePacket**

En `buildResponse()`:
```cpp
SlavePacket resp = {
    ...
    .encoderDelta = (int8_t)constrain(Encoder::getCount(), -127, 127),
    ...
};
```

**3. S3 Extender recibe encoderDelta y lo convierte a CC**

En `processSlaveResponse()` (ACTUAL - INCORRECTO):
```cpp
// --- Encoder → CC ---
if (ch.encoderDelta != 0) {
    uint8_t cc  = 16 + midiCh;  // CC 16-23 (V-Pot control, NO LED ring)
    uint8_t val = (ch.encoderDelta > 0) ? 65 : 63;
    byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
    sendMIDIBytes(msg, 3);
}
```

**PROBLEMAS:**
- Usa CC 16-23 (correcto para V-Pot control)
- Pero envía solo 65 o 63, ignorando tamaño del delta
- Debería enviar valor proporcional (1-62 para CW, 64-127 para CCW)

---

### Corrección requerida

Según protocolo Mackie:

```
CC 16-23: V-Pot Control (rotación relativa)
    Valores 0-62: CW (clockwise) increment
    Valores 63-127: CCW (counter-clockwise) decrement
    Ticks = CC Value % 64
    
Codificación correcta:
    delta > 0: val = constrain(delta, 1, 62)          // 1-62 ticks CW
    delta < 0: val = 64 + constrain(-delta, 1, 64)    // 64-127 ticks CCW (64=1 tick CCW, 127=64 ticks CCW)
    delta = 0: no enviar
```

**Implementación corregida:**
```cpp
if (ch.encoderDelta != 0) {
    uint8_t cc  = 16 + midiCh;  // CC 16-23
    uint8_t val;
    
    if (ch.encoderDelta > 0) {
        // Incremento: valores 1-62
        val = constrain((uint8_t)ch.encoderDelta, 1, 62);
    } else {
        // Decremento: valores 64-127
        // encoderDelta = -1 → val = 64
        // encoderDelta = -64 → val = 127
        val = 64 + constrain((uint8_t)(-ch.encoderDelta), 1, 64);
    }
    
    byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
    sendMIDIBytes(msg, 3);
}
```

**Sincronización VPot:**

Problema actual: No hay validación de que Logic y S2 estén sincronizados.

Solución: S2 debería recibir `vpotValue` de Logic cada ciclo y validar que su estado local coincida:

```cpp
// En RS485Handler::onMasterData() (S2 side)
int8_t logicVPotPos = (pkt.vpotValue & 0x0F);  // bits 3-0 = posición
if (logicVPotPos != Encoder::currentVPotLevel) {
    // Desincronía detectada → resincronizar
    Encoder::currentVPotLevel = logicVPotPos;
    needsVPotRedraw = true;
    // Log para debugging
}
```

---

## Faders (Pitch Wheel)

### Especificación Mackie

```
MIDI Pitch Wheel Value = Fader Position
    0     = Fader mínimo (fondo)
    8192  = Fader centro (-∞ dB en algunos DAWs, 0 dB en otros)
    14848 = Fader máximo (tope físico)
    
Nota: No es 16383 (14-bit max) sino 14848 (Mackie custom)
Motivo: Zona muerta 1535 unidades (16383 - 14848)
```

### Flujo actual (INCORRECTO)

**PTxx → S3:**
```cpp
if (ch.touchState) {
    uint16_t pb = ch.faderPos & 0x3FFF;  // INCORRECTO
    byte msg[3] = { (byte)(0xE0 | midiCh), (byte)(pb & 0x7F), (byte)(pb >> 7) };
    sendMIDIBytes(msg, 3);
}
```

**Problemas:**
1. `ch.faderPos` es 13-bit (0-8191, no 14-bit)
2. Máscara `& 0x3FFF` no escala, solo preserva bits
3. Resultado: máximo 8191 → Logic interpreta como ~50% → muestra -10.0 dB

### Corrección requerida

Escalar ADC de 13-bit (0-8191) a pitch wheel de 14-bit Mackie (0-14848):

```cpp
// Fader actual en S2 es 13-bit ADC (0-8191)
// Convertir a 14-bit Mackie pitch wheel (0-14848)
uint16_t pb = (ch.faderPos * 14848UL) / 8191;

// Luego codificar como pitch wheel MIDI 14-bit:
// LSB = bits 6-0
// MSB = bits 13-7
byte lsb = pb & 0x7F;
byte msb = (pb >> 7) & 0x7F;
byte msg[3] = { (byte)(0xE0 | midiCh), lsb, msb };
```

---

## Botones (Note On/Off)

### Mapeo correcto

```
Note 0-7:   REC/RDY channel 1-8
    Velocity 127 = REC on
    Velocity 0 = REC off
    
Note 8-15:  SOLO channel 1-8
Note 16-23: MUTE channel 1-8
Note 24-31: SELECT channel 1-8
```

**Implementación actual (en main.cpp S3):**
```cpp
const uint8_t noteBase[4] = { 0, 8, 16, 24 };
for (uint8_t bit = 0; bit < 4; bit++) {
    if (changed & (1 << bit)) {
        bool    isOn = (ch.buttons & (1 << bit)) != 0;
        uint8_t note = noteBase[bit] + midiCh;
        uint8_t vel  = isOn ? 127 : 0;
        byte msg[3]  = { (byte)(isOn ? 0x90 : 0x80), note, vel };
        sendMIDIBytes(msg, 3);
    }
}
```

Este es **correcto**.

---

## VU Meter (Channel Pressure)

### Especificación

```
Channel Pressure = (Channel_Number * 16) + Meter_Level
    Channel_Number: 0-7 (canal 1-8)
    Meter_Level: 0-15
        0-12: nivel en % (level / 12 * 100%)
        13: (reservado)
        14: set overload/clip indicator
        15: clear overload/clip indicator
```

**Ejemplo:**
- Canal 1, nivel 50% → pressure = (0 * 16) + 6 = 6
- Canal 2, clip on → pressure = (1 * 16) + 14 = 30

### Implementación actual

En `processChannelPressure()` (MIDIProcessor.cpp):
```cpp
if (channel == 0) {
    targetChannel = (value >> 4) & 0x0F;
    byte mcu_level = value & 0x0F;
    if (targetChannel >= 8) return;
    switch (mcu_level) {
        case 0x0F: clearClip = true; normalizedLevel = vuLevels[targetChannel]; break;
        case 0x0E: newClipState = true; normalizedLevel = 1.0f; vuLevel7bit = 127; break;
        case 0x0D: case 0x0C: normalizedLevel = 1.0f; vuLevel7bit = 120; break;
        default:
            normalizedLevel = (mcu_level <= 11) ? (float)mcu_level / 11.0f : 0.0f;
            vuLevel7bit = (uint8_t)(normalizedLevel * 127.0f);
            break;
    }
    rs485.setVuLevel(targetChannel + 1, vuLevel7bit);
}
```

Este código **interpreta correctamente** el channel pressure de Logic.

---

## Automation Mode (bits 5-7 de flags)

### Mapeo de modos

```
Bits 7:5 de MasterPacket.flags = Modo de automatización

0 (000) = AUTO_OFF       → Sin grabación
1 (001) = AUTO_READ      → Lectura de datos grabados
2 (010) = AUTO_WRITE     → Grabación activa
3 (011) = AUTO_TRIM      → Ajuste fino
4 (100) = AUTO_TOUCH     → Grabación al tocar fader
5 (101) = AUTO_LATCH     → Mantiene valor al soltar
```

Logic envía estos modos automáticamente al cambiar el modo de automatización de un canal.

**Implementación actual:** Código S3 recibe y S2 procesa correctamente.

---

## Resumen de correcciones necesarias

| Componente | Problema actual | Corrección |
|---|---|---|
| **VPot delta** | Envía 65 o 63 (binario) | Enviar valor proporcional 1-62 (CW) o 64-127 (CCW) |
| **VPot sync** | Sin validación | Comparar con vpotValue de Logic cada ciclo |
| **Fader escala** | Usa 0-8191 (13-bit) | Escalar a 0-14848 (14-bit Mackie) |
| **Botones** | ✓ Correcto | Sin cambios |
| **VU Meter** | ✓ Correcto | Sin cambios |
| **Automation** | ✓ Correcto | Sin cambios |

---

## Próximos pasos

1. **Corregir VPot delta** en `main.cpp` S3, `processSlaveResponse()`
2. **Corregir escala de fader** en `main.cpp` S3, `processSlaveResponse()`
3. **Añadir validación VPot** en `RS485Handler.cpp` S2, `onMasterData()`
4. **Validar con hardware real** que Logic responda correctamente
5. **Documentar en CLAUDE.md** el estado final de implementación
