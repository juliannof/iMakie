#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Adafruit_NeoTrellis.h"

TFT_eSPI tft = TFT_eSPI();              // Objeto de pantalla principal
TFT_eSprite header = TFT_eSprite(&tft); // Sprite para cabecera
TFT_eSprite mainArea = TFT_eSprite(&tft); // Sprite principal para todo el área
TFT_eSprite vuSprite = TFT_eSprite(&tft);

const int pinBoton = 4;          // Pin del botón del joystick
const int pinX = 3;              // Pin del eje X del joystick
const int pinY = 5;              // Pin del eje Y del joystick

#define Y_DIM 4                  // Filas de la matriz NeoTrellis
#define X_DIM 8                  // Columnas de la matriz NeoTrellis

// Configuración de paneles NeoTrellis
Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {
  { Adafruit_NeoTrellis(0x2E), Adafruit_NeoTrellis(0x2F) }
};

Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

bool ledState[X_DIM * Y_DIM] = {false}; // Estado de cada LED
const uint8_t brightness = 35;          // Brillo fijo en 35 (~14%)
bool blinkState = true;                 // Estado del parpadeo (ENCENDIDO)
unsigned long previousBlinkTime = 0;    // Tiempo de referencia para el parpadeo
const unsigned long blinkInterval = 250; // Intervalo de parpadeo (250ms)

const uint32_t baseColors[4] = {
  0xFF0000, // Rojo
  0xFFFF00, // Amarillo
  0xFF0000, // Rojo para mute (parpadeará)
  0xFFFFFF  // Blanco
};

// Configuración de la interfaz TFT
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define TRACK_WIDTH SCREEN_WIDTH / 8
#define BUTTON_HEIGHT 30
#define BUTTON_WIDTH TRACK_WIDTH - 8
#define BUTTON_SPACING 5
#define HEADER_HEIGHT 30
#define METERS_HEIGHT 50
#define FOOTER_HEIGHT 20
#define MAINAREA_HEIGHT (200 - HEADER_HEIGHT) // 170 px de alto

// Nombres de las pistas
const char* trackNames[8] = {
  "Voz", "Guitarra", "Bajo", "Bateria", "Teclado", "Coro", "FX", "Master"
};

// Estados de los botones en TFT (para visualización)
bool muteStates[8] = {false};
bool soloStates[8] = {false};
bool recStates[8] = {false};

// Colores de la interfaz
#define DARK_BLUE tft.color565(0, 0, 80)// Azul muy oscuro
#define TFT_BG_COLOR TFT_BLACK
#define TFT_TEXT_COLOR TFT_WHITE
#define TFT_BUTTON_COLOR TFT_DARKGREY
#define TFT_REC_COLOR TFT_RED
#define TFT_SOLO_COLOR TFT_ORANGE
#define TFT_MUTE_COLOR TFT_RED
#define TFT_BUTTON_TEXT TFT_WHITE
#define TFT_HEADER_COLOR DARK_BLUE
#define TFT_FOOTER_COLOR TFT_DARKGREY

// Niveles VU para cada pista (0.0 a 1.0)
float vuLevels[8] = {0.0};
unsigned long lastVUUpdate = 0;

unsigned long songMillis = 0;    // 1min 23s → para ejemplo
int bar = 1, beat = 1, subBeat = 1, tick = 00;
float logicTempo = 120.0f;        // 120 BPM para simular

const int TICKS_PER_BEAT = 960; // 960 ticks MIDI por beat (negra)
const int BEATS_PER_BAR = 4;    // 4/4
const int SUBBEATS_PER_BEAT = 4; // (opcional, para corcheas, semicorcheas...)

float secondsPerBeat = 60.0 / logicTempo; // cuánto dura un beat real
int midiTick = 0; // ticks progresivos




// ====================================================================
// Funciones clave para optimización
// ====================================================================

// Aplicar brillo a un color RGB
uint32_t applyBrightness(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
  
  return (static_cast<uint32_t>(r) << 16) | 
         (static_cast<uint32_t>(g) << 8) | 
         static_cast<uint32_t>(b);
}

// Actualizar LEDs con manejo eficiente del parpadeo
void updateLeds() {
  unsigned long currentTime = millis();
  
  // Manejo del parpadeo solo para botones de mute ACTIVOS
  if (currentTime - previousBlinkTime >= blinkInterval) {
    previousBlinkTime = currentTime;
    blinkState = !blinkState;
    
    // Actualizar solo si hay botones mute activos
    bool muteActive = false;
    for (uint8_t i = 0; i < X_DIM; i++) {
      uint8_t idx = 2 * X_DIM + i;
      if (ledState[idx]) {
        muteActive = true;
        trellis.setPixelColor(
          idx, 
          blinkState ? applyBrightness(baseColors[2]) : 0
        );
      }
    }
    
    if (muteActive) {
      trellis.show();
    }
  }
}

// Dibujar botón optimizado en sprite
void drawButton(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                const char* label, bool active, uint16_t activeColor) {
  // Dibujar fondo
  uint16_t btnColor = active ? activeColor : TFT_BUTTON_COLOR;
  sprite.fillRoundRect(x, y, w, h, 5, btnColor);
  
  // Dibujar borde
  //sprite.drawRoundRect(x, y, w, h, 5, TFT_BLACK);
  
  // Dibujar texto
  sprite.setTextColor(TFT_BUTTON_TEXT, btnColor);
  sprite.setTextSize(1);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString(label, x + w/2, y + h/2 - 0);
}

// Función para dibujar medidores VU estilo profesional con segmentos VERTICALES
void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level) {
    const int numSegments = 12;  // Duplicamos segmentos para mejor resolución vertical
    const int padding = 1;
    const int segmentHeight = (h - padding * (numSegments - 1)) / numSegments;
    const int segmentWidth = w - 4; // Ancho de los segmentos
    
    // Calcular segmentos activos basado en el nivel (0.0 a 1.0)
    int activeSegments = round(level * numSegments);
    
    // Dibujar desde abajo hacia arriba
    for (int i = 0; i < numSegments; i++) {
        uint16_t segY = y + h - (i + 1) * (segmentHeight + padding); // Posición desde abajo
        
        // Seleccionar color según el segmento (verde/amarillo/rojo)
        uint16_t color;
        if (i < 8) {         // 0-15: Verde
            color = TFT_GREEN;
            if (i >= activeSegments) color = 0x2104; // Verde oscuro inactivo
        } else if (i < 10) {  // Amarillo
            color = TFT_YELLOW;
            if (i >= activeSegments) color = 0x4228; // Amarillo oscuro
        } else {              // Rojo
            color = TFT_RED;
            if (i >= activeSegments) color = 0x4800; // Rojo oscuro
        }
        
        // Dibujar segmento con efecto 3D
        sprite.fillRect(x, segY, segmentWidth, segmentHeight, color);
        
        // Efectos de iluminación solo en segmentos activos
        if (i < activeSegments) {
            // Resaltado superior (efecto de iluminación)
            sprite.drawFastHLine(x, segY, segmentWidth, tft.color565(160, 160, 160));
            // Sombra inferior (efecto 3D)
            //sprite.drawFastHLine(x, segY + segmentHeight - 1, segmentWidth, tft.color565(40, 40, 40));
        }
    }
    
}

// Dibujar interfaz completa usando sprites
void drawInterface() {
  // Limpiar sprite principal
  mainArea.fillSprite(TFT_BG_COLOR);
  
  // Dibujar separadores verticales
  for (int i = 1; i < 8; i++) {
    mainArea.drawFastVLine(i * TRACK_WIDTH, 0, SCREEN_HEIGHT, TFT_DARKGREY);
  }
  
  
  // Dibujar botones y nombres de pista
  for (int track = 0; track < 8; track++) {
    uint16_t x = track * TRACK_WIDTH + 4;
    
    // Botón REC
    drawButton(
      mainArea, 
      x, 
      5, 
      BUTTON_WIDTH, 
      BUTTON_HEIGHT, 
      "REC", 
      recStates[track], 
      TFT_REC_COLOR
    );
    
    // Botón SOLO
    drawButton(
      mainArea, 
      x, 
      5 + BUTTON_HEIGHT + BUTTON_SPACING, 
      BUTTON_WIDTH, 
      BUTTON_HEIGHT, 
      "SOLO", 
      soloStates[track], 
      TFT_SOLO_COLOR
    );
    
    // Botón MUTE
    drawButton(
      mainArea, 
      x, 
      5 + 2*(BUTTON_HEIGHT + BUTTON_SPACING), 
      BUTTON_WIDTH, 
      BUTTON_HEIGHT, 
      "MUTE", 
      muteStates[track], 
      TFT_MUTE_COLOR
    );
    
    // Nombre de pista
    mainArea.setTextColor(TFT_TEXT_COLOR, TFT_BG_COLOR);
    mainArea.setTextSize(1);
    mainArea.setTextDatum(TC_DATUM);
    int centerX = track * TRACK_WIDTH + TRACK_WIDTH / 2;
    int labelY = 5 + 3*(BUTTON_HEIGHT + BUTTON_SPACING) + 5;
    mainArea.drawString(trackNames[track], centerX, labelY);
    
    // Si los medidores deben estar desplazados una pista a la derecha:
    //const uint16_t meterX = (track) * TRACK_WIDTH + TRACK_WIDTH / 2 - 25;
    
  }
  
  // Dibujar pie de página
  mainArea.fillRect(0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, FOOTER_HEIGHT, TFT_FOOTER_COLOR);
  mainArea.setTextColor(TFT_LIGHTGREY, TFT_FOOTER_COLOR);
  mainArea.setTextSize(1);
  mainArea.setTextDatum(BC_DATUM);
  mainArea.drawString("V1.0 - iMAKIE CONTROL", SCREEN_WIDTH/2, SCREEN_HEIGHT - 5);
  
  // Dibujar sprite completo en pantalla
  mainArea.pushSprite(0, HEADER_HEIGHT); // Esto lo sitúa en Y = 30

  Serial.println("****************");
  Serial.println("Renuevo pantalla");
  Serial.println("****************");
}



void drawVUMeters() {
    vuSprite.fillSprite(TFT_BG_COLOR);
    for (int track = 0; track < 8; track++) {
        uint16_t baseX = track * 60 + 7;   // Ajustado a zona VU
        drawMeter(vuSprite, baseX, 5,50, 120, vuLevels[track]);
    }

    // Dibuja los separadores de columnas SÓLO SOBRE el área vumeter
    for (int i = 1; i < 8; i++) {
        vuSprite.drawFastVLine(i * TRACK_WIDTH, 0, 125, TFT_DARKGREY);
    }
    // Push de solo la zona VU (ejemplo bajo la cabecera, el Y adecuado)
    vuSprite.pushSprite(0, HEADER_HEIGHT + 140);
}


// Actualizar interfaz TFT basado en estado físico
void updateTFTfromNeoTrellis() {
  // Rec (Fila 0)
  for (int i = 0; i < 8; i++) {
    recStates[i] = ledState[i];
  }
  
  // Solo (Fila 1)
  for (int i = 0; i < 8; i++) {
    soloStates[i] = ledState[8 + i];
  }
  
  // Mute (Fila 2)
  for (int i = 0; i < 8; i++) {
    muteStates[i] = ledState[16 + i];
  }
  
  // Redibujar interfaz completa
  drawInterface();
}

// Callback para eventos de teclas
TrellisCallback blink(keyEvent evt) {
  uint8_t keyNum = evt.bit.NUM;
  uint8_t row = keyNum / X_DIM;
  uint8_t col = keyNum % X_DIM;

  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {

    // --- PROTECCIÓN SOLO / MUTE ---
    if (row == 2) { // Intento de MUTE
      // Si esta pista está en SOLO, ignorar la acción MUTE
      if (ledState[1 * X_DIM + col]) { // Fila 1 = SOLO
        //Serial.printf("No puedes mutear la pista %d porque está en SOLO\n", col + 1);
        return 0;
      }
    }

    // El resto igual
    ledState[keyNum] = !ledState[keyNum];

    // Si se activa SOLO, forzar MUTE OFF en la misma pista
    if (row == 1 && ledState[keyNum]) { // Fila SOLO, activando
      if (ledState[2 * X_DIM + col]) {
        ledState[2 * X_DIM + col] = false;                 // Apagar ledState de MUTE en esta pista
        trellis.setPixelColor(2 * X_DIM + col, 0);         // Apagar LED físico de MUTE
      }
    }

    // Manejar SELECT exclusivo (fila 3)
    if (row == 3) {
      for (uint8_t i = 0; i < X_DIM; i++) {
        uint8_t idx = row * X_DIM + i;
        if (idx != keyNum) {
          ledState[idx] = false;
          trellis.setPixelColor(idx, 0);
        }
      }
    }

    // Actualizar LED físico
    if (ledState[keyNum]) {
      trellis.setPixelColor(keyNum, applyBrightness(baseColors[row]));
    } else {
      trellis.setPixelColor(keyNum, 0);
    }
    trellis.show();

    // Actualizar TFT
    updateTFTfromNeoTrellis();
  }
  return 0;
}

// Leer estado del joystick eficientemente
void leerJoystick() {
  static uint32_t lastReadTime = 0;
  if (millis() - lastReadTime > 100) { // Leer cada 100ms
    int estadoBoton = digitalRead(pinBoton);
    int valorX = analogRead(pinX);
    int valorY = analogRead(pinY);

    Serial.print("[");
    Serial.print(millis());
    Serial.print(" ms] Joystick X: ");
    Serial.print(valorX);
    Serial.print(" | Y: ");
    Serial.print(valorY);
    Serial.print(" | Botón: ");
    Serial.println(estadoBoton == LOW ? "PULSADO" : "SUELTO");

    lastReadTime = millis();
  }
}

void drawHeaderSprite() {
  header.fillSprite(TFT_HEADER_COLOR);

  // Crear las cadenas
  char timeStr[16];
  unsigned long total = songMillis / 10;
  unsigned long cent = total % 100;
  unsigned long sec = (songMillis / 1000) % 60;
  unsigned long min = (songMillis / 60000) % 60;
  unsigned long hours = (songMillis / 3600000);
  snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu.%02lu", hours, min, sec, cent);

  char compasStr[20];
  snprintf(compasStr, sizeof(compasStr), "%d %d %d %03d", bar, beat, subBeat, tick);

  // Tiempo a la izquierda
  header.setTextColor(TFT_BLUE, TFT_HEADER_COLOR);
  header.setTextSize(2);
  header.setTextDatum(ML_DATUM);
  header.drawString(timeStr, 10, HEADER_HEIGHT/2);

  // Compás/Tempo a la derecha
  header.setTextColor(TFT_BLUE, TFT_HEADER_COLOR);
  header.setTextDatum(MR_DATUM);
  header.drawString(compasStr, header.width() - 10, HEADER_HEIGHT/2);

  header.pushSprite(0, 0);
}

void updateVUMeters() {
    static unsigned long lastVUUpdate = 0;
    unsigned long currentTime = millis();
    static unsigned long lastTimeUpdate = 0;
    static float tickAccumulator = 0;
    static int TICKS_PER_BEAT = 960;
    static int BEATS_PER_BAR = 4;
    static int SUBBEATS_PER_BEAT = 4; // Para ejemplo, corcheas o semicorcheas

    // Actualizar VU meters cada 50ms
    if (currentTime - lastVUUpdate > 50) {
        lastVUUpdate = currentTime;
        for (int i = 0; i < 8; i++) {
            vuLevels[i] = (float)random(0, 1001) / 1000.0;
            Serial.printf("[%lu ms] Pista %d: %.2f%%\n", millis(), i + 1, vuLevels[i] * 100);
        }
    }

    // --- Simulación de reloj/cursor lógico ---
    if (lastTimeUpdate == 0) lastTimeUpdate = currentTime; // Inicializar el primer ciclo
    float deltaSec = (currentTime - lastTimeUpdate) / 1000.0f;
    lastTimeUpdate = currentTime;

    songMillis += (unsigned long)(deltaSec * 1000.0f); // Suma el tiempo real transcurrido

    // Calcula los ticks que avanzan: (ticks/beat) * (beats/seg) * (segundos transcurridos)
    float ticksPerSecond = logicTempo * TICKS_PER_BEAT / 60.0f;
    float ticksToAdvance = deltaSec * ticksPerSecond;
    tickAccumulator += ticksToAdvance;

    // Actualiza ticks MIDI y avanza posición del compás
    while (tickAccumulator >= 1.0f) {
        tick++;
        tickAccumulator -= 1.0f;

        // Avanza subBeat, beat, bar según formato estándar MIDI/Logic
        if (tick > TICKS_PER_BEAT) {
            tick = 1;
            subBeat++;
            if (subBeat > SUBBEATS_PER_BEAT) {
                subBeat = 1;
                beat++;
                if (beat > BEATS_PER_BAR) {
                    beat = 1;
                    bar++;
                }
            }
        }
    }

   
}


void setup() {
  Serial.begin(115200);
  
  // Inicializar TFT
  tft.init();
  tft.setRotation(1); // Orientación horizontal
  tft.fillScreen(TFT_BG_COLOR);
  
  // Configurar pin de luz de fondo
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Configurar sprite principal
  mainArea.setColorDepth(8);
  mainArea.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);

  // header
  header.setColorDepth(8);
  header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT); // Anchura completa, altura deseada

  //VUMeter
  vuSprite.setColorDepth(8);
  vuSprite.createSprite(8 * 60, 130); // 8 pistas x 60 px ancho, altura 130 px (ajusta según tu layout)
  
  // Dibujar interfaz inicial
  drawInterface();


  // En setup y cada vez que quieras actualizar el tempo en pantalla:
  drawHeaderSprite();
  
  // Inicializar NeoTrellis
  if (!trellis.begin()) {
    Serial.println("Error iniciando NeoTrellis");
    while(1);
  }
  
  // Configurar botones NeoTrellis
  for (int i = 0; i < X_DIM * Y_DIM; i++) {
    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    trellis.registerCallback(i, blink);
  }
  
  // Configurar joystick
  pinMode(pinBoton, INPUT_PULLUP);
  analogReadResolution(12); // Máxima resolución ADC
  
  // Iniciar temporizador de parpadeo
  previousBlinkTime = millis();

  // Apaga todos los LEDs al iniciar
  for (int i = 0; i < X_DIM * Y_DIM; i++) trellis.setPixelColor(i, 0);
  trellis.show();
  
  Serial.println("Sistema iniciado");
}

void loop() {
  trellis.read();       // Leer estado de botones NeoTrellis
  updateLeds();         // Actualizar LEDs con parpadeo (si es necesario)


  // Refresco rápido SOLO de los vúmetros (por ejemplo, 30 fps)
  static unsigned long lastVURender = 0;
  if (millis() - lastVURender > 33) {
      updateVUMeters();   // Aquí actualizas valores vuLevels[] con nuevos random o del audio real
      drawVUMeters();     // Nueva función: solo dibuja vúmetros y hace pushSprite en ese rectángulo
      lastVURender = millis();
  }
  drawHeaderSprite();
  leerJoystick();       // Leer estado del joystick
  
  delay(10);            // Espera breve para reducir carga de CPU
}