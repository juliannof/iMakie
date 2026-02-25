// ====================================================================
// --- REEMPLAZAR ESTE BLOQUE COMPLETO EN LA PARTE SUPERIOR DE TU CÓDIGO ---
// ====================================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Adafruit_NeoTrellis.h"

// --- UART y Protocolo ---
#define PICO_TX_PIN   1
#define PICO_RX_PIN   2
#define UART_BAUDRATE 921600

#define START_FRAME 0xAA // El byte que la Pico envía para indicar inicio de paquete
#define END_FRAME   0xBB // El byte que la Pico envía para indicar fin de paquete

// --- Parser UART (para reensamblar frames de la Pico) ---
byte uart_buffer[256];
int uart_idx = 0;
bool in_frame = false;

// --- Parser MIDI (para decodificar el contenido del frame UART) ---
byte midi_buffer[256];
int midi_idx = 0;
bool in_sysex = false;
byte last_status_byte = 0; // Para manejar "Running Status" de MIDI

// Protocolo UART para enviar MIDI crudo del ESP32 a la Pico
#define ESP_TO_USB_START 0xBB // Byte de inicio para esta dirección
#define FRAME_END        0xCC // Byte de fin


#define START_BYTE         0xAA
#define CMD_REC_ARM        0x10
#define CMD_SOLO           0x11
#define CMD_MUTE           0x12
#define CMD_SELECT         0x13
#define CMD_RAW_SYSEX      0x50
#define CMD_PICO_HEARTBEAT 0xFE

// --- TFT y Sprites ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite header = TFT_eSprite(&tft);
TFT_eSprite mainArea = TFT_eSprite(&tft);
TFT_eSprite vuSprite = TFT_eSprite(&tft);

// --- Hardware y Colores ---
const int pinBoton = 4, pinX = 3, pinY = 5;
#define Y_DIM 4
#define X_DIM 8
Adafruit_NeoTrellis t_array[Y_DIM/4][X_DIM/4] = {{ Adafruit_NeoTrellis(0x2E), Adafruit_NeoTrellis(0x2F) }};
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis*)t_array, Y_DIM/4, X_DIM/4);
bool ledState[X_DIM*Y_DIM] = {false};
const uint8_t brightness = 35;
bool blinkState = true;
unsigned long previousBlinkTime = 0;
const unsigned long blinkInterval = 250;
const uint32_t baseColors[4] = {0xFF0000, 0xFFFF00, 0xFF0000, 0xFFFFFF};

// --- Configuración de la Interfaz ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define TRACK_WIDTH (SCREEN_WIDTH / 8)
#define BUTTON_HEIGHT 30
#define BUTTON_WIDTH (TRACK_WIDTH - 8)
#define BUTTON_SPACING 5
#define HEADER_HEIGHT 30
#define VU_METER_AREA_Y (HEADER_HEIGHT + 140)
#define VU_METER_HEIGHT 150
#define TFT_BG_COLOR TFT_BLACK
#define TFT_TEXT_COLOR TFT_WHITE
#define TFT_BUTTON_COLOR TFT_DARKGREY
#define TFT_REC_COLOR TFT_RED
#define TFT_SOLO_COLOR TFT_ORANGE
#define TFT_MUTE_COLOR TFT_RED
#define TFT_BUTTON_TEXT TFT_WHITE
#define TFT_HEADER_COLOR tft.color565(0, 0, 80)
#define TFT_FOOTER_COLOR TFT_DARKGREY
#define TFT_SELECT_BG_COLOR tft.color565(70, 70, 170) // Un gris azulado muy oscuro para el fondo


// --- Variables de Estado de la Interfaz ---
String trackNames[8];
bool recStates[8] = {false};
bool soloStates[8] = {false};
bool muteStates[8] = {false};
bool selectStates[8] = {false};
float vuLevels[8] = {0.0f};
bool needsRedraw = true;
String redrawReason = "Initial Draw";
unsigned long redrawCounter = 0;

// --- Estado del Transporte (ejemplo) ---
unsigned long songMillis = 0;
int bar = 1, beat = 1, subBeat = 1, tick = 0;
float logicTempo = 120.0f;

// --- Parser UART ---
enum class ParserState { WAITING_START, WAITING_CMD, WAITING_LEN, READING_DATA, READING_CHECKSUM };
ParserState currentState = ParserState::WAITING_START;
byte packet_cmd, packet_len, packet_dataindex, packet_checksum;
byte packet_data[256];

// ====================================================================
// --- 2. LÓGICA DE COMUNICACIÓN CON LA PICO ---
// ====================================================================



// ====================================================================
// --- 3. FUNCIONES DE DIBUJO Y LÓGICA DE HARDWARE ---
// ====================================================================

uint32_t applyBrightness(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  r = (r * brightness) / 255; g = (g * brightness) / 255; b = (b * brightness) / 255;
  return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

void updateLeds() {
  // Flag para saber si hemos hecho algún cambio y necesitamos llamar a trellis.show()
  static bool ledsChanged = false;
  unsigned long currentTime = millis();

  // --- PARTE A: ACTUALIZAR EL ESTADO DE LOS LEDS NO PARPADEANTES ---
  // Recorremos las 8 pistas para los botones REC, SOLO y SELECT
  for (uint8_t i = 0; i < 8; i++) {
    // Sincronizar el estado del array 'ledState' con el estado real
    ledState[i] = recStates[i];
    ledState[i + X_DIM] = soloStates[i];
    ledState[i + 3 * X_DIM] = selectStates[i]; // Asumimos que la fila 3 es SELECT

    // Asignar colores
    trellis.setPixelColor(i,                 recStates[i]    ? applyBrightness(baseColors[0]) : 0);
    trellis.setPixelColor(i + X_DIM,         soloStates[i]   ? applyBrightness(baseColors[1]) : 0);
    trellis.setPixelColor(i + 3 * X_DIM,     selectStates[i] ? applyBrightness(baseColors[3]) : 0);
  }
  ledsChanged = true; // Hemos actualizado estos LEDs

  // --- PARTE B: LÓGICA DE PARPADEO PARA LOS LEDS DE MUTE ---
  if (currentTime - previousBlinkTime >= blinkInterval) {
    previousBlinkTime = currentTime;
    blinkState = !blinkState;
    
    // Recorremos las 8 pistas para los botones MUTE
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t mute_idx = i + 2 * X_DIM;
        
        // Sincronizar el estado
        ledState[mute_idx] = muteStates[i];

        if (muteStates[i]) {
            // Si el MUTE está activo, aplicamos el estado de parpadeo
            trellis.setPixelColor(mute_idx, blinkState ? applyBrightness(baseColors[2]) : 0);
            ledsChanged = true;
        } else {
            // Si no, nos aseguramos de que esté apagado
            trellis.setPixelColor(mute_idx, 0);
        }
    }
  }
  
  // Si se ha realizado algún cambio en los LEDs, actualizamos el hardware
  if (ledsChanged) {
      trellis.show();
      ledsChanged = false;
  }
}
// ====================================================================
// --- ASEGÚRATE DE TENER ESTA FUNCIÓN DE ENVÍO ANTES ---
// ====================================================================
void sendToPico(byte command, byte track_index, byte state) {
  byte payload[] = {track_index, state};
  byte len = sizeof(payload);
  byte checksum = command; // Simplificado para mayor claridad

  Serial2.write(START_BYTE);
  Serial2.write(command);
  Serial2.write(len);
  Serial2.write(payload, len);
  Serial2.write(checksum & 0xFF);
  
  Serial.printf(">>> ENVIADO A PICO: Cmd=0x%02X, Pista=%d, Estado=%d\n", command, track_index + 1, state);
}


// ====================================================================
// --- REEMPLAZA TU FUNCIÓN blink() CON ESTA ---
// ====================================================================

TrellisCallback blink(keyEvent evt) {
  uint8_t keyNum = evt.bit.NUM;
  uint8_t row = keyNum / X_DIM;
  uint8_t col = keyNum % X_DIM;

  Serial.printf("Leyendo NEOTRELLIS AHOra");

  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    Serial.printf("[NEOTRELLIS] Pulsada tecla en Fila: %d, Columna: %d\n", row, col);

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
    //updateTFTfromNeoTrellis();
  }
  return 0;
}


void leerJoystick() { /* Tu código aquí */ }

void drawButton(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor) {
    uint16_t btnColor = active ? activeColor : TFT_BUTTON_COLOR;
    sprite.fillRoundRect(x, y, w, h, 5, btnColor);
    sprite.setTextColor(TFT_BUTTON_TEXT, btnColor);
    sprite.setTextSize(1);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label, x + w / 2, y + h / 2);
}

void drawMeter(TFT_eSprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float level) { /* Tu código aquí */ }
void drawHeaderSprite() { /* Tu código aquí */ }

void drawVUMeters() {
    vuSprite.fillSprite(TFT_BG_COLOR);
    for (int track = 0; track < 8; track++) {
        uint16_t baseX = track * 60 + 7;
        drawMeter(vuSprite, baseX, 5, 50, 120, vuLevels[track]);
    }
    for (int i = 1; i < 8; i++) vuSprite.drawFastVLine(i * TRACK_WIDTH, 0, 125, TFT_DARKGREY);
    vuSprite.pushSprite(0, HEADER_HEIGHT + 140);
}


void drawMainArea() {
  // Optimización: si la bandera 'needsRedraw' es falsa, la función no hace nada.
  if (!needsRedraw) return;

  // --- COMIENZA EL BLOQUE DE DIAGNÓSTICO ---
  redrawCounter++;
  Serial.println("=============================================");
  Serial.printf(">>> INICIO REDIBUJADO #%lu\n", redrawCounter);
  Serial.printf(">>> Motivo: %s\n", redrawReason.c_str());
  // ------------------------------------------

  // Preparar el sprite para dibujar
  mainArea.fillSprite(TFT_BG_COLOR); // Limpiar con el color de fondo por defecto

  // -- DIBUJAR FONDOS DE COLUMNA Y ELEMENTOS --
  for (int track = 0; track < 8; track++) {
    uint16_t col_x = track * TRACK_WIDTH;
    
    // --- LÓGICA DE RESALTADO DE PISTA SELECCIONADA ---
    // Si esta es la pista seleccionada, dibuja un rectángulo de fondo más claro
    if (selectStates[track]) {
        mainArea.fillRect(col_x, 0, TRACK_WIDTH, mainArea.height(), TFT_SELECT_BG_COLOR);
        Serial.printf("    -> Resaltando fondo de Pista %d\n", track + 1);
    }
    // ----------------------------------------------------

    // Dibujar botones encima del fondo
    uint16_t button_x = col_x + 4;
    drawButton(mainArea, button_x, 5, BUTTON_WIDTH, BUTTON_HEIGHT, "REC", recStates[track], TFT_REC_COLOR);
    drawButton(mainArea, button_x, 5 + BUTTON_HEIGHT + BUTTON_SPACING, BUTTON_WIDTH, BUTTON_HEIGHT, "SOLO", soloStates[track], TFT_SOLO_COLOR);
    drawButton(mainArea, button_x, 5 + 2 * (BUTTON_HEIGHT + BUTTON_SPACING), BUTTON_WIDTH, BUTTON_HEIGHT, "MUTE", muteStates[track], TFT_MUTE_COLOR);
    
    // Dibujar Nombre de pista
    // El color de fondo del texto debe coincidir con el fondo de la celda
    uint16_t textBgColor = selectStates[track] ? TFT_SELECT_BG_COLOR : TFT_BG_COLOR;
    mainArea.setTextColor(TFT_TEXT_COLOR, textBgColor);
    mainArea.setTextSize(1);
    mainArea.setTextDatum(MC_DATUM);
    mainArea.drawString(trackNames[track], track * TRACK_WIDTH + TRACK_WIDTH / 2, 5 + 3 * (BUTTON_HEIGHT + BUTTON_SPACING) + 10);
  }

  // Dibujar los separadores verticales DESPUÉS de los fondos para que se vean por encima
  for (int i = 1; i < 8; i++) {
    mainArea.drawFastVLine(i * TRACK_WIDTH, 0, mainArea.height(), TFT_DARKGREY);
  }
  
  // Volcar el sprite a la pantalla física
  mainArea.pushSprite(0, HEADER_HEIGHT);
  
  // Reseteamos la bandera para evitar redibujados innecesarios
  needsRedraw = false;
  
  // --- FIN DEL BLOQUE DE DIAGNÓSTICO ---
  Serial.println("<<< FIN REDIBUJADO");
  Serial.println("=============================================");
  // --------------------------------------
}

// ====================================================================
// --- 3. LÓGICA DE PROCESAMIENTO ---
// ====================================================================



void processMackieSysEx(byte* payload, byte len) {
  // `payload` contiene los datos crudos del MIDI (ej: 00 00 66 14 20...)
  
  // 1. Verificar la cabecera de Mackie Control
  byte mcu_id[] = {0x00, 0x00, 0x66, 0x14};
  if (len < 5 || memcmp(payload, mcu_id, 4) != 0) {
    return; // No es un mensaje Mackie
  }
  
  // 2. Extraer el byte de comando Mackie
  byte command_byte = payload[4];

  // 3. Procesar según el comando
  switch (command_byte) {
    
    // ----- CASO PARA NOMBRES DE PISTA (CMD 0x12) -----
    case 0x12: {
      if (len >= 6) {
        byte offset = payload[5];
        for (int i = 0; i < (len - 6) / 7; i++) {
          byte track_index = offset + i;
          if (track_index < 8) {
            char name_buf[8];
            memcpy(name_buf, &payload[6 + i * 7], 7);
            name_buf[7] = '\0';
            for (int j = 6; j >= 0; j--) { if (name_buf[j] == ' ') name_buf[j] = '\0'; else break; }
            if (trackNames[track_index] != name_buf) {
              trackNames[track_index] = String(name_buf);
              needsRedraw = true;
              redrawReason = "Nuevos nombres de pista";
            }
          }
        }
      }
      break;
    }

    // ----- ¡NUEVO! CASO PARA VÚMETROS DE CHANNEL STRIP (CMD 0x20) -----
    case 0x20: {
      // Formato: [Posición] [Valor]
      if (len == 6) { // El payload SysEx es [ID Mackie] [0x20] [Pos] [Valor]
        byte position = payload[5];
        byte value = payload[6];

        // El protocolo Mackie divide la pantalla en dos filas.
        // La fila superior, usada para vúmetros, va de la posición 0 a 55.
        if (position >= 0 && position <= 55) {
          // Cada pista ocupa 7 caracteres.
          int track_index = position / 7;
          
          if (track_index < 8) {
            // El valor del vúmetro es un código de 0 a 7. Lo convertimos a un flotante 0.0 - 1.0.
            // 0x07 es el máximo, por lo que dividimos por 7.
            float level = (float)value / 7.0f;
            
            // Actualizar solo si el nivel ha cambiado significativamente
            if (abs(vuLevels[track_index] - level) > 0.05) {
              vuLevels[track_index] = level;
              Serial.printf("[VU Strip] Pista %d -> Nivel: %.2f (Valor crudo: %d)\n", track_index + 1, level, value);
            }
          }
        }
      }
      break;
    }
  }
}

void processFrame() {
  byte calculated_checksum = packet_cmd;
  for (int i = 0; i < packet_len; i++) { calculated_checksum += packet_data[i]; }
  calculated_checksum &= 0xFF;
  if (calculated_checksum != packet_checksum) return;

  switch (packet_cmd) {
    case CMD_RAW_SYSEX:
      processMackieSysEx(packet_data, packet_len);
      break;
    case CMD_REC_ARM:  if (packet_len==2 && packet_data[0]<8 && recStates[packet_data[0]] != (bool)packet_data[1]) { recStates[packet_data[0]] = packet_data[1]; needsRedraw=true; redrawReason="Feedback REC"; } break;
    case CMD_SOLO:   if (packet_len==2 && packet_data[0]<8 && soloStates[packet_data[0]] != (bool)packet_data[1]) { soloStates[packet_data[0]] = packet_data[1]; needsRedraw=true; redrawReason="Feedback SOLO"; } break;
    case CMD_MUTE:   if (packet_len==2 && packet_data[0]<8 && muteStates[packet_data[0]] != (bool)packet_data[1]) { muteStates[packet_data[0]] = packet_data[1]; needsRedraw=true; redrawReason="Feedback MUTE"; } break;
    case CMD_SELECT: if (packet_len==2 && packet_data[0]<8 && selectStates[packet_data[0]] != (bool)packet_data[1]) { selectStates[packet_data[0]] = packet_data[1]; needsRedraw=true; redrawReason="Feedback SELECT"; } break;
    case CMD_PICO_HEARTBEAT:
      Serial.println("- Pico Heartbeat OK");
      break;
  }
}

// ====================================================================
// --- AÑADIR ESTA SECCIÓN COMPLETA ANTES DE setup() ---
// ====================================================================
// --- LÓGICA DE DECODIFICACIÓN MIDI ---

void processMackieSysEx(byte* payload, int len) {
    byte mcu_id[] = {0x00, 0x00, 0x66, 0x14};
    if (len < 5 || memcmp(payload, mcu_id, 4) != 0) return;

    byte command = payload[4];

    if (command == 0x12) { // Nombres de Pista
        byte offset = payload[5];
        for (int i = 0; i < (len - 6) / 7; i++) {
            byte track_idx = offset + i;
            if (track_idx < 8) {
                char name_buf[8]; memcpy(name_buf, &payload[6 + i*7], 7); name_buf[7] = '\0';
                for (int j=6; j>=0; j--) { if (name_buf[j]==' ') name_buf[j]='\0'; else break; }
                if (strlen(name_buf) > 0 && trackNames[track_idx] != name_buf) {
                    trackNames[track_idx] = String(name_buf);
                    Serial.printf("[NOMBRE] Pista %d -> '%s'\n", track_idx + 1, name_buf);
                    needsRedraw = true;
                }
            }
        }
    }
    // Aquí irá la lógica para vúmetros SysEx (0x6F) si es necesario.
}

void processNote(byte status, byte note, byte velocity) {
    if (note > 31) return; // Fuera del rango de nuestros botones
    int group = note / 8;
    int track_idx = note % 8;
    bool is_on = (status & 0xF0) == 0x90 && velocity > 0;
    
    Serial.printf("[BOTÓN] Feedback: Pista %d, Grp %d, Estado %d\n", track_idx+1, group, is_on);

    switch(group) {
        case 0: if(recStates[track_idx] != is_on) { recStates[track_idx] = is_on; needsRedraw = true; } break;
        case 1: if(soloStates[track_idx]!= is_on) { soloStates[track_idx] = is_on; needsRedraw = true; } break;
        case 2: if(muteStates[track_idx]!= is_on) { muteStates[track_idx] = is_on; needsRedraw = true; } break;
        case 3: if(selectStates[track_idx]!=is_on){ selectStates[track_idx] = is_on; needsRedraw = true; } break;
    }
}

void processMidiByte(byte b) {
    // Parser manual de MIDI que ensambla los mensajes a partir de bytes crudos.
    if (b == 0xF0) { in_sysex = true; midi_idx = 0; }
    else if (b == 0xF7) {
        if (in_sysex) {
            in_sysex = false;
            processMackieSysEx(midi_buffer, midi_idx);
        }
    }
    else if (in_sysex) { if (midi_idx < sizeof(midi_buffer)) midi_buffer[midi_idx++] = b; }
    else if (b & 0x80) { // Es un nuevo byte de estado
        last_status_byte = b;
        midi_idx = 0;
    } else { // Es un byte de datos
        if (last_status_byte != 0) {
            midi_buffer[midi_idx++] = b;
            byte cmd_type = last_status_byte & 0xF0;
            int msg_len = 0;
            
            if (cmd_type == 0xC0 || cmd_type == 0xD0) msg_len = 2; // Program Change, Channel Pressure
            else if (cmd_type >= 0x80 && cmd_type <= 0xE0) msg_len = 3; // La mayoría son de 3 bytes
            
            if (midi_idx == msg_len - 1) { // Mensaje completo
                if (cmd_type == 0x90 || cmd_type == 0x80) { // Note On/Off
                    processNote(last_status_byte, midi_buffer[0], midi_buffer[1]);
                }
                // Aquí iría la lógica para Control Change, etc.
                
                // Para "Running Status", no reseteamos last_status_byte, solo el índice de datos.
                midi_idx = 0;
            }
        }
    }
}

// ====================================================================
// --- 4. DECODIFICACIÓN Y SETUP/LOOP ---
// ====================================================================
void setup() {
  Serial.begin(115200);
  while(!Serial);

  // Iniciar comunicación UART con la Pico
  Serial.println("\n--- iMakie - MODO CEREBRO (Receptor) ---");
  Serial2.begin(UART_BAUDRATE, SERIAL_8N1, PICO_RX_PIN, PICO_TX_PIN);
  Serial2.setRxBufferSize(4096); // Aumentar el búfer para la ráfaga de datos
  Serial.println("[SETUP] Puerto Serial2 iniciado. Esperando datos de la Pico...");
  
  // Tu código de inicialización de TFT, Trellis, etc.
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BG_COLOR);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  mainArea.createSprite(SCREEN_WIDTH, 140);
  header.createSprite(SCREEN_WIDTH, HEADER_HEIGHT);
  vuSprite.createSprite(SCREEN_WIDTH, VU_METER_HEIGHT);

  for (int i=0; i<8; i++) trackNames[i] = "Pista " + String(i+1); // Nombres por defecto

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

  needsRedraw = true;
  Serial.println("\n--- iMakie - Sistema Integrado ---");
}

void loop() {
    // PARTE 1: Reensamblar frames UART y procesar su contenido
    while (Serial2.available()) {
        byte b = Serial2.read();

        if (b == START_FRAME) {
            in_frame = true;
            uart_idx = 0;
        } else if (b == END_FRAME) {
            if (in_frame) {
                // Frame UART completo recibido.
                Serial.printf("[UART] Frame recibido (%d bytes): ", uart_idx);
                // Imprimir el contenido crudo para depurar
                for (int i = 0; i < uart_idx; i++) Serial.printf("%02X ", uart_buffer[i]);
                Serial.println();

                // Ahora procesar su contenido byte a byte con el parser MIDI
                for (int i = 0; i < uart_idx; i++) {
                    processMidiByte(uart_buffer[i]);
                }
            }
            in_frame = false;
        } else if (in_frame && uart_idx < sizeof(uart_buffer)) {
            uart_buffer[uart_idx++] = b;
        }
    }

    // PARTE 2: Actualizar la interfaz y leer hardware local
    if (needsRedraw) { 
        drawMainArea(); 
        needsRedraw = false; 
    }
    drawVUMeters();
    trellis.read();
    updateLeds(); // Ejecuta la lógica de parpadeo y actualización de LEDs

    
    delay(5);
}