# Menú SAT — iMakie PTxx Track S2

## Navegación global

| Botón  | GPIO | Función |
|--------|------|---------|
| REC    | 37   | ▲ Subir / incrementar |
| SOLO   | 38   | ▼ Bajar / decrementar |
| MUTE   | 39   | ◄ Atrás / cancelar |
| SELECT | 40   | ► Entrar / confirmar |

**Entrar en SAT:** REC mantenido 1 segundo → barra de progreso roja → SAT abre.
**Brillo:** sube a 255 al entrar, vuelve a BRIGHTNESS_NORMAL al salir.
**Motor y RS485:** se suspenden al entrar, se restauran al salir.

---

## Árbol de menús

```
MAIN
├── Identidad
│   ├── Track ID      → editor numérico 1-8
│   └── Etiqueta      → editor carácter a carácter (6 chars)
├── Motor
│   ├── PWM Mínimo    → editor numérico 0-120
│   └── PWM Máximo    → editor numérico 50-255
├── Touch
│   ├── Habilitado    → toggle ON/OFF (guarda inmediatamente)
│   └── Umbral %      → editor numérico 10-90
├── Diagnostico
│   ├── Test Display  → 7 patrones visuales
│   ├── Test Encoder  → lectura en vivo + historial
│   ├── Test NeoPixel → selector pixel + color
│   └── Test Fader    → ADC + touch + motor manual
├── Config WiFi       → lanza WiFiManager (captive portal)
└── Reiniciar         → confirmación → ESP.restart()
```

---

## MAIN

Lista de 6 opciones con scroll.
Si `trackId == 0` el header muestra `!SIN CONFIGURAR!` con banner rojo.
MUTE sale del SAT (bloqueado si trackId == 0).

---

## Identidad

### Track ID
Editor numérico. Rango 1–8.
REC/SOLO cambian el valor. SELECT guarda en NVS y aplica a RS485 (`rs485.begin(id)`).
MUTE cancela sin guardar.
Si el módulo es nuevo (trackId == 0), el editor arranca en 1.

### Etiqueta (sin uso operacional actual)
Editor de 6 caracteres. Un carácter activo a la vez, resaltado en rojo.
REC sube el carácter: A→Z→a→z→0→9→espacio→A...
SOLO baja el carácter en sentido inverso.
SELECT avanza al siguiente carácter. En el último carácter guarda.
MUTE retrocede al carácter anterior. En el primero cancela y vuelve.

---

## Motor

### PWM Mínimo
Rango 0–120. Es el PWM por debajo del cual el motor no mueve el fader (zona muerta baja).
Valor típico: 40.

### PWM Máximo
Rango 50–255. PWM máximo que se envía al DRV8833.
Valor típico: 220.

Ambos se guardan en NVS y se entregan por `onConfigSaved` para que el controlador de motor los aplique.

---

## Touch

### Habilitado
Toggle ON/OFF. SELECT lo invierte y guarda inmediatamente. Muestra toast "Touch: ON" / "Touch: OFF".

### Umbral %
Rango 10–90. Porcentaje del valor base capacitivo por debajo del cual se detecta contacto.
80 = detecta si el valor cae por debajo del 80% del baseline.

---

## Diagnostico

### Test Display
7 patrones navegables con REC/SOLO. SELECT avanza al siguiente.

| # | Patrón |
|---|--------|
| 0 | Colores sólidos (9 colores, REC/SOLO/SELECT cambian) |
| 1 | Gradiente RGB horizontal |
| 2 | Checker blanco/negro 8px |
| 3 | Texto en tamaños 1/2/3 con colores |
| 4 | Formas geométricas (rect, círculo, triángulo) |
| 5 | 32 barras escala de grises |
| 6 | Barrido de líneas animado (loop continuo) |

MUTE sale al menú Diagnostico.

### Test Encoder
Muestra en tiempo real sin parar:
- Pines A y B en crudo (HIGH/LOW con color)
- Estado SW del encoder (pulsado/libre)
- Contador grande con signo
- Flecha de dirección << / >>
- Gráfica de barras historial últimas 20 posiciones

REC/SOLO resetean el contador. SELECT limpia el historial. MUTE sale.

### Test NeoPixel
Selector de pixel: 0, 1, 2, 3, o ALL (todos).
REC sube el pixel seleccionado. SOLO baja.
SELECT avanza al siguiente color de la paleta (10 colores + apagar).
Los LEDs físicos se actualizan en tiempo real.
Paleta: Rojo, Verde, Azul, Amarillo, Magenta, Cyan, Blanco, Naranja, Acento (#e94560), Apagar.
MUTE apaga todos los LEDs y sale.

### Test Fader
Pantalla dividida en 4 zonas que se actualizan cada 25ms:

**FADER** — raw ADC, porcentaje, rango calibrado [min-max].
Barra horizontal de posición. Marca vertical amarilla en 75% (0dB unity gain).
La calibración es automática: min/max se actualizan mientras se mueve el fader.

**TOUCH** — raw capacitivo vs baseline, barra de ratio, indicador TOCADO/libre con círculo verde/gris.

**MOTOR** — barra PWM centrada (−255 a +255). Verde = sube, rojo = baja.
REC mueve el motor hacia arriba (+20 PWM). SOLO hacia abajo (−20 PWM).
SELECT para el motor y resetea la calibración del fader.

**GRÁFICA** — miniplot de las últimas 80 muestras del fader. Línea amarilla en 75%.

MUTE para el motor y sale.

---

## Config WiFi
Acción directa. Llama al callback `onWiFiLaunch` y muestra toast "Lanzando WiFiManager...".
No hay submenú — el portal AP se gestiona externamente.

---

## Reiniciar
Muestra confirmación: "Reiniciar el dispositivo?"
SELECT confirma → toast "Reiniciando..." → 900ms → `ESP.restart()`.
MUTE cancela y vuelve al MAIN.

---

## Editores

### Editor numérico
Muestra el título, el valor grande centrado con flechas ▲▼, y el rango [min-max].
REC +1, SOLO −1. SELECT guarda en NVS. MUTE cancela sin guardar.

### Editor de etiqueta
6 cajas de carácter. La activa se resalta en rojo.
Ciclo de caracteres: A-Z → a-z → 0-9 → espacio → (repite).
SELECT avanza posición / guarda al llegar al final.
MUTE retrocede posición / cancela al llegar al inicio.

### Toast
Overlay centrado negro con borde rojo. Desaparece a 1.3s o con cualquier botón.

### Confirmación
Dos botones: SI (SELECT) en rojo, NO (MUTE) en gris.

---

## Persistencia NVS

Namespace: `"ptxx"`

| Clave      | Tipo   | Default | Descripción |
|------------|--------|---------|-------------|
| trackId    | uint8  | 0       | 0 = no configurado, fuerza apertura SAT |
| label      | string | "CH-01 "| Etiqueta local (6 chars) |
| pwmMin     | uint8  | 40      | PWM mínimo motor |
| pwmMax     | uint8  | 220     | PWM máximo motor |
| touchEn    | bool   | true    | Touch habilitado |
| touchThr   | uint8  | 80      | Umbral touch % |

Solo se guarda al confirmar con SELECT. Nunca al cancelar con MUTE.
