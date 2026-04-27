# Guía de Comentarios RS485 — Patrones para Código

**Propósito:** Documentar restricciones de timing en código RS485 sin oscurecer la lógica.

---

## Principios

1. **Solo restricciones no obvias** — No comentar booleanos o math simple
2. **Timing explícito** — Si está relacionado con microsegundos, menciona el número
3. **Razón + Consecuencia** — Explica el QUÉ y el PORQUÉ
4. **Evitar comentarios de control de versión** — No escribir "fixed in 2026-04-27"

---

## Patrones de comentarios

### 1. Función en ruta crítica

**Ubicación:** Encima de la firma de función  
**Cuándo:** Si timing < 1ms

```c
// ═════════════════════════════════════════════════════════════════
//  onMasterData — RUTA CRÍTICA RS485 (~400µs máximo)
// ═════════════════════════════════════════════════════════════════
// Recibe paquete del master (16 bytes, CRC ya validado).
// Duración máxima: ~400µs antes de buildResponse() + sendResponse()
// (Master espera SlavePacket dentro de 3000µs)
//
// ¡RESTRICCIÓN! No hacer operaciones bloqueantes:
// - NO updateAllNeopixels()  → 15-30ms (SPI bloqueante)
// - NO updateDisplay()       → 10-100ms (SPI bloqueante)
// - NO Serial.printf()       → UART síncrono
// - NO delay() / sleep
// ═════════════════════════════════════════════════════════════════
void onMasterData(const MasterPacket& pkt) {
```

---

### 2. Decisión arquitectónica

**Ubicación:** Inline, donde la decisión se toma  
**Cuándo:** Si contradice intuición o hay alternativa obvia

```c
// ¡¡CRÍTICO!! NO llamar updateAllNeopixels() aquí
// - Adafruit NeoPixel.show() ≈ 15-30ms (deshabilita interrupciones)
// - Bloquearía respuesta RS485 → timeout del master
// - Mitigación: Flag neoWaitingHandshake + updateAllNeopixels() en main.cpp post-sendResponse()
if (newState == ConnectionState::CONNECTED) {
    neoWaitingHandshake = false;
    // updateAllNeopixels();  ← REMOVIDO (ver comentario arriba)
}
```

---

### 3. Timing específico

**Ubicación:** Encima de operaciones de baja latencia  
**Cuándo:** Si el número importa para debugging

```c
// ════════════════════════════════════════════════════════════════
// TIMING CRÍTICO — RS485 requiere setup/hold de EN
// ════════════════════════════════════════════════════════════════
// Transceiver típico (MAX485, SN75176) requiere:
//   - Setup: 30-50µs ANTES de enviar (EN HIGH → ready to TX)
//   - Hold: 50-100µs DESPUÉS de TX completo (EN LOW tras último bit)
//
// Paquete: 9 bytes × 10 bits/byte = 90 bits @ 500kbaud = 180µs
// ════════════════════════════════════════════════════════════════

digitalWrite(RS485_ENABLE_PIN, HIGH);
delayMicroseconds(50);  // Setup time (MAX485 spec 30-50µs)
Serial1.write((const uint8_t*)&tx, sizeof(SlavePacket));
Serial1.flush();        // Wait for TX complete (~180µs)
delayMicroseconds(50);  // Hold time
digitalWrite(RS485_ENABLE_PIN, LOW);
```

---

### 4. Flag de optimización

**Ubicación:** Línea antes del check del flag  
**Cuándo:** Si el flag previene operación costosa

```c
void updateAllNeopixels() {
    // OPTIMIZACIÓN: Si master no ha conectado aún, no actualizar LEDs
    // Evita ~30ms de SPI innecesario durante handshake
    if (neoWaitingHandshake) return;

    // Check si estado cambió desde última actualización
    if (recStates == lastRec && soloStates == lastSolo && ...) return;

    // [Cambió, procesar]
    handleButtonLedState(ButtonId::REC);
    showNeopixels();  // ← 15-30ms SPI bloqueante (por eso lo evitamos arriba)
}
```

---

### 5. Alternativas rechazadas

**Ubicación:** En bloque comentado si código antiguo existe  
**Cuándo:** Si la versión anterior es educativa

```c
// VERSIÓN ANTERIOR (RECHAZADA — 25-43% timeouts):
// if (newState == ConnectionState::CONNECTED) {
//     neoWaitingHandshake = false;
//     updateAllNeopixels();  // ← BLOQUEABA RS485 (15-30ms SPI)
// }
//
// NUEVA VERSIÓN (CORRECTA):
if (newState == ConnectionState::CONNECTED) {
    neoWaitingHandshake = false;
    // updateAllNeopixels() → main.cpp post-sendResponse()
}
```

---

### 6. Estructura de datos timing

**Ubicación:** Encima de struct si tiene timing requirements  
**Cuándo:** Si duración de envío/recepción importa

```c
// ═══════════════════════════════════════════════════════════════
//  MasterPacket — 16 bytes, ~256µs @ 500kbaud
// ═══════════════════════════════════════════════════════════════
// Header(1) + ID(1) + trackName(7) + flags(1) + faderTarget(2) +
// vuLevel(1) + vpotValue(1) + connected(1) + crc(1)
//
// Master envía contiguamente en ruta crítica.
// Slave debe responder en < 3000µs.
struct MasterPacket {
    uint8_t  header;       // 0xAA
    uint8_t  id;           // Slave ID (0=broadcast)
    char     trackName[7]; // Mackie scribble strip
    // ... etc
};
```

---

## Anti-patrones: QUÉ NO HACER

❌ **NO comentar lo obvio:**
```c
// ← MALO
digitalWrite(RS485_ENABLE_PIN, HIGH);  // Enable RS485
digitalWrite(RS485_ENABLE_PIN, LOW);   // Disable RS485

// ← BIEN (solo si hay sorpresa)
delayMicroseconds(50);  // Hold time — MAX485 needs 50µs after TX
```

❌ **NO usar comentarios para versionado:**
```c
// ← MALO
neoWaitingHandshake = false;  // Fixed 2026-04-27 timeout issue

// ← BIEN (si aún relevante)
neoWaitingHandshake = false;  // SAFE: neoPixels updated later
```

❌ **NO documentar trivialidades:**
```c
// ← MALO
uint8_t crc = rs485_crc8(_rxBuf, sizeof(MasterPacket) - 1);  // Calculate CRC

// ← BIEN (si hay sorpresa)
uint8_t crc = rs485_crc8(_rxBuf, sizeof(MasterPacket) - 1);
if (crc != _rxBuf[sizeof(MasterPacket) - 1]) {
    // CRC mismatch → discard packet (no reintento automático)
    _crcErrors++;
}
```

---

## Cuándo usar qué tipo de comentario

| Situación | Tipo | Ejemplo |
|---|---|---|
| Restricción timing | Block comment + inline | `// ¡CRÍTICO! ...` |
| Razon de design | Inline | `// Avoiding 30ms SPI blocker` |
| Número específico importante | Inline | `// Setup: 50µs per MAX485 spec` |
| Alternativa rechazada | Block (si educativa) | `// VERSIÓN ANTERIOR (RECHAZADA)` |
| Flag de optimización | Inline | `// OPTIMIZACIÓN: evita ...` |
| Obvio | NADA | No comentar |

---

## Checklist antes de commitear RS485

- [ ] Toda operación blocking tiene `// ~Xms` documentado
- [ ] Restricciones `!CRÍTICO!` marcadas en onMasterData()
- [ ] Timing valores en comentarios (50µs, 180µs, 3000µs, etc.)
- [ ] Razón de cada flag documentada (neoWaitingHandshake, needsTOTALRedraw)
- [ ] Alternativas rechazadas en comentarios si son educativas
- [ ] Sin comentarios obvios (digitalWrite, incrementos, etc.)
- [ ] Funciones críticas tienen header block (═══...)

---

## Templates reusables

### Template: Ruta crítica

```c
// ═════════════════════════════════════════════════════════════════
//  FUNCTION_NAME — RUTA CRÍTICA (~Xµs máximo)
// ═════════════════════════════════════════════════════════════════
// Brevísima descripción.
//
// RESTRICCIÓN: 
// - NO ...
// - NO ...
// ═════════════════════════════════════════════════════════════════
void function_name() {
```

### Template: Bottleneck

```c
// ¡¡CRÍTICO!! NO operación_bloqueante() aquí
// - Razón específica: ~Xms (descripción)
// - Impacto: timeout RS485 / timeout master
// - Mitigación: mover a main.cpp post-sendResponse()
```

### Template: Timing exacto

```c
// ════════════════════════════════════════════════════════════════
// TIMING CRÍTICO — Descripción
// ════════════════════════════════════════════════════════════════
// Especificación:
//   - Setup: X µs
//   - Operation: Y µs (9 bytes × 10 bits @ 500kbaud = 180µs)
//   - Hold: Z µs
// ════════════════════════════════════════════════════════════════
```

---

## Recursos para copy-paste

Líneas de separación:
```
// ═════════════════════════════════════════════════════════════════
// ─────────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════
```

Marcadores:
```
// ¡CRÍTICO! ...
// ¡¡CRÍTICO!! ...
// ✓ ...
// ✗ ...
// ⚠️ ...
// OPTIMIZACIÓN: ...
// VERSIÓN ANTERIOR (RECHAZADA):
```

---

**Próximo paso:** Cuando edites RS485.cpp, RS485Handler.cpp, o main.cpp, usa estos patrones para documentar decisiones de timing.
