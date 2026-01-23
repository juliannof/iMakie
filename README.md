# iMakie
A Mackie Control interface with ESP32
---
<img alt="ESP32 S2" src="https://www.wemos.cc/en/latest/_static/boards/s2_mini_v1.0.0_4_16x9.jpg">

### Tabla de Asignación de Pines Actualizada (Botones en Pines Altos)

| Componente | Función | Definición | Pin GPIO | Justificación y Notas |
| :---------------- | :------------------ | :---------------------------- | :----------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| RSA0N11M9A0J (Pot.) | Posición Fader | FADER_POT | **10** | **ADC1_CH9**. Libre, sin UART/touch, boot-safe. 11dB (0-3.6V) evita saturación (2.68V). RC (470 Ω, 0.1 µF), cap 0.1 µF. Lejos PWM (16/18). VCC=3.3V. |
| RSA0N11M9A0J (Touch) | Tacto Capacitivo | FADER_TOUCH | **1** | **Touch1**. Priorizado para Mackie. Calibra umbral. Desactiva UART0_TXD, Touch2-9. EMI riesgo. |
| Motor PWM (IN1) | Control Fader | MOTOR_IN1 | 18 | PWM 20 kHz. Caps 0.1 µF+10 µF en DRV8833. Ferrita. 10V. Lejos FADER_POT (10). |
| Motor PWM (IN2) | Control Fader | MOTOR_IN2 | 16 | PWM 20 kHz. Slew rate. Cerca ENCODER_B (12), EMI riesgo. 10V. |
| Driver Enable | Habilitación (HIGH) | DRV_ENABLE | 14 | Conectado a 5V/10V. |
| Encoder A (INT) | Panorama (INT) | ENCODER_A | 13 | Libre, sin touch/UART. Pull-up 4.7 kΩ a 3.3V. Interrupción. |
| Encoder B (DIR) | Panorama (DIR) | ENCODER_B | 12 | Libre. Pull-up externo. Cerca PWM (16), EMI riesgo. |
| Botón Encoder | Pulsador (Jog Select) | ENCODER_BUTTON | 11 | Libre. Pull-up externo/interno a 3.3V. Feedback en TFT. |
| NeoPixel | Feedback Botones | NEOPIXEL | 36 | Libre, input/output. 800 kHz, 5V, lógica 3.3V. 4 LEDs (~240 mA): Rec=0 (rojo), Solo=1 (amarillo), Mute=2 (verde), Select=3 (azul). |
| **TFT - SCLK** | Reloj SPI | TFT_SCLK/SCL | **7** | HSPI, <20 MHz (DMA). LovyanGFX, lógica 3.3V, backlight 5V. |
| **TFT - MOSI** | Datos SPI | TFT_MOSI/SDA | **6** | HSPI, ~100 mA, 5V backlight. |
| **TFT - RST** | Reset | TFT_RST | **33** | Output digital o reset soft (LovyanGFX). |
| **TFT - DC** | Data/Command | TFT_DC | **4** | Movido a GPIO 3. Libre, Touch2/boot-sensitive (desactiva Touch2, pull-up 3.3V). |
| **TFT - CS** | Chip Select | TFT_CS | **5** | HSPI, LovyanGFX. |
| **TFT - BL** | BACKLIGHT (PWM) | TFT_BL | **3** | **LedC PWM**. Con driver MOSFET. Pin libre, lejos de pines SPI/PWM de motor. |
| I2C SDA | Datos I2C | I2C_SDA | 8 | Por defecto (Arduino). Pull-up 4.7 kΩ a 3.3V. Cerca SPI (5-7), EMI riesgo con DMA. |
| I2C SCL | Reloj I2C | I2C_SCL | 9 | Por defecto. Pull-up 4.7 kΩ a 3.3V. EMI riesgo. |
| **Botón 1** | Función de Control | BUTTON_1 | **37** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |
| **Botón 2** | Función de Control | BUTTON_2 | **38** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |
| **Botón 3** | Función de Control | BUTTON_3 | **39** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |
| **Botón 4** | Función de Control | BUTTON_4 | **40** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |



<img alt="ESP32 S3" src="https://naylampmechatronics.com/img/cms/001206/Pinout%20ESP32-S3-DevKitC-1.jpg">

| **GPIO** | **Función Asignada** | **Dispositivo** | **Estado** |
| :--- | :--- | :--- | :--- |
| **0** | (Libre) | - | Boot |
| **1** | -| - |   |
| **2** | - | - |   |
| **3** | - | - |   |
| **4** | - | - |   |
| **5** | - | - |   |
| **6** | - | - |   |
| **7** | - | - |   |
| **8** | SDA I²C (Trellis) | NetTrellis | OK |
| **9** | SCL I²C (Trellis) | NetTrellis | OK |
| **10** | **SCK Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **11** | **MOSI Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **12** | **DC Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **13** | **RST Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **14** | **CS Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **15** | RS485 ────► GPIO 15 (RX) ESP envía, Pico recibe | RP2040 | OK |
| **16** | RS485 GPIO 16 (RX) ◄──── GPIO 0 (TX) Pico envía, ESP recibe | RP2040 | OK |
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

| GPIO | Categoría | Componente | Función | Notas Técnicas |
| :--- | :--- | :--- | :--- | :--- |
| **4** | Encoder | ENC 1 | **Jog Wheel (A)** | |
| **5** | Encoder | ENC 1 | **Jog Wheel (B)** | CC 60 (`0x3C`) |
| **6** | Encoder | ENC 2 | **Zoom H (A)** | |
| **7** | Encoder | ENC 2 | **Zoom H (B)** | Asignable (CC Genérico) |
| **15** | Encoder | ENC 3 | **Zoom V (A)** | |
| **16** | Encoder | ENC 3 | **Zoom V (B)** | Asignable (CC Genérico) |
| **17** | Encoder | ENC 4 | **Nudge (A)** | |
| **18** | Encoder | ENC 4 | **Nudge (B)** | Asignable (CC Genérico) |
| **8** | Botón | BTN 1 | **REWIND (<<)** | Nota MIDI 91 |
| **9** | Botón | BTN 2 | **FFWD (>>)** | Nota MIDI 92 |
| **10** | Botón | BTN 3 | **STOP** | Nota MIDI 93 |
| **11** | Botón | BTN 4 | **PLAY** | Nota MIDI 94 |
| **12** | Botón | BTN 5 | **RECORD** | Nota MIDI 95 |
| **13** | LED | LED 1 | **Feedback RW** | PWM Activo |
| **14** | LED | LED 2 | **Feedback FF** | PWM Activo |
| **21** | LED | LED 3 | **Feedback STOP** | PWM Activo |
| **47** | LED | LED 4 | **Feedback PLAY** | PWM Activo |
| **48** | LED | LED 5 | **Feedback REC** | *Precaución: Puede tener LED RGB on-board* |
| **43** | Comms | UART (HW) | **TX -> Pico** | Conectar a Pico RX |
| **44** | Comms | UART (HW) | **RX <- Pico** | Conectar a Pico TX |
| **1** | Comms | RS-485 | **RX (Bus)** | |
| **2** | Comms | RS-485 | **TX (Bus)** | |
| **42** | Comms | RS-485 | **DE / RE** | Control de Dirección |

### ⚠️ Notas de Hardware
*   **Encoders:** Conectar pines A y B a los GPIOs. El pin común (C) del encoder va a GND. Habilitar Pull-ups internos en el código.
*   **Botones:** Conectar un lado a GPIO y el otro a GND. Lógica invertida (LOW = Pulsado).
*   **LEDs:** Conectar vía resistencia (220Ω - 1kΩ) a GND. Lógica positiva (HIGH = Encendido).
*   **GPIO 48:** En algunos DevKits, este pin tiene un LED RGB soldado. Si interfiere visualmente, usar GPIO 38 o desoldar el LED de la placa.
