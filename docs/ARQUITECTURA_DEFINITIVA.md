# iMakie — Arquitectura (Consulta rápida)

## Componentes

| Dispositivo | MCU | Pantalla | Rol | RS485 |
|---|---|---|---|---|
| **P4** | ESP32-P4 | SÍ | Master MCU | Bus A (9 S2) |
| **S3 Extender** | ESP32-S3 | NO | Relay puro | Bus B (8 S2) |
| **S2** | ESP32-S2 | SÍ | Slave | A o B |

---

## Flujo: Logic ↔ P4/S3 ↔ S2

```
Logic MIDI → P4/S3 → RS485 → S2 → RS485 → P4/S3 → Logic MIDI
```

**P4:** dibuja pantalla propia  
**S3:** sin pantalla, relay puro  
**S2:** hardware físico (fader, encoder, botones, motor, display local, LEDs)

---

## MIDI (Logic → P4/S3)

| Datos | MIDI | Rango |
|---|---|---|
| **VPot LED** | CC 48-55 | 0-127 |
| **VU Meter** | Channel Pressure | 0-127 |
| **Fader** | Pitch Bend | -8192 a +6656 (máx: 14848) |
| **Botones** | Note On/Off | 0-127 |

---

## MIDI (P4/S3 → Logic)

- **VPot rotation:** CC 16-23, valor 65=CW / 63=CCW
- **Fader position:** Pitch Bend escalado (0-14848 máximo)
- **Botones:** Note On/Off

---

## RS485 (P4/S3 ↔ S2)

**Master → Slave (16B):** vpotValue, faderTarget, vuLevel, flags, autoMode  
**Slave → Master (9B):** faderPos, touchState, buttons, encoderDelta

---

## Referencia funcional

**S3 Extender = referencia**
- MIDI correcto
- RS485 correcto
- De P4 copia SOLO lo indicado

---

## Crítico (implementar sin preguntar)

1. **Fader máximo = 14848** (validado: Logic envía -8192 a +6656)
2. **VPot = 65/63** (CW/CCW binario, CC 16-23)
3. **Encoder reset post-VPot** (no post-RS485)
4. **RS485 ~20ms ciclo**, timeout 1000ms

---

**Última actualización:** 2026-04-29  
**Estado:** Definitivo.
