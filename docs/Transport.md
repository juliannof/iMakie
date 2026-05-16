# Transport — Controles y Feedback (iMakie S3 Extender)

Documentación exhaustiva del subsistema de transporte. Incluye botones físicos (RW/FF/STOP/PLAY/REC), LEDs de feedback, mapeo MIDI, y handshake Mackie MCU.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (5 botones, 5 LEDs, MIDI feedback)

---

## 1. HARDWARE TRANSPORT

### 1.1 Pinout Transport (S3)

| Función | Nota MIDI | GPIO BTN | GPIO LED | Lógica | Vel MIDI |
|---------|-----------|----------|----------|--------|----------|
| **RW** (Rewind) | 0x5B (91) | 3 | 4 | Activo LOW | 127 on / 0 off |
| **FF** (Fast Forward) | 0x5C (92) | 7 | 8 | Activo LOW | 127 on / 0 off |
| **STOP** | 0x5D (93) | 5 | 6 | Activo LOW | 127 on / 0 off |
| **PLAY** | 0x5E (94) | 9 | 10 | Activo LOW | 127 on / 0 off |
| **REC** (Record) | 0x5F (95) | 11 | 12 | Activo LOW | 127 on / 0 off |

**Hardware:**
- **Botones:** Switches mecánicos, pull-up interno, activo LOW
- **LEDs:** Directo GPIO (no PWM), control on/off via MIDI feedback
- **Debounce:** 20ms (similar a BUTTONS.md)

---

## 2. MAPEO MIDI TRANSPORT

### 2.1 Notas MIDI

```cpp
#define MIDI_RW    0x5B   // 91 - Rewind
#define MIDI_FF    0x5C   // 92 - Fast Forward
#define MIDI_STOP  0x5D   // 93 - Stop
#define MIDI_PLAY  0x5E   // 94 - Play
#define MIDI_REC   0x5F   // 95 - Record
```

**Canal:** MIDI 1 (omnidireccional)

**Velocidad:**
- **127** = encendido (button presionado, LED on)
- **0** = apagado (button suelto, LED off)

---

## 3. FLUJO CONTROL BIDIRECCIONAL

### 3.1 Dirección A: Logic → S3 → LEDs

```
Logic Pro
    │ Note On (nota 91-95, vel 127)
    ↓
S3 MidiProcessor::processMidiNote()
    │ Recibe nota + velocidad
    │ Identifica función (RW/FF/STOP/PLAY/REC)
    ↓
Transporte::setLedByNote(note, velocity)
    │ Si velocity=127 → LED ON
    │ Si velocity=0 → LED OFF
    ↓
digitalWrite(GPIO_LED, HIGH/LOW)
    │ Controla LED física
    ↓
Usuario ve feedback visual
    └─ LED enciende cuando Logic prepara/graba
```

**Ejemplo:**
- Logic entra en PLAY → envía Note On 0x5E vel 127
- S3 setLedByNote(0x5E, 127) → digitalWrite(GPIO10, HIGH)
- LED PLAY enciende
- Usuario presiona STOP → nota 0x5D vel 127
- LED STOP enciende, LED PLAY apaga

### 3.2 Dirección B: S3 Botones → Logic

```
Usuario presiona BTN REC
    ↓
GPIO11 transición LOW
    ↓
ButtonManager::update() (debounce 20ms)
    │ Detecta flanco LOW → HIGH (release)
    ↓
buttonPressed[REC] = true
    ↓
S3 Main loop:
    ├─ Envía Note On (0x5F, vel 127) a Logic
    │  (incluido en MIDI output buffer)
    ↓
Logic recibe MIDI
    ├─ Nota 0x5F = REC command
    ├─ Vel 127 = presionado
    ├─ Actualiza estado grabación
    ├─ Envía feedback MIDI confirmando
    ↓
S3 recibe feedback
    ├─ setLedByNote(0x5F, 127)
    ├─ LED REC se enciende
```

---

## 4. HANDSHAKE MACKIE MCU (FAMILIA 0x14)

### 4.1 Protocolo Completo

**Fase 1 — Sondeo (cualquier familia):**
```
Logic → S3:  F0 00 00 66 <any> 00 F7
S3 → Logic:  F0 00 00 66 14 01 00 00 00 01 00 00 00 00 F7
             └─ Familia 0x14 (identificar como Extender)
```

**Fase 2 — Handshake (familia 0x14):**
```
Logic → S3:  F0 00 00 66 14 21 01 F7      (solicita conexión)
S3 → Logic:  F0 00 00 66 14 21 01 F7      (confirma CONNECTED)

Logic → S3:  F0 00 00 66 14 0C 00 F7      (tipo superficie = Master)
S3 → Logic:  F0 00 00 66 14 0C 00 F7      (confirma)

S3 → Logic:  F0 00 00 66 14 10 00 F7      (suscripción feedback)
             └─ Solicita recibir Note On/Off en tiempo real
```

### 4.2 Resultados

- **CONNECTED:** Logic considera S3 operacional
- **Feedback en tiempo real:** Logic envía todas las notas transport
- **LEDs sincronizados:** Feedback visual instantáneo

---

## 5. ARQUITECTURA SOFTWARE

### 5.1 Componentes

**MidiProcessor.cpp:**
```cpp
void MidiProcessor::processMidiNote(uint8_t note, uint8_t velocity) {
    switch (note) {
        case MIDI_RW:    // 0x5B
        case MIDI_FF:    // 0x5C
        case MIDI_STOP:  // 0x5D
        case MIDI_PLAY:  // 0x5E
        case MIDI_REC:   // 0x5F
            Transporte::setLedByNote(note, velocity);
            break;
    }
}
```

**Transporte.cpp:**
```cpp
void Transporte::setLedByNote(uint8_t note, uint8_t velocity) {
    uint8_t gpio_led;
    
    switch (note) {
        case MIDI_RW:   gpio_led = GPIO_LED_RW; break;
        case MIDI_FF:   gpio_led = GPIO_LED_FF; break;
        case MIDI_STOP: gpio_led = GPIO_LED_STOP; break;
        case MIDI_PLAY: gpio_led = GPIO_LED_PLAY; break;
        case MIDI_REC:  gpio_led = GPIO_LED_REC; break;
    }
    
    digitalWrite(gpio_led, (velocity > 0) ? HIGH : LOW);
}
```

**ButtonManager.cpp:**
```cpp
// Detecta presión, envía Note On
void ButtonManager::update() {
    for (int i = BUTTON_RW; i <= BUTTON_REC; i++) {
        if (buttonPressed[i]) {
            uint8_t note = transport_notes[i];  // 0x5B-0x5F
            midiOut.noteOn(note, 127, MIDI_CH1);
            buttonPressed[i] = false;
        }
    }
}
```

### 5.2 Timing Crítico

```
Usuario presiona BTN
         ↓
GPIO transition (microsegundos)
         ↓
ButtonManager::update() ejecuta (cada 20ms loop)
         ↓
Debounce 20ms verifica estable
         ↓
MIDI transmite (< 10ms en puerto 31250 bauds)
         ↓
Logic recibe MIDI (< 1ms)
         ↓
Logic actualiza estado
         ↓
Logic envía feedback MIDI (< 1ms)
         ↓
S3 recibe feedback (< 1ms)
         ↓
Transporte::setLedByNote() ejecuta
         ↓
LED enciende (instantáneo)

Latencia total: < 60ms (imperceptible al usuario)
```

---

## 6. TROUBLESHOOTING

### 6.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Botón no responde | GPIO BTN no leído o debounce fallo | Test GPIO con Serial.println() |
| LED no enciende | GPIO LED abierto o GPIO configuración | Verificar digitalWrite(GPIO_LED, HIGH) en código |
| MIDI no enviado | MidiProcessor no procesa transporte | Check processMidiNote() llamado |
| LED lag (>100ms) | Loop principal bloqueante | Verificar timing loop 20ms |
| Handshake falla | Familia 0x14 no respondida | Verificar respuesta SysEx 0x14 |
| LED siempre on/off | Lógica invertida (HIGH/LOW) | Probar digitalWrite opuesto |

### 6.2 Debugging Logs

**Botón presionado correctamente:**
```
[TRANSPORT] RW presionado (GPIO3 LOW)
[TRANSPORT] RW release (GPIO3 HIGH después debounce)
[TRANSPORT] MIDI Note On 0x5B vel 127
[TRANSPORT] S3 recibe feedback Note On 0x5B vel 127
[TRANSPORT] LED RW ON (GPIO4 HIGH)
```

**Handshake exitoso:**
```
[MACKIE] Sondeo recibido
[MACKIE] Respondiendo familia 0x14
[MACKIE] Connect request recibido (0x21 01)
[MACKIE] Connected!
[MACKIE] Suscripción feedback (0x10 00)
```

---

## 7. REFERENCIAS

- **BUTTONS.md** — Arquitectura ButtonManager, debounce, GPIO
- **LEDS.md** — NeoPixel (S2), no relacionado pero similar debounce
- **SAT.md** — Sistema Auto-Test, no impacta transport
- **MASTER_S3-P4/S3/.../README.md** — Hardware S3, pinout
- **CLAUDE.md** — Directivas generales

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado Transport.md como documento exhaustivo, extraído de S3 README
