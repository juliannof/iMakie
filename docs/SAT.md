# SAT — Sistema de Auto-Test (iMakie S2)

Documentación exhaustiva del menú de diagnóstico SAT. Incluye navegación, opciones, integración con subsistemas, y troubleshooting.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (acceso via encoder push >3s)

---

## 1. ACCESO Y NAVEGACIÓN

### 1.1 Activación SAT

```
Display normal
         ↓
Usuario presiona encoder >3 segundos
         ↓
SAT menu abierto
         ↓
Display cambia a interfaz SAT
```

**Código:**
```cpp
// SatMenu.cpp
if (Encoder::isPushed() && millis() - pushTime > 3000) {
    openMenu();  // Abre SAT menu
}
```

### 1.2 Navegación Encoder

```
Usuario gira encoder
         ↓
SAT incrementa/decrementa índice menú
         ↓
Display redibuja opción seleccionada
         ↓
Usuario presiona encoder nuevamente
         ↓
SAT ejecuta callback de opción
         ↓
Regresa a menú principal
```

**Detalle:** `SatMenu::handleEncoder()` consume contador del encoder (no afecta VPot en display normal)

---

## 2. OPCIONES SAT

### 2.1 Menú Raíz

| Opción | GPIO Asociado | Acción | Módulo |
|--------|--|---------|--------|
| **Motor** | GPIO14 (EN) | Submenu: Off/On/Calib/Drive | MOTOR.md |
| **Brightness** | GPIO3 (BL PWM) | Slider 0-255 backlight | DISPLAY.md |
| **RS485** | GPIO9 (RX) | Toggle conexión (debug) | RS485.md |
| **LEDs Test** | GPIO36 (DIN) | Secuencia RGB índice 0-11 | LEDS.md |
| **WiFi OTA** | — | Boot OTA-only mode | WIFI.md |
| **Reboot** | — | Reinicia S2 | — |

### 2.2 Motor Submenu

```
Motor
  ├─ Off          ← Desactiva motor completamente
  ├─ On           ← Activa motor
  ├─ Calibrar     ← Inicia calibración automática
  ├─ Drive        ← Test PWM manual (slider 0-255)
  └─ Volver       ← Regresa a menú principal
```

---

## 3. SAT Y MOTOR CALIBRACIÓN

### 3.1 Flujo Calibración en SAT

```
Usuario selecciona "Motor > Calibrar"
         ↓
SatMenu::openCalibrationScreen()
         ↓
main.cpp loop() ejecuta Motor::update() CADA FRAME
         ↓
Motor máquina estados: KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN → DONE
         ↓
SAT pantalla dibuja estado actual (Motor::getCalibState())
         ↓
Si falla:
  ├─ Usuario presiona REC (botón físico)
  ├─ ButtonManager::update() detecta presión
  ├─ SatMenu::update() reconoce en pantalla CALIB
  ├─ Motor::startCalib() reinicia
  └─ Vuelve a paso 3
         ↓
Si éxito:
  ├─ Motor::getCalibState() = DONE
  ├─ SAT dibuja "✓ Calibración completa"
  ├─ min/max guardados en config
  └─ Usuario presiona encoder para volver
```

### 3.2 Arquitectura Crítica

```cpp
// main.cpp loop() — SIEMPRE ejecuta (incluso con SAT abierto)
Motor::update();

// SatMenu — NUNCA ejecuta Motor::update()
// Solo dibuja estado: Motor::getCalibState()
if (satMenu->isOpen() && screenType == CALIB) {
    uint8_t calibState = Motor::getCalibState();
    drawCalibrationScreen(calibState);
}
```

**¿Por qué?** Evitar race conditions entre SAT y main.cpp. Garantiza coherencia: calibración en SAT = calibración en loop.

### 3.3 Reinicio Calibración (REC button)

```cpp
// SatMenu.cpp - detecta REC presionado durante CALIB
if (satMenu->screenType == Scr::MOTOR_CALIB) {
    if (ButtonManager::getButtonState(BUTTON_REC)) {
        Motor::startCalib();  // Reinicia
        log_i("[SAT] Motor calibración reiniciada por REC");
    }
}
```

---

## 4. SAT Y DISPLAY

### 4.1 Suspensión de Sprites

```cpp
// SatMenu.cpp - al abrir SAT
void _satSuspendSprites() {
    header.deleteSprite();
    mainArea.deleteSprite();
    vuSprite.deleteSprite();
    vPotSprite.deleteSprite();
    log_i("[SAT] Sprites suspendidos | PSRAM libre: %d", ESP.getFreePsram());
}
```

**Razón:** SAT menu requiere ~50KB PSRAM. Libera ~163KB al suspender sprites.

### 4.2 Restauración de Sprites

```cpp
// Cuando cierra SAT o usuario presiona "Volver"
void _satRestoreSprites() {
    mainArea.setColorDepth(16);
    mainArea.setPsram(true);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);
    // ... resto de sprites
    needsTOTALRedraw = true;
}
```

### 4.3 Pantalla SAT (LovyanGFX directo)

```
SAT menu usa tft (ST7789V3) directamente — no sprites
┌─────────────────────────────┐
│ SAT Menu                    │
│                             │
│ ◉ Motor                     │
│ ○ Brightness               │
│ ○ RS485                     │
│ ○ LEDs Test                 │
│ ○ WiFi OTA                  │
│ ○ Reboot                    │
│                             │
└─────────────────────────────┘
```

---

## 5. SAT Y ENCODER

### 5.1 Consumo de Encoder

```cpp
// main.cpp loop()
if (!satMenu->isEncoderConsumed()) {
    // Procesar VPot
    Encoder::update();
    // ...
} else {
    // SAT consume encoder para navegación
    satMenu->handleEncoder();
}
```

**Resultado:** Cuando SAT abierto, encoder navega menú (no afecta fader Logic)

### 5.2 Reset Encoder en SAT

```cpp
// SatMenu.cpp - al cerrar
void SatMenu::close() {
    // ...
    Encoder::reset();  // Limpia contador
    needsTOTALRedraw = true;
}
```

---

## 6. SAT Y LEDS

### 6.1 LEDs Test — Secuencia Manual

```
SAT menu → LEDs Test
         ↓
SatMenu::_satLedsTest()
         ↓
Slider 0-255 para intensidad RGB
         ↓
Encoder: selecciona LED índice (0-11)
         ↓
Botones: REC=rojo, SOLO=verde, MUTE=azul, SELECT=blanco
         ↓
setNeopixelState(idx, r, g, b)
showNeopixels()
```

**Código callback:**
```cpp
static void _satLedsTest(int idx, uint8_t r, uint8_t g, uint8_t b) {
    log_i("[SAT-LED] idx=%d rgb=(%d,%d,%d)", idx, r, g, b);
    setNeopixelState(idx, r, g, b);
    showNeopixels();
}
```

---

## 7. SAT Y WIFI/OTA

### 7.1 Activación OTA-Only Mode

```
SAT menu → WiFi OTA
         ↓
SatMenu::_satWiFiOta()
         ↓
// 1. Guardar flag
Preferences prefs;
prefs.begin("ptxx", false);
prefs.putBool("otaMode", true);
prefs.end();

// 2. Apagar pantalla (libera PSRAM)
setScreenBrightness(0);

// 3. Cerrar SAT y reiniciar
ESP.restart();
```

### 7.2 Boot OTA-Only (2026-05-16)

```cpp
// main.cpp setup()
bool otaMode = prefs.getBool("otaMode", false);

if (otaMode) {
    // Limpiar flag INMEDIATAMENTE (una sola ejecución)
    prefs.remove("otaMode");
    
    // Iniciar display mínimo (sin sprites)
    initDisplay(true);  // true = otaOnlyMode
    
    // Iniciar WiFi + OTA
    otaManager.begin();
    otaManager.enableForUpload(true);
    
    return;  // Esperar upload
}
```

---

## 8. SAT Y RS485

### 8.1 RS485 Toggle (Debug)

```
SAT menu → RS485 On/Off
         ↓
SatMenu::_satRS485Toggle()
         ↓
Si RS485 deshabilitado:
  ├─ LED11 = rojo (status error)
  ├─ Motor se detiene
  └─ No hay comunicación master
         ↓
Si RS485 habilitado:
  ├─ LED11 = verde (si conexión OK)
  ├─ Motor acepta comandos master
  └─ Vuelve a funcionamiento normal
```

---

## 9. SAT Y BOTONES

### 9.1 REC Button Special

```
Contexto: SAT > Motor > Calibración
         ↓
Usuario presiona REC
         ↓
ButtonManager::update() detecta presión
         ↓
SatMenu::update() reconoce screenType == CALIB
         ↓
Motor::startCalib() → reinicia calibración
```

**Sin REC presionado en otros menús:** Comportamiento normal (consume flag)

---

## 10. SAT STATE MACHINE

```
SAT Closed
    ↓
[Encoder push >3s]
    ↓
SAT Opened (suspende sprites, inicia loop SAT)
    ├─ Encoder ↑↓ navega
    ├─ Encoder press ejecuta
    ├─ REC reinicia operación actual
    ├─ main.cpp sigue ejecutando Motor::update()
    └─ Display dibuja menú SAT
    ↓
[Encoder press → "Volver" / "Back"]
    ↓
SAT Closed (restaura sprites, needsTOTALRedraw=true)
    ↓
Display normal
```

---

## 11. TROUBLESHOOTING

### 11.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| SAT no abre | Encoder push >3s no detecta | Test GPIO11 con Serial.println() |
| Menú lag | Redibujado bloqueante | Check SatMenu::update() frecuencia |
| Calibración infinita | Motor::startCalib() no reinicia | Verificar guard cooldown 2000ms |
| LEDs Test no responde | GPIO36 no funciona o NeoPixel error | Test con Serial, verificar Adafruit |
| WiFi OTA boot loop | Flag otaMode no se limpia | Limpiar NVS manualmente |
| Encoder congelado en SAT | isEncoderConsumed() siempre true | Check SatMenu::close() resetea |
| Pantalla negra en SAT | Sprites no restaurados | Verificar _satRestoreSprites() |

### 11.2 Debugging Logs

**SAT abierto correctamente:**
```
[SAT] Menu opened
[SAT] Sprites suspendidos | PSRAM libre: 122000
[SAT] Screen: MAIN_MENU
[SAT] Option: 0 (Motor)
```

**Calibración en SAT:**
```
[SAT] Motor Calibración seleccionada
[CALIB] Iniciando KICK_UP
[CALIB] KICK_UP → GOING_UP (ADC=26200)
[CALIB] ✓ DONE: min=200 max=26300
[SAT] Sprites restaurados | PSRAM libre: 163000
```

---

## 12. REFERENCIAS

- **MOTOR.md** — Control motor, máquina calibración, APIs
- **DISPLAY.md** — ST7789V3, sprites PSRAM, layout
- **ENCODER.md** — ISR Gray code, sequenciamiento, consumo
- **LEDS.md** — WS2812B, asignación, control brillo
- **WIFI.md** — OTA-only mode, provisioning
- **RS485.md** — Protocolo, timing, suspensión

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado SAT.md como documento exhaustivo, consolidado de CLAUDE.md y MOTOR.md
