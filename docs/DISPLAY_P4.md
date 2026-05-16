# DISPLAY P4 — ST7701S MIPI-DSI (ESP32-P4)

Documentación del subsistema de display P4. Display grande 480×800 con LVGL v9 para interfaz master MCU.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (ST7701S MIPI-DSI, LVGL v9)

---

## 1. HARDWARE DISPLAY P4

### 1.1 Especificación

| Parámetro | Valor |
|-----------|-------|
| **Chip** | ST7701S |
| **Interface** | MIPI-DSI 2-lane |
| **Resolución** | 480×800 píxeles |
| **Modo color** | RGB888 (24-bit) |
| **Orientación** | Portrait (480×800) |
| **Alimentación** | Integrado en placa GUITION |

### 1.2 Pinout (Integrado en placa)

- **MIPI-DSI:** Integrado en placa GUITION JC4880P433C
- **Power:** 5V rail desde placa
- **No requiere GPIO adicionales** — comunicación por MIPI DSI

---

## 2. LVGL v9

### 2.1 Inicialización

```cpp
// P4 setup()
lv_init();
// LovyanGFX init (crea display buffer)
tft.init();
// LVGL registra display
lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
```

### 2.2 Modo Portrait vs Landscape

**Portrait (actual):**
```
480 px ancho
 │
 ├─ Menú izquierda (faders)
 ├─ Faders verticales
 ├─ VU meter
 └─ Transport bottom
 
800 px alto
```

**Landscape (por rotación software):**
```
800 px ancho
 │
 ├─ Faders horizontales
 ├─ Transport sidebar
 └─ VU meter

480 px alto
```

**Implementación:** LVGL permite rotación de widgets via `lv_obj_set_style_transform_rotation(obj, 900, 0)` (900 = 90° en decisegundos)

### 2.3 Rotación de Widgets

```cpp
// Rotar widget 90° clockwise
lv_obj_set_style_transform_rotation(my_widget, 900, 0);

// Ajustar coordenadas mentalmente
// X/Y se intercambian: (x, y) → (y, 480-x)
```

---

## 3. CONFIGURACIÓN LVGL

### 3.1 platformio.ini

```ini
[env:esp32-p4]
platform = espressif32
board = esp32-p4-function-ev-board
framework = arduino
lib_deps =
    lvgl/lvgl@^9.0.0
    LovyanGFX
```

### 3.2 lv_conf.h (configuración LVGL)

```cpp
#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 800
#define LV_COLOR_DEPTH 24    // RGB888
#define LV_MEM_SIZE (256*1024)
#define LV_TICK_CUSTOM 1     // Custom tick (millis())
```

---

## 4. TROUBLESHOOTING

### 4.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Display negro | MIPI-DSI no inicializado | Verificar lv_init(), tft.init() en orden |
| Colores invertidos | RGB order incorrecto | Check lv_disp_drv.color_chroma_key |
| Lag de widgets | LVGL redibuja bloqueante | Usar tasks/timers para no bloquear |
| Rotación no funciona | Widget transform_rotation no aplicado | Verificar `lv_obj_set_style_transform_rotation()` |
| Fuente ilegible | Fuente no cargada | Verificar lv_font_load() |

### 4.2 Debugging

**LVGL debug logs:**
```cpp
lv_log_register_print_cb([](lv_log_level_t level, const char* msg) {
    Serial.printf("[LVGL] %s\n", msg);
});
```

---

## 5. REFERENCIAS

- **DISPLAY.md** — Display S2 (ST7789V3, LovyanGFX sprites), diferencias con P4
- **LVGL v9 docs:** https://docs.lvgl.io/
- **P4 README:** [MASTER_S3-P4/P4/README.md](../MASTER_S3-P4/P4/README.md)

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado DISPLAY_P4.md como documento P4-específico, extraído de CLAUDE.md
