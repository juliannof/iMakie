
# iMakie
A Mackie Control interface with ESP32
---
<img alt="ESP32 S2" src="https://www.wemos.cc/en/latest/_static/boards/s2_mini_v1.0.0_4_16x9.jpg">

Aquí la tabla definitiva **PTxx Track S2 rev2**:

| GPIO | Definición | Función | Notas |
|------|-----------|---------|-------|
| **1** | `FADER_TOUCH_PIN` | Touch capacitivo fader | T1 nativo ESP32-S2, sin IC externo |
| **3** | `TFT_BL` | Backlight display | PWM 500Hz, MOSFET driver |
| **4** | `TFT_MOSI` | SPI display | SPI3_HOST |
| **5** | `TFT_CS` | SPI display | SPI3_HOST |
| **6** | `TFT_DC` | SPI display | SPI3_HOST |
| **7** | `TFT_SCLK` | SPI display | SPI3_HOST, 5MHz |
| **8** | `RS485_TX_PIN` | RS485 TX | Serial1 |
| **9** | `RS485_RX_PIN` | RS485 RX | Serial1 |
| **10** | `FADER_POT_PIN` | ADC fader | ADC1_CH9 |
| **11** | `ENCODER_SW_PIN` | Encoder botón | Pull-up |
| **12** | `ENCODER_PIN_B` | Encoder dirección | Pull-up |
| **13** | `ENCODER_PIN_A` | Encoder clock | Pull-up, interrupción |
| **14** | `MOTOR_EN` | DRV8833 enable | |
| **16** | `MOTOR_IN2` | DRV8833 PWM | |
| **18** | `MOTOR_IN1` | DRV8833 PWM | |
| **33** | `TFT_RST` | Reset display | Pulso LOW 100ms obligatorio |
| **35** | `RS485_ENABLE_PIN` | RS485 TX enable | HIGH=TX, LOW=RX |
| **36** | `NEOPIXEL_PIN` | WS2812B ×4 | NEO_GRB |
| **37** | `BUTTON_PIN_REC` | Botón REC | ▲ up, long-press → SAT |
| **38** | `BUTTON_PIN_SOLO` | Botón SOLO | ▼ down |
| **39** | `BUTTON_PIN_MUTE` | Botón MUTE | ◄ back |
| **40** | `BUTTON_PIN_SELECT` | Botón SELECT | ► enter |

**Eliminados en rev2:** AT42QT1011, MP1584EN local
**Alimentación:** 5V y 9V desde buses centrales del S3 master

¿La pasamos a `config.h`?
