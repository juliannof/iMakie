
# iMakie
A Mackie Control interface with ESP32
---
<img alt="ESP32 S2" src="https://www.wemos.cc/en/latest/_static/boards/s2_mini_v1.0.0_4_16x9.jpg">

Aquí la tabla definitiva **PTxx Track S2 rev2**:

| GPIO | Función | Define | Notas |
|------|---------|--------|-------|
| **1** | Touch fader (T1) | `FADER_TOUCH_PIN` | Pin T del RSA0N11M9A0J. Desactivado hasta PCB |
| **3** | Backlight TFT (PWM) | `TFT_BL` | LEDC PWM 500Hz |
| **4** | DC TFT | `TFT_DC` | SPI3 |
| **5** | CS TFT | `TFT_CS` | SPI3 |
| **6** | MOSI TFT | `TFT_MOSI` | SPI3 |
| **7** | SCLK TFT | `TFT_SCLK` | SPI3 |
| **8** | RS485 TX | `RS485_TX_PIN` | UART |
| **9** | RS485 RX | `RS485_RX_PIN` | UART |
| **10** | ADC fader | `FADER_POT` | ADC1_CH9 |
| **11** | Encoder SW | `ENCODER_SW_PIN` | Pull-up interno |
| **12** | Encoder B | `ENCODER_PIN_B` | Pull-up interno |
| **13** | Encoder A | `ENCODER_PIN_A` | Pull-up interno |
| **14** | Motor EN | `MOTOR_EN` | DRV8833 |
| **15** | LED integrado | `LED_BUILTIN_PIN` | Debug touch |
| **16** | Motor IN2 | `MOTOR_IN2` | DRV8833 PWM |
| **17** | RS485 EN | `RS485_ENABLE_PIN` | Half-duplex |
| **18** | Motor IN1 | `MOTOR_IN1` | DRV8833 PWM |
| **19** | USB D- | — | ⛔ Reservado |
| **20** | USB D+ | — | ⛔ Reservado |
| **33** | RST TFT | `TFT_RST` | Pulse manual obligatorio |
| **36** | NeoPixel data | `NEOPIXEL_PIN` | NeoPixelBus I2S, 4× WS2812B |
| **37** | Botón REC | `BUTTON_PIN_REC` | Pull-up interno |
| **38** | Botón SOLO | `BUTTON_PIN_SOLO` | Pull-up interno |
| **39** | Botón MUTE | `BUTTON_PIN_MUTE` | Pull-up interno |
| **40** | Botón SELECT | `BUTTON_PIN_SELECT` | Pull-up interno |

**Libres:** 0, 2, 21, 34, 35, 41, 42, 45, 46, 47

**Eliminados en rev2:** AT42QT1011, MP1584EN local
**Alimentación:** 5V y 9V desde buses centrales del S3 master

¿La pasamos a `config.h`?
