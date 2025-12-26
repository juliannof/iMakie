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

| GPIO | Función asignada                 | Dispositivo/Nota                  | Comentarios                           |
|------|---------------------------------|----------------------------------|---------------------------------------|
| 0    | Libre                           | -                                | Expansión / futuro                     |
| 1    | Libre                           | -                                | Expansión / futuro                     |
| 2    | DC pantalla                     | SPI Pantalla                     | Data/Command                            |
| 3    | Libre                           | -                                | Expansión / futuro                     |
| 4    | RESET pantalla                  | SPI Pantalla                     | Reset pantalla                          |
| 5    | Libre                           | -                                | Expansión / futuro                     |
| 6    | No usar                        | Flash QSPI                       | Flash interna                           |
| 7    | No usar                        | Flash QSPI                       | Flash interna                           |
| 8    | SDA I²C NetTrellis              | NetTrellis                        | Pull-up externa 4.7kΩ                  |
| 9    | SCL I²C NetTrellis              | NetTrellis                        | Pull-up externa 4.7kΩ                  |
| 10   | No usar                        | Flash QSPI                       | Flash interna                           |
| 11   | No usar                        | Flash QSPI                       | Flash interna                           |
| 12   | MISO SPI pantalla               | SPI Pantalla                     | Entrada pantalla                        |
| 13   | MOSI SPI pantalla               | SPI Pantalla                     | Salida pantalla                         |
| 14   | SCK SPI pantalla                | SPI Pantalla                     | Clock                                   |
| 15   | CS SPI pantalla                 | SPI Pantalla                     | Chip select                             |
| 16   | TX UART → RP2040 Zero           | UART MIDI                        | Nivel lógico 3.3V                       |
| 17   | RX UART ← RP2040 Zero           | UART MIDI                        | Nivel lógico 3.3V                       |
| 18   | SDA I²C S2 esclavos             | 9 × ESP32-S2                     | Pull-up externa 4.7kΩ, 100 kHz         |
| 19   | SCL I²C S2 esclavos             | 9 × ESP32-S2                     | Pull-up externa 4.7kΩ, 100 kHz         |
| 20   | Libre                           | -                                | Expansión / futuro                     |
| 21   | Libre                           | -                                | Expansión / futuro                     |
| 22   | Libre                           | -                                | Expansión / futuro                     |
| 23   | Libre                           | -                                | Expansión / futuro                     |
| 24   | Libre                           | -                                | Expansión / futuro                     |
| 25   | Botón 1                         | Botones físicos                  | Entrada con pull-up interno            |
| 26   | Botón 2                         | Botones físicos                  | Entrada con pull-up interno            |
| 27   | Botón 3                         | Botones físicos                  | Entrada con pull-up interno            |
| 28   | Botón 4                         | Botones físicos                  | Entrada con pull-up interno            |
| 29   | Botón 5                         | Botones físicos                  | Entrada con pull-up interno            |
| 30   | Libre                           | -                                | Expansión / futuro                     |
| 31   | Libre                           | -                                | Expansión / futuro                     |
| 32   | LED 1                            | LEDs asociados a botones         | Salida digital                           |
| 33   | LED 2                            | LEDs asociados a botones         | Salida digital                           |
| 34   | LED 3                            | LEDs asociados a botones         | Salida digital                           |
| 35   | LED 4                            | LEDs asociados a botones         | Salida digital                           |
| 36   | LED 5                            | LEDs asociados a botones         | Salida digital                           |
| 37   | Libre                           | -                                | Expansión / futuro                     |
| 38   | Libre                           | -                                | Expansión / futuro                     |
| 39   | Libre                           | -                                | Expansión / futuro                     |
| 40   | Libre                           | -                                | Expansión / futuro                     |
| 41   | Libre                           | -                                | Expansión / futuro                     |
| 42   | Libre                           | -                                | Expansión / futuro                     |
| 43   | Libre                           | -                                | Expansión / futuro                     |













