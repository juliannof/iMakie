#include <Arduino.h>

// --- CONFIGURACIÓN ---
#define RX_PIN 16
#define TX_PIN 15
#define ENABLE_PIN 1
#define LED_PIN LED_BUILTIN 

const uint8_t START_BYTE = 0xAA;
const uint8_t RESP_BYTE  = 0xBB;
const int NUM_SLAVES = 2; // Cambia a 9 cuando los tengas todos

// Estructuras
struct __attribute__((packed)) SlaveData {
  uint8_t  header; 
  uint8_t  id; 
  char     trackName[8]; 
  uint8_t  flags; 
  float    vuPeak; 
  float    faderPos; 
  float    vuLevel;
};

struct __attribute__((packed)) SlaveResponse {
  uint8_t  header; 
  uint8_t  id; 
  float    physPos; 
  uint8_t  touchState; 
  uint8_t  buttons;      
};

// --- BASE DE DATOS DEL MASTER ---
SlaveData slaves[NUM_SLAVES + 1]; 
SlaveResponse faderStats[NUM_SLAVES + 1];
bool slaveActive[NUM_SLAVES + 1]; // Seguimiento de esclavos activos

// Variables de Control del Bus
enum ComState { SENDING, WAITING, GAP };
ComState currentState = SENDING;
uint8_t currentSlaveID = 1;
uint32_t timerRef = 0;
uint32_t lastStatsPrint = 0;

// --- ESTADÍSTICAS SIMPLES ---
struct SimpleStats {
  uint32_t packetsSent = 0;
  uint32_t responsesReceived = 0;
  uint32_t timeouts = 0;
  uint32_t lastResponseTime = 0;
  
  void print() {
    float successRate = (packetsSent > 0) ? 
      ((float)responsesReceived / packetsSent) * 100.0f : 0.0f;
    
    Serial.printf("\n📊 Estadísticas:\n");
    Serial.printf("Enviados: %u | Recibidos: %u | Timeouts: %u\n", 
                  packetsSent, responsesReceived, timeouts);
    Serial.printf("Tasa éxito: %.1f%%\n", successRate);
    Serial.printf("Tiempo última respuesta: %u ms\n", 
                  millis() - lastResponseTime);
  }
};

SimpleStats stats;

// --- FUNCIONES MEJORADAS ---
void updateBus();
void processSerialCommands();
void updateSlaveData();
void printPeriodicStats();

void setup() {
  Serial.begin(115200);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);

  Serial1.begin(500000, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Inicializar base de datos
  for(int i = 1; i <= NUM_SLAVES; i++) {
    slaves[i].header = START_BYTE;
    slaves[i].id = i;
    snprintf(slaves[i].trackName, 8, "TRK-%02d", i);
    slaves[i].vuPeak = 0.0;
    slaves[i].faderPos = 0.5; // Posición inicial 50%
    slaves[i].vuLevel = 0.0;
    slaves[i].flags = 0x01; // Fader activo
    
    slaveActive[i] = true; // Todos activos por defecto
    
    faderStats[i].id = i;
    faderStats[i].physPos = 0.0;
    faderStats[i].touchState = 0;
    faderStats[i].buttons = 0;
  }
  
  Serial.println("\n🔧 MASTER RS485 SIMPLE");
  Serial.println("Comandos: s=stats, t=test, r=reset, a=activar/desactivar");
}

void loop() {
  // 1. Mantener la comunicación viva
  updateBus();
  
  // 2. Procesar comandos por Serial
  processSerialCommands();
  
  // 3. Actualizar datos de esclavos (simulados)
  updateSlaveData();
  
  // 4. Imprimir estadísticas periódicas
  printPeriodicStats();
}

void updateBus() {
  switch (currentState) {
    case SENDING:
      if (!slaveActive[currentSlaveID]) {
        // Saltar esclavo desactivado
        timerRef = micros();
        currentState = GAP;
        break;
      }
      
      // Limpiar buffer antes de enviar
      while(Serial1.available()) Serial1.read(); 
      
      // Preparar datos actualizados para este esclavo
      slaves[currentSlaveID].vuPeak = random(0, 100) / 100.0;
      slaves[currentSlaveID].vuLevel = random(0, 100) / 100.0;
      
      // Habilitar transmisión
      digitalWrite(ENABLE_PIN, HIGH);
      delayMicroseconds(10); // Margen seguro
      
      // Enviar datos
      Serial1.write((uint8_t*)&slaves[currentSlaveID], sizeof(SlaveData));
      Serial1.flush(); 
      
      delayMicroseconds(10); // Esperar transmisión completa
      digitalWrite(ENABLE_PIN, LOW); // Volver a recepción
      
      // LED indicador
      digitalWrite(LED_PIN, HIGH);
      
      timerRef = micros();
      currentState = WAITING;
      stats.packetsSent++;
      break;

    case WAITING:
      // Apagar LED después de 50us
      if (micros() - timerRef > 50) {
        digitalWrite(LED_PIN, LOW);
      }
      
      if (Serial1.available() > 0) {
        if (Serial1.peek() == RESP_BYTE) {
          if (Serial1.available() >= sizeof(SlaveResponse)) {
            // Guardar respuesta
            Serial1.readBytes((uint8_t*)&faderStats[currentSlaveID], sizeof(SlaveResponse));
            
            // Verificar que el ID coincida
            if (faderStats[currentSlaveID].id == currentSlaveID) {
              Serial.printf("✅ ID %d | Pos: %.3f | Touch: %d\n", 
                          currentSlaveID, 
                          faderStats[currentSlaveID].physPos, 
                          faderStats[currentSlaveID].touchState);
              
              stats.responsesReceived++;
              stats.lastResponseTime = millis();
            } else {
              Serial.printf("⚠️ ID incorrecto: %d != %d\n", 
                          faderStats[currentSlaveID].id, currentSlaveID);
            }
            
            timerRef = micros(); 
            currentState = GAP;
          }
        } else {
          Serial1.read(); // Descartar byte inválido
        }
      }

      // Timeout después de 1.5ms
      if (micros() - timerRef > 1500) {
        if (slaveActive[currentSlaveID]) {
          Serial.printf("❌ ID %d | TIMEOUT\n", currentSlaveID);
          stats.timeouts++;
        }
        timerRef = micros();
        currentState = GAP;
      }
      break;

    case GAP:
      // Esperar 500us entre esclavos
      if (micros() - timerRef >= 500) {
        currentSlaveID++;
        if (currentSlaveID > NUM_SLAVES) currentSlaveID = 1;
        currentState = SENDING;
      }
      break;
  }
}

void processSerialCommands() {
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch(cmd) {
      case 's': // Mostrar estadísticas
        stats.print();
        Serial.println("\n📋 Estado esclavos:");
        for(int i = 1; i <= NUM_SLAVES; i++) {
          Serial.printf("ID %d: %s | Pos: %.3f | Touch: %d\n",
                       i, 
                       slaveActive[i] ? "ACTIVO" : "INACTIVO",
                       faderStats[i].physPos,
                       faderStats[i].touchState);
        }
        break;
        
      case 't': { // <--- LLAVE DE APERTURA AGREGADA
        Serial.print("ID a testear (1-");
        Serial.print(NUM_SLAVES);
        Serial.print("): ");
        // OJO: Este while bloquea el loop principal
        while(!Serial.available()); 
        int testID = Serial.parseInt();
        
        if (testID >= 1 && testID <= NUM_SLAVES) {
          Serial.printf("\n🔍 Testeando ID %d...\n", testID);
          uint8_t tempID = currentSlaveID;
          currentSlaveID = testID;
          currentState = SENDING;
          
          // Esperar resultado
          uint32_t start = millis();
          while(millis() - start < 100 && currentState != GAP) {
            updateBus();
          }
          
          currentSlaveID = tempID;
        }
        break;
      } // <--- LLAVE DE CIERRE AGREGADA
        
      case 'r': // Reset estadísticas
        stats.packetsSent = 0;
        stats.responsesReceived = 0;
        stats.timeouts = 0;
        Serial.println("📊 Estadísticas reseteadas");
        break;
        
      case 'a': { // <--- LLAVE DE APERTURA AGREGADA
        Serial.print("ID a activar/desactivar (1-");
        Serial.print(NUM_SLAVES);
        Serial.print("): ");
        // OJO: Este while bloquea el loop principal
        while(!Serial.available());
        int toggleID = Serial.parseInt();
        
        if (toggleID >= 1 && toggleID <= NUM_SLAVES) {
          slaveActive[toggleID] = !slaveActive[toggleID];
          Serial.printf("ID %d ahora está %s\n", 
                       toggleID, 
                       slaveActive[toggleID] ? "ACTIVO" : "INACTIVO");
        }
        break;
      } // <--- LLAVE DE CIERRE AGREGADA
        
      case '\n': // Ignorar newlines
      case '\r':
        break;
        
      default:
        Serial.println("❌ Comando no reconocido");
        Serial.println("s=stats, t=test, r=reset, a=activar/desactivar");
        break;
    }
    
    // Limpiar restos en el buffer serial para evitar lecturas de '\n' o '\r' extra
    while(Serial.available()) Serial.read();
  }
}

void updateSlaveData() {
  static uint32_t lastUpdate = 0;
  
  // Actualizar cada 100ms (simulación)
  if (millis() - lastUpdate > 100) {
    // Cambiar posición de fader de forma cíclica
    static float pos = 0.0;
    pos += 0.01;
    if (pos > 1.0) pos = 0.0;
    
    // Actualizar posiciones para todos los esclavos (simulación)
    for(int i = 1; i <= NUM_SLAVES; i++) {
      slaves[i].faderPos = pos;
    }
    
    lastUpdate = millis();
  }
}

void printPeriodicStats() {
  // Imprimir cada 5 segundos
  if (millis() - lastStatsPrint > 5000) {
    Serial.printf("\n⏱️  Ciclo: ID %d | %s\n", 
                 currentSlaveID,
                 currentState == SENDING ? "Enviando" : 
                 currentState == WAITING ? "Esperando" : "Pausa");
    
    stats.print();
    lastStatsPrint = millis();
  }
}