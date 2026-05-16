# iMakie — ESP32-S3 Extender

Controlador Mackie MCU extendido para Logic Pro. Controla 8 tracks S2 adicionales (IDs 1–8) vía RS485 bus B, y transport buttons (RW/FF/STOP/PLAY/REC) locales.

**Chip:** ESP32-S3  
**Familia Mackie:** 0x14  
**Slaves controlados:** 8 (IDs 1–8) en RS485 bus B  
**Transport buttons:** 5 (RW/FF/STOP/PLAY/REC)

---

## Hardware S3

### Pinout definitivo S3

| Función | GPIO | Tipo |
|---------|------|------|
| **RS485 TX (bus B)** | 15 | UART TX |
| **RS485 RX (bus B)** | 16 | UART RX |
| **RS485 EN (driver enable)** | 1 | GPIO output |
| **LED REC** | 12 | GPIO output (PWM capable) |
| **BTN REC** | 11 | GPIO input (activo LOW) |
| **LED PLAY** | 10 | GPIO output (PWM capable) |
| **BTN PLAY** | 9 | GPIO input (activo LOW) |
| **LED FF (fast forward)** | 8 | GPIO output (PWM capable) |
| **BTN FF** | 7 | GPIO input (activo LOW) |
| **LED STOP** | 6 | GPIO output (PWM capable) |
| **BTN STOP** | 5 | GPIO input (activo LOW) |
| **LED RW (rewind)** | 4 | GPIO output (PWM capable) |
| **BTN RW** | 3 | GPIO input (activo LOW) |

---

## RS485 (Bus B)

**Especificación:**
- **Baudrate:** 500 kbaud
- **Protocolo:** Binario custom con CRC8 (ver [docs/RS485.md](../../../../docs/RS485.md))
- **Topología:** Star — S3 master, 8 slaves S2 (IDs 1–8)
- **Timing crítico:**
  - TX_EN: 30µs
  - TX_DONE: 30µs
  - RESP_TIMEOUT: 3000µs (3ms)
  - GAP: 300µs entre slaves
  - POLL_CYCLE: ~20ms (master interroga todos los slaves)

**Pinout RS485:**
- TX = GPIO15
- RX = GPIO16
- EN (driver enable) = GPIO1

**Timeout handling:**
- Si slave no responde en 3000µs → retry con FLAG_CALIB
- Si persiste >1s sin datos → marcar NOT_CALIBRATED
- S2 recibe timeout → LED11=rojo, motor suspendido

**Referencia:** [docs/RS485.md](../../../../docs/RS485.md) — especificación completa de protocolo, paquetes, máquina de estados

---

## Transport — Botones y LEDs — 📌 Ver docs/Transport.md

**Documentación exhaustiva:**
→ **[docs/Transport.md](../../../../docs/Transport.md)** (pinout, MIDI notas 0x5B-0x5F, flujo bidireccional, handshake Mackie, troubleshooting)

---

## Comunicación

### RS485 Cycle (20ms típicamente)

```
T=0ms:      Master (S3) envía MasterPacket a Slave 1 (ID=1)
T≈150µs:    Slave 1 envía SlavePacket
T=600µs:    Master envía MasterPacket a Slave 2 (ID=2)
T≈750µs:    Slave 2 envía SlavePacket
...
T≈15ms:     Master envía MasterPacket a Slave 8 (ID=8)
T≈15.15ms:  Slave 8 envía SlavePacket
T≈20ms:     Ciclo repite
```

### MasterPacket (16B) → Slave

```cpp
struct MasterPacket {
    uint8_t start_byte;           // 0xAA
    uint8_t slave_id;             // 1-8
    char track_name[8];           // Nombre track
    uint16_t fader_target;        // Target ADC (0-27000)
    uint8_t vu_level;             // VU level
    uint8_t flags;                // CALIB, REC, SOLO, MUTE, SELECT, etc
    uint8_t crc8;                 // CRC8 (poly 0x07)
};
```

### SlavePacket (9B) ← Slave

```cpp
struct SlavePacket {
    uint8_t start_byte;           // 0xBB
    uint8_t slave_id;             // 1-8
    uint16_t fader_pos;           // Posición actual ADC
    uint8_t buttons;              // Bits 0-3: REC/SOLO/MUTE/SELECT
    int8_t encoder_delta;         // Encoder delta desde último ciclo
    uint8_t encoder_button;       // Encoder push button
    uint8_t flags;                // CALIB_DONE, CALIB_SENDING, etc
    uint8_t crc8;                 // CRC8
};
```

---

## Compilación

```bash
cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER
pio run -e esp32-s3-devkitc-1
```

**Platform:** espressif32  
**Framework:** Arduino  
**Librerías:** LovyanGFX, LVGL v9, WiFi, MQTT (opcional)

---

## Referencias

- **RS485 Protocol:** [docs/RS485.md](../../../../docs/RS485.md)
- **Mackie MCU:** [docs/SAT.md — Mackie handshake](../../../../docs/SAT.md)
- **Arquitectura general:** [CLAUDE.md](../../../../CLAUDE.md)
- **Estado técnico:** [STATUS.md](../../../../STATUS.md)
- **Master P4:** [MASTER_S3-P4/P4/README.md](../P4/README.md)
- **Slave S2:** [S2/README.md](../../../../S2/README.md)
