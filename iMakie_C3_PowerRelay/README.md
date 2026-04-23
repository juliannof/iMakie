# iMakie PTxx — ESP32-C3 Mesa Control

> Control de mesa con motor paso a paso y relé de activación de 10V

---

## Descripción

Sistema de control de mesa motorizada con sensor ultrasónico de presencia. Al completar el movimiento de extensión, activa un relé de 10V para alimentar otros dispositivos del sistema iMakie.

```
Sensor detecta → Motor extiende mesa → Relé activa 10V
Sensor detecta → Motor retrae mesa → Relé desactiva 10V
```

---

## Hardware

| Componente | Especificación |
|------------|----------------|
| **MCU** | ESP32-C3-DevKitM-1 |
| **Motor** | Paso a paso (driver compatible AccelStepper) |
| **Sensor** | Ultrasónico HC-SR04 (distancia < 4cm) |
| **Final de carrera** | Switch NC/NO para homing |
| **Relé** | Módulo 1 canal, carga 10V |

---

## Pinout ESP32-C3

| GPIO | Función | Dispositivo |
|------|---------|-------------|
| **2** | Control relé (HIGH = ON) | Relé 10V |
| **5** | DIR | Driver motor |
| **6** | STEP | Driver motor |
| **7** | ECHO | Sensor ultrasónico |
| **10** | TRIG | Sensor ultrasónico |
| **20** | ENABLE | Driver motor |
| **21** | Final de carrera (INPUT_PULLUP) | Endstop |

---

## Funcionamiento

### 1. Arranque
- Sistema ejecuta **homing** automático con final de carrera
- Motor retrocede hasta activar el endstop
- Posición `0` establecida
- Relé en estado **OFF**

### 2. Detección de Presencia (en HOME)
- Sensor ultrasónico detecta objeto a < 4cm
- Motor se extiende hasta `MAX_TRAVEL_STEPS` (40,000 pasos)
- Al completar movimiento → **Relé ON** (10V activado)

### 3. Detección de Presencia (en AWAY)
- Sensor detecta nuevamente
- Motor retrocede a posición HOME
- Al llegar a HOME (endstop) → **Relé OFF** (10V desactivado)

### Máquina de Estados

```
SYSTEM_INIT
    ↓
HOMING_SEARCHING → HOMING_BACKOFF
    ↓
IDLE_AT_HOME_POSITION ←──────┐
    ↓ (sensor)               │
MOVING_AWAY_FROM_HOME        │
    ↓                        │
IDLE_AWAY_FROM_HOME          │
    ↓ (sensor)               │
MOVING_TOWARDS_HOME ─────────┘
```

---

## Configuración

Todos los parámetros en **`src/config.h`**:

```cpp
// Parámetros del motor
#define MAX_SPEED           1500.0f     // Velocidad máxima (pasos/seg)
#define ACCELERATION        500.0f      // Aceleración (pasos/seg^2)
#define MAX_TRAVEL_STEPS    40000L      // Recorrido total desde home

// Sensor ultrasónico
#define UMBRAL_DISTANCIA    4           // Distancia de activación (cm)
#define SENSOR_DELAY        100         // Tiempo entre lecturas (ms)

// Pines GPIO
#define RELAY_PIN           2           // Control del relé
#define DIR_PIN             5           // Dirección motor
#define STEP_PIN            6           // Paso motor
// ... (ver config.h completo)
```

---

## Compilar y Subir

```bash
cd iMakie_C3_PowerRelay
pio run -t upload
pio device monitor
```

### Salida esperada en monitor:

```
------------------------------------
Sistema de control con AccelStepper
------------------------------------
Iniciando secuencia de Homing...
Final de carrera encontrado.
Home establecido en posicion 0.
Retrocediendo 100 pasos para liberar el final de carrera.
Retroceso completado.
Homing completado. Sistema en IDLE_AT_HOME_POSITION.

Sensor activado a 3.5 cm.
Estado actual: IDLE_AT_HOME_POSITION
Inicio de MOVING_AWAY_FROM_HOME.
Movimiento MOVING_AWAY_FROM_HOME completado.
Motor en IDLE_AWAY_FROM_HOME.
[RELAY] 10V ON
```

---

## Dependencias

- **AccelStepper** 1.64 (instalada automáticamente por PlatformIO)

---

## Plataforma

- **Framework:** Arduino
- **Plataforma:** pioarduino 55.03.37 (ESP-IDF 5.x)
- **Logs:** Nivel verbose (`CORE_DEBUG_LEVEL=5`)
- **USB CDC:** Habilitado en boot

---

## Integración con iMakie PTxx

Este módulo es un componente auxiliar del sistema iMakie PTxx. El relé de 10V puede usarse para:

- Activar alimentación de motores de faders
- Controlar iluminación de panel
- Gestión de power management del sistema
- Cualquier carga que requiera sincronización con la posición de la mesa

---

## Archivos del Proyecto

```
iMakie_C3_PowerRelay/
├── platformio.ini          # Configuración PlatformIO
├── README.md               # Este archivo
├── .gitignore             # Exclusiones Git
└── src/
    ├── main.cpp           # Código principal (motor + relé)
    └── config.h           # Configuración de pines y parámetros
```

---

## Troubleshooting

### El motor no hace homing
- Verificar conexión del final de carrera en GPIO21
- Comprobar que el endstop es NC (normalmente cerrado) o ajustar `INPUT_PULLUP`
- Revisar conexión del driver (DIR, STEP, ENABLE)

### El sensor no detecta
- Medir distancia con multímetro en pines TRIG/ECHO
- Verificar alimentación del sensor (5V)
- Ajustar `UMBRAL_DISTANCIA` en config.h

### El relé no activa
- Verificar conexión en GPIO2
- Comprobar alimentación del módulo relé
- Medir voltaje en pin de control (debe ser 3.3V cuando ON)

### Motor se mueve erráticamente
- Verificar `MAX_SPEED` y `ACCELERATION` (reducir si es necesario)
- Comprobar alimentación del driver
- Revisar ajuste de corriente del driver (potenciómetro)

---

## Licencia

Proyecto privado — Parte del sistema iMakie PTxx

---

**Versión:** 1.0.0  
**Última actualización:** Abril 2025  
**Hardware:** ESP32-C3-DevKitM-1