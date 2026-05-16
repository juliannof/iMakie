# DISPLAY — Pantalla ST7789V3 y Sprites PSRAM (iMakie S2)

Documentación exhaustiva del subsistema de display. Incluye hardware ST7789V3, layout, sprites LovyanGFX, actualización, y troubleshooting.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (240×280 SPI3, sprites PSRAM)

---

## 1. HARDWARE DISPLAY

### 1.1 Panel ST7789V3

| Parámetro | Valor |
|-----------|-------|
| **Chip** | ST7789V3 |
| **Resolución** | 240×280 píxeles |
| **Interface** | SPI3_HOST |
| **Frecuencia escritura** | 10MHz (unidireccional, sin MISO) |
| **Frecuencia lectura** | 8MHz |
| **Modo color** | RGB565 (16-bit) |
| **Backlight** | PWM 500Hz, GPIO3 |
| **Alimentación** | Rail 5V (PCB V2) |

### 1.2 Configuración LovyanGFX

```cpp
// LovyanGFX_config.h
tft.setColorDepth(16);           // RGB565
tft.setMemoryHeightInBit(320);   // memory_height
tft.setOffsetYInBit(20);         // offset_y (ST7789 interno)
tft.setInvert(true);             // invertir colores
tft.setRGBOrder(false);          // GRB order (no RGB)
tft.setFrequency(10000000, 8000000);  // write=10MHz, read=8MHz
```

**Pulso RST obligatorio:**
```cpp
// ANTES de tft.init()
digitalWrite(GPIO33, LOW);
delay(100);
digitalWrite(GPIO33, HIGH);
tft.init();
```

### 1.3 Pinout Display

| Señal | GPIO | Función |
|-------|------|---------|
| SCLK (CLK) | 7 | Clock SPI3 |
| MOSI (DIN) | 4 | Data in (sin MISO) |
| DC | 6 | Data/Command select |
| CS | 5 | Chip Select |
| RST | 33 | Reset (manual pulso 100ms) |
| BL | 3 | Backlight PWM 500Hz |

---

## 2. LAYOUT DISPLAY

### 2.1 Estructura Visual

```
┌─────────────────────────────┐  Y=0
│     [Header 40px]           │  ← Track name + flags (REC/SOLO/MUTE/SELECT)
├─────────────────────────────┤  Y=40
│                             │
│      [Main Area]            │
│      (180×240)              │  ← Gráfico barras/fader + info
│                             │
│                             │
├─────────────────────────────┤  Y=280
│     [VU Meter 60px]         │  ← Pico + decay exponencial
├─────────────────────────────┤  Y=340
│     [VPot Ring 60px]        │  ← Anillo 15 posiciones (-7..+7) + encoder
└─────────────────────────────┘  Y=400

Total virtual: 240×400 (offset_y=20 en ST7789 físico)
```

**Componentes:**

| Sprite | Dimensiones | Localización | Contenido |
|--------|------------|--------------|----------|
| `header` | 240×40 | Y=0-39 | Track name + flags |
| `mainArea` | 180×240 | Y=40-279 | Fader gráfico, info |
| `vuSprite` | 60×240 | Y=40-279 (right) | VU meter vertical |
| `vPotSprite` | 240×60 | Y=340-399 | VPot ring + encoder |

### 2.2 Sprites PSRAM

**Obligatorio:** PSRAM habilitado y `setPsram(true)` antes de `createSprite()`

```cpp
// Display.cpp setup()
mainArea.setColorDepth(16);
mainArea.setPsram(true);
mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

header.setColorDepth(16);
header.setPsram(true);
header.createSprite(TFT_WIDTH, HEADER_HEIGHT);

vuSprite.setColorDepth(16);
vuSprite.setPsram(true);
vuSprite.createSprite(TFT_WIDTH - MAINAREA_WIDTH, MAINAREA_HEIGHT);

vPotSprite.setColorDepth(16);
vPotSprite.setPsram(true);
vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);
```

**Verificación dirección PSRAM:**
```cpp
if (esp_ptr_external_ram(mainArea.getBuffer())) {
    Serial.println("[PSRAM] ✓ mainArea en PSRAM (0x3f8xxxxx)");
}
```

---

## 3. ACTUALIZACIÓN DISPLAY

### 3.1 Ciclo Principal

```cpp
// main.cpp loop()
updateDisplay();

// Display.cpp
void updateDisplay() {
    if (needsTOTALRedraw) {
        redrawAll();           // Todas sprites
        needsTOTALRedraw = false;
    } else if (needsVPotRedraw) {
        redrawVPot();          // Solo VPot ring
        needsVPotRedraw = false;
    }
    // ... pushImage() a tft
}
```

### 3.2 Banderas de Redibujado

```cpp
// Declaradas en Display.cpp
extern bool needsTOTALRedraw;      // Redibuja todo
extern bool needsMainAreaRedraw;   // Main + VU
extern bool needsHeaderRedraw;     // Solo header
extern bool needsVUMetersRedraw;   // Solo VU
extern bool needsVPotRedraw;       // Solo VPot ring
```

**Cuándo actualizar:**
- `needsTOTALRedraw` — SAT cierra, boot, conexión RS485 restaurada
- `needsVPotRedraw` — Encoder movió (cada cambio)
- `needsMainAreaRedraw` — Fader cambió posición (>5 cuentas delta)
- `needsVUMetersRedraw` — VU level cambió (actualización continua)

### 3.3 Timing Actualización

```
Frame target: 30 FPS (33ms/frame)

Loop:
  0ms    updateDisplay()
    ├─ Redibuja sprites en RAM (PSRAM)
    ├─ pushImage() a tft (SPI3)
    └─ ~5-10ms total
  
  5ms    Otras tareas (motor, encoder, buttons)
  
  33ms   Next frame
```

**Latencia:** <50ms típicamente (imperceptible)

---

## 4. CONFIGURACIÓN LOVYANGFX

### 4.1 platformio.ini (S2)

```ini
[env:lolin_s2_mini]
platform = espressif32
board = lolin_s2_mini
framework = arduino
board_build.arduino.memory_type = qio_qspi
```

**Obligatorio:** `qio_qspi` para PSRAM (QSPI en paralelo con flash)

### 4.2 Bibliotecas

```
LovyanGFX 1.2.19 (sin NeoPixelBus — conflicto LEDC)
Adafruit_NeoPixel (no NeoPixelBus)
```

---

## 5. TROUBLESHOOTING DISPLAY

### 5.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Display negro | RST no ejecutado o SPI desconectado | Verificar pulso GPIO33 ANTES init() |
| Imagen invertida | Configuración invert/rgb_order mal | Revisar LovyanGFX_config.h |
| Parpadeos frecuentes | needsTOTALRedraw siempre true | Check SAT menu, conexión RS485 |
| Sprites no se crean | PSRAM no habilitado | Verificar `setPsram(true)` ANTES `createSprite()` |
| Memoria insuficiente | Sprites demasiado grandes o sin PSRAM | Usar `esp_ptr_external_ram()` para debug |
| SPI timeout | Frecuencia escriba demasiado alta | Reducir freq_write de 10MHz a 5MHz |
| Display lag | Redibujado bloqueante en loop principal | Usar banderas needsXXXRedraw (no redibuja siempre) |

### 5.2 Debugging Logs

**Display inicializado correctamente:**
```
[DISPLAY] Inicializando ST7789V3
[DISPLAY] LovyanGFX init OK
[DISPLAY] Sprites PSRAM:
  header: 19200 bytes (0x3f8xxxxx)
  mainArea: 86400 bytes (0x3f8xxxxx)
  vuSprite: 28800 bytes (0x3f8xxxxx)
  vPotSprite: 28800 bytes (0x3f8xxxxx)
[DISPLAY] Total: 163200 bytes
[DISPLAY] ✓ Ready
```

**Pulso RST debug:**
```cpp
Serial.println("[DISPLAY] Pulso RST en GPIO33...");
digitalWrite(33, LOW);
delay(100);
digitalWrite(33, HIGH);
Serial.println("[DISPLAY] RST listo");
```

---

## 6. SAT Y DISPLAY

### 6.1 SAT Menu Suspende Sprites

```cpp
// SatMenu.cpp - _satSuspendSprites()
void _satSuspendSprites() {
    header.deleteSprite();
    mainArea.deleteSprite();
    vuSprite.deleteSprite();
    vPotSprite.deleteSprite();
    log_i("Sprites suspendidos | PSRAM libre: %d", ESP.getFreePsram());
}
```

**Razón:** Libera ~163KB PSRAM para SAT menu (diagnósticos)

### 6.2 SAT Restaura Sprites

```cpp
// Cuando cierra SAT o usuario selecciona "Restore"
void _satRestoreSprites() {
    mainArea.setColorDepth(16);
    mainArea.setPsram(true);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);
    // ... resto de sprites
    needsTOTALRedraw = true;
}
```

---

## 7. COLORES Y PALETA

### 7.1 Macros Colores (16-bit RGB565)

```cpp
// config.h
#define COLOR_16_BITS(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

// VU Meter colores
#define VU_GREEN_OFF  COLOR_16_BITS(0, 20, 0)       // Verde oscuro
#define VU_GREEN_ON   TFT_GREEN                    // Verde brillante
#define VU_YELLOW_OFF COLOR_16_BITS(20, 20, 0)     // Amarillo oscuro
#define VU_YELLOW_ON  TFT_YELLOW                   // Amarillo brillante
#define VU_RED_OFF    COLOR_16_BITS(20, 0, 0)       // Rojo oscuro
#define VU_RED_ON     TFT_RED                      // Rojo brillante
#define VU_PEAK_COLOR COLOR_16_BITS(150, 150, 150) // Gris pico
```

---

## 8. HISTORIA CAMBIOS

### 8.1 2026-05-10: Calibración Brillo

**Problema:** Pantalla demasiado brillante (100%) o demasiado oscura

**Fix:** `setScreenBrightness(255)` en boot, slider SAT 0-255

**Status:** ✅ Resuelto

---

## 9. REFERENCIAS

- **MOTOR.md** — SAT suspende/restaura sprites
- **BUTTONS.md** — Actualización display en response a botones
- **CLAUDE.md** — Directivas obligatorias
- **S2/README.md** — Display pinout

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado DISPLAY.md como documento exhaustivo, trasladado contenido de CLAUDE.md
