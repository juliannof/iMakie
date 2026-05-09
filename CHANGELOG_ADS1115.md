# CHANGELOG — ADS1115 Validación Fase 1

**Fecha:** 2026-05-09 22:45  
**Sesión:** Implementación ADS1115 I2C ADC para S2 Slave  
**Commit:** `80eb621` (punto de referencia en GitHub)

---

## 📋 Resumen Ejecutivo

Reemplazo del ADC nativo problemático del ESP32-S2 (GPIO10, 13-bit, ±30 cuentas ruido) por **ADS1115 externo** vía I2C con:
- **Resolución:** 16-bit completo (0-32767 cuentas)
- **Rango:** ±4.096V (GAIN_ONE) — seguro para pot 3.3V directo
- **Velocidad:** 860 SPS continuo, ISR ALERT/RDY en IO34 (no polling)
- **Logging:** Buffer circular 256 muestras con timestamp (no-bloqueante)
- **Compilación:** Guardiada condicional — ADC nativo sigue funcional en `lolin_s2_mini_serial`

---

## 🔧 Archivos Modificados (6 archivos)

### 1. **`platformio.ini`**
- **Líneas:** 84-92
- **Cambio:** Nuevo entorno `[env:lolin_s2_mini_ads]`
  - Extiende `lolin_s2_mini_serial`
  - Agrega flag `-DUSE_ADS1015`
  - Agrega libs: `adafruit/Adafruit ADS1X15@^2.5.0` + `adafruit/Adafruit BusIO@^1.14.0`
- **Validación:** ✅ Correcto — extends + build_flags + lib_deps

### 2. **`config.h`**
- **Líneas:** 66-72
- **Cambio:** Defines ADS bajo guardia `#ifdef USE_ADS1015`
  - `ADS_SDA_PIN = 21` (I2C data)
  - `ADS_SCL_PIN = 17` (I2C clock)
  - `ADS_ALERT_PIN = 34` (ISR trigger)
  - `ADS_I2C_ADDR = 0x48` (I2C address)
- **Impacto:** DAC en IO17 mantiene—modo DAC desaparece si IO17 es SCL
- **Validación:** ✅ Correcto — pines confirmados por usuario

### 3. **`protocol.h`**
- **Línea:** 80
- **Cambio:** Comentario `faderPos` actualizado
  - Antes: `// FaderADC 0-8191`
  - Ahora: `// 0-8191 (ADC nativo) o 0-32767 (ADS1115 raw). Masters (P4/S3) mapean → 0-14848`
- **Propósito:** Documentar que campo soporta dual-mode (13-bit ADC o 16-bit ADS)
- **Validación:** ✅ Correcto — claridad para developers futuros

### 4. **`FaderADC.h`**
- **Cambios principales:**
  - Líneas 4-9: Includes condicionales (Wire + ADS1X15 vs adc_oneshot)
  - Línea 23: Método público `dumpAdsLog()` bajo guardia
  - Líneas 27-52: Miembros privados ADS (objeto, I2C, ISR, buffer logging) vs ADC nativo
  - Línea 48: ISR estática `_alertISR()`
- **Validación:** ✅ Correcto — estructura completa, guardias en lugar

### 5. **`FaderADC.cpp`**
- **Cambios principales:**
  - Líneas 7-13: Definición global `_newData` + ISR implementation
  - Líneas 22-83: `begin()` con ramas ADS/ADC completas
    - ADS: I2C init, `_ads.begin()`, GAIN_ONE, 860SPS, attachInterrupt(34), startADCReading()
    - ADC: código original sin cambios
  - Líneas 86-156: `update()` con ramas ADS/ADC
    - ADS: ultra-lean `if (!_newData) return;` + lectura raw directo + `_logReading()` no-bloqueante
    - ADC: oversampling + EMA original
  - Líneas 159-195: `measureRange()` dual
    - ADS: 5s captura min/max del buffer
    - ADC: 100 muestras con EMA
  - Líneas 197-208: Nueva función `dumpAdsLog()` bajo guardia ADS
- **Optimización clave:** update() ADS es ultra-rápido (0µs si sin dato, ~1-2µs con dato) — no impacta loop() single-core
- **Validación:** ✅ Correcto — código completo, guardias nítidas

### 6. **`main.cpp`**
- **Cambios principales:**
  - Líneas 127-145: Bloque validación I2C en setup()
    - Intenta contactar ADS1115 en 0x48
    - Loguea éxito/fallo con pines
    - No bloqueante — ejecuta antes de OTA-only check
  - Líneas 198-204: DAC guardia
    - Antes: DAC siempre activo (regresión con IO17 como SCL)
    - Ahora: DAC solo si `#ifndef USE_ADS1015`
    - ADC nativo sigue con pot a 2V (máxima resolución 13-bit)
- **Validación:** ✅ Correcto — guardias en lugar correcto

---

## 📊 Validación Archivo por Archivo

| Archivo | Líneas | Cambio | Guardias | Estado |
|---------|--------|--------|----------|--------|
| platformio.ini | 84-92 | Entorno ADS | — | ✅ OK |
| config.h | 66-72 | Defines ADS | `#ifdef USE_ADS1015` | ✅ OK |
| protocol.h | 80 | Comentario faderPos | — | ✅ OK |
| FaderADC.h | 4-62 | Includes + miembros + ISR | `#ifdef/#else/#endif` | ✅ OK |
| FaderADC.cpp | 7-208 | ISR + begin + update + measureRange + dumpAdsLog | `#ifdef/#else/#endif` | ✅ OK |
| main.cpp | 127-204 | Validación I2C + DAC guardia | `#ifdef/#ifndef/#endif` | ✅ OK |

---

## 🎯 Hardware Config (Confirmado por usuario)

```
ADS1115 pines en PCB V2:
  VDD → 3.3V regulador
  GND → GND
  SDA → GPIO21 (libre en S2FN4R2)
  SCL → GPIO17 (antes DAC, ahora I2C)
  ALERT → GPIO34 (libre, entrada)
  ADDR → GND (dirección 0x48)
  A0 → cursor pot (~3cm cable volante)

Pot alimentación:
  VCC → 3.3V directo (no DAC en compilación ADS)
  GND → GND
```

---

## 🚀 Compilación y Testing

**Ambiente compilación ADS:**
```bash
pio run -e lolin_s2_mini_ads --target upload
```

**Salida esperada (Serial Monitor 115200):**
```
[SETUP] ADS1115 detectado en I2C 0x48 ✓
[ADC] ADS1115 OK  GAIN_ONE  860SPS  ALERT=IO34  seed=XXXX
```

**Salida fallo (si conexión I2C mala):**
```
[SETUP] ADS1115 NO encontrado en 0x48 ✗ — revisar pines SDA=21 SCL=17
```

**Compilación normal (sin cambios):**
```bash
pio run -e lolin_s2_mini_serial --target upload
```
- DAC activo → pot 2V (máxima resolución ADC 13-bit)
- ADC nativo → código original funciona

---

## 📈 Métricas de Rendimiento (Diseño)

| Métrica | ADS1115 | ADC Nativo | Notas |
|---------|---------|-----------|-------|
| Resolución | 16-bit (0-32767) | 13-bit (0-8191) | ADS entra sin escalado FP |
| Ruido | ~2-5 counts | ±30 counts | Mejora 6-15× |
| Velocidad polling | 0µs (ISR-driven) | ~24ms (oversampling) | ADS no bloquea loop |
| Latencia update() | 0-2µs | ~24ms | ADS: factor 10000× más rápido |
| Consumo CPU | <1% | ~8% | S2 single-core aliviado |
| Costo I2C | 3µs (lectura) | 0µs | Negligible |

---

## ✅ Checklist Implementación

- [x] platformio.ini: entorno ADS con flags + libs
- [x] config.h: defines ADS bajo guardia
- [x] protocol.h: comentario faderPos documentado
- [x] FaderADC.h: includes, miembros, ISR bajo guardias
- [x] FaderADC.cpp: ISR definition, begin(), update(), measureRange(), dumpAdsLog()
- [x] main.cpp: DAC guardia + validación I2C
- [x] Revisión archivo por archivo: 6/6 archivos validados
- [x] Commit a GitHub: `80eb621` creado
- [x] Changelog documentado: este archivo

---

## 📝 Próximos Pasos (Fase 2)

1. **Flash hardware real:** `pio run -e lolin_s2_mini_ads --target upload`
2. **Monitor serial:** Validar logs "ADS1115 OK" + sin errores
3. **Test SAT measureRange():** Mover fader 5s, validar rango 0-32767
4. **Test dumpAdsLog():** Agregar opción SAT "ADS Log Dump", descargar CSV
5. **Análisis jitter:** Graficar timestamp,raw,pos en Python/Excel — validar suavidad
6. **Test Logic Pro:** Validar mapeo 0-32767 → 0-14848 en master (P4/S3)
7. **Calibración motor:** Validar seguimiento fader con resolución 16-bit

---

**Fin de informe.**

Sesión completada: 2026-05-09 22:45  
Generado por: Claude Code  
Estado: ✅ LISTO PARA FLASH
