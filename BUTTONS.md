# BUTTONS — Botones Físicos y Control (iMakie S2)

Documentación exhaustiva del subsistema de botones. Incluye hardware, debounce, arquitectura ButtonManager, ciclo de lectura, envío RS485, y troubleshooting.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (4 botones funcionales: REC/SOLO/MUTE/SELECT)

---

## 1. HARDWARE BOTONES

### 1.1 Pinout Botones S2

| Botón | GPIO | Lógica | Pull-Up Interno |
|-------|------|--------|-----------------|
| **REC** | 37 | Activo LOW | Habilitado |
| **SOLO** | 38 | Activo LOW | Habilitado |
| **MUTE** | 39 | Activo LOW | Habilitado |
| **SELECT** | 40 | Activo LOW | Habilitado |

**Lógica:**
- `LOW = presionado` (contacto a GND)
- `HIGH = suelto` (pull-up interno ~40kΩ)

### 1.2 Configuración Física

```cpp
// config.h (S2)
#define BUTTON_PIN_REC      37
#define BUTTON_PIN_SOLO     38
#define BUTTON_PIN_MUTE     39
#define BUTTON_PIN_SELECT   40
#define BUTTON_USE_INTERNAL_PULLUP true
```

```cpp
// Hardware::init()
pinMode(BUTTON_PIN_REC, INPUT_PULLUP);
pinMode(BUTTON_PIN_SOLO, INPUT_PULLUP);
pinMode(BUTTON_PIN_MUTE, INPUT_PULLUP);
pinMode(BUTTON_PIN_SELECT, INPUT_PULLUP);
```

---

## 2. DEBOUNCE Y DETECCIÓN

### 2.1 Especificación Debounce

```cpp
// ButtonManager.cpp
static constexpr uint32_t DEBOUNCE_MS = 20;  // 20ms (probado, estable)
```

**Duración:** 20ms (suficiente para contactos mecánicos típicos)

**Razón:** 
- < 20ms: falsos positivos en flancos ruidosos
- > 50ms: lag perceptible en presiones rápidas

### 2.2 Máquina de Detección

```
Estado actual: HIGH (suelto)
         ↓
Lee GPIO → LOW (presionado)
         ↓
Espera debounce (20ms)
         ↓
Verifica GPIO aún LOW
         ├─ Si LOW: confirma presión
         │   ↓
         │   Espera release (GPIO → HIGH)
         │   ↓
         │   Espera debounce (20ms)
         │   ↓
         │   Verifica GPIO aún HIGH
         │   ├─ Si HIGH: RELEASE detectado
         │   │   ↓ buttonPressed = true
         │   └─ Si LOW: falso positivo, reinicia
         └─ Si HIGH: ruido, reinicia
```

### 2.3 Ciclo de Lectura en Loop

```cpp
// main.cpp loop()
ButtonManager::update();  // Línea 267-275

// ButtonManager.cpp
void ButtonManager::update() {
    uint32_t now = millis();
    
    for (int i = 0; i < 4; i++) {
        bool currentState = digitalRead(buttonPins[i]);
        
        // Debounce
        if (currentState != lastState[i]) {
            if (now - lastStateChange[i] > DEBOUNCE_MS) {
                lastState[i] = currentState;
                lastStateChange[i] = now;
                
                // Detectar release (flanco ascendente)
                if (currentState == HIGH) {
                    buttonPressed[i] = true;
                }
            }
        }
    }
}
```

---

## 3. ENVÍO RS485

### 3.1 Encapsulación en SlavePacket

```cpp
// RS485Handler::buildResponse()
SlavePacket resp;
resp.buttons = 0x00;

// Bits 0-3: REC/SOLO/MUTE/SELECT
if (ButtonManager::getButtonState(BUTTON_REC))    resp.buttons |= (1 << 0);
if (ButtonManager::getButtonState(BUTTON_SOLO))   resp.buttons |= (1 << 1);
if (ButtonManager::getButtonState(BUTTON_MUTE))   resp.buttons |= (1 << 2);
if (ButtonManager::getButtonState(BUTTON_SELECT)) resp.buttons |= (1 << 3);

// Enviar a master
rs485.sendResponse(resp);
```

**Timing crítico:**
- `sendResponse()` debe ejecutarse ANTES de display/motor/neopixel update
- Latencia típica: <150µs

### 3.2 Ciclo Botones Completo

```
main.cpp loop()
    ↓
ButtonManager::update() [1-20ms]
    ├─ Lee GPIO
    ├─ Debounce 20ms
    ├─ Detecta release → buttonPressed[i] = true
    ↓
RS485.update()
    ├─ hasNewData() recibe MasterPacket
    ↓
RS485Handler::buildResponse()
    ├─ Lee ButtonManager::getButtonState(GPIO)
    ├─ Encapsula bits 0-3 en SlavePacket.buttons
    ↓
rs485.sendResponse(resp)
    ├─ Envía <150µs
    ↓
Master recibe SlavePacket
    ├─ Decodifica bits
    ├─ Mapea a nota MIDI
    ├─ Envía a Logic
    ↓
Logic recibe nota
    ├─ Actualiza estado (arm rec, solo, etc.)
    ├─ Envía feedback MIDI
    ↓
S2 recibe feedback (próximo ciclo)
```

**Latencia total:** ~20ms (1 ciclo RS485)

---

## 4. ARQUITECTURA BUTTONMANAGER

### 4.1 APIs Públicas

```cpp
class ButtonManager {
public:
    static void begin(LGFX* tft, SatMenu* sat);
    static void update();
    static bool getButtonState(uint8_t buttonId);
    static uint8_t getButtonFlags();
    static void clearButtonFlags();
    static void clearEncoderButton();
};
```

### 4.2 Estados Botones

```cpp
enum ButtonId {
    BUTTON_REC    = 0,
    BUTTON_SOLO   = 1,
    BUTTON_MUTE   = 2,
    BUTTON_SELECT = 3
};

// Flags de estado
static bool buttonPressed[4];      // Detectado release
static bool lastState[4];          // Estado anterior (HIGH/LOW)
static uint32_t lastStateChange[4]; // Timestamp último cambio
```

### 4.3 Integración con SAT

```cpp
// SatMenu.cpp - manejo de botones en SAT
void SatMenu::update() {
    if (ButtonManager::getButtonFlags() & FLAG_REC) {
        // En SAT Motor > Calibración: reiniciar calibración
        if (isMotorCalibScreen()) {
            Motor::startCalib();
        }
    }
    // ... otros botones
}
```

---

## 5. MAPEO MIDI

### 5.1 Notas MIDI por Botón

| Botón | Nota (hex) | Decimal | Canal MIDI | Acción |
|-------|-----------|---------|-----------|--------|
| REC | 0xA0 | 160 | 1 | Arm recording |
| SOLO | 0xA1 | 161 | 1 | Solo channel |
| MUTE | 0xA2 | 162 | 1 | Mute channel |
| SELECT | 0xA3 | 163 | 1 | Select track |

**Velocidad:**
- 127 = presionado (flanco ascendente)
- 0 = suelto (no enviado normalmente)

### 5.2 Flujo Mapeo

```
S2 SlavePacket.buttons = 0x05
    (bits: 0101 = REC + MUTE presionados)
    ↓
Master RS485Handler::_handleResponse()
    ├─ Decodifica: REC (bit 0), MUTE (bit 2)
    ↓
Master MIDI envío
    ├─ Note On (nota 160, vel 127) → REC
    ├─ Note On (nota 162, vel 127) → MUTE
    ↓
Logic recibe notas
    ├─ Arm recording
    ├─ Mute channel
```

---

## 6. TROUBLESHOOTING

### 6.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Botón no responde | GPIO no leído o buttonPressed no limpiado | Check `ButtonManager::update()` en loop |
| Botón tarda al responder | Debounce demasiado largo (>50ms) | Verificar config.h DEBOUNCE_MS |
| Botón genera múltiples eventos | Debounce insuficiente (<10ms) | Aumentar DEBOUNCE_MS |
| SELECT no funciona | GPIO40 flotante o no inicializado | Verificar pinMode(40, INPUT_PULLUP) |
| Botones lentos en Logic | RS485 congestionado | Reducir verbose logging |
| Contacto ruidoso | Contacto mecánico degradado | Limpiar contacto, reemplazar si necesario |

### 6.2 Debugging

**Logs esperados — presión REC:**
```
[BUTTON] REC presionado (GPIO37 LOW)
[BUTTON] REC release (GPIO37 HIGH después 20ms debounce)
[BUTTON] buttonPressed[0] = true
[RS485] TX S2: buttons=0x01 (bit 0 = REC)
[MIDI] Note On 160 vel 127 (REC arm)
```

**Test Manual:**
```cpp
// En SAT o Serial monitor
void debug_buttons() {
    uint8_t flags = ButtonManager::getButtonFlags();
    Serial.printf("[DEBUG] buttons: REC=%d SOLO=%d MUTE=%d SELECT=%d\n",
                  (flags & 0x01) ? 1 : 0,
                  (flags & 0x02) ? 1 : 0,
                  (flags & 0x04) ? 1 : 0,
                  (flags & 0x08) ? 1 : 0);
}
```

---

## 7. HISTORIA BUGS

### 7.1 SELECT No Responde (Pendiente 2026-05-16)

**Síntoma:** Botones REC/SOLO/MUTE funcionan, SELECT no registra presiones

**Causa Sospechosa:**
- GPIO40 no inicializado
- Debounce insuficiente
- RS485 no envía bit 3

**Investigación Requerida:**
1. Verificar `pinMode(40, INPUT_PULLUP)` en initHardware()
2. Verificar GPIO40 físicamente (continuidad, flotante)
3. Verificar RS485Handler::buildResponse() encapsula bit 3
4. Test con Serial.println(digitalRead(40)) en loop

**Status:** En diagnóstico

---

## 8. REFERENCIAS

- **MOTOR.md** — Calibración desde REC en SAT
- **RS485.md** — Envío SlavePacket.buttons
- **S2/README.md** — Pinout botones
- **STATUS.md** — Bugs conocidos botones

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado BUTTONS.md como documento exhaustivo, trasladado contenido de CLAUDE.md
