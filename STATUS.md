# iMakie — Estado, Bugs y Pendientes

---

## S2 (ESP32-S2)

### Bugs S2
(ninguno)

### Pendientes S2
10. **Encoder — RESUELTO (2026-04-28)** — Problema de sequenciamiento: `Encoder::reset()` estaba en línea 214 (inmediatamente post-RS485), antes de procesar VPot. Esto causaba contador=0 al leer para VPot → VPot ring nunca cambiaba en Logic. SAT funcionaba porque procesaba sin ese reset intermedio. Fix: mover `reset()` a línea 242 (post-VPot, pre-updateDisplay) asegura que RS485 y Display usan el mismo delta.
11. **FaderTouch — EN DESARROLLO** — Detección por sostenimiento (tiempo). Perfecto sin plástico, necesita ajuste con plástico. Lógica actual: raw debe sostenerse > baseline×1.015 durante 6 frames (120ms) para detectar toque. Baseline actualizado siempre con IIR (alpha=1/16, no congelado). TEST_TOUCH en SAT testea correctamente. Próximos pasos: validar con plástico real, ajustar thresholds si es necesario.
12. revisar FaderADC tras reescritura — validar lecturas actuales con hardware real
13. ADS1015 pedido — cuando llegue, reemplazar lectura ADC nativa por I2C ADS1015 para resolver ruido en fader

#### S2 — NeoPixel
**NeoPixel secuencia de brillo (2026-04-28 16:15) — IMPLEMENTADO:**
- Azul tenue (NEOPIXEL_DIM_BRIGHTNESS=5) al inicio/reposo
- Colores muy tenues (NEOPIXEL_ULTRA_DIM=1) cuando Logic conecta primera vez
- O encendido (NEOPIXEL_DEFAULT_BRIGHTNESS=30) o tenue de morir (NEOPIXEL_ULTRA_DIM=1)
- Optimización monocore: detección de cambios interna sin flags innecesarios
- Comparación de estado (neoWaitingHandshake + 4 botones) en updateAllNeopixels()

**HW_STATUS display en boot screen (2026-04-28 16:15) — IMPLEMENTADO:**
- 10 componentes hardware con estado color-coded (Rojo=0, Naranja=1, Blanco=2)
- Fuente bitmap pequeña (setTextFont(1)) consistente con SAT
- Inyección automática vía pre_build.py desde config.h markers

---

## S3 (ESP32-S3 Extender)

### Bugs S3
- **Note Off en botones de transporte** — **RESUELTO** — `onButtonReleased` envía `0x80 + note + 0x00`.
- **Handshake MCU** — **RESUELTO** — protocolo completo implementado (ver sección handshake en CLAUDE.md).
- **Transport LEDs** — **RESUELTO** — notas 91–95 mapeadas a LEDs físicos en `setLedByNote()`.
- **RS485 intermitente** — **FUNCIONANDO CON TIMEOUTS** — Sistema comunica: LEDs actualizan, Display muestra datos. Timeouts ocasionales e impredecibles (~10-20 consecutivos, luego OK, repite). Comunicación física funciona a 500kbaud. NO se debe modificar arquitectura actual de lectura sin probar compilación first.

### Pendientes S3
1. **LED REC — RESUELTO**
2. **LED FF — RESUELTO**
3. **LED RW — RESUELTO**
4. **RS485 intermitente — DOCUMENTADO, BAJO CONTROL** — Sistema S3 funciona: comunica datos, LEDs y Display actualizan correctamente. Timeouts ocasionales no bloquean operación. Patrón: ~10-20 timeouts, luego respuesta OK, repite. Causa desconocida (posible: timing hardware, timeout 1500µs, ISR conflicts). No intentar buffer circular o cambios arquitectónicos sin compilar first. Problema arrastrado desde S3 original.
5. VPot ring LEDs (CC 16–23, 48–55), jog wheel (CC 60), rude solo (nota 115)

---

## P4 (ESP32-P4 Master)

### Bugs P4
1. **`uiOfflineCreate()` doble llamada en `setup()`** — ~~primera antes de `prefs.begin()`, segunda después. Leak de LVGL garantizado.~~ **RESUELTO** — solo existe una llamada, después de `prefs.begin()`.
2. **Pantalla negra en boot** — Dos causas independientes: (a) ~~backlight off si `lastPage` era 1 o 2 (backlight solo se encendía vía `uiMenuInit()` → `uiPage3Create()`). Fix: `displaySetBrightness()` en `setup()` después de `initDisplay()`.~~ **RESUELTO**. (b) ~~`UIOffline` empieza con `s_blink_label` en HIDDEN y solo lo muestra cuando el logo termina de revelarse (~5-6 s); sin logo, Display negra permanente.~~ **RESUELTO** — label visible desde el inicio, parpadeo activo desde el primer tick independientemente del estado del logo.
3. **Handshake MCU incorrecto** — ~~código antiguo implementaba un challenge/response propio; P4 actuaba como HOST generando challenges aleatorios.~~ **RESUELTO** — ver sección "Mackie MCU — handshake" en CLAUDE.md.

### Pendientes P4
6. mutex o double-buffer en `vuLevels[]`
7. respuesta táctil lenta en vista de faders — investigar qué bloquea el hilo de touch, especialmente en UIPage faders
8. no muestra datos en Display tras conectarse — posible regresión en la transición a CONNECTED tras cambios en handshake
9. NeoTrellis sin implementar — atención a pines I2C nuevos (SDA=GPIO33, SCL=GPIO31)

---

## Cross-system

### Pendientes Cross-system
14. RS485: verificar pines en S3 (legacy) y P4 (TX=50, RX=51, EN=52) — confirmar que ambos compilan y funcionan correctamente
15. LEDs NeoPixel master: implementar control de brillo centralizado — estudiar si viable vía MIDI (CC o SysEx dedicado) para controlar brillo de todos los slaves desde Logic/P4

