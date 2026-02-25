#include <Arduino.h>

// Configuración de pines para ESP32-S2
const int MOTOR_IN1 = 18;        // GPIO18 - Canal PWM 0
const int MOTOR_IN2 = 16;        // GPIO16 - Canal PWM 1
const int ENABLE_PIN = 34;       // GPIO34 - nSLEEP del DRV8833

// Configuración PWM para ESP32-S2
const int PWM_FREQ = 20000;      // Frecuencia PWM 20kHz
const int PWM_RESOLUTION = 8;    // Resolución 8-bits (0-255)
const int PWM_CHANNEL_IN1 = 0;   // Canal PWM para IN1 (0-7)
const int PWM_CHANNEL_IN2 = 1;   // Canal PWM para IN2 (0-7)

// Variables para control
bool demoRunning = false;
bool testSequenceRunning = false;
unsigned long lastDemoTime = 0;
unsigned long sequenceStartTime = 0;
int demoState = 0;
int sequenceState = 0;
int currentSpeed = 160;          // Velocidad actual por defecto
bool driverEnabled = true;       // Estado del driver

// Declaraciones de funciones
void handleSerialCommand(char command);
void runDemoSequence();
void runTestSequence();
void motorForward(int speed);
void motorBackward(int speed);
void motorStop();
void disableDriver();
void enableDriver();
void printInstructions();
void startDemo();
void stopDemo();
void startTestSequence();
void stopTestSequence();
void setSpeed(int speed);
void showCurrentStatus();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Configurar pin de habilitación - Iniciar HIGH
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH);
  
  // Configurar pines PWM usando ledc
  ledcSetup(PWM_CHANNEL_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_IN1, PWM_CHANNEL_IN1);
  
  ledcSetup(PWM_CHANNEL_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_IN2, PWM_CHANNEL_IN2);
  
  // Inicializar motores en 0
  motorStop();
  
  Serial.println("=== CONTROL DRV8833 - RANGO 150-225 ===");
  Serial.println("Motor funcionando correctamente en ambas direcciones");
  showCurrentStatus();
  printInstructions();
}

void loop() {
  if (Serial.available()) {
    char command = Serial.read();
    handleSerialCommand(command);
  }
  
  // Ejecutar demo solo si está activada
  if (demoRunning) {
    runDemoSequence();
  }
  
  // Ejecutar secuencia de prueba si está activada
  if (testSequenceRunning) {
    runTestSequence();
  }
}

void handleSerialCommand(char command) {
  switch(command) {
    case 'f': // Forward con velocidad actual
      enableDriver();
      stopDemo();
      stopTestSequence();
      motorForward(currentSpeed);
      Serial.printf("Adelante - Velocidad: %d\n", currentSpeed);
      break;
    case 'b': // Backward con velocidad actual
      enableDriver();
      stopDemo();
      stopTestSequence();
      motorBackward(currentSpeed);
      Serial.printf("Atrás - Velocidad: %d\n", currentSpeed);
      break;
    case 's': // Stop
      stopDemo();
      stopTestSequence();
      motorStop();
      Serial.println("Parada");
      break;
    case 'x': // Deshabilitar driver (ENABLE=0V) y detener secuencias
      stopDemo();
      stopTestSequence();
      disableDriver();
      Serial.println("DRV8833 Deshabilitado - ENABLE=0V");
      break;
    case 'e': // Habilitar driver
      enableDriver();
      Serial.println("DRV8833 Habilitado - ENABLE=3.3V");
      break;
    case '+': // Incrementar velocidad
      stopDemo();
      stopTestSequence();
      setSpeed(currentSpeed + 5);
      Serial.printf("Velocidad incrementada a: %d\n", currentSpeed);
      break;
    case '-': // Decrementar velocidad
      stopDemo();
      stopTestSequence();
      setSpeed(currentSpeed - 5);
      Serial.printf("Velocidad decrementada a: %d\n", currentSpeed);
      break;
    case '1': // Velocidad 150
      stopDemo();
      stopTestSequence();
      setSpeed(150);
      Serial.println("Velocidad establecida a 150");
      break;
    case '2': // Velocidad 165
      stopDemo();
      stopTestSequence();
      setSpeed(165);
      Serial.println("Velocidad establecida a 165");
      break;
    case '3': // Velocidad 180
      stopDemo();
      stopTestSequence();
      setSpeed(180);
      Serial.println("Velocidad establecida a 180");
      break;
    case '4': // Velocidad 195
      stopDemo();
      stopTestSequence();
      setSpeed(195);
      Serial.println("Velocidad establecida a 195");
      break;
    case '5': // Velocidad 210
      stopDemo();
      stopTestSequence();
      setSpeed(210);
      Serial.println("Velocidad establecida a 210");
      break;
    case '6': // Velocidad 225
      stopDemo();
      stopTestSequence();
      setSpeed(225);
      Serial.println("Velocidad establecida a 225");
      break;
    case 'd': // Demo continua
      enableDriver();
      stopTestSequence();
      startDemo();
      break;
    case 'p': // Parar demo
      stopDemo();
      break;
    case 't': // Secuencia de prueba CONTINUA
      enableDriver();
      stopDemo();
      startTestSequence();
      break;
    case 'c': // Mostrar estado actual
      showCurrentStatus();
      break;
    case '?': // Help
      printInstructions();
      break;
    default:
      Serial.println("Comando no reconocido. Presiona '?' para ayuda.");
      break;
  }
}

void setSpeed(int speed) {
  // Limitar al rango 150-225
  currentSpeed = constrain(speed, 150, 225);
  showCurrentStatus();
}

void showCurrentStatus() {
  float dutyCycle = (currentSpeed / 255.0) * 100.0;
  float voltage = (dutyCycle / 100.0) * 3.3;
  
  Serial.println("\n=== ESTADO ACTUAL ===");
  Serial.printf("Velocidad configurada: %d\n", currentSpeed);
  Serial.printf("Duty Cycle: %.1f%%\n", dutyCycle);
  Serial.printf("Voltaje efectivo: %.2fV\n", voltage);
  Serial.printf("Rango disponible: 150-225 (%.1f%% - %.1f%%)\n", 
                (150/255.0)*100, (225/255.0)*100);
  Serial.printf("DRV8833: %s\n", driverEnabled ? "HABILITADO" : "DESHABILITADO");
  Serial.printf("Secuencia prueba: %s\n", testSequenceRunning ? "ACTIVA (presiona 'x' para detener)" : "INACTIVA");
  Serial.printf("Demo: %s\n", demoRunning ? "ACTIVA" : "INACTIVA");
  Serial.println("====================\n");
}

// Funciones de control del motor
void motorForward(int speed) {
  speed = constrain(speed, 150, 225);
  ledcWrite(PWM_CHANNEL_IN1, speed);
  ledcWrite(PWM_CHANNEL_IN2, 0);
}

void motorBackward(int speed) {
  speed = constrain(speed, 150, 225);
  ledcWrite(PWM_CHANNEL_IN1, 0);
  ledcWrite(PWM_CHANNEL_IN2, speed);
}

void motorStop() {
  ledcWrite(PWM_CHANNEL_IN1, 0);
  ledcWrite(PWM_CHANNEL_IN2, 0);
}

void disableDriver() {
  digitalWrite(ENABLE_PIN, LOW);
  driverEnabled = false;
  motorStop();
}

void enableDriver() {
  digitalWrite(ENABLE_PIN, HIGH);
  driverEnabled = true;
}

void printInstructions() {
  Serial.println("\n=== COMANDOS DRV8833 - RANGO 150-225 ===");
  Serial.println("f - Adelante con velocidad actual");
  Serial.println("b - Atrás con velocidad actual");
  Serial.println("s - Parada suave");
  Serial.println("x - Deshabilitar DRV8833 (ENABLE=0V) - DETIENE SECUENCIAS");
  Serial.println("e - Habilitar DRV8833 (ENABLE=3.3V)");
  Serial.println();
  Serial.println("+ - Incrementar velocidad (+5)");
  Serial.println("- - Decrementar velocidad (-5)");
  Serial.println();
  Serial.println("1 - Velocidad 150 (58.8%)");
  Serial.println("2 - Velocidad 165 (64.7%)");
  Serial.println("3 - Velocidad 180 (70.6%)");
  Serial.println("4 - Velocidad 195 (76.5%)");
  Serial.println("5 - Velocidad 210 (82.4%)");
  Serial.println("6 - Velocidad 225 (88.2%)");
  Serial.println();
  Serial.println("d - Iniciar demo continua");
  Serial.println("p - Parar demo");
  Serial.println("t - SECUENCIA CONTINUA: Adelante 0.25s -> Parar 0.5s -> Atrás 0.25s (velocidad actual)");
  Serial.println("    (Se repite hasta presionar 'x')");
  Serial.println("c - Mostrar estado actual");
  Serial.println("? - Mostrar esta ayuda");
  Serial.println("========================================\n");
}

void startDemo() {
  demoRunning = true;
  demoState = 0;
  lastDemoTime = millis();
  Serial.println("Demo continua iniciada - Rango 150-225");
}

void stopDemo() {
  demoRunning = false;
  motorStop();
}

void startTestSequence() {
  testSequenceRunning = true;
  sequenceState = 0;
  sequenceStartTime = millis();
  Serial.println("SECUENCIA CONTINUA INICIADA:");
  Serial.printf("1. Adelante 0.25s a velocidad %d\n", currentSpeed);
  Serial.println("2. Parar 0.5s");
  Serial.printf("3. Atrás 0.25s a velocidad %d\n", currentSpeed);
  Serial.println("4. Parar 0.1s y REPETIR");
  Serial.println("Presiona 'x' para detener la secuencia");
}

void stopTestSequence() {
  testSequenceRunning = false;
  motorStop();
  Serial.println("Secuencia detenida");
}

void runDemoSequence() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastDemoTime > 500) {
    switch(demoState) {
      case 0:
        motorForward(160);
        Serial.println("Demo: Adelante");
        break;
      case 1:
        motorStop();
        Serial.println("Demo: Parada");
        break;
      case 2:
        motorBackward(160);
        Serial.println("Demo: Atrás");
        break;
      case 3:
        motorStop();
        Serial.println("Demo: Parada");
        break;
    
    }
    
    demoState = (demoState + 1) % 6;
    lastDemoTime = currentTime;
  }
}

void runTestSequence() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - sequenceStartTime;
  
  switch(sequenceState) {
    case 0: // Adelante por 0.25 segundos a velocidad actual
      motorForward(currentSpeed);
      if (elapsedTime >= 110) {
        sequenceState = 1;
        sequenceStartTime = currentTime;
        motorStop();
        Serial.println("Secuencia: Parada (0.5s)");
      }
      break;
      
    case 1: // Parar por 0.5 segundo
      if (elapsedTime >= 200) {
        sequenceState = 2;
        sequenceStartTime = currentTime;
        Serial.printf("Secuencia: Atrás 0.25s a %d\n", currentSpeed);
      }
      break;
      
    case 2: // Atrás por 0.25 segundos a velocidad actual
      motorBackward(currentSpeed);
      if (elapsedTime >= 110) {
        sequenceState = 3;
        sequenceStartTime = currentTime;
        motorStop();
        Serial.println("Secuencia: Parada breve y reinicio...");
      }
      break;
      
    case 3: // Parada breve de 100ms y REINICIAR SECUENCIA
      if (elapsedTime >= 100) {
        sequenceState = 0;  // Reiniciar a estado 0
        sequenceStartTime = currentTime;
        // No imprimimos nada para no saturar el serial
      }
      break;
  }
}