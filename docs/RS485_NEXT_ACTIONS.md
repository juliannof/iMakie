# Plan de Acción RS485 — Próximas Sesiones

**Auditoría completada:** 2026-04-27  
**Estado actual:** RS485Handler.cpp está correcto (post-optimización 2026-04-27)  
**Prioridad:** Validación + comentarios + monitoreo

---

## Tarea 1: Instrumentación de timing (30 min)

**Objetivo:** Capturar métricas REALES vs. teóricas  
**Dónde:** main.cpp loop()

### Cambios propuestos:

```c
// Antes de RS485:
uint32_t t_rs485_start = micros();

if (rs485.hasNewData()) {
    uint32_t t_on_master_start = micros();
    RS485Handler::onMasterData(rs485.getData());
    uint32_t t_on_master_time = micros() - t_on_master_start;

    uint32_t t_build_start = micros();
    SlavePacket resp = RS485Handler::buildResponse(faderADC, *satMenu);
    uint32_t t_build_time = micros() - t_build_start;

    uint32_t t_send_start = micros();
    rs485.sendResponse(resp);
    uint32_t t_send_time = micros() - t_send_start;

    // Log cada 100 ciclos
    static int count = 0;
    if (++count >= 100) {
        Serial.printf("[PERF] onMasterData=%lu µs, buildResponse=%lu µs, sendResponse=%lu µs\n",
                      t_on_master_time, t_build_time, t_send_time);
        count = 0;
    }
}

uint32_t t_display_start = micros();
updateDisplay();
uint32_t t_display_time = micros() - t_display_start;

uint32_t t_neopixel_start = micros();
updateAllNeopixels();
uint32_t t_neopixel_time = micros() - t_neopixel_start;

static int display_count = 0;
if (++display_count >= 50) {
    Serial.printf("[PERF] display=%lu µs, neopixel=%lu µs\n",
                  t_display_time, t_neopixel_time);
    display_count = 0;
}
```

**Expected output:**
```
[PERF] onMasterData=245 µs, buildResponse=312 µs, sendResponse=342 µs
[PERF] onMasterData=251 µs, buildResponse=318 µs, sendResponse=345 µs
...
[PERF] display=45 µs, neopixel=22 µs
[PERF] display=52 µs, neopixel=18 µs
[PERF] display=8543 µs, neopixel=1200 µs  ← Total redraw
```

---

## Tarea 2: Comentar código RS485 (45 min)

**Archivo:** `track S2/.../src/RS485/RS485Handler.cpp`  
**Objetivo:** Aplicar guía de comentarios RS485_COMMENTING_GUIDE.md

### Secciones a comentar:

1. **onMasterData() header** (línea 28)
   - Duración máxima ~400µs
   - Restricciones (NO neopixels, NO display)

2. **onMasterData() CONNECTED branch** (línea 37-40)
   - Explicar por qué NO updateAllNeopixels()
   - Duración evitada: ~15-30ms

3. **onMasterData() DISCONNECTED branch** (línea 41-51)
   - Misma explicación
   - Flag neoWaitingHandshake

4. **checkTimeout()** (línea 140-159)
   - NO updateAllNeopixels() aquí tampoco
   - Timeout delay = 500ms (línea 141)

**Archivo:** `track S2/.../src/RS485/RS485.cpp`  
**Objetivo:** Timing comments

1. **sendResponse() header** (línea 31)
   - Duración total: ~360µs

2. **Transceiver timing block** (línea 37-51)
   - Ya tiene buenos comentarios
   - Validar que especificación es correcta

---

## Tarea 3: Validar timing con hardware (1 hora)

**Herramientas necesarias:**
- Analizador lógico (esp32 tiene GPIO de debug)
- Monitor serial a 115200 baud
- Osciloscopio (opcional, para RS485 bus)

**Mediciones a capturar:**

1. **GPIO36 (NeoPixel) durante updateAllNeopixels()**
   - Duración real de neopixels.show()
   - Varianza (min/max)
   - Correlación con número de LEDs

2. **GPIO35 (RS485_EN) durante sendResponse()**
   - Setup time real (debería ser ~50µs)
   - Hold time real
   - Si hay jitter

3. **Loop() duración total**
   - Con RS485 data
   - Sin RS485 data
   - Con updateDisplay() TOTAL redraw

**Script sugerido:**
```bash
# Conectar S2 via USB-C
cd "track S2/iMakie - Track ESP32S2 V1"
platformio device monitor -e lolin_s2_mini -b 115200 | grep PERF
```

Esperar 10 segundos y capturar estadísticas.

---

## Tarea 4: Identificar varianza (30 min)

**Análisis:**
- Buscar outliers en duración (picos > 2× promedio)
- Si hay picos, investigar causa (ISR? garbage collection? logging?)

**Indicadores problema:**
```
[PERF] onMasterData=245 µs  ← Normal
[PERF] onMasterData=1500 µs ← ¡¡PROBLEMA!! 6× más lento
```

**Investigación:**
1. Activar logging en RS485Handler.cpp temporalmente
2. Capturar qué path toma onMasterData cuando lento
3. Posibles causas:
   - `needsTOTALRedraw` set = fuerza redraw
   - Motor::startCalib() activado
   - Encoder acumuló muchos deltas
   - Serial.printf sin filtro
   - ISR de botones durante call

---

## Tarea 5: Propuesta de mejora (1 hora)

**Si métricas muestran problemas:**

### Opción A: FreeRTOS task para NeoPixels (RECOMENDADO)

Mover `updateAllNeopixels()` a tarea separada:
```c
// Core 0 (idle), baja prioridad
TaskHandle_t neopixelTask = nullptr;

void neopixelUpdateTask(void *param) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));  // Wait for notification or 50ms timeout
        updateAllNeopixels();  // Ya sin restrict timing
    }
}

// En setup():
xTaskCreate(neopixelUpdateTask, "NeoPixel", 2048, nullptr, 1, &neopixelTask);

// En main.cpp (línea 242):
// xTaskNotifyGive(neopixelTask);  // Wake up task asynchronously
// Luego updateAllNeopixels() nunca bloquea loop
```

**Ventajas:**
- ✓ No bloquea main loop
- ✓ No interfiere RS485
- ✓ NeoPixels actualizan cada 50ms (visible)

**Desventajas:**
- Más complejo (FreeRTOS)
- Latencia visual de 50ms

### Opción B: Polling no-bloqueante (ALTERNATIVA LIGERA)

Refactorizar NeoPixel para usar estado máquina:
```c
enum NeoPixelUpdateState { IDLE, SET_PIXELS, SHOW_PIXELS };

void updateAllNeopixelsNonBlocking() {
    static NeoPixelUpdateState state = IDLE;
    static uint32_t lastUpdate = 0;

    if (millis() - lastUpdate < 50) return;  // Rate limit

    switch (state) {
        case IDLE:
            if (neoWaitingHandshake) return;
            if (estado_cambió()) {
                state = SET_PIXELS;
            }
            break;
        case SET_PIXELS:
            handleButtonLedState(...);  // 50µs
            state = SHOW_PIXELS;
            break;
        case SHOW_PIXELS:
            // SPLIT neopixels.show() en chunks?
            // (Adafruit no soporta, pero posible custom impl)
            neopixels.show();  // ← Aún bloqueante, pero scheduled
            state = IDLE;
            lastUpdate = millis();
            break;
    }
}
```

**Ventajas:**
- ✓ Sigue siendo simple
- ✓ No requiere FreeRTOS

**Desventajas:**
- ✗ Aún bloquea (solo que menos frecuente)
- ✗ Más lógica

---

## Tarea 6: Validación post-cambios (30 min)

**Si implementas mejora:**

1. Compilar: `platformio run -e lolin_s2_mini`
2. Cargar: `platformio run -e lolin_s2_mini -t upload`
3. Monitorear 30 segundos
4. Capturar salida `[PERF]`
5. Comparar: antes vs. después

**Métricas target:**
- onMasterData: 200-400µs (sin cambio)
- buildResponse: 300-500µs (sin cambio)
- sendResponse: 300-400µs (sin cambio)
- neopixel: < 50µs si polling, o async si task
- display: 50-100ms (sin cambio)

---

## Checklist de implementación

### Hoy (auditoría):
- [x] Leer RS485Handler.cpp
- [x] Leer RS485.cpp
- [x] Leer main.cpp
- [x] Leer Display.cpp
- [x] Leer Neopixel.cpp
- [x] Crear RS485_TIMING_AUDIT.md
- [x] Crear RS485_COMMENTING_GUIDE.md

### Próxima sesión (implementación):
- [ ] Tarea 1: Instrumentación de timing (30 min)
- [ ] Tarea 2: Comentar código (45 min)
- [ ] Tarea 3: Validar con hardware (60 min)
- [ ] Tarea 4: Identificar varianza (30 min)
- [ ] Tarea 5: Proponer mejora (60 min) — SOLO SI MÉTRICAS MUESTRAN PROBLEMA
- [ ] Tarea 6: Validación post-cambios (30 min)

**Total estimado:** 3-4 horas (sin Tarea 5 = 2 horas)

---

## Referencias

- `docs/RS485_PROTOCOL.md` — Especificación completa
- `docs/RS485_TIMING_AUDIT.md` — Análisis línea-por-línea (este documento)
- `docs/RS485_COMMENTING_GUIDE.md` — Patrones de comentarios
- `CLAUDE.md` — Contexto general del proyecto

---

## Notas finales

**Situación actual (2026-04-27):**
- ✅ RS485Handler.cpp optimizado correctamente
- ✅ Bottleneck principal (Neopixel en onMasterData) evitado
- ⚠️ Varianza de timing desconocida (necesita medición)
- ⚠️ Potencial problema si updateAllNeopixels() duración > 50ms

**No hay urgencia.** Sistema funciona, pero métricas reales confirmarán si optimizaciones futuras son necesarias.

**Siguiente paso:** Instrumentación de timing en próxima sesión.
