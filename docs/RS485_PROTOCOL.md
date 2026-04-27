# RS485 — Protocolo de Comunicación

**Resumen:** Bus serial bidireccional 500 kbaud que conecta master (P4 o S3) con 8-9 slaves (S2). Protocolo binario custom con CRC8, tiempos críticos microsegundos, máquina de estados.

---

## Visión general

### Topología de red

```
Logic Pro (macOS)
    │ USB-MIDI
    ▼
ESP32-P4  ──RS485 bus A──  9× ESP32-S2 slaves (IDs 1–9)
(master MCU)

ESP32-S3  ──RS485 bus B──  8× ESP32-S2 slaves (IDs 1–8)
(extender)
```

- **Bus A (P4):** 9 slaves, TX=GPIO50, RX=GPIO51, EN=GPIO52 (P4)
- **Bus B (S3):** 8 slaves, TX=GPIO15, RX=GPIO16, EN=GPIO1 (S3)
- **Slaves (S2):** TX=GPIO8, RX=GPIO9, EN=GPIO35 (todos idénticos)

### Características físicas

| Parámetro | Valor | Notas |
|-----------|-------|-------|
| **Baudrate** | 500 kbaud | 500,000 bits/segundo |
| **Formato** | 8N1 | 8 bits data, sin paridad, 1 bit stop |
| **Voltaje** | 3.3V (TTL) | Transceivers TTL/RS485 |
| **CRC** | CRC8 0x07 | Polinomio: x^8 + x^2 + x^1 + 1 |
| **Redundancia** | Ninguna | Sin ACK, sin reintento automático |

---

## Especificación de paquetes

### Master → Slave: MasterPacket (16 bytes)

```c
struct MasterPacket {
    uint8_t  header;        // 0xAA (byte de inicio)
    uint8_t  id;            // ID esclavo: 1-17 (broadcast: 0)
    char     trackName[7];  // Mackie Scribble Strip (7 chars ASCII, sin null)
    uint8_t  flags;         // bits 0-3: REC|SOLO|MUTE|SELECT
                            // bits 5-7: autoMode (AUTO_OFF..AUTO_LATCH)
                            // bit 4: FLAG_CALIB (one-shot, master lo limpia)
    uint16_t faderTarget;   // Posición objetivo fader: 0-16383 (14-bit, little-endian)
    uint8_t  vuLevel;       // Medidor VU: 0-127
    uint8_t  vpotValue;     // VPot ring (CC raw): bits 6=center, 5-4=modo, 3-0=pos
    uint8_t  connected;     // 1=CONNECTED, 0=DISCONNECTED
    uint8_t  crc;           // CRC8 de bytes [0..14]
};
```

**Diagrama byte a byte:**
```
Byte 0:    0xAA              (header)
Byte 1:    ID (0-17)         (target slave)
Bytes 2-8: "TRACK\0\0"       (7 chars)
Byte 9:    flags             (REC|SOLO|MUTE|SELECT|CALIB|AUTO_MODE)
Bytes 10-11: faderTarget     (little-endian: 14-bit 0-16383)
Byte 12:   vuLevel           (0-127)
Byte 13:   vpotValue         (CC encoding)
Byte 14:   connected         (0 o 1)
Byte 15:   CRC8([0..14])
```

**Ejemplo (track "Drums" REC encendido, fader en 50%):**
```
0xAA 0x01  44 72 75 6D 73 00 00  0x01  0x00 0x40  0x40  0x00  0x01  0x??
(AA)  (1)  D  r  u  m  s  -  -   (REC) (fader 50%) (VU40) (CC0) (CONN) (CRC)
```

### Slave → Master: SlavePacket (9 bytes)

```c
struct SlavePacket {
    uint8_t  header;        // 0xBB (byte de inicio)
    uint8_t  id;            // Echo del ID enviado
    uint16_t faderPos;      // Posición actual fader (ADC 13-bit): 0-8191 (little-endian)
    uint8_t  touchState;    // 0=libre, 1=tocado
    uint8_t  buttons;       // bits 0-3: REC|SOLO|MUTE|SELECT (estado actual)
                            // bit 4: SLAVE_FLAG_CALIB_DONE (calibración completada)
                            // bit 5: SLAVE_FLAG_CALIB_ERROR (error en calibración)
                            // bit 6: SLAVE_FLAG_NOT_CALIBRATED (fader sin calibrar)
    int8_t   encoderDelta;  // Rotación acumulada: -127..+127 (se resetea a 0 tras lectura)
    uint8_t  encoderButton; // 0=liberado, 1=presionado
    uint8_t  crc;           // CRC8 de bytes [0..7]
};
```

**Diagrama byte a byte:**
```
Byte 0:    0xBB               (header)
Byte 1:    ID (echo)
Bytes 2-3: faderPos           (little-endian: 13-bit 0-8191)
Byte 4:    touchState         (0 o 1)
Byte 5:    buttons            (REC|SOLO|MUTE|SELECT|CALIB_DONE|CALIB_ERR|NOT_CALIB)
Byte 6:    encoderDelta       (-127..+127)
Byte 7:    encoderButton      (0 o 1)
Byte 8:    CRC8([0..7])
```

**Ejemplo (fader 50%, encoder girado +4, botón REC presionado):**
```
0xBB 0x01  0x00 0x10  0x00  0x01  0x04  0x01  0x??
(BB)  (1)  (fader)    (touch) (REC) (+4)  (btn)  (CRC)
```

---

## Flags y campos especiales

### Flags de automatización (MasterPacket bits 5-7)

| Valor (bits 7:5) | Nombre | Descripción | Uso |
|---|---|---|---|
| 0 (000) | AUTO_OFF | Sin automatización | Estado neutral |
| 1 (001) | AUTO_READ | Lectura de automatización | Logic reproduce datos guardados |
| 2 (010) | AUTO_WRITE | Grabación de automatización | Usuario graba nuevos datos |
| 3 (011) | AUTO_TRIM | Trim (ajuste fino) | Ajuste fino sobre grabación |
| 4 (100) | AUTO_TOUCH | Toque (activa escritura) | Tocar fader = grabar |
| 5 (101) | AUTO_LATCH | Latch (mantiene valor) | Soltar = mantiene último valor |

### Flags de calibración (SlavePacket byte 5)

| Bit | Nombre | Significado |
|---|---|---|
| 0 | REC | Botón REC presionado |
| 1 | SOLO | Botón SOLO presionado |
| 2 | MUTE | Botón MUTE presionado |
| 3 | SELECT | Botón SELECT presionado |
| 4 | CALIB_DONE | Calibración completada exitosamente |
| 5 | CALIB_ERROR | Error en calibración (reintento pendiente) |
| 6 | NOT_CALIB | Fader sin calibrar (requiere FLAG_CALIB) |

---

## Máquina de estados (master → slave)

### Ciclo de comunicación completo

```
1. SEND
   └─ Master envía MasterPacket a slave[id]
      TX_ENABLE (50µs) → 16 bytes @ 500kbaud (256µs) → TX_DONE (50µs)
      Total: ~356µs

2. WAIT_RESP
   └─ Master espera SlavePacket (9 bytes ≈ 180µs + overhead)
      ├─ RX TIMEOUT (3000µs) → incrementa contador, GAP
      └─ RX COMPLETO (9 bytes) → valida CRC, procesa datos, GAP

3. GAP
   └─ Espera mínimo 300µs antes de siguiente slave
      (garantiza que slave deshabilite transceiver)

4. _nextSlave()
   └─ Incrementa id, si id > NUM_SLAVES → reset a 1
      delay(POLL_CYCLE_MS - elapsed_us)  ← rellena ciclo 20ms

5. Vuelve a SEND
```

### Diagrama de timing (slave individual)

```
Master TX    ║ 50µs setup │ 256µs data │ 50µs hold │
Slave RX     ║             └─────────────────┘
Slave PROC   ║                              └─ ~200µs process
Slave TX     ║                                 └─ 50µs setup │ 180µs data │ 50µs hold
Master RX    ║                                             └────────────────┘
Gap          ║                                                              └─ 300µs
Next SEND    ║                                                                   └─ TX

Total por slave: ~1200µs típico (con 1 slave) → 20ms ciclo ÷ 16 ≈ ~1.25ms/slave
```

### Estados de tiempo (S3 Master actual)

| Parámetro | Valor | Descripción |
|-----------|-------|---|
| **TX_ENABLE_US** | 50µs | Setup time para que transceiver transmita |
| **TX_DONE_US** | 50µs | Hold time después de TX completo |
| **RESP_TIMEOUT_US** | 3000µs | Máximo esperando SlavePacket (aumentado de 1500µs) |
| **GAP_US** | 300µs | Espera mínima entre esclavos |
| **POLL_CYCLE_MS** | 20ms | Ciclo completo (8 slaves ÷ 20ms ≈ 2.5ms/slave) |
| **RS485_BAUD** | 500000 | Bits por segundo |

**Ratios críticos:**
- `SlavePacket time` = 9 bytes × 10 bits/byte ÷ 500kbaud = **180µs**
- `RESP_TIMEOUT` = 3000µs = **16.7× SlavePacket time** → margen para ISR overhead
- `Ciclo 8 slaves` = 8 × (256 + 180 + 300)µs + gaps = ~5ms → cabe en 20ms budget

---

## CRC8 — Cálculo y validación

**Polinomio:** `x^8 + x^2 + x^1 + 1` (código 0x07)

### Cálculo (pseudocódigo)

```c
uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;  // Desplaza + XOR con polinomio
            } else {
                crc = crc << 1;
            }
            crc &= 0xFF;  // Mantén 8 bits
        }
    }
    return crc;
}
```

### Validación

```c
// Master recibe SlavePacket
uint8_t calculated_crc = crc8(rx_buffer, 8);  // Bytes [0..7]
if (calculated_crc != rx_buffer[8]) {
    // CRC mismatch → descartar paquete
    stats.crc_errors++;
    continue;  // Siguiente slave
}
```

### Ejemplos

**Paquete válido (SlavePacket):**
```
0xBB 0x01 0x00 0x10 0x00 0x01 0x04 0x01
CRC = crc8([0xBB, 0x01, 0x00, 0x10, 0x00, 0x01, 0x04, 0x01]) = 0x??
```

---

## Sincronización y lectura

### Búsqueda de encabezado

**Master lectura:** `_readResponse()` busca header `0xBB`

```c
// Limpia buffer antes de SEND
Serial1.flush();
while (Serial1.available()) {
    Serial1.read();  // Descarta basura
}

// Entra en WAIT_RESP
uint32_t start = micros();
while (micros() - start < RESP_TIMEOUT_US) {
    if (Serial1.available()) {
        uint8_t byte = Serial1.read();
        if (byte == 0xBB) {  // ← Found header
            // Acumula 8 bytes restantes
            for (int i = 1; i < 9; i++) {
                while (!Serial1.available() && (micros() - start < RESP_TIMEOUT_US));
                packet[i] = Serial1.read();
            }
            break;
        }
    }
}
```

**Problema identificado:** Sin timeout inter-byte, si se pierde sincronización, master espera 3000µs sin poder recuperarse. Solución futura: timeout de 50µs entre bytes.

### Limpieza pre-TX

**Slave (antes de enviar):**

```c
// Limpia buffer recibido (puede haber eco)
Serial1.flush();

// Envío
digitalWrite(RS485_ENABLE_PIN, HIGH);
delayMicroseconds(50);
Serial1.write((const uint8_t*)&tx_packet, sizeof(SlavePacket));
Serial1.flush();
delayMicroseconds(50);
digitalWrite(RS485_ENABLE_PIN, LOW);
```

---

## Protocolo de calibración (one-shot)

Secuencia para calibrar fader (motor):

### 1. Master detecta necesidad

```
Master recibe SlavePacket con bit SLAVE_FLAG_NOT_CALIBRATED = 1
→ Marca _ch[id].calibrate = true
```

### 2. Master envía FLAG_CALIB

```
Master construye MasterPacket con:
  - flags = (... | FLAG_CALIB)  // bit 4
  - Envía a slave
```

### 3. Slave inicia calibración

```
Slave recibe FLAG_CALIB
→ Motor::init() → máquina de calibración:
   IDLE
    ↓
   KICK_UP (max PWM, 500ms)     → fuerza movimiento inicial
    ↓
   GOING_UP (PWM mediano)       → hasta ADC máximo
    ↓
   SETTLE_UP (PWM bajo, 200ms)  → estabiliza
    ↓
   KICK_DOWN → GOING_DOWN → SETTLE_DOWN (simétrico)
    ↓
   CALIBRATED (marca bit SLAVE_FLAG_CALIB_DONE)
```

### 4. Master recibe confirmación

```
Master recibe SlavePacket con bit SLAVE_FLAG_CALIB_DONE = 1
→ Marca _ch[id].calibrated = true
→ Deja de enviar FLAG_CALIB en futuras transacciones
```

### 5. Error / Retries

```
Si SLAVE_FLAG_CALIB_ERROR = 1:
  → _ch[id].calibRetries++
  → Reintentar hasta 3 veces
  → Si persiste, marcar NOT_CALIBRATED
```

**Nota 2026-04-27:** Calibración desactivada en S3 Master porque hardware S2 no tiene motores conectados. Reactivar cuando motores estén presentes.

---

## Configuración por dispositivo

### P4 Master (RS485 Bus A)

```c
// platformio.ini
[env:esp32-p4-devkit-c]
board = esp32-p4-devkit-c

// config.h
#define RS485_TX_PIN    50
#define RS485_RX_PIN    51
#define RS485_ENABLE_PIN 52
#define NUM_SLAVES      9
#define RS485_BAUD      500000
```

**Tarea:** `taskCore1()` ejecuta máquina RS485

### S3 Extender (RS485 Bus B)

```c
// platformio.ini
[env:esp32-s3-devkit-c-1]
board = esp32-s3-devkit-c-1

// config.h
#define RS485_TX_PIN    15
#define RS485_RX_PIN    16
#define RS485_ENABLE_PIN 1
#define NUM_SLAVES      8
#define RS485_BAUD      500000
```

**Tarea:** `runTask()` (Core 0)

### S2 Slave (todos los buses)

```c
// platformio.ini
[env:lolin_s2_mini]
board = lolin_s2_mini

// config.h
#define RS485_TX_PIN    8
#define RS485_RX_PIN    9
#define RS485_ENABLE_PIN 35
#define RS485_BAUD      500000
```

**Ubicación:** ISR de Serial1 + `sendResponse()` en `main.cpp`

---

## RS485Profiler — Diagnóstico sin overhead

### ¿Qué es?

Herramienta que mide tiempos de operación RS485 (TX, RX, GAP, ciclos) sin bloqueo. Reporta estadísticas cada 100 ciclos.

### Diferencia vs. logging tradicional

| Aspecto | `log_i()` por byte | RS485Profiler |
|---------|---|---|
| **Frecuencia** | Cada byte RX | Cada 100 ciclos (agregado) |
| **Overhead** | Alto: UART/Serial síncrono | Bajo: solo `micros()` |
| **Bloquea Task** | SÍ (espera UART) | NO |
| **Impacto** | Puede causar timeouts artificiales | Nada (mide, no bloquea) |
| **Uso** | Debugging detallado | Diagnóstico en producción |

### Salida esperada

```
[PROF] Ciclo 637000: RX_WAIT avg=2801µs min=397µs max=3056µs TO:85.0% (85/100)
[PROF]   Slave 1: RX=30 TO=20 CRC=0 ID_MM=0 avg=2700µs min=400µs max=2950µs (TO:40%)
[PROF]   Slave 2: RX=0 TO=50 CRC=0 ID_MM=5 avg=0µs min=0µs max=0µs (TO:100%)
[PROF]   Slave 3: RX=0 TO=30 CRC=0 ID_MM=0 avg=0µs min=0µs max=0µs (TO:100%)
```

### Interpretación

| Campo | Significado |
|---|---|
| `avg=2801µs` | Tiempo promedio esperando respuesta |
| `min=397µs` | Mejor respuesta (esclavo respondió rápido) |
| `max=3056µs` | Peor respuesta (cerca del timeout 3000µs) |
| `TO:85%` | Tasa timeout (85 de 100 intentos timeouted) |
| `RX=30` | Respuestas exitosas (30 veces) |
| `TO=20` | Timeouts (20 veces) |
| `CRC=0` | Errores CRC (0 en este reporte) |
| `ID_MM=5` | Mismatches de ID (recibió respuesta de otro slave, 5 veces) |

### Diagnóstico por patrón

| Síntoma | Causa probable | Fix |
|---|---|---|
| `avg ≈ TIMEOUT_US` | Timing muy ajustado | Aumentar RESP_TIMEOUT_US |
| `TO > 50%` | Slaves no responden o no conectados | Verificar conexiones, NUM_SLAVES |
| `min << avg << max` | Variabilidad alta (ISR, logging) | Reducir logging, aumentar timeout |
| `ID_MM > 5` | Desincronización de bus | Revisar cable RS485, terminación |
| `CRC > 3` | Ruido en bus | Revisar shielding, baudrate |
| `avg < 1000µs, TO=0%` | Sistema limpio | OK, mantener |

### Habilitación en código

**S3/P4:**
```cpp
// En RS485.cpp
#define ENABLE_PROFILER 1

// Reporta cada 100 ciclos
if ((++_cycleCount % 100) == 0) {
    printStats();  // Salida a Serial log
}
```

---

## Optimización RS485 — Historial (2026-04-27)

### Problema diagnosticado

- **Síntoma:** 25-43% timeout rates en S3 master con solo 3 slaves
- **Extrapolación:** 8 slaves en producción → >80% timeout (inoperable)
- **Impacto:** Comunicación intermitente, comandos perdidos, pantalla/LEDs no actualizan

### Causa raíz identificada

**1. S3 Master — Calibración innecesaria**
- Hardware S2 actual **no tiene motores** (DRV8833 desactivado en PCB)
- Lógica retentaba calibración 3 veces con retries automáticos
- Cada retry = envío FLAG_CALIB → tráfico RS485 extra → timeouts artificiales

**2. S2 Slave — Neopixel SPI en ruta crítica (BOTTLENECK PRINCIPAL)**
- `RS485Handler::onMasterData()` llamaba `updateAllNeopixels()` **antes** de `buildResponse()`
- SPI WS2812B: 12 LEDs × ~960 ciclos @ 800kHz = **~15ms por update**
- Todo RS485 (ISR + respuesta) bloqueado durante esos 15ms
- Master timeout solo 1500µs (después aumentado a 3000µs) esperando 180µs
- Resultado: casi **100% timeouts**

**3. S2 Slave — Transceiver timing insuficiente**
- Setup delay: 10µs (spec real MAX485/SN75176: 30-50µs)
- Sin hold delay → posible interference en bus
- Contribuye a colisiones y CRC errors

### Soluciones aplicadas

#### Cambio 1: S3 Master — Desactivar retries

Archivo: `S3/src/RS485/RS485.cpp`

Antes:
```cpp
// Retentaba calibración 3 veces
if (!_ch[_currentId].calibrated && !_ch[_currentId].calibrating && 
    _ch[_currentId].calibRetries < 3) {
    _ch[_currentId].calibrate = true;  // ← Genera FLAG_CALIB innecesario
}
```

Después:
```cpp
// Bypass: sin motores en PCB, todo calibrado
_ch[_currentId].calibrated = true;  // ← No genera tráfico
```

#### Cambio 2: S2 Slave — Remover Neopixels de ruta crítica RS485

Archivo: `track S2/src/RS485/RS485Handler.cpp`

Antes:
```cpp
if (newState == ConnectionState::CONNECTED) {
    updateAllNeopixels();  // ← 15ms bloqueante
}
```

Después:
```cpp
if (newState == ConnectionState::CONNECTED) {
    // updateAllNeopixels() → REMOVIDO
    // Neopixels se actualizan en main.cpp DESPUÉS de sendResponse()
}
```

#### Cambio 3: S2 Slave — Mejorar transceiver timing

Archivo: `track S2/src/RS485/RS485.cpp`

Antes:
```cpp
digitalWrite(RS485_ENABLE_PIN, HIGH);
delayMicroseconds(10);  // ← INSUFICIENTE
Serial1.write(...);
Serial1.flush();
digitalWrite(RS485_ENABLE_PIN, LOW);  // ← SIN HOLD
```

Después:
```cpp
digitalWrite(RS485_ENABLE_PIN, HIGH);
delayMicroseconds(50);  // Setup (spec 30-50µs)
Serial1.write(...);
Serial1.flush();        // Wait TX (~180µs)
delayMicroseconds(50);  // Hold
digitalWrite(RS485_ENABLE_PIN, LOW);
```

### Resultados esperados

**Antes de cambios (con 3 slaves):**
```
[PROF] RX_WAIT avg=2800µs TO:32.0%
```

**Después de cambios:**
```
[PROF] RX_WAIT avg=1200µs TO:5.0%
```

- Respuesta ~2.3× más rápida
- Timeout rate cae a aceptable
- Con 8 slaves: `avg≈1200µs, TO≈8%` ✓

---

## Troubleshooting RS485

### Síntoma: Alto porcentaje de timeouts (>10%)

**Verificación:**
1. ¿Cuántos slaves conectados físicamente vs. NUM_SLAVES en config?
   - Si desconexión, TO% aumenta naturalmente (esperado: ~11% por slave faltante)
2. ¿Hay logging en ruta crítica?
   - Grep `log_i()` en `RS485.cpp` durante WAIT_RESP
   - Solución: remover o usar Profiler
3. ¿Transceiver timing correcto?
   - S2: verificar `delayMicroseconds(50)` antes/después TX
   - P4/S3: verificar TX_ENABLE_US, TX_DONE_US configurados

### Síntoma: ID Mismatch (ID_MM > 5)

**Causa:** Master recibe respuesta de slave incorrecto (desincronización de bus)

**Verificación:**
1. ¿Cable RS485 es diferencial balanceado?
   - Pin A (no-invertido) y Pin B (invertido) deben estar juntos
2. ¿Terminaciones de 120Ω en ambos extremos?
   - Revisar PCB o agregar resistencias pull
3. ¿Hay ruido magnético cerca?
   - Separar de motores, fuentes de poder
4. ¿Baudrate correcto en todos los dispositivos?
   - P4: 500000, S3: 500000, S2: 500000

### Síntoma: CRC errors (CRC > 3 por 100 ciclos)

**Causa:** Ruido en bus, timing incorrecto, baudrate mismatch

**Verificación:**
1. Revisar cable RS485 (integridad, no roto)
2. Revisar baudrate en config.h de todos los dispositivos
3. Si S2 falla: revisar `Serial1.setRxBufferSize()` **antes** de `begin()`
4. Aumentar RESP_TIMEOUT_US temporalmente para detectar si es ISR overhead

### Síntoma: Slave no responde (RX=0 para un slave)

**Causa:** Slave no conectado, dead, o reset en loop

**Verificación:**
1. Monitor serial del S2: ¿recibe MasterPacket?
   - Agregar log en `RS485Handler::onMasterData()`
2. ¿S2 está en modo SAT bloqueando RS485?
   - Presionar Encoder push < 3s para salir de SAT
3. ¿Problema de alimentación en S2?
   - Revisar LED status (pin 11): ¿rojo o verde?
4. ¿RS485 EN está stuck HIGH?
   - Revisar GPIO35 con voltímetro (debe ser LOW cuando idle)

### Síntoma: Master nunca recibe respuesta (timeout siempre)

**Causa:** RS485 physical layer broken, transceiver dead, o cable desconectado

**Verificación:**
1. ¿Transceiver tiene poder?
   - Revisar 3.3V en pin VCC
2. ¿Hay voltaje en los pines A/B del RS485?
   - Con transceiver deshabilitado (EN=LOW): 2.5-3.3V
   - Durante TX: diferencial ±1.5V
3. ¿Cable conectado en ambos extremos?
   - A → A (no-invertido)
   - B → B (invertido)
4. ¿El ISR de Serial1 está habilitado?
   - Revisar `attachInterrupt()` en setup

---

## Mejoras futuras

1. **Timeout inter-byte (50µs)** — Si se pierde un byte, timeout rápido vs. esperar 3ms
2. **Buffer circular RX** — Evitar overflow si master lento procesando
3. **Reintento automático** — Si CRC error, reenviar en siguiente ciclo
4. **Backoff exponencial** — Si slave no responde N veces, saltar ciclo y revisar después
5. **Logging granular con timestamping** — Para análisis post-mortem sin overhead en tiempo real

---

## Referencias de código

| Sistema | Archivo |
|---|---|
| **P4 Master** | `S3/src/RS485/RS485.cpp` (sí, está en carpeta S3) |
| **S3 Master** | `S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.cpp` |
| **S2 Slave RX** | `track S2/src/RS485/RS485Handler.cpp` |
| **S2 Slave TX** | `track S2/src/RS485/RS485.cpp` |
| **Test SAT** | `track S2/src/SatMenu.cpp` (funciones de test RS485) |

---

## Especificaciones de transceivers recomendados

| Modelo | Setup | Hold | Temp | Notas |
|---|---|---|---|---|
| MAX485 | 40ns | 50ns | -40..85°C | Estándar industrial |
| SN75176 | 50ns | 100ns | -40..85°C | Texas Instruments |
| SP3485 | 30ns | 30ns | -40..85°C | Low power |
| LT1785 | 30ns | 30ns | -40..85°C | Precision Monolithic |

**Recomendación actual:** MAX485 o equivalente (50ns ≈ 50µs @ CPU = 50 ciclos @ 1MHz) 

Usar valores conservadores en código (50µs) para todas las marcas.

---

## Historial de cambios

| Fecha | Cambio | Razón |
|---|---|---|
| 2026-04-26 | Identificado timeout 25-43% | Diagnosis inicial |
| 2026-04-27 | Aumentar RESP_TIMEOUT_US: 1500→3000µs | Margen ISR overhead |
| 2026-04-27 | Transceiver timing: 10→50µs | Spec real MAX485 30-50µs |
| 2026-04-27 | Desactivar calibración en S3 | Sin motores en PCB |
| 2026-04-27 | Mover Neopixels fuera RS485 ruta crítica | Bottleneck principal: 15ms SPI |

---

## Notas finales

- RS485 es **single master** (P4 o S3, nunca ambos) en un bus
- Bus A (P4) y Bus B (S3) son **completamente independientes**
- S2 siempre es slave — nunca inicia comunicación
- Logic es la **única fuente de verdad** para estados de botones
- No hay ACK/NACK — master confía en recepción de SlavePacket
- Sin timeout inter-byte, sistema asume que si empieza lectura, completa en <3ms

