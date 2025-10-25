# iMakie
A Mackie Control interface with ESP32
---
### 1. **Contexto y Requerimientos**
- **Setup**:
  - **Placa**: LOLIN S2 Mini (ESP32-S2) para prototipado. Placa personalizada en diseño final. Pines expuestos: 1-18, 21, 33-40.
  - **Alimentación**:
    - **10V**: Motor del fader (DRV8833, 2.7-10.8V, ~500 mA).
    - **5V**: TFT backlight (~100 mA), NeoPixel (4 LEDs, ~240 mA máx, 60 mA/LED).
    - **3.3V**: Regulador externo (desde 5V) para ESP32-S2 (~100 mA), potenciómetro (RSA0N11M9A0J, ~0.33 mA), encoder (~10 mA), TFT lógica (~10 mA). Total: ~120 mA. Bypassa regulador AMS1117 del LOLIN (~600 mA).
  - **I2C**: Comunica con ESP32-S3 maestro (3.3V). GPIO 8/9 (Arduino default) por estabilidad; problemas previos en otros pines (ej. 3/4, probablemente EMI, pull-ups débiles, cables largos).
  - **RSA0N11M9A0J**: Fader motorizado (10 kΩ, lineal) con touch capacitivo. 1 pin ADC (potenciómetro, 3.3V) + 1 pin touch (Touch1). Ruido por PWM 20 kHz (DRV8833). Saturación a 2.68V (6dB, necesita 11dB).
  - **Motor PWM**: DRV8833, 2 pines (IN1/IN2, 20 kHz) para fader, jumper enable, 10V (o 5V).
  - **Encoder**: 2 pines (A/B, interrupciones, panorama) + 1 botón (select para jog wheel). Sin LEDs, feedback en TFT.
  - **Botones**: Rec, Solo, Mute, Select manejados por ESP32-S3. ESP32-S2 recibe comandos I2C para actualizar NeoPixel (feedback) y TFT.
  - **TFT 240x280**: Probablemente ST7789, LovyanGFX con DMA (HSPI, 5 pines: SCLK, MOSI, CS, DC, RST). Lógica 3.3V, backlight 5V (~100 mA). Muestra nombre del track, vúmetro, panorama, posición del fader en dB.
  - **NeoPixel**: WS2812, 1 pin datos (800 kHz), **4 LEDs (~240 mA máx)**, 5V alimentación, lógica 3.3V. Feedback visual (Rec=0, Solo=1, Mute=2, Select=3).
  - **Debug USB**: UART en GPIO 21/15 (USB nativo del ESP32-S2).
  - **Sin WiFi**: Elimina ~50 mV de ruido en ADC.
- **Desafíos**:
  - **I2C**: Problemas previos en pines no predeterminados (EMI de PWM/SPI, pull-ups débiles, cables largos). GPIO 8/9 cerca de SPI (5-7) con DMA, riesgo EMI.
  - **TFT DMA**: HSPI (GPIO 5-7, DC reasignado) aumenta EMI, afecta I2C/ADC.
  - **Ruido**: RSA0N11M9A0J (ADC/touch) sensible a PWM 20 kHz (GPIO 16/18).
  - **Saturación**: ADC a 2.68V (6dB, ~2.56V máx) necesita 11dB (0-3.6V).
  - **NeoPixel**: 4 LEDs (1 por botón) controlados por ESP32-S2 tras comandos I2C.
  - **TFT**: Debe mostrar nombre del track, vúmetro, panorama, y posición del fader en dB, actualizado vía I2C.
  - **Placa Personalizada**: Layout crítico para minimizar EMI. Regulador 3.3V desde 5V debe soportar ~120 mA.
- **Objetivos**:
  - I2C estable en GPIO 8/9 (3.3V) para ESP32-S3.
  - TFT con LovyanGFX DMA (HSPI) mostrando nombre, vúmetro, panorama, fader en dB.
  - RSA0N11M9A0J (ADC + touch), encoder (panorama), 4 LEDs NeoPixel (feedback de botones).
  - Minimizar ruido PWM y resolver saturación.
  - Esquema de alimentación (10V, 5V, 3.3V desde 5V) para placa personalizada.

---

### 2. **Alimentación para la Placa Personalizada**
- **Entradas**:
  - **10V**: Motor del fader (DRV8833, ~500 mA, 2.7-10.8V). Conecta a VM del DRV8833.
  - **5V**: TFT backlight (~100 mA), NeoPixel (4 LEDs, ~240 mA máx). Total: ~340 mA. Fuente 5V ≥500 mA recomendada.
  - **3.3V**: Regulador externo (desde 5V) para ESP32-S2 (~100 mA), potenciómetro (RSA0N11M9A0J, ~0.33 mA), encoder (~10 mA), TFT lógica (~10 mA). Total: ~120 mA.
- **Regulador 3.3V**:
  - **Recomendación**: LM1117-3.3 (LDO, 800 mA) o TPS7A4700 (LDO, 1 A, bajo ruido) para convertir 5V a 3.3V.
  - **Circuito**:
    - Entrada 5V a VIN del regulador.
    - Salida 3.3V a ESP32-S2 (3V3 pin), potenciómetro VCC, encoder pull-ups, TFT lógica.
    - Capacitores: 10 µF (entrada 5V-GND), 10 µF + 0.1 µF (salida 3.3V-GND).
    - Ferrita en entrada 5V para reducir ripple.
  - **Justificación**: LM1117-3.3 suficiente para ~120 mA. TPS7A4700 mejor para bajo ruido en ADC (crítico para RSA0N11M9A0J).
- **Consideraciones**:
  - **5V**: Soporta TFT backlight (~100 mA) + NeoPixel (~240 mA) = ~340 mA. Fuente 5V ≥500 mA (ej. AMS1117-5.0 o externa).
  - **10V**: Motor (~500 mA). Fuente 10V ≥1 A recomendada.
  - **GND Común**: Conecta todos los GND (10V, 5V, 3.3V) en placa final para evitar bucles de tierra.
  - **Placa Final**: Regulador 3.3V cerca de ESP32-S2, trazas cortas, GND plane.

**Crítica**: Alimentación 10V/5V/3.3V es robusta, pero regulador 3.3V debe ser bajo ruido (TPS7A4700) o el ADC sufrirá. NeoPixel a 240 mA es manejable, pero fuente 5V débil = flickering. Breadboard = conexiones sueltas, placa final necesita GND plane. ¿Capacidad de tu fuente? ¡Sin calcular, fallarás!

---

### 3. **I2C en GPIO 8/9 (3.3V)**
- **Por Qué 8/9**:
  - Por defecto en Arduino/PlatformIO (`Wire.begin()`, `pins_arduino.h`: SDA=8, SCL=9).
  - Estables, optimizados en drivers. Problemas previos en otros pines (ej. 3/4) por:
    - **EMI**: Cercanía a PWM (16/18) o SPI (5-7).
    - **Pull-ups**: Internos débiles (~10 kΩ, datasheet 3.5) o externos insuficientes.
    - **Cables**: Largos/sin apantallar en breadboard.
  - GPIO 8 (HSPI_DC) y 9 (Touch9) reasignables, sin conflictos UART/boot.
- **Configuración**:
  - Pull-ups externos 4.7 kΩ a 3.3V (externo). ESP32-S3 maestro a 3.3V.
  - Cables apantallados, <10 cm, trenzados con GND (breadboard). Trazas cortas en placa final.
  - Reloj I2C ≤100 kHz para estabilidad.
  - Dirección I2C esclavo (LOLIN S2 Mini): Asumo 0x08 (ajustar según ESP32-S3).
- **Uso**:
  - ESP32-S2 recibe comandos I2C del ESP32-S3 para:
    - Actualizar posición del fader (PWM).
    - Actualizar NeoPixel (estado de Rec, Solo, Mute, Select).
    - Mostrar en TFT: nombre del track, vúmetro, panorama, posición del fader en dB.
  - ESP32-S2 envía al ESP32-S3: posición del fader (ADC), touch, y datos del encoder (panorama).
- **Mitigación**:
  - Placa final: Líneas I2C cortas, lejos de PWM (16/18) y SPI (5-7). GND plane.
  - Capacitor 0.1 µF (SDA/SCL a GND) para filtrar ruido.
  - Test con `i2c_scanner` para confirmar comunicación con ESP32-S3.

**Crítica**: I2C en 8/9 es sólido, pero SPI DMA (5-7) dispara EMI. Problemas pasados = cables malos o pull-ups débiles. Placa final necesita layout impecable. ¡Sin osciloscopio, estás ciego a ruido!

---

### 4. **TFT con LovyanGFX y DMA**
- **TFT 240x280**: Asumo ST7789 (3.3V lógica, 5V backlight, ~100 mA).
- **LovyanGFX DMA**:
  - ESP32-S2 soporta DMA en HSPI (datasheet 3.6).
  - Pines: SCLK=5, MOSI=6, CS=7, DC=2 (movido de 8), RST=33.
  - Frecuencia: <20 MHz (ST7789 máx), ~10 MHz para menos EMI.
- **Uso**: Muestra:
  - Nombre del track (texto, ej. "Track 1").
  - Vúmetro (gráfico, basado en datos I2C del ESP32-S3).
  - Panorama (gráfico o valor, basado en encoder).
  - Posición del fader en dB (ej. -∞ a +10 dB, mapeado desde ADC).
- **Alimentación**: Lógica a 3.3V (externo), backlight a 5V (resistencia limitadora según datasheet ST7789).
- **Placa Final**: Líneas SPI cortas, lejos de I2C (8/9) y ADC (10). GND plane.

**Crítica**: DMA acelera TFT, pero SPI >10 MHz es una bomba EMI para I2C (8/9) y ADC (10). DC en GPIO 2 (Touch2/boot-sensitive) arriesga arranque. ¡Mal layout = crash!

---

### 5. **NeoPixel como Feedback de Botones**
- **Configuración**: 4 LEDs WS2812 (5V alimentación, 3.3V lógica, ~240 mA máx). 1 LED por botón (Rec=0, Solo=1, Mute=2, Select=3).
- **Uso**: Feedback visual controlado por comandos I2C del ESP32-S3 (ej. Rec=rojo, Solo=amarillo, Mute=verde, Select=azul).
- **Pin**: GPIO 36 (lejos de PWM/SPI, bajo EMI).
- **Alimentación**: 5V, capacitor 1000 µF (VCC-GND) para picos de corriente.
- **Placa Final**: Línea de datos (GPIO 36) corta, cerca de NeoPixel. Fuente 5V ≥500 mA.

**Crítica**: 4 LEDs es eficiente, pero sincronización vía I2C exige precisión. Breadboard = conexiones inestables. ¡Mal layout en placa final = flickering!

---

### 6. **Pinout Optimizado**
Asigno **I2C a GPIO 8/9** (3.3V), **potenciómetro a GPIO 10** (ADC1_CH9), **TFT DC a GPIO 2**, **NeoPixel (4 LEDs)** para feedback de **Rec, Solo, Mute, Select**. Pines minimizan EMI, compatibles con placa personalizada. Como los botones son manejados por el ESP32-S3, los pines GPIO 14, 17, 34, 35 quedan libres.



¡Absolutamente! Mover los botones a pines GPIO más altos es una excelente idea. Los pines de numeración más alta en el ESP32-S2 (especialmente los >34, pero no los GPIOs de flash/PSRAM) son a menudo más "limpios" en términos de funciones multiplexadas y sensibilidades de arranque, lo que puede simplificar el desarrollo y evitar sorpresas.

Además, libera algunos de los pines de numeración más baja (GPIO 2, 4, 15, 21) que podrían ser útiles para otros propósitos más adelante (aunque ahora mismo el 2 y 4 quedan como muy buenas opciones libres).

Vamos a revisar los pines disponibles y reasignar los botones.

### Pines Libres y Altamente Recomendados para Botones:

Analizando tu tabla actual y los GPIOs del ESP32-S2, tenemos los siguientes pines de numeración alta que están actualmente libres:

*   **GPIO 37, 38, 39, 40, 41, 42, 43, 44, 45, 47** (GPIO 46 no es de uso general y no se saca en el D1).

Podemos elegir 4 de estos para los botones.

### Tabla de Asignación de Pines Actualizada (Botones en Pines Altos)

| Componente | Función | Definición | Pin GPIO | Justificación y Notas |
| :---------------- | :------------------ | :---------------------------- | :----------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| RSA0N11M9A0J (Pot.) | Posición Fader | FADER_POT | **10** | **ADC1_CH9**. Libre, sin UART/touch, boot-safe. 11dB (0-3.6V) evita saturación (2.68V). RC (470 Ω, 0.1 µF), cap 0.1 µF. Lejos PWM (16/18). VCC=3.3V. |
| RSA0N11M9A0J (Touch) | Tacto Capacitivo | FADER_TOUCH | **1** | **Touch1**. Priorizado para Mackie. Calibra umbral. Desactiva UART0_TXD, Touch2-9. EMI riesgo. |
| Motor PWM (IN1) | Control Fader | MOTOR_IN1 | 18 | PWM 20 kHz. Caps 0.1 µF+10 µF en DRV8833. Ferrita. 10V. Lejos FADER_POT (10). |
| Motor PWM (IN2) | Control Fader | MOTOR_IN2 | 16 | PWM 20 kHz. Slew rate. Cerca ENCODER_B (12), EMI riesgo. 10V. |
| Driver Enable | Habilitación (HIGH) | DRV_ENABLE | 34 | Conectado a 5V/10V. |
| Encoder A (INT) | Panorama (INT) | ENCODER_A | 13 | Libre, sin touch/UART. Pull-up 4.7 kΩ a 3.3V. Interrupción. |
| Encoder B (DIR) | Panorama (DIR) | ENCODER_B | 12 | Libre. Pull-up externo. Cerca PWM (16), EMI riesgo. |
| Botón Encoder | Pulsador (Jog Select) | ENCODER_BUTTON | 11 | Libre. Pull-up externo/interno a 3.3V. Feedback en TFT. |
| NeoPixel | Feedback Botones | NEOPIXEL | 36 | Libre, input/output. 800 kHz, 5V, lógica 3.3V. 4 LEDs (~240 mA): Rec=0 (rojo), Solo=1 (amarillo), Mute=2 (verde), Select=3 (azul). |
| **TFT - SCLK** | Reloj SPI | TFT_SCLK | **5** | HSPI, <20 MHz (DMA). LovyanGFX, lógica 3.3V, backlight 5V. |
| **TFT - MOSI** | Datos SPI | TFT_MOSI | **6** | HSPI, ~100 mA, 5V backlight. |
| **TFT - CS** | Chip Select | TFT_CS | **7** | HSPI, LovyanGFX. |
| **TFT - DC** | Data/Command | TFT_DC | **3** | Movido a GPIO 3. Libre, Touch2/boot-sensitive (desactiva Touch2, pull-up 3.3V). |
| **TFT - RST** | Reset | TFT_RST | **33** | Output digital o reset soft (LovyanGFX). |
| **TFT - BL** | BACKLIGHT (PWM) | TFT_BL | **35** | **LedC PWM**. Con driver MOSFET. Pin libre, lejos de pines SPI/PWM de motor. |
| I2C SDA | Datos I2C | I2C_SDA | **8** | Por defecto (Arduino). Pull-up 4.7 kΩ a 3.3V. Cerca SPI (5-7), EMI riesgo con DMA. |
| I2C SCL | Reloj I2C | I2C_SCL | **9** | Por defecto. Pull-up 4.7 kΩ a 3.3V. EMI riesgo. |
| **Botón 1** | Función de Control | BUTTON_1 | **37** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |
| **Botón 2** | Función de Control | BUTTON_2 | **38** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |
| **Botón 3** | Función de Control | BUTTON_3 | **39** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |
| **Botón 4** | Función de Control | BUTTON_4 | **40** | Pin de propósito general. Ideal para entrada. Pull-up externo/interno. |

---

### Cambios y Justificaciones:

1.  **Botones Reubicados:**
    *   `BUTTON_1`: **GPIO 2** -> **GPIO 37**
    *   `BUTTON_2`: **GPIO 4** -> **GPIO 38**
    *   `BUTTON_3`: **GPIO 15** -> **GPIO 39**
    *   `BUTTON_4`: **GPIO 21** -> **GPIO 40**

    Estos pines (37, 38, 39, 40) son GPIOs de propósito general excelentes para entradas digitales y no tienen funciones sensibles durante el arranque.

2.  **Pines Liberados para Otros Usos:**
    *   **GPIO 2**
    *   **GPIO 4**
    *   **GPIO 15**
    *   **GPIO 21**

    Estos pines quedan ahora completamente libres. El **GPIO 2** y **GPIO 4** son especialmente valiosos por su versatilidad. **GPIO 15** y **GPIO 21** siguen siendo las TX/RX del UART0, por lo que si más tarde necesitas un UART físico, ya los tienes disponibles (o dos GPIOs de propósito general adicionales).

Esta configuración es aún más robusta y te proporciona mayor flexibilidad para futuras expansiones o para evitar posibles conflictos que a veces surgen con pines de numeración más baja.


---

### 7. **Esquema de Conexiones para Placa Personalizada**
- **RSA0N11M9A0J**:
  - **Potenciómetro**: VCC a 3.3V (regulador), GND a GND, wiper a GPIO 10 vía RC (470 Ω, 0.1 µF a GND). Cap 0.01 µF (VCC-GND).
  - **Touch**: Pin touch a GPIO 1. Cap 0.1 µF (GPIO 1-GND).
  - **Layout**: Línea ADC (GPIO 10) corta, lejos de PWM (16/18) y SPI (5-7). GND plane.
- **Motor (DRV8833)**:
  - IN1/IN2 a GPIO 18/16 (PWM 20 kHz). VM a 10V, VCC a 3.3V (regulador). Caps 0.1 µF+10 µF (VM-GND). Ferrita en 10V. Enable a jumper (5V/10V).
  - **Layout**: Motor lejos de ADC (10) e I2C (8/9). Ferrita en 10V.
- **Encoder (Panorama)**:
  - A/B a GPIO 13/12, botón a GPIO 11. Pull-ups 4.7 kΩ a 3.3V. Cap 0.1 µF por pin a GND (debounce).
  - **Layout**: Líneas cortas, cerca de ESP32-S2.
- **NeoPixel**:
  - Datos a GPIO 36. VCC a 5V, GND común. Cap 1000 µF (VCC-GND). 4 LEDs (Rec=0, Solo=1, Mute=2, Select=3).
  - **Layout**: Línea datos corta, cerca de GPIO 36. Fuente 5V ≥500 mA.
- **TFT (ST7789)**:
  - SCLK=5, MOSI=6, CS=7, DC=2, RST=33. Lógica a 3.3V (regulador), backlight a 5V (resistencia limitadora según datasheet ST7789).
  - **Layout**: Líneas SPI cortas, lejos de I2C (8/9) y ADC (10). GND plane.
- **I2C**:
  - SDA/SCL a GPIO 8/9. Pull-ups 4.7 kΩ a 3.3V. Cables apantallados (<10 cm) en breadboard, trazas cortas en placa final. Conectar a ESP32-S3 maestro (SDA/SCL, 3.3V).
  - **Layout**: Líneas I2C lejos de PWM (16/18) and SPI (5-7). Cap 0.1 µF (SDA/SCL a GND).
- **UART (Debug)**:
  - TX/RX a GPIO 21/15 para USB debug.
- **Alimentación**:
  - **10V**: Motor (DRV8833, ~500 mA). Ferrita en entrada.
  - **5V**: TFT backlight (~100 mA), NeoPixel (~240 mA). Fuente ≥500 mA. Cap 10 µF (5V-GND).
  - **3.3V**: Regulador (LM1117-3.3 o TPS7A4700) desde 5V. Alimenta ESP32-S2, potenciómetro, encoder, TFT lógica (~120 mA). Caps 10 µF + 0.1 µF (3.3V-GND).
  - **Layout**: GND plane común. Separar trazas 10V/5V de 3.3V (ADC/I2C). Ferrita en 5V.

---

### 8. **Medidas para Ruido y Saturación**
Para **RSA0N11M9A0J** (GPIO 10 pot, GPIO 1 touch):

1. **Atenuación ADC 11dB**:
   - **Solución**: Configurar ADC a 11dB (0-3.6V). Pot: VCC a 3.3V (regulador), wiper a GPIO 10, GND.
   - **Justificación**: Evita saturación (2.68V). Sin divisor.
   - **Crítica**: Pierde precisión (~2 mV/bit). PWM 20 kHz persiste.

2. **Filtro RC**:
   - **Solución**: \( R = 470 \Omega \), \( C = 0.1 \mu F \), \( f_c \approx 3.4 kHz \). Atenúa 20 kHz (~15 dB).
   - **Implementación**: R en serie (wiper a GPIO 10), C a GND, cerca de ESP32-S2.
   - **Crítica**: Latencia ~47 µs, prueba con PID.

3. **Condensadores**:
   - **Solución**: 0.1 µF (GPIO 10-GND), 0.01 µF (pot VCC-GND), 0.1 µF+10 µF (DRV8833 VM-GND). Caps 10 µF + 0.1 µF en 3.3V y 5V.
   - **Crítica**: Reduce ripple, no EMI radiado.

4. **Touch en GPIO 1**:
   - **Solución**: Calibrar umbral capacitivo. Desactiva Touch2-9.
   - **Crítica**: Tacto duplicado inútil para Mackie. EMI = falsos positivos.

5. **I2C Estabilidad (8/9)**:
   - **Solución**: Pull-ups 4.7 kΩ a 3.3V, cables apantallados (<10 cm) en breadboard, trazas cortas en placa final. Reloj ≤100 kHz.
   - **Crítica**: SPI DMA (5-7) = EMI riesgo. Problemas pasados = mal cableado.

6. **Layout en Placa Personalizada**:
   - **Solución**: GND plane. Separar trazas ADC (10), I2C (8/9) de PWM (16/18) y SPI (5-7). Líneas cortas. Ferrita en 10V (motor) y 5V (NeoPixel/TFT).
   - **Crítica**: Breadboard = EMI caos. Placa final debe ser impecable.

7. **EMI Motor**:
   - **Solución**: Caps 0.1 µF+10 µF en DRV8833. Ferrita en 10V. Slew rate en PWM.
   - **Crítica**: EMI radiado persiste sin layout optimizado.

---

### 9. **Críticas Diabólicas**
- **I2C en 8/9**: Estable a 3.3V, pero SPI DMA (5-7) = EMI. Problemas pasados = cables malos. ¡Sin osciloscopio, estás ciego!
- **Potenciómetro en 10**: ADC seguro, pero ruido PWM 20 kHz. RC/caps obligatorios.
- **Touch en 1**: Redundante, sensible EMI. ¿Por qué duplicar para Mackie?
- **TFT DMA**: Rápido, pero EMI afecta I2C/ADC. DC en GPIO 2 arriesga arranque. Mostrar vúmetro y dB exige sincronización I2C precisa.
- **NeoPixel Feedback**: 4 LEDs eficiente, pero sincronización vía I2C depende del ESP32-S3. Fuente 5V debe soportar ~340 mA.
- **Alimentación**: 10V/5V/3.3V sólido, pero regulador 3.3V debe ser bajo ruido. Breadboard = conexiones sueltas.
- **Global**: Placa personalizada necesita GND plane, trazas cortas, separación PWM/ADC. ¿ADS1115 por I2C para fader?

---

### 10. **Recomendación Final**
**Pinout arriba** está optimizado para un track de **Mackie Control** en la placa personalizada: I2C en GPIO 8/9 (3.3V), TFT con DMA (LovyanGFX) mostrando nombre del track, vúmetro, panorama, posición del fader en dB, RSA0N11M9A0J (GPIO 10/1), encoder (panorama, GPIO 13/12/11), **4 LEDs NeoPixel** (Rec=rojo, Solo=amarillo, Mute=verde, Select=azul). **Alimentación**: 10V (motor, ~500 mA), 5V (TFT backlight, NeoPixel, ≥500 mA), 3.3V (regulador LM1117-3.3 o TPS7A4700 desde 5V, ~120 mA).

**Implementación**:
- **I2C**: GPIO 8/9, pull-ups 4.7 kΩ a 3.3V, trazas cortas, cables apantallados (<10 cm) en breadboard. Test con ESP32-S3 (`i2c_scanner`).
- **Potenciómetro**: GPIO 10, 11dB, RC (470 Ω, 0.1 µF), VCC=3.3V. Trazas cortas, lejos PWM/SPI.
- **Touch**: GPIO 1, calibrar umbral, desactiva Touch2-9.
- **TFT**: SCLK=5, MOSI=6, CS=7, DC=2, RST=33, <20 MHz, DMA. Lógica 3.3V, backlight 5V. Muestra nombre, vúmetro, panorama, fader en dB.
- **Encoder (Panorama)**: GPIO 13/12 (A/B), 11 (botón). Pull-ups 4.7 kΩ a 3.3V. Feedback en TFT.
- **NeoPixel**: GPIO 36, 5V, 4 LEDs (Rec=0, Solo=1, Mute=2, Select=3). Cap 1000 µF.
- **Motor**: Caps 0.1 µF+10 µF, ferrita, 10V.
- **Debug USB**: UART 21/15.
- **Placa Personalizada**: GND plane, separar ADC/I2C de PWM/SPI. Regulador 3.3V (TPS7A4700 recomendado). Fuente 5V ≥500 mA, 10V ≥1 A.
- **Test**: Serial Plotter (USB) para ADC. Osciloscopio para I2C/SPI.

**Siguientes Pasos**: Confirma número de tracks, dirección I2C del ESP32-S3, protocolo MIDI (HUI confirmado?), regulador 3.3V/5V (modelo?), esquema PCB (KiCad/Eagle?). ¡Dímelo para afinar!

**Crítica Diabólica Final**: Track perfecto para Mackie Control, pero I2C en 8/9 cerca de SPI DMA = EMI riesgo. Touch duplicado es inútil. TFT con vúmetro y dB es genial, pero exige I2C robusto. Placa personalizada necesita GND plane y layout impecable. Breadboard = caos EMI. ¿Sin osciloscopio? Adivinas ruido. ¡Detalles o caerás! 😈

**Fecha/Hora**: 23 de octubre de 2025, 23:46 CEST.
, `LGFX_LOLIN_S2_MINI.hpp

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
