#include <Arduino.h>

// --- CONFIGURACIÓN HARDWARE ---
#define USBSerial Serial
#define RX_PIN 9
#define TX_PIN 8
#define ENABLE_PIN 17
#define LED_PIN 15

// --- CONFIGURACIÓN PROTOCOLO ---
const uint8_t MY_ID = 2;
const uint8_t START_BYTE = 0xAA;
const uint8_t RESP_BYTE = 0xBB;

// --- CONSTANTES DE TIMING ---
const uint32_t BUS_SETTLING_TIME_US = 5;
const uint32_t TX_ENABLE_DELAY_US = 100;
const uint32_t TX_DISABLE_DELAY_US = 100;
const uint32_t BUS_RELEASE_TIME_US = 50;
const uint32_t TX_TIMEOUT_US = 5000;  // 5ms timeout para transmisión

// --- ESTRUCTURAS DE DATOS ---
struct __attribute__((packed)) SlaveData {
    uint8_t  header;
    uint8_t  id;
    char     trackName[8];
    uint8_t  flags;
    float    vuPeak;
    float    faderPos;
    float    vuLevel;
    uint8_t  crc;  // Checksum para integridad
};

struct __attribute__((packed)) SlaveResponse {
    uint8_t  header;
    uint8_t  id;
    float    physPos;
    uint8_t  touchState;
    uint8_t  buttons;
    uint8_t  crc;  // Checksum para integridad
};

// --- ESTADÍSTICAS DE DIAGNÓSTICO ---
struct Statistics {
    uint32_t packetsReceived;
    uint32_t crcErrors;
    uint32_t wrongIdPackets;
    uint32_t bufferOverflows;
    uint32_t transmissionsSent;
    uint32_t transmissionTimeouts;
    uint32_t validationErrors;
    
    void reset() {
        packetsReceived = 0;
        crcErrors = 0;
        wrongIdPackets = 0;
        bufferOverflows = 0;
        transmissionsSent = 0;
        transmissionTimeouts = 0;
        validationErrors = 0;
    }
    
    void print() {
        USBSerial.println("=== ESTADÍSTICAS ===");
        USBSerial.printf("Paquetes RX: %u\n", packetsReceived);
        USBSerial.printf("Errores CRC: %u\n", crcErrors);
        USBSerial.printf("ID Incorrecto: %u\n", wrongIdPackets);
        USBSerial.printf("Buffer Overflow: %u\n", bufferOverflows);
        USBSerial.printf("Transmisiones: %u\n", transmissionsSent);
        USBSerial.printf("TX Timeouts: %u\n", transmissionTimeouts);
        USBSerial.printf("Validaciones: %u\n", validationErrors);
        USBSerial.println("===================");
    }
};

Statistics stats;

// --- VARIABLES GLOBALES ---
volatile SlaveData receivedData;
volatile SlaveResponse myStatus;
volatile bool newDataAvailable = false;
volatile bool transmissionInProgress = false;
volatile uint32_t transmissionStartTime = 0;

// --- BUFFER CIRCULAR CON DMA IMPLÍCITO ---
#define CIRCULAR_BUFFER_SIZE 256
volatile uint8_t circularBuffer[CIRCULAR_BUFFER_SIZE];
volatile uint16_t cbHead = 0;  // Índice de escritura (ISR)
volatile uint16_t cbTail = 0;  // Índice de lectura (loop)
volatile uint32_t bufferOverflowCount = 0;  // ✅ NUEVO: Contador de overflow

// --- MÁQUINA DE ESTADOS DE RECEPCIÓN ---
enum RxState {
    STATE_WAIT_HEADER,
    STATE_RECEIVE_PACKET,
    STATE_PACKET_COMPLETE
};

volatile RxState rxState = STATE_WAIT_HEADER;
volatile uint8_t packetBytesReceived = 0;
volatile uint8_t currentPacket[sizeof(SlaveData)];

// --- SIMULACIÓN NO BLOQUEANTE ---
struct FaderSimulation {
    float position;
    bool direction;
    uint32_t lastUpdate;
    uint32_t updateInterval;
    
    void init() {
        position = 0.0;
        direction = true;
        lastUpdate = 0;
        updateInterval = 10; // ms
    }
    
    void update() {
        uint32_t now = millis();
        if (now - lastUpdate >= updateInterval) {
            if (direction) {
                position += 0.005;
                if (position >= 1.0) direction = false;
            } else {
                position -= 0.005;
                if (position <= 0.0) direction = true;
            }
            lastUpdate = now;
        }
    }
    
    float getPosition() { return position; }
    uint8_t getTouchState() { return (position > 0.8) ? 1 : 0; }
    uint8_t getButtons() { return (millis() / 1000 % 2 == 0) ? 0x01 : 0x00; }
};

FaderSimulation faderSim;

// --- LED NO BLOQUEANTE ---
struct NonBlockingLED {
    uint32_t offTime;
    bool active;
    
    void init() {
        offTime = 0;
        active = false;
        digitalWrite(LED_PIN, LOW);
    }
    
    void trigger(uint32_t duration_us = 500) {
        digitalWrite(LED_PIN, HIGH);
        offTime = micros() + duration_us;
        active = true;
    }
    
    void update() {
        if (active && micros() > offTime) {
            digitalWrite(LED_PIN, LOW);
            active = false;
        }
    }
};

NonBlockingLED statusLED;

// --- FUNCIONES DE CRC/CHECKSUM ---
uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ✅ NUEVO: Validación de datos recibidos
bool isDataValid(const SlaveData* data) {
    // Validar rangos de floats (0.0 a 1.0)
    if (data->faderPos < 0.0f || data->faderPos > 1.0f) {
        USBSerial.println("Validación: faderPos fuera de rango");
        return false;
    }
    if (data->vuPeak < 0.0f || data->vuPeak > 1.0f) {
        USBSerial.println("Validación: vuPeak fuera de rango");
        return false;
    }
    if (data->vuLevel < 0.0f || data->vuLevel > 1.0f) {
        USBSerial.println("Validación: vuLevel fuera de rango");
        return false;
    }
    
    // Validar que trackName tenga caracteres imprimibles o null
    for (int i = 0; i < 8; i++) {
        if (data->trackName[i] != 0 && 
            (data->trackName[i] < 32 || data->trackName[i] > 126)) {
            USBSerial.println("Validación: trackName con caracteres inválidos");
            return false;
        }
    }
    
    return true;
}

// --- INTERRUPCIÓN DE RECEPCIÓN UART (con DMA implícito) ---
void IRAM_ATTR uartRxHandler() {
    while (Serial1.available()) {
        uint8_t rxByte = Serial1.read();
        
        // ✅ ARREGLADO: Detectar overflow antes de escribir
        uint16_t nextHead = (cbHead + 1) % CIRCULAR_BUFFER_SIZE;
        if (nextHead != cbTail) {
            circularBuffer[cbHead] = rxByte;
            cbHead = nextHead;
        } else {
            // ✅ NUEVO: Registrar overflow
            bufferOverflowCount++;
            stats.bufferOverflows++;
        }
    }
}

// --- PROCESAMIENTO DE BUFFER CIRCULAR ---
void processCircularBuffer() {
    // ✅ ARREGLADO: Captura atómica del head para evitar race condition
    noInterrupts();
    uint16_t localHead = cbHead;
    uint32_t localOverflowCount = bufferOverflowCount;
    interrupts();
    
    // ✅ NUEVO: Reportar overflow si ocurrió
    static uint32_t lastOverflowCount = 0;
    if (localOverflowCount != lastOverflowCount) {
        USBSerial.printf("⚠️ BUFFER OVERFLOW! Total: %u\n", localOverflowCount);
        lastOverflowCount = localOverflowCount;
    }
    
    // Procesar datos disponibles
    while (cbTail != localHead) {
        uint8_t byte = circularBuffer[cbTail];
        cbTail = (cbTail + 1) % CIRCULAR_BUFFER_SIZE;
        
        switch (rxState) {
            case STATE_WAIT_HEADER:
                if (byte == START_BYTE) {
                    currentPacket[0] = byte;
                    packetBytesReceived = 1;
                    rxState = STATE_RECEIVE_PACKET;
                }
                break;
                
            case STATE_RECEIVE_PACKET:
    currentPacket[packetBytesReceived++] = byte;
    
    if (packetBytesReceived >= sizeof(SlaveData)) {
        // Verificar CRC
        uint8_t receivedCRC = currentPacket[sizeof(SlaveData) - 1];
        uint8_t calculatedCRC = calculateCRC8((const uint8_t*)currentPacket, sizeof(SlaveData) - 1); // CAST AQUÍ
        
        if (receivedCRC != calculatedCRC) {
            stats.crcErrors++;
            USBSerial.printf("❌ CRC Error: Calc=0x%02X, Recv=0x%02X\n",
                           calculatedCRC, receivedCRC);
        } else if (currentPacket[1] != MY_ID) {
            stats.wrongIdPackets++;
            // No mostrar error, es normal recibir paquetes de otros slaves
        } else {
            // ✅ NUEVO: Validar datos antes de aceptar
            SlaveData* tempData = (SlaveData*)currentPacket;
            if (isDataValid(tempData)) {
                // Copiar datos y marcar como disponibles
                memcpy((void*)&receivedData, (const void*)currentPacket, sizeof(SlaveData)); // CAST AQUÍ
                newDataAvailable = true;
                stats.packetsReceived++;
            } else {
                stats.validationErrors++;
            }
        }
        
        rxState = STATE_WAIT_HEADER;
        packetBytesReceived = 0;
    }
    break;
                
            default:
                rxState = STATE_WAIT_HEADER;
                break;
        }
    }
}

// --- TRANSMISIÓN NO BLOQUEANTE RS485 ---
enum TxState {
    TX_IDLE,
    TX_PREPARE,
    TX_ENABLE_BUS,
    TX_SEND_DATA,
    TX_DISABLE_BUS,
    TX_COMPLETE
};

volatile TxState txState = TX_IDLE;
volatile uint32_t txTimer = 0;

bool startTransmission() {
    if (txState != TX_IDLE) {
        return false;  // Ya hay una transmisión en progreso
    }
    
    // Preparar respuesta
    myStatus.header = RESP_BYTE;
    myStatus.id = MY_ID;
    myStatus.physPos = faderSim.getPosition();
    myStatus.touchState = faderSim.getTouchState();
    myStatus.buttons = faderSim.getButtons();
    
    // Calcular CRC
    uint8_t* dataPtr = (uint8_t*)&myStatus;
    myStatus.crc = calculateCRC8(dataPtr, sizeof(SlaveResponse) - 1);
    
    txState = TX_PREPARE;
    transmissionStartTime = micros();
    transmissionInProgress = true;
    
    return true;
}

void updateTransmission() {
    if (txState == TX_IDLE) return;
    
    uint32_t now = micros();
    
    // ✅ NUEVO: Timeout de seguridad para evitar bloqueos
    if (now - transmissionStartTime > TX_TIMEOUT_US) {
        USBSerial.println("⚠️ TX TIMEOUT! Reseteando transmisión...");
        digitalWrite(ENABLE_PIN, LOW);
        txState = TX_IDLE;
        transmissionInProgress = false;
        stats.transmissionTimeouts++;
        return;
    }
    
    switch (txState) {
        case TX_PREPARE:
            // Limpiar buffer de transmisión
            while (Serial1.available()) Serial1.read();
            txState = TX_ENABLE_BUS;
            txTimer = now;
            break;
            
        case TX_ENABLE_BUS:
            if (now - txTimer >= BUS_SETTLING_TIME_US) {
                digitalWrite(ENABLE_PIN, HIGH);
                txState = TX_SEND_DATA;
                txTimer = now;
            }
            break;
            
        case TX_SEND_DATA:
            if (now - txTimer >= TX_ENABLE_DELAY_US) {
                Serial1.write((uint8_t*)&myStatus, sizeof(SlaveResponse));
                Serial1.flush();
                txState = TX_DISABLE_BUS;
                txTimer = now;
            }
            break;
            
        case TX_DISABLE_BUS:
            if (now - txTimer >= TX_DISABLE_DELAY_US) {
                digitalWrite(ENABLE_PIN, LOW);
                txState = TX_COMPLETE;
                txTimer = now;
            }
            break;
            
        case TX_COMPLETE:
            if (now - txTimer >= BUS_RELEASE_TIME_US) {
                transmissionInProgress = false;
                txState = TX_IDLE;
                stats.transmissionsSent++;
                
                uint32_t totalTime = now - transmissionStartTime;
                USBSerial.printf("✅ TX Complete: %u us\n", totalTime);
                
                // Feedback visual
                statusLED.trigger(300);
            }
            break;
    }
}

// --- SETUP ---
void setup() {
    USBSerial.begin(115200);
    delay(100);  // Pequeño delay para estabilizar USB
    
    // Configurar GPIO
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, LOW);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Configurar UART con buffer grande para DMA implícito
    Serial1.setRxBufferSize(512);  // Buffer grande -> usa DMA automáticamente
    Serial1.begin(500000, SERIAL_8N1, RX_PIN, TX_PIN);
    
    // Configurar interrupción de recepción
    Serial1.onReceive(uartRxHandler);
    
    // Inicializar sistemas
    faderSim.init();
    statusLED.init();
    stats.reset();
    
    USBSerial.println("========================================");
    USBSerial.println("ESP32-S2 RS485 Slave - FIXED VERSION");
    USBSerial.println("✅ Race conditions resueltas");
    USBSerial.println("✅ Buffer overflow detectado");
    USBSerial.println("✅ Timeout en transmisión");
    USBSerial.println("✅ Validación de datos");
    USBSerial.println("========================================");
}

// --- LOOP PRINCIPAL (NO BLOQUEANTE) ---
void loop() {
    uint32_t loopStart = micros();
    
    // 1. Procesar buffer circular (datos recibidos)
    processCircularBuffer();
    
    // 2. Manejar nueva data disponible
    if (newDataAvailable && !transmissionInProgress) {
        USBSerial.printf("📥 RX ID %d | Track: %s | Fader: %.2f\n",
                        receivedData.id,
                        receivedData.trackName,
                        receivedData.faderPos);
        
        newDataAvailable = false;
        
        // Iniciar transmisión de respuesta
        if (startTransmission()) {
            USBSerial.printf("📤 Starting response for ID %d\n", receivedData.id);
        }
    }
    
    // 3. Actualizar transmisión en curso (máquina de estados)
    updateTransmission();
    
    // 4. Actualizar simulación de fader
    faderSim.update();
    
    // 5. Actualizar LED de estado
    statusLED.update();
    
    // 6. Medir y reportar tiempo de ciclo + estadísticas
    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 10000) {  // Cada 10 segundos
        uint32_t loopTime = micros() - loopStart;
        USBSerial.printf("\n⏱️  Loop time: %u us | Fader: %.3f\n", loopTime, faderSim.getPosition());
        stats.print();
        USBSerial.println();
        lastReport = millis();
    }
    
    // 7. Pequeño yield para que el sistema respire
    if (micros() - loopStart < 100) {
        delayMicroseconds(10);
    }
}
