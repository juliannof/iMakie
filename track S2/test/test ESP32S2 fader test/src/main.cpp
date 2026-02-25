#include <Arduino.h>

// --- CONFIGURACIÓN DE HARDWARE ---
const int MOTOR_IN1 = 18;        // PWM para IN1 del DRV8833
const int MOTOR_IN2 = 16;        // PWM para IN2 del DRV8833  
const int ENABLE_PIN = 14;       // nSLEEP del DRV8833
const int POT_PIN = 10;          // Entrada ADC del fader
const int STATUS_LED = 15;       // LED de estado

// --- PARÁMETROS OPERATIVOS ---
const int PWM_FREQ = 20000;
const int PWM_RESOLUTION = 10;    // 10 bits (0-1023)
const int MIN_PWM_DRIVE = 640;    // PWM mínimo para mover el motor (67% de 1023)
const unsigned long CALIB_TIMEOUT = 10000; // 10s timeout
const int ADC_BUFFER_SIZE = 10;
const int ADC_STABILITY_THRESHOLD = 15;
const unsigned long CALIB_STABLE_TIME = 450;
const int ADC_MAX_VALUE = 4095;

// --- CANALES PWM PARA ESP32-S2 ---
const int PWM_CHANNEL_1 = 0;  // Canal 0 para MOTOR_IN1
const int PWM_CHANNEL_2 = 1;  // Canal 1 para MOTOR_IN2

// --- ESTRUCTURA DE CALIBRACIÓN ---
struct CalibrationData {
    int minPositionADC = 0;
    int maxPositionADC = ADC_MAX_VALUE;
    bool isCalibrated = false;
    String errorMessage = "";
};
CalibrationData calData;

// --- VARIABLES GLOBALES ---
int adcBuffer[ADC_BUFFER_SIZE];
int adcIndex = 0;
int calibState = -1; // -1: no calibrando, 0: buscando max, 1: buscando min
unsigned long lastStableTime = 0;
int lastStableValue = 0;
unsigned long calibStartTime = 0;

// --- PROTOTIPOS ---
int runStartupTest();
bool performAutoCalibration();
int readSmoothedADC();
bool validateCalibration();
void blinkStatus(int count, int duration);
void driverEnable(bool enable);
void driveMotor(int speed);
void updateCalibrationState();

void setup() {
    Serial.begin(115200);
    
    // Configurar PWM del motor con 10 bits - CORREGIDO PARA ESP32-S2
    ledcSetup(PWM_CHANNEL_1, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_2, PWM_FREQ, PWM_RESOLUTION);
    
    // Usar ledcAttachChannel en lugar de ledcAttachPin para ESP32-S2
    ledcAttachChannel(MOTOR_IN1, PWM_CHANNEL_1, 0, 0);
    ledcAttachChannel(MOTOR_IN2, PWM_CHANNEL_2, 0, 0);
    
    // Configurar pines
    pinMode(ENABLE_PIN, OUTPUT);
    pinMode(STATUS_LED, OUTPUT);
    pinMode(POT_PIN, INPUT);
    
    driverEnable(false);
    digitalWrite(STATUS_LED, LOW);
    
    // Configurar ADC para ESP32-S2
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    Serial.println("\n=== SISTEMA DE CALIBRACIÓN AUTOMÁTICA ===");
    Serial.print("Resolución PWM: "); Serial.print(PWM_RESOLUTION); Serial.println(" bits (0-1023)");
    Serial.print("MIN_PWM_DRIVE: "); Serial.println(MIN_PWM_DRIVE);
    
    // Ejecutar test de inicio
    int testResult = runStartupTest();
    
    Serial.print("\n>>> RESULTADO FINAL: ");
    Serial.println(testResult);
    
    // Estado final
    if (testResult == 1) {
        Serial.println("✅ SISTEMA CALIBRADO Y LISTO");
        blinkStatus(3, 200);
    } else {
        Serial.println("❌ SISTEMA NO CALIBRADO");
        Serial.print("Error: ");
        Serial.println(calData.errorMessage);
        blinkStatus(5, 100);
    }
}

void loop() {
    static unsigned long lastPrintMs = 0;
    unsigned long now = millis();
    
    if (calData.isCalibrated && (now - lastPrintMs >= 1000)) {
        int currentADC = readSmoothedADC();
        int percentage = map(currentADC, calData.minPositionADC, calData.maxPositionADC, 0, 100);
        percentage = constrain(percentage, 0, 100);
        
        const int pwmMax = (1 << PWM_RESOLUTION) - 1; // 1023 si PWM_RESOLUTION=10
        int pwmEq = map(currentADC, calData.minPositionADC, calData.maxPositionADC, 0, pwmMax);
        pwmEq = constrain(pwmEq, 0, pwmMax);
        
        Serial.print("Posición: ");
        Serial.print(percentage);
        Serial.print("% (ADC: ");
        Serial.print(currentADC);
        Serial.print(" / PWM equivalente: ");
        Serial.print(pwmEq);
        Serial.println(")");
        
        lastPrintMs = now;
    }
    
    // sin delay(): loop libre de bloqueos
    // opcional en ESP32-S2: yield(); o vTaskDelay(1);
}

// --- TEST DE INICIO COMPLETO ---
int runStartupTest() {
    Serial.println("\n1. INICIANDO TEST DE SENSOR...");
    
    // Test del sensor ADC
    int sensorValue = readSmoothedADC();
    Serial.print("   Lectura ADC inicial: ");
    Serial.println(sensorValue);
    
    if (sensorValue < 0 || sensorValue > ADC_MAX_VALUE) {
        calData.errorMessage = "ADC fuera de rango: " + String(sensorValue);
        return 0;
    }
    
    Serial.println("   ✅ Sensor OK");
    
    // Calibración automática con motor
    Serial.println("\n2. INICIANDO CALIBRACIÓN AUTOMÁTICA RSA0N11M9A0J");
    bool calSuccess = performAutoCalibration();
    
    return calSuccess ? 1 : 0;
}

// --- CALIBRACIÓN AUTOMÁTICA CON MOVIMIENTO DE MOTOR ---
bool performAutoCalibration() {
    driverEnable(true);
    calibState = 0; // Comenzar buscando el máximo
    calibStartTime = millis();
    lastStableTime = millis();
    lastStableValue = readSmoothedADC();
    
    Serial.println("   🔄 Buscando límite SUPERIOR...");
    driveMotor(MIN_PWM_DRIVE); // Mover hacia arriba
    
    // Bucle principal de calibración
    while (calibState >= 0) {
        updateCalibrationState();
        
        // Timeout general de calibración
        if (millis() - calibStartTime > CALIB_TIMEOUT) {
            calData.errorMessage = "Timeout en calibración";
            driveMotor(0);
            driverEnable(false);
            calibState = -1;
            return false;
        }
        
        delay(50);
    }
    
    driveMotor(0);
    driverEnable(false);
    
    return validateCalibration();
}

// --- ACTUALIZACIÓN DEL ESTADO DE CALIBRACIÓN ---
void updateCalibrationState() {
    int currentValue = readSmoothedADC();
    
    // Detectar estabilidad
    if (abs(currentValue - lastStableValue) <= ADC_STABILITY_THRESHOLD) {
        if (millis() - lastStableTime > CALIB_STABLE_TIME) {
            // Estabilidad detectada - procesar según fase actual
            if (calibState == 0) { // Buscando máximo
                calData.maxPositionADC = currentValue;
                Serial.print("   ✅ Límite SUPERIOR encontrado: ");
                Serial.println(calData.maxPositionADC);
                
                // Cambiar a buscar mínimo
                calibState = 1;
                lastStableTime = millis();
                lastStableValue = currentValue;
                Serial.println("   🔄 Buscando límite INFERIOR...");
                driveMotor(-MIN_PWM_DRIVE); // Mover hacia abajo
            } 
            else if (calibState == 1) { // Buscando mínimo
                calData.minPositionADC = currentValue;
                Serial.print("   ✅ Límite INFERIOR encontrado: ");
                Serial.println(calData.minPositionADC);
                
                // Finalizar calibración
                calibState = -1;
                calData.isCalibrated = true;
            }
        }
    } else {
        // Reiniciar temporizador de estabilidad
        lastStableTime = millis();
        lastStableValue = currentValue;
    }
}

// --- VALIDACIÓN DE CALIBRACIÓN ---
bool validateCalibration() {
    Serial.println("\n3. VALIDANDO CALIBRACIÓN...");
    
    // Verificar que min < max
    if (calData.minPositionADC >= calData.maxPositionADC) {
        calData.errorMessage = "MIN >= MAX (" + String(calData.minPositionADC) + " >= " + String(calData.maxPositionADC) + ")";
        Serial.println("   ❌ " + calData.errorMessage);
        return false;
    }
    
    // Verificar rango mínimo (al menos 50% del total)
    int range = calData.maxPositionADC - calData.minPositionADC;
    int minRequiredRange = ADC_MAX_VALUE / 2;
    
    if (range < minRequiredRange) {
        calData.errorMessage = "Rango insuficiente: " + String(range) + " < " + String(minRequiredRange);
        Serial.println("   ❌ " + calData.errorMessage);
        return false;
    }
    
    Serial.print("   ✅ Rango válido: ");
    Serial.print(range);
    Serial.print(" (");
    Serial.print((range * 100) / ADC_MAX_VALUE);
    Serial.println("% del total)");
    
    calData.isCalibrated = true;
    return true;
}

// --- CONTROL DEL MOTOR CON 10 BITS ---
void driverEnable(bool enable) {
    digitalWrite(ENABLE_PIN, enable ? HIGH : LOW);
    digitalWrite(STATUS_LED, enable ? LOW : HIGH);
    Serial.print("   Driver: ");
    Serial.println(enable ? "HABILITADO" : "DESHABILITADO");
}

void driveMotor(int speed) {
    if (digitalRead(ENABLE_PIN) == LOW) {
        ledcWrite(PWM_CHANNEL_1, 0);
        ledcWrite(PWM_CHANNEL_2, 0);
        return;
    }
    
    // Para 10 bits, el rango es 0-1023
    int pwmValue = constrain(abs(speed), 0, 1023);
    
    // Debug opcional del PWM
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 500) {
        Serial.print("   PWM: ");
        Serial.print(pwmValue);
        Serial.print("/1023 (");
        Serial.print((pwmValue * 100) / 1023);
        Serial.println("%)");
        lastDebug = millis();
    }
    
    if (speed > 0) {
        // Dirección positiva
        ledcWrite(PWM_CHANNEL_1, 0);
        ledcWrite(PWM_CHANNEL_2, pwmValue);
    } else if (speed < 0) {
        // Dirección negativa
        ledcWrite(PWM_CHANNEL_1, pwmValue);
        ledcWrite(PWM_CHANNEL_2, 0);
    } else {
        // Parar
        ledcWrite(PWM_CHANNEL_1, 0);
        ledcWrite(PWM_CHANNEL_2, 0);
    }
}

// --- LECTURA DE ADC SUAVIZADA ---
int readSmoothedADC() {
    // Leer ADC con resolución de 12 bits (0-4095)
    adcBuffer[adcIndex] = analogRead(POT_PIN);
    adcIndex = (adcIndex + 1) % ADC_BUFFER_SIZE;
    
    long sum = 0;
    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
        sum += adcBuffer[i];
    }
    return sum / ADC_BUFFER_SIZE;
}

// --- INDICACIÓN VISUAL ---
void blinkStatus(int count, int duration) {
    for (int i = 0; i < count; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(duration);
        digitalWrite(STATUS_LED, LOW);
        if (i < count - 1) delay(duration);
    }
}