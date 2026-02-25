#include <Arduino.h>

// --- UART y Protocolo ---
#define PICO_TX_PIN   1 // Conectado a RX de la Pico (GP17)
#define PICO_RX_PIN   2 // Conectado a TX de la Pico (GP16)
#define UART_BAUDRATE 921600

// --- Protocolo UART (debe coincidir con la Pico) ---
#define START_BYTE         0xAA
// Comandos de Botones
#define CMD_REC_ARM        0x10
#define CMD_SOLO           0x11
#define CMD_MUTE           0x12
#define CMD_SELECT         0x13
// Comandos de Datos
#define CMD_TRACK_NAME     0x20
#define CMD_VU_METER       0x30
// Comandos de Sistema
#define CMD_RAW_SYSEX      0x50
#define CMD_PICO_HEARTBEAT 0xFE

// --- Almacenamiento de Estado ---
String trackNames[8]; // Array para guardar los nombres de las pistas

// --- Parser UART ---
enum class ParserState { WAITING_START, WAITING_CMD, WAITING_LEN, READING_DATA, READING_CHECKSUM };
ParserState currentState = ParserState::WAITING_START;
byte packet_cmd;
byte packet_len;
byte packet_data[256]; // Buffer grande para paquetes SysEx
byte packet_dataindex;
byte packet_checksum;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial2.begin(UART_BAUDRATE, SERIAL_8N1, PICO_RX_PIN, PICO_TX_PIN);
  Serial2.setRxBufferSize(2048); // Buffer de hardware grande para no perder datos
  Serial.println("\n--- iMakie - Receptor Final Completo ---");
}

// --- Decodificador de SysEx Crudo ---
void processMackieSysEx(byte* payload, byte len) {
  byte mcu_id[] = {0x00, 0x00, 0x66, 0x14};
  if (len < 5 || memcmp(payload, mcu_id, 4) != 0) return;

  byte command_byte = payload[4];

  if (command_byte == 0x12) { // Comando de Nombres de Pista
    if (len >= 6) {
      byte offset = payload[5];
      byte* text_data = &payload[6];
      int text_len = len - 6;
      for (int i = 0; i < text_len / 7; i++) {
        byte track_index = offset + i;
        if (track_index < 8) {
          char name_buffer[8];
          memcpy(name_buffer, &text_data[i * 7], 7);
          name_buffer[7] = '\0';
          // Limpiar espacios al final
          for (int j=6; j>=0; j--) { if (name_buffer[j]==' ') name_buffer[j]='\0'; else break; }
          if (strlen(name_buffer) > 0) {
            trackNames[track_index] = String(name_buffer);
            Serial.printf("[NOMBRE] Pista %d -> '%s'\n", track_index + 1, name_buffer);
          }
        }
      }
    }
  } else if (command_byte == 0x6F && len >= 14 && payload[5] == 0x20) { // Vúmetros
      byte* meter_data = &payload[6];
      for (int i = 0; i < 8; i++) {
          byte vu_value = meter_data[i] >> 1;
          if (vu_value > 0) {
              String name = trackNames[i];
              Serial.printf("[VÚMETRO] Pista %d (%s) -> Nivel: %d\n", i + 1, name.c_str(), vu_value);
          }
      }
  }
}

// --- Procesador principal de paquetes UART ---
void processFrame() {
  // Validar checksum (calculado sobre comando y payload)
  byte calculated_checksum = packet_cmd;
  for (int i = 0; i < packet_len; i++) { calculated_checksum += packet_data[i]; }
  calculated_checksum &= 0xFF;

  if (calculated_checksum != packet_checksum) {
    Serial.println("[ERROR] Checksum incorrecto.");
    return;
  }

  // --- PROCESAR EL COMANDO DEL FRAME ---
  switch (packet_cmd) {
    case CMD_RAW_SYSEX:
      processMackieSysEx(packet_data, packet_len);
      break;

    case CMD_REC_ARM:
    case CMD_SOLO:
    case CMD_MUTE:
    case CMD_SELECT: {
      if (packet_len == 2) {
        const char* cmd_name = "???";
        if      (packet_cmd == CMD_REC_ARM) cmd_name = "REC";
        else if (packet_cmd == CMD_SOLO)    cmd_name = "SOLO";
        else if (packet_cmd == CMD_MUTE)    cmd_name = "MUTE";
        else if (packet_cmd == CMD_SELECT)  cmd_name = "SELECT";
        
        byte track_index = packet_data[0];
        bool state = packet_data[1] ? true : false;
        String name = (track_index < 8) ? trackNames[track_index] : "???";

        Serial.printf("[BOTÓN] Pista %d (%s) - %s: %s\n", track_index + 1, name.c_str(), cmd_name, state ? "ON" : "OFF");
      }
      break;
    }
      
    case CMD_PICO_HEARTBEAT:
      Serial.println("- Pico Heartbeat OK");
      break;

    default:
        Serial.printf("[WARN] Comando UART desconocido: 0x%02X\n", packet_cmd);
        break;
  }
}

// --- Loop Principal con Parser UART robusto ---
void loop() {
  while (Serial2.available() > 0) {
    byte b = Serial2.read();
    switch (currentState) {
      case ParserState::WAITING_START:
        if (b == START_BYTE) currentState = ParserState::WAITING_CMD;
        break;
      case ParserState::WAITING_CMD:
        // Validar si el comando es uno de los que esperamos
        if ((b >= CMD_REC_ARM && b <= CMD_SELECT) || b == CMD_RAW_SYSEX || b == CMD_PICO_HEARTBEAT) {
            packet_cmd = b;
            currentState = ParserState::WAITING_LEN;
        } else {
            currentState = ParserState::WAITING_START; // Comando no válido, reiniciar
        }
        break;
      case ParserState::WAITING_LEN:
        packet_len = b;
        packet_dataindex = 0;
        if (packet_len >= sizeof(packet_data)) {
            Serial.println("[ERROR] Paquete demasiado largo, descartando.");
            currentState = ParserState::WAITING_START;
        } else {
            currentState = (packet_len > 0) ? ParserState::READING_DATA : ParserState::READING_CHECKSUM;
        }
        break;
      case ParserState::READING_DATA:
        packet_data[packet_dataindex++] = b;
        if (packet_dataindex >= packet_len) currentState = ParserState::READING_CHECKSUM;
        break;
      case ParserState::READING_CHECKSUM:
        packet_checksum = b;
        processFrame();
        currentState = ParserState::WAITING_START;
        break;
    }
  }
}