# iMakie
A Mackie Control interface with ESP32

Thanks for the additional details! I understand that **I2C has been problematic on pins other than GPIO 8 and 9** (the default I2C pins for the LOLIN S2 Mini in Arduino/PlatformIO), and you want to use a **240x280 TFT display with LovyanGFX leveraging DMA** (Direct Memory Access) for faster SPI transfers. The project prioritizes the **RSA0N11M9A0J** (10 kΩ motorized fader with touch), **encoder with button**, **4 additional buttons**, **NeoPixel**, and **motor PWM at 20 kHz**, with an **ESP32 communicating with the LOLIN S2 Mini (ESP32-S2) via I2C**. The **potentiometer** (part of RSA0N11M9A0J) suffers from **noise** (from 20 kHz PWM and WiFi ~50 mV, datasheet 3.2.3) and **saturation at 2.68V** (due to 6dB attenuation). Since you’re prototyping and open to moving components, I’ll propose an **optimized pinout** prioritizing **I2C on GPIO 8/9** (to avoid past issues), **TFT with DMA** using LovyanGFX, and robust readings for the RSA0N11M9A0J, encoder, and buttons. I’ll address noise, saturation, and conflicts, using the **ESP32-S2 datasheet** (Espressif), LOLIN S2 Mini pinout (1-18, 21, 33-40), PlatformIO documentation, and electronics principles. As the devil’s advocate, I’ll critique every decision to expose risks. Let’s dive in!

---

### 1. **Context and Requirements**
- **Setup**:
  - **Board**: LOLIN S2 Mini (ESP32-S2). Exposed pins: 1-18, 21, 33-40.
  - **I2C**: Critical for communication with an external ESP32. Past issues on non-default pins (not 8/9), likely due to EMI, improper pull-ups, or wiring. GPIO 8/9 are default in Arduino (`Wire.begin()`).
  - **TFT 240x280**: Likely ST7789, using LovyanGFX with **DMA** for fast SPI transfers. Requires 5 pins (SCLK, MOSI, CS, DC, RST). High current (~100 mA).
  - **RSA0N11M9A0J**: Motorized fader (10 kΩ, linear) with touch. Needs 1 ADC pin (potentiometer) + 1 touch/ADC pin (touch). Noise-sensitive, saturation at 2.68V (6dB, needs 11dB).
  - **Encoder**: 2 pins (A/B) + 1 button. Interrupt-driven, noise-sensitive.
  - **4 Buttons**: Digital inputs, pull-ups required.
  - **NeoPixel**: WS2812, 1 data pin (800 kHz).
  - **Motor PWM**: 20 kHz, DRV8833, 2 pins (IN1/IN2), jumper for enable. EMI source.
  - **UART Debug**: Needed, reassignable to avoid conflicts.
- **Challenges**:
  - **I2C Issues**: Non-default pins (e.g., 3/4) failed, possibly due to EMI (PWM, SPI, WiFi), weak pull-ups, or long cables. GPIO 8/9 near SPI (5-7) risk EMI.
  - **TFT DMA**: ESP32-S2 supports SPI DMA (datasheet 3.6), but LovyanGFX DMA requires specific SPI peripherals (HSPI/VSPI) and pins. High SPI frequency (>10 MHz) increases EMI.
  - **Noise**: Potentiometer (RSA0N11M9A0J) affected by 20 kHz PWM (GPIO 16/18) and WiFi (~50 mV). Touch sensitive to EMI/humidity.
  - **Saturation**: 2.68V on ADC (6dB, ~2.56V max) requires 11dB (0-3.6V) or corrected divider.
  - **Power**: Motor (~500 mA), TFT (~100 mA), NeoPixel (~60 mA/LED) stress LOLIN’s 3.3V regulator (AMS1117, ~600 mA).
- **Objectives**:
  - Assign I2C to GPIO 8/9 (default, stable) for ESP32 communication.
  - Configure TFT with LovyanGFX DMA (HSPI, pins 5-8/33).
  - Prioritize RSA0N11M9A0J (ADC + touch), encoder, 4 buttons, NeoPixel.
  - Minimize noise (20 kHz PWM, WiFi) and fix saturation.
  - Ensure stability in prototyping (breadboard, LOLIN S2 Mini).

**Devil’s Advocate Critique**: I2C on 8/9 is a safe bet, but past failures on other pins scream bad wiring or EMI ignorance. TFT DMA sounds fancy, but SPI EMI will wreck I2C and ADC. RSA0N11M9A0J’s dual touch is overkill – why two touch systems? Pin count is tight, and LOLIN’s weak regulator will collapse. Prototyping flexibility doesn’t excuse poor planning. Let’s optimize, but I’ll tear it apart!

---

### 2. **I2C on GPIO 8/9 and Past Issues**
- **Why GPIO 8/9?**:
  - Default in Arduino/PlatformIO for LOLIN S2 Mini (`Wire.begin()` uses GPIO 8 SDA, 9 SCL; `pins_arduino.h`).
  - Stable in many setups due to driver optimization and community testing.
  - Past issues on other pins (e.g., 3/4) likely due to:
    - **EMI**: Proximity to PWM (16/18) or SPI (5-7).
    - **Pull-ups**: Weak internal pull-ups (~10 kΩ, datasheet 3.5) or missing external pull-ups (4.7 kΩ recommended).
    - **Cables**: Long/unshielded wires in breadboard acting as EMI antennas.
    - **Configuration**: Incorrect I2C clock (e.g., >400 kHz) or slave address conflicts.
- **ESP32-S2 I2C**:
  - Supports I2C master/slave on any GPIO (datasheet 3.5).
  - GPIO 8/9: No UART/touch conflicts (GPIO 8 is HSPI_DC, 9 is Touch9, both reassignable).
  - External pull-ups (4.7 kΩ) critical for stability, especially with ESP32 slave.
- **Mitigation**:
  - Use GPIO 8/9 with 4.7 kΩ pull-ups.
  - Short, shielded cables (<10 cm).
  - Test with `i2c_scanner` to confirm ESP32 slave (e.g., address 0x08).

**Critique**: GPIO 8/9 are default, but near SPI (5-7) with DMA-enabled TFT. High-speed SPI (>10 MHz) will couple EMI to I2C. Past failures suggest you didn’t use pull-ups or shielding. Why not fix wiring instead of avoiding 3/4? Lazy!

---

### 3. **TFT with LovyanGFX and DMA**
- **TFT 240x280**:
  - Likely ST7789 (common for 240x280, 3.3V, SPI).
  - LovyanGFX supports DMA on ESP32-S2 (HSPI/VSPI, datasheet 3.6).
  - DMA reduces CPU load for large transfers (240x280 = 67.2K pixels, ~200 KB/frame at 16-bit color).
- **Requirements**:
  - Pins: SCLK, MOSI, CS, DC, RST (5 pins).
  - HSPI (GPIO 5-8) preferred for DMA (LovyanGFX default, `LGFX_LOLIN_S2_MINI.hpp`).
  - Frequency: <20 MHz (ST7789 max, ~10 MHz safer to avoid CPU choke).
  - Current: ~100 mA, needs 5V external supply.
- **DMA Setup**:
  - LovyanGFX enables DMA automatically if supported (ESP32-S2 HSPI).
  - Config in `LGFX`:
    ```cpp
    LGFX tft;
    void setup() {
      tft.init();
      tft.setRotation(1); // Adjust for 240x280
    }
    ```
  - Pins: SCLK=5, MOSI=6, CS=7, DC=2 (moved from 8), RST=33.

**Critique**: DMA is great for TFT speed, but high SPI frequency (>10 MHz) is an EMI bomb near I2C (8/9). Moving DC to GPIO 2 risks boot issues (Touch2). LovyanGFX is flexible, but misconfigure DMA and you’ll crash the ESP32-S2. Did you test current draw? LOLIN’s regulator will choke!

---

### 4. **RSA0N11M9A0J Analysis**
- **Specs**: 10 kΩ linear motorized fader with touch (Alps Alpine).
  - **Potentiometer**: VCC=3.3V, wiper to ADC (GPIO 10), GND. 11dB (0-3.6V) avoids saturation.
  - **Touch**: Capacitive, connects to touch pin (GPIO 1, Touch1) or ADC (if resistive). Prioritize touch pin for sensitivity.
- **Challenges**:
  - ADC noise from 20 kHz PWM (GPIO 16/18) and WiFi (~50 mV).
  - Touch sensitive to EMI/humidity.
  - Motorized fader may draw current (check datasheet, ~100 mA).

**Critique**: Dual touch (RSA0N11M9A0J + fader touch) is redundant – pick one! ADC and touch pins are EMI magnets. Did you test touch reliability? Humidity or PWM will trigger false positives.

---

### 5. **Pinout Optimized**
I assign **I2C to GPIO 8/9** (default, stable), move **potentiometer to GPIO 10** (ADC1_CH9), **TFT DC to GPIO 2**, and include encoder, 4 buttons, NeoPixel, and TFT with DMA. Pins are chosen to minimize EMI, conflicts, and ensure stability.

| Component | Function | Definition | Pin GPIO | Justification and Notes |
|---|---|---|---|---|
| RSA0N11M9A0J (Pot.) | Position | FADER_POT | **10** | **ADC1_CH9**. Free, no UART/touch, boot-safe. 11dB (0-3.6V) avoids saturation (2.68V). RC (470 Ω, 0.1 µF), cap 0.1 µF. Far from PWM (16/18), less EMI. VCC=3.3V, 10 kΩ. |
| RSA0N11M9A0J (Touch) | Capacitive Touch | FADER_TOUCH | **1** | **Touch1**. Prioritized for touch. Calibrate threshold (`touchRead(1)`). Disable UART0_TXD, Touch2-9. EMI/humidity risk. |
| Motor PWM (IN1) | PWM Control | MOTOR_IN1 | 18 | PWM 20 kHz. Caps 0.1 µF+10 µF on DRV8833. Ferrite. Far from GPIO 10. |
| Motor PWM (IN2) | PWM Control | MOTOR_IN2 | 16 | PWM 20 kHz. Slew rate (`ledc_set_fade`). Near encoder (12), EMI risk. |
| Driver Enable | Enable (HIGH) | DRV_ENABLE | - | Jumper 3.3V. Frees GPIO 33. |
| Encoder A (INT) | Interrupt | ENCODER_A | 13 | Free, no touch/UART. Pull-up external (4.7 kΩ). |
| Encoder B (DIR) | Direction | ENCODER_B | 12 | Free. Pull-up external. Near PWM (16), EMI risk. |
| Encoder Button | Push | ENCODER_BUTTON | 11 | Free. Pull-up external/internal. |
| Button 1 | Digital Input | BUTTON_1 | 14 | Free, no touch/UART. Pull-up external (4.7 kΩ). |
| Button 2 | Digital Input | BUTTON_2 | 17 | Free. Pull-up external. Near PWM (16), EMI risk. |
| Button 3 | Digital Input | BUTTON_3 | 34 | Free, input-only. Pull-up external mandatory. Far from PWM/SPI. |
| Button 4 | Digital Input | BUTTON_4 | 35 | Input-only. Pull-up external. Near crystal (noise risk). |
| NeoPixel | LED Data | NEOPIXEL | 36 | Free, input/output. 800 kHz. Far from PWM/SPI. High current (~60 mA/LED). |
| TFT - SCLK | SPI Clock | TFT_SCLK | 5 | HSPI, <20 MHz (DMA-enabled). LovyanGFX. |
| TFT - MOSI | SPI Data | TFT_MOSI | 6 | HSPI, high current (~100 mA), 5V external. |
| TFT - CS | Chip Select | TFT_CS | 7 | HSPI, LovyanGFX. |
| TFT - DC | Data/Command | TFT_DC | **2** | Moved from 8 (I2C). Free, Touch2/boot-sensitive (disable Touch2, pull-up external). |
| TFT - RST | Reset | TFT_RST | 33 | Output digital or soft reset (LovyanGFX). |
| UART (Debug) | Serial TX/RX | UART_TX/RX | 21/15 | UART0 reassigned. Frees GPIO 1/3. |
| I2C SDA | I2C Data | I2C_SDA | **8** | Default (Arduino). Pull-up 4.7 kΩ. Near SPI (5-7), EMI risk with DMA. |
| I2C SCL | I2C Clock | I2C_SCL | **9** | Default. Pull-up 4.7 kΩ. EMI risk from SPI DMA. |

**Free Pins**: 3, 4, 37-40.

---

### 6. **PlatformIO Configuration**
- **Arduino**:
  ```cpp
  #include <Wire.h>
  #include <LovyanGFX.hpp>
  #include <Adafruit_NeoPixel.h>
  #define I2C_SDA 8
  #define I2C_SCL 9
  #define POT_PIN 10
  #define TOUCH_PIN 1
  #define NEOPIXEL_PIN 36
  #define BUTTON_1 14
  #define BUTTON_2 17
  #define BUTTON_3 34
  #define BUTTON_4 35
  LGFX tft;
  Adafruit_NeoPixel strip(16, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800); // Adjust LED count
  void setup() {
    Wire.begin(I2C_SDA, I2C_SCL); // I2C on 8/9
    analogSetAttenuation(ADC_11db); // Pot 0-3.6V
    pinMode(BUTTON_1, INPUT_PULLUP);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(BUTTON_3, INPUT); // External pull-up
    pinMode(BUTTON_4, INPUT); // External pull-up
    strip.begin();
    tft.init(); // LovyanGFX: SCLK=5, MOSI=6, CS=7, DC=2, RST=33
    tft.startWrite(); // Enable DMA
    tft.setRotation(1); // Adjust for 240x280
    touch_pad_deinit(TOUCH_PAD_NUM2); // Disable Touch2-9
  }
  ```
  - `platformio.ini`:
    ```ini
    [env:lolin_s2_mini]
    platform = espressif32
    board = lolin_s2_mini
    framework = arduino
    lib_deps = 
        lovyangfx/LovyanGFX
        adafruit/Adafruit NeoPixel
    ```
- **ESP-IDF**:
  ```c
  #include <driver/i2c.h>
  #include <driver/adc.h>
  #include <driver/touch_pad.h>
  void app_main() {
    // I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    // ADC
    adc1_config_channel_atten(ADC1_CHANNEL_9, ADC_ATTEN_DB_11);
    // Touch
    touch_pad_init();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_SW);
    touch_pad_config(TOUCH_PAD_NUM1, 0);
    touch_pad_set_thresh(TOUCH_PAD_NUM1, 500); // Calibrate
    touch_pad_deinit(TOUCH_PAD_NUM2); // Disable Touch2-9
  }
  ```
  - `platformio.ini`:
    ```ini
    [env:lolin_s2_mini]
    platform = espressif32
    board = lolin_s2_mini
    framework = espidf
    ```

---

### 7. **Measures to Minimize Noise and Saturation**
For **RSA0N11M9A0J** (GPIO 10 pot, GPIO 1 touch):

1. **ADC Attenuation 11dB**:
   - **Solution**: `analogSetAttenuation(ADC_11db);` (Arduino) or `adc1_config_channel_atten(ADC1_CHANNEL_9, ADC_ATTEN_DB_11);` (ESP-IDF). Pot: VCC=3.3V, wiper to GPIO 10, GND.
   - **Justification**: Avoids saturation (2.68V, 6dB limit ~2.56V). No divider needed.
   - **Critique**: Loses precision (~2 mV/bit). EMI/WiFi (~50 mV) persists.

2. **RC Filter**:
   - **Solution**: \( R = 470 \Omega \), \( C = 0.1 \mu F \), \( f_c \approx 3.4 kHz \). Attenuates 20 kHz (~15 dB).
   - **Implementation**: R in series (wiper to GPIO 10), C to GND, near pin.
   - **Critique**: Latency ~47 µs, test with PID. 10 kΩ pot OK, but verify.

3. **Decoupling Capacitors**:
   - **Solution**: 0.1 µF (GPIO 10-GND), 0.01 µF (pot VCC-GND), 0.1 µF+10 µF (DRV8833).
   - **Implementation**: Solder near pins. 5V external for motor+TFT+NeoPixel.
   - **Critique**: Reduces ripple, not radiated EMI. LOLIN regulator collapses without 5V.

4. **Software Filter**:
   - **Solution**: Moving average (8 samples):
     ```cpp
     #define SAMPLES 8
     int readPot() {
       long sum = 0;
       for (int i = 0; i < SAMPLES; i++) {
         sum += analogRead(10);
         delayMicroseconds(100);
       }
       return sum / SAMPLES;
     }
     ```
   - **Critique**: Latency ~0.8 ms. Combine with RC. PID tuning needed.

5. **Touch on GPIO 1**:
   - **Solution**: `touchRead(1)` (Arduino) or `touch_pad_set_thresh(TOUCH_PAD_NUM1, thresh);` (ESP-IDF). Calibrate threshold.
   - **Implementation**: Disable Touch2-9 (`touch_pad_deinit(TOUCH_PAD_NUM2)`).
   - **Critique**: Redundant with RSA0N11M9A0J touch. EMI/humidity = false positives.

6. **I2C Stability (GPIO 8/9)**:
   - **Solution**: `Wire.begin(8, 9);` (Arduino) or ESP-IDF as above. 4.7 kΩ pull-ups. Short, shielded cables (<10 cm). Test with ESP32 slave (`i2c_scanner`).
   - **Implementation**: Address ESP32 slave (e.g., 0x08). Clock ≤100 kHz for stability.
   - **Critique**: Near SPI (5-7) with DMA, EMI risk. Past issues suggest poor wiring.

7. **Layout and Shielding**:
   - **Solution**: Shielded cables (<10 cm) for I2C, pot, touch. Ferrite on motor. PCB: GND plane, separate ADC/I2C from PWM/SPI.
   - **Critique**: Breadboard = EMI disaster. LOLIN not industrial.

8. **Motor EMI**:
   - **Solution**: Caps 0.1 µF+10 µF on DRV8833. Slew rate (`ledc_set_fade`). 5V external.
   - **Critique**: Reduces but doesn’t eliminate radiated EMI.

9. **TFT DMA**:
   - **Solution**: LovyanGFX on HSPI (5-7, 2, 33). `<20 MHz. `tft.startWrite()` enables DMA.
   - **Critique**: High SPI frequency = EMI to I2C/ADC. Test DMA stability.

---

### 8. **Devil’s Advocate Critiques**
- **I2C on 8/9**: Default, stable, but near SPI (5-7) with DMA. High-speed SPI (>10 MHz) will couple EMI to I2C. Past failures on 3/4 suggest bad pull-ups or cables, not pins. Fix wiring!
- **Potentiometer on 10**: Safe ADC, but 20 kHz/WiFi noise persists. RC/caps mandatory. VCC=5V? Fried GPIO 10.
- **Touch on 1**: Redundant with RSA0N11M9A0J touch. EMI/humidity = false positives. Interrupts clash with encoder.
- **TFT DMA**: Fast, but EMI from SPI wrecks I2C/ADC. DC on GPIO 2 (Touch2/boot-sensitive) risks boot issues. Misconfigure LovyanGFX and crash.
- **Buttons/NeoPixel**: 34/35 need external pull-ups. NeoPixel current (~60 mA/LED) kills regulator without 5V.
- **Global**: Breadboard = EMI mess. LOLIN regulator weak. No oscilloscope? You’re blind to noise. ADS1115 (I2C 8/9) better for fader.

---

### 9. **Final Recommendation**
**Pinout above** is optimized for I2C on GPIO 8/9 (default, stable), TFT with DMA (LovyanGFX), and RSA0N11M9A0J (GPIO 10/1). **I2C issues** likely stem from long cables or weak pull-ups – use 4.7 kΩ and shielded cables.

**Implementation**:
- **I2C**: `Wire.begin(8, 9);`, 4.7 kΩ pull-ups, shielded cables. Test with ESP32 slave (`i2c_scanner`).
- **Potentiometer**: GPIO 10, `analogRead(10)`, 11dB, RC (470 Ω, 0.1 µF), cap 0.1 µF, VCC=3.3V.
- **Touch**: GPIO 1, `touchRead(1)`, calibrate, disable Touch2-9.
- **TFT**: LovyanGFX (SCLK=5, MOSI=6, CS=7, DC=2, RST=33), `<20 MHz, DMA via `tft.startWrite()`.
- **Buttons/Encoder**: Pull-ups 4.7 kΩ.
- **NeoPixel**: GPIO 36, `Adafruit_NeoPixel`.
- **Motor**: Caps, ferrite, 5V external.
- **Test**: Serial Plotter (UART 21/15) for noise on GPIO 10. Oscilloscope for I2C/ADC.

**Devil’s Advocate Final Critique**: I2C on 8/9 fixes past issues, but SPI DMA (5-7) near 8/9 is an EMI trap. RSA0N11M9A0J on 10/1 is solid, but dual touch is pointless. TFT DMA is fast but risky – misconfigure and crash. Breadboard = EMI hell, LOLIN regulator will die without 5V. ADS1115 on I2C 8/9 for fader? Schematic, code, or PCB details? Spill it, LOLIN’s no superhero! 😈

**Date/Time**: October 23, 2025, 10:52 PM CEST.
