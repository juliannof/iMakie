
# iMakie
A Mackie Control interface with ESP32
---
<img alt="ESP32 S2" src="https://www.wemos.cc/en/latest/_static/boards/s2_mini_v1.0.0_4_16x9.jpg">
| GPIO | Función | Dispositivo | Notas |
|------|---------|-------------|-------|
| **1** | FADER_TOUCH | Touch1 nativo ESP32-S2, activo sin IC externo |
| **3** | TFT_BL | ST7789V3 | PWM 500Hz backlight |
| **4** | TFT_MOSI | ST7789V3 | SPI3_HOST |
| **5** | TFT_CS | ST7789V3 | SPI3_HOST |
| **6** | TFT_DC | ST7789V3 | SPI3_HOST |
| **7** | TFT_SCLK | ST7789V3 | SPI3_HOST, 5MHz |
| **10** | FADER_POT | Potenciómetro | ADC1_CH9 |
| **11** | ENCODER_BTN | Encoder | Pull-up |
| **12** | ENCODER_B | Encoder | Pull-up |
| **13** | ENCODER_A | Encoder | Pull-up, interrupción |
| **14** | DRV_ENABLE | DRV8833 | |
| **16** | MOTOR_IN2 | DRV8833 | PWM |
| **18** | MOTOR_IN1 | DRV8833 | PWM |
| **20** | RS485_EN | MAX485 | TX enable |
| **33** | TFT_RST | ST7789V3 | Pulso LOW 100ms obligatorio |
| **36** | NEOPIXEL | WS2812B ×4 | NEO_GRB |
| **37** | BTN_REC | Botón | ▲ / up |
| **38** | BTN_SOLO | Botón | ▼ / down |
| **39** | BTN_MUTE | Botón | ◄ / back |
| **40** | BTN_SELECT | Botón | ► / enter |

GPIO **8/9** (I2C) y **1** quedan libres en rev2. ¿Hay algo nuevo que quieras asignar, o los dejamos como pads de reserva en PCB?
