# iMakie PTxx — Mackie Control Clone on ESP32

> A DIY Mackie Control surface built on ESP32-S3 (master) and ESP32-S2 Mini (slave) units, communicating over RS485 and interfacing with DAWs via USB MIDI.

---

## Table of Contents

- [System Overview](#system-overview)
- [Hardware Architecture](#hardware-architecture)
  - [Master Unit — ESP32-S3](#master-unit--esp32-s3)
  - [Slave Units — ESP32-S2 Mini](#slave-units--esp32-s2-mini)
- [Communication Protocol — RS485](#communication-protocol--rs485)
- [Key Hardware Decisions](#key-hardware-decisions)
- [Firmware Modules](#firmware-modules)
- [Development Environment](#development-environment)
- [Known Constraints & Workarounds](#known-constraints--workarounds)

---

## System Overview

iMakie PTxx emulates a **Mackie Control Universal** surface. It supports up to **17 slave channels** (16 tracks + 1 master) on a single RS485 bus coordinated by one ESP32-S3 master.

```
DAW (Logic Pro / macOS)
        │  USB MIDI (Mackie Control protocol)
        ▼
  ┌─────────────┐
  │  ESP32-S3   │  ◄── Master Unit
  │  (Master)   │
  └──────┬──────┘
         │  RS485 @ 500 kbaud (half-duplex)
         │  Custom binary protocol + CRC8
    ┌────┴────────────────────────── ... ──┐
    ▼                                      ▼
┌──────────┐                         ┌──────────┐
│ESP32-S2  │                         │ESP32-S2  │
│ Slave 1  │  ...  (up to 17 total)  │ Slave 17 │
└──────────┘                         └──────────┘
```

Each slave controls one channel strip, independently managing its motorized fader, touch detection, illuminated buttons, NeoPixel LEDs, and TFT display.

---

## Hardware Architecture

### Master Unit — ESP32-S3

| Function          | Detail                                      |
|-------------------|---------------------------------------------|
| USB MIDI          | Native USB via TinyUSB, Mackie Control HID  |
| RS485 Bus         | UART + half-duplex, 500 kbaud               |
| Display           | TFT via TFT_eSPI                            |
| DAW Integration   | Logic Pro (tested), compatible with any DAW supporting Mackie Control |

### Slave Units — ESP32-S2 Mini

Each slave unit contains:

| Component             | Detail                                                      |
|-----------------------|-------------------------------------------------------------|
| Motorized fader       | H-bridge PWM motor driver + ADC position feedback           |
| Touch detection       | T-Pin capacitive touch (GPIO)                               |
| Illuminated buttons   | REC / SOLO / MUTE / SELECT — NeoPixel RGB LEDs              |
| Display               | TFT via TFT_eSPI                                            |
| Rotary encoder        | Per-channel parameter control                               |
| RS485                 | Half-duplex UART, addressed slave protocol                  |

---

## Communication Protocol — RS485

- **Baud rate:** 500 kbaud
- **Topology:** Half-duplex, single master, up to 17 slaves
- **Framing:** Custom binary packets with CRC8 integrity check
- **Addressing:** Each slave has a fixed ID (0–16)

### Packet Flow

```
Master → Slave (MasterPacket):  position target, button LED states, display data
Slave  → Master (SlavePacket):  fader ADC position, touch state, button events

SlavePacket path:
  ESP32-S2 → RS485 → ESP32-S3 → USB MIDI Pitch Bend → DAW
```

### Protocol Integrity

- CRC8 on every packet
- Circular buffer on master RX with race condition protection on head pointer
- Silent overflow prevention (buffer full → drop, not corrupt)

---

## Key Hardware Decisions

### ESP32-S2 ADC Limitation — Fader Position

The ESP32-S2 ADC saturates above **~1.1V**, making direct 3.3V fader VCC unusable.

**Solution implemented:**
- Use the S2's hardware **DAC on GPIO17** to power the fader VCC rail at ~1.1V
  ```cpp
  dacWrite(17, 85);  // ≈ 1.08V
  ```
- Set ADC attenuation to `ADC_0db` (0–1.1V range)
- Reroute `RS485_ENABLE` to **GPIO35** (freed GPIO17 for DAC)
- PCB modification: trace cut + bridge wire required

**Confirmed working ADC range:** `0 – 14845`

### ESP32-S2 USB CDC Logging

Unlike the S3, the S2 requires explicit build flags for USB CDC to work:

```ini
; platformio.ini
build_flags =
  -D ARDUINO_USB_CDC_ON_BOOT=1
  -D ARDUINO_USB_MODE=1
```

```cpp
// setup()
Serial.begin(115200);
Serial.setDebugOutput(true);  // redirects log_x() macros to USB CDC
```

### Fader VCC Rail Stabilization

The `faderADC.begin()` call must happen **after** the DAC VCC rail has had time to stabilize. Calling it too early captures a bad baseline, causing the motor to aggressively drive on first movement.

```cpp
dacWrite(17, 85);
delay(50);          // allow rail to stabilize
faderADC.begin();
```

---

## Firmware Modules

> Detailed documentation per module is organized in separate files.

| Module                  | File                          | Description                                      |
|-------------------------|-------------------------------|--------------------------------------------------|
| Motor control           | `motor.cpp / motor.h`         | PWM drive, dead-zone, stall detection, calibration |
| Fader ADC               | `faderADC.cpp / faderADC.h`   | ADC read, filtering, DAC VCC management          |
| Touch detection         | `touch.cpp / touch.h`         | T-Pin capacitive read, touch gate logic          |
| RS485 communication     | `rs485.cpp / rs485.h`         | Packet RX/TX, CRC8, circular buffer              |
| Slave packet            | `slavePacket.cpp`             | SlavePacket build & dispatch to master           |
| Button & LED            | `buttons.cpp / leds.cpp`      | NeoPixel state machine, debounced input          |
| Display                 | `display.cpp`                 | TFT_eSPI wrapper, channel info rendering         |
| Main loop               | `main.cpp`                    | Init sequence, task scheduling                   |

---

## Development Environment

| Tool              | Detail                          |
|-------------------|---------------------------------|
| Framework         | Arduino via PlatformIO          |
| IDE               | VS Code + PlatformIO extension  |
| Target boards     | ESP32-S3 (master), ESP32-S2 Mini (slave) |
| MIDI testing      | Logic Pro on macOS              |
| Reference device  | Behringer BCF2000               |

---

## Known Constraints & Workarounds

| Constraint                              | Workaround                                              |
|-----------------------------------------|---------------------------------------------------------|
| S2 ADC saturates above 1.1V            | DAC GPIO17 as fader VCC rail @ ~1.1V                   |
| S2 USB CDC silent by default           | Explicit build flags + `Serial.setDebugOutput(true)`    |
| Motor oscillation at target position   | Dead-zone simplified; no active holding torque (mechanical friction only) |
| Fader aggressive first move            | `faderADC.begin()` called after DAC rail stabilizes    |
| RS485 circular buffer race condition   | Head pointer protected; overflow drops silently         |
| Motor stall detection asymmetry (WIP)  | Movement-delta approach with `STALL_THRESHOLD` + `STALL_TIME_MS` (in progress) |
| Touch gate feedback loop (WIP)         | Touch gate prevents motor movement from triggering touch events (in progress) |

---

*Last updated: March 2026 — iMakie PTxx active development phase.*
| **16** | RS485 GPIO 16 (RX) ◄──── GPIO 0 (TX) Pico envía, ESP recibe | RS485 |  |
| **17** |  - | - |   |
| **18** | **Backlight** | Pantalla | **FIJO (Tu elección)** |
| **19** | USB D- | USB Nativo | Reservado |
| **20** | USB D+ | USB Nativo | Reservado |
| **21** | - | - |   |
| **33-37**| ⛔ **PROHIBIDO** | **PSRAM N16R8** | **NO TOCAR** |
| **38** | - | - |   |
| **39** | - | - |   |
| **40** | - | - |   |
| **41** | - | - |   |
| **42** | - | - |   |


***

## 🗺️ Mapa de Pines: Unidad de Control S3-2 (Xtender)

Este mapa de pines está optimizado para **ESP32-S3 (N16R8)**, evitando pines reservados por la PSRAM Octal (33-37), USB Nativo (19-20) y Strapping Pins críticos.

| **GPIO** | **Función Asignada** | **Dispositivo** | **Estado** |
| :--- | :--- | :--- | :--- |
| **0** | (Libre) | - | Boot |
| **1** | ENABLE RS485| - |   |
| **2** |  (Libre) | - |   |
| **3** | LED 1 TRANSPORTE | - |   |
| **4** | LED 2 TRANSPORTE | - |   |
| **5** | LED 3 TRANSPORTE | - |   |
| **6** | LED 4 TRANSPORTE | - |   |
| **7** | LED 5 TRANSPORTE | - |   |
| **8** | BOTON 1 TRANSPORTE  | - |  |
| **9** | BOTON 2 TRANSPORTE | - |  |
| **10** |  BOTON 3 TRANSPORTE | - | |
| **11** |   BOTON 4 TRANSPORTE | - | |
| **12** |   BOTON 5 TRANSPORTE | - | |
| **13** | ENCODER 1 PIN3 | | |
| **14** | ENCODER 1 PIN1 | | |
| **15** | RS485 ────► GPIO 15 (RX) ESP envía, Pico recibe | RP2040 |  |
| **16** | RS485 GPIO 16 (RX) ◄──── GPIO 0 (TX) Pico envía, ESP recibe | RP2040 |  |
| **17** |  - | - |   |
| **18** | **Backlight** | Pantalla | **FIJO** |
| **19** | USB D- | USB Nativo | Reservado |
| **20** | USB D+ | USB Nativo | Reservado |
| **21** | - | - |   |
| **33-37**| ⛔ **PROHIBIDO** | **PSRAM N16R8** | **NO TOCAR** |
| **38** | - | - |   |
| **39** | - | - |   |
| **40** | - | - |   |
| **41** | - | - |   |
| **42** | - | - |   |
| **43** | U0TXD | - |  **NO TOCAR** |
| **44** | U0RXD| - |  **NO TOCAR** |
| **45** | - | - |   |
| **46** | - | - |   |
| **47** | - | - |   |


