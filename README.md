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
| **1** | **SDA I²C (Bus S3)** | A esclavos S2 |   |
| **2** | **SCL I²C (Bus S3)** | A esclavos S2 |   |
| **3** | **Botón 5** | Input | Movido aquí |
| **4** | **Botón 1** | Input | OK |
| **5** | **Botón 2** | Input | OK |
| **8** | SDA I²C (Trellis) | NetTrellis | OK |
| **9** | SCL I²C (Trellis) | NetTrellis | OK |
| **10** | **SCK Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **11** | **MOSI Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **12** | **DC Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **13** | **RST Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **14** | **CS Pantalla** | Pantalla | **FIJO (Tu elección)** |
| **15** | **TX UART** GPIO 15 (TX) ────► GPIO 1 (RX) ESP envía, Pico recibe | RP2040 | OK |
| **16** | **RX UART** GPIO 16 (RX) ◄──── GPIO 0 (TX) Pico envía, ESP recibe | RP2040 | OK |
| **17** | **Botón 3** | Input | OK |
| **18** | **Backlight** | Pantalla | **FIJO (Tu elección)** |
| **19** | USB D- | USB Nativo | Reservado |
| **20** | USB D+ | USB Nativo | Reservado |
| **21** | **Botón 4** | Input | OK |
| **33-37**| ⛔ **PROHIBIDO** | **PSRAM N16R8** | **NO TOCAR** |
| **38** | **LED 1** | Salida | OK |
| **39** | **LED 2** | Salida | OK |
| **40** | **LED 3** | Salida | OK |
| **41** | **LED 4** | Salida | OK |
| **42** | **LED 5** | Salida | OK |
