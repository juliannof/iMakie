#include <Arduino.h>

// --- CONFIGURACIÓN ESCLAVO ---
const uint8_t MY_ID = 1; 
const uint8_t START_BYTE = 0xAA;
const uint8_t RESP_BYTE  = 0xBB;

#define RX_PIN 16
#define TX_PIN 15
#define ENABLE_PIN 1  
#define LED_PIN LED_BUILTIN

// Estructura que RECIBIMOS (Master -> Esclavo)
struct __attribute__((packed)) SlaveData {
  uint8_t  header;       
  uint8_t  id;           
  char     trackName[8]; 
  uint8_t  flags;        
  float    vuPeak;
  float    faderPos;
  float    vuLevel;
};

// Estructura que ENVIAMOS (Esclavo -> Master)
struct __attribute__((packed)) SlaveResponse {
  uint8_t  header;       
  uint8_t  id;           
  float    physPos;      
  uint8_t  touchState;   
  uint8_t  buttons;      
};

SlaveData receivedData;
SlaveResponse myStatus;

// Simulación de movimiento para pruebas
void simulateHardware(SlaveResponse &resp) {
  static float mockPos = 0.0;
  static bool up = true;
  
  if (up) mockPos += 0.003;
  else mockPos -= 0.003;

  if (mockPos >= 1.0) up = false;
  if (mockPos <= 0.0) up = true;

  resp.physPos = mockPos;
  resp.touchState = (mockPos > 0.7) ? 1 : 0; // Touch "virtual" al pasar de 0.7
  resp.buttons = 0x00;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // Modo escucha
  pinMode(LED_PIN, OUTPUT);

  // Serial1 con tus pines del S3
  Serial1.begin(500000, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial1.setTimeout(1);

  log_i("Esclavo S3 Online | ID: %d | RX:%d TX:%d EN:%d", MY_ID, RX_PIN, TX_PIN, ENABLE_PIN);
}

void loop() {
  // Solo procesamos si hay bytes suficientes para una trama completa
  if (Serial1.available() >= sizeof(SlaveData)) {
    if (Serial1.read() == START_BYTE) {
      if (Serial1.peek() == MY_ID) {
        
        uint32_t t_start = micros();
        
        // Cargar datos
        uint8_t* p = (uint8_t*)&receivedData;
        p[0] = START_BYTE;
        Serial1.readBytes(&p[1], sizeof(SlaveData) - 1);
        
        uint32_t t_end = micros();

        // 1. Preparar Respuesta
        simulateHardware(myStatus);
        myStatus.header = RESP_BYTE;
        myStatus.id = MY_ID;

        // 2. Respuesta RS485 (Giro de Bus)
        digitalWrite(ENABLE_PIN, HIGH);
        delayMicroseconds(10); // Tiempo para que el transceptor conmute
        
        Serial1.write((uint8_t*)&myStatus, sizeof(SlaveResponse));
        Serial1.flush(); // Esperar a que el último bit salga del cable
        
        digitalWrite(ENABLE_PIN, LOW); // Volver a escuchar

        // 3. Log de latencia interna del esclavo
        log_i("RX ID %d | Proc: %u us | Fader In: %.2f | SIM Out: %.2f", 
              receivedData.id, 
              (uint32_t)(t_end - t_start), 
              receivedData.faderPos,
              myStatus.physPos);

        // Blink de actividad
        digitalWrite(LED_PIN, HIGH);
        delayMicroseconds(200);
        digitalWrite(LED_PIN, LOW);
      }
    }
  }
}