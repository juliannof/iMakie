#include <Arduino.h>
#include <PID_v1.h>

// --- INCLUDES ESPECIFICOS PARA ESP32S2 (USB CDC) ---
#if CONFIG_IDF_TARGET_ESP32S2
  #include "USB.h"
  #include "USBCDC.h"
  USBCDC USBSerial;
#endif

// --- CONFIGURACION DE HARDWARE ---
const int MOTOR_IN1 = 18;        // PWM 1 (20kHz), conecta a IN1/AIN1 del DRV8833
const int MOTOR_IN2 = 16;        // PWM 2 (20kHz), conecta a IN2/BIN1 del DRV8833
const int ENABLE_PIN = 34;       // Conecta a nSLEEP del DRV8833 (Activo HIGH para habilitar)
const int POT_PIN = 10;          // ADC1_CH9 (Fader position - GPIO10)
const int STATUS_LED = 15;       // LED indicador integrado ESP32-S2 (GPIO15)

// --- PARAMETROS OPERATIVOS ---
const int PWM_FREQ = 20000;       // Frecuencia PWM recomendada para DRV8833
const int PWM_RESOLUTION = 10;    // Resolución 10 bits (0-1023)
const int MIN_PWM_DRIVE = 690;    // PWM mínimo para superar inercia estática (AJUSTAR EMPÍRICAMENTE)
const unsigned long CALIB_TIMEOUT = 15000; // 15s para calibración automática por cada extremo
const unsigned long PID_SAMPLE_RATE = 20;  // 20ms de muestreo PID
const int ADC_BUFFER_SIZE = 10;            // Tamaño del buffer para lectura ADC promediada
const int ADC_STABILITY_THRESHOLD = 20; // Umbral para detectar estabilidad ADC en calibración
const unsigned long CALIB_STABLE_TIME = 500; // Tiempo en ms que la ADC debe estar estable para confirmar un límite
const int ADC_MAX_VALUE = 4095; // Valor máximo de la lectura ADC (12 bits)

// --- Validacion de rango para calibracion ---
const int MIN_RANGE_DIFF_ADC = 3000; // Diferencia mínima entre maxPositionADC y minPositionADC (ej. 3000/4095 ~ 73% del rango total)
const int MAX_ADC_DEVIATION_FROM_0 = 500;  // minPositionADC no debe estar demasiado lejos de 0
const int MAX_ADC_DEVIATION_FROM_4095 = 500; // maxPositionADC no debe estar demasiado lejos de 4095

// --- OBJETOS GLOBALES ---
double pidInput, pidOutput, pidSetpoint; 
const double Kp = 3.0, Ki = 0.8, Kd = 0.5;   // Parámetros PID (AJUSTAR EMPÍRICAMENTE)
PID positionPID(&pidInput, &pidOutput, &pidSetpoint, Kp, Ki, Kd, DIRECT);

// Calibración
int minPositionADC = 0;              // Valor ADC del límite inferior
int maxPositionADC = ADC_MAX_VALUE;  // Valor ADC del límite superior
bool isCalibrated = false;           // true si el fader está calibrado
// calibState: -1: no calibrando, 0: buscando max, 1: buscando min
int calibState = -1;                 
unsigned long lastSensorStableTime = 0; // Para detección de estabilidad ADC
int lastSensorValue = 0;             // Última lectura ADC para estabilidad

// Filtro ADC
int adcBuffer[ADC_BUFFER_SIZE];
int adcIndex = 0;

// --- PROTOTIPOS DE FUNCIONES ---
void driverEnable(bool enable);      // Habilita/deshabilita el driver nSLEEP
void driveMotor(int speed);          // Controla dirección y velocidad del motor DC
void calibrateSystem();              // Proceso de calibración automática
void processCalibrationPhase();      // Ejecuta una fase de la calibración
void finishCalibration();            // Finaliza la calibración y configura el sistema
void driveMotorContinuously(int pwmSpeed); // Mueve el motor en una dirección sin parar (para calibración)
void moveToPosition(int targetADC);  // Mueve el fader a una posición ADC deseada
int readSmoothedADC();               // Lee el ADC con filtro de media móvil
void readSerialCommands();           // Procesa comandos recibidos por serial
void executeCommand(String cmd);     // Ejecuta un comando
void stopSystemEmergently();         // Detiene el motor y el sistema ante un error
void blinkStatus(int count, int duration); // Parpadea el LED de estado
void calibrationFailed(const String& reason); // Manejador de fallos de calibracion


void setup() {
  #if CONFIG_IDF_TARGET_ESP32S2
    USB.begin();
    USBSerial.begin(115200); 
    while(!USBSerial); 
    #define Serial USBSerial
  #else
    Serial.begin(115200);
  #endif
  
  ledcSetup(0, PWM_FREQ, PWM_RESOLUTION); 
  ledcSetup(1, PWM_FREQ, PWM_RESOLUTION); 
  ledcAttachPin(MOTOR_IN1, 0);
  ledcAttachPin(MOTOR_IN2, 1);
  
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(POT_PIN, INPUT);
  
  driverEnable(false); 
  digitalWrite(STATUS_LED, LOW);
  
  analogReadResolution(12);       
  analogSetAttenuation(ADC_11db); // Esto es crucial para un fader 0-3.3V
  
  positionPID.SetOutputLimits(-((1 << PWM_RESOLUTION) -1), (1 << PWM_RESOLUTION) -1);
  positionPID.SetSampleTime(PID_SAMPLE_RATE); 
  positionPID.SetMode(AUTOMATIC);             
  pidSetpoint = floor(ADC_MAX_VALUE/2); 
  
  Serial.println("\n~~~ SISTEMA FADER MOTORIZADO (MOTOR DC con DRV8833)~~~");
  Serial.println("Control por PWM y PID de posición\n");
  
  Serial.println(">> INICIANDO CALIBRACION AUTOMATICA EN 3 SEGUNDOS...");
  digitalWrite(STATUS_LED, HIGH); 
  delay(3000);
  digitalWrite(STATUS_LED, LOW); 
  
  driverEnable(true); 
  calibrateSystem(); 
  
  driverEnable(false); 
}

void loop() {
  pidInput = readSmoothedADC(); 
  
  if(calibState >= 0) {
    processCalibrationPhase(); 
    return; 
  }
  
  if(isCalibrated && (digitalRead(ENABLE_PIN) == HIGH) && positionPID.Compute()) {
    int motorPWM = static_cast<int>(pidOutput);
    
    // Aplicar el umbral MIN_PWM_DRIVE para superar la inercia (si es necesario)
    if (abs(motorPWM) > 0 && abs(motorPWM) < MIN_PWM_DRIVE) {
        motorPWM = (motorPWM > 0) ? MIN_PWM_DRIVE : -MIN_PWM_DRIVE;
    }
    
    driveMotor(motorPWM); 
  } else if (digitalRead(ENABLE_PIN) == LOW) { 
      driveMotor(0); 
  }
  
  readSerialCommands(); 
  
  // Puedes ajustar este delay. Si PID_SAMPLE_RATE es muy bajo, este delay ya no es necesario
  // si el PID.Compute() se basa en el tiempo transcurrido.
  // Sin embargo, si quieres asegurar una tasa de bucle MINIMA del PID, puedes dejarlo.
  // La librería PID_v1 ya maneja su propio SampleTime, así que este delay es más bien para el propio loop().
  delay(PID_SAMPLE_RATE); 
}

// --- FUNCIONES DE CONTROL ---
void driverEnable(bool enable) {
  digitalWrite(ENABLE_PIN, enable ? HIGH : LOW);
  // El LED de estado indica si el driver está habilitado (apagado) o deshabilitado (encendido)
  digitalWrite(STATUS_LED, enable ? LOW : HIGH); 
  Serial.print("Driver: ");
  Serial.println(enable ? "HABILITADO" : "DESHABILITADO");
}

void driveMotor(int speed) {
  // Si el driver está deshabilitado, forzar apagado del motor
  if (digitalRead(ENABLE_PIN) == LOW) {
      ledcWrite(0, 0); // Ambos pines apagados
      ledcWrite(1, 0);
      return;
  }
  
  int maxPwmValue = (1 << PWM_RESOLUTION) - 1; // Calcula 2^PWM_RESOLUTION - 1 (para 10 bits es 1023)
  int pwmValue = constrain(abs(speed), 0, maxPwmValue); // Asegura que el valor esté dentro del rango PWM

  // --- LINEAS DE DEPURACIÓN EN driveMotor (COMENTADAS POR DEFECTO) ---
  
  static unsigned long lastDebugPrintTime = 0;
  if (millis() - lastDebugPrintTime > 100) { 
    Serial.print("driveMotor - Speed: "); Serial.print(speed);
    Serial.print(", abs(Speed): "); Serial.print(abs(speed));
    Serial.print(", pwmValue: "); Serial.print(pwmValue);
    Serial.print(", Direction: "); 
    if(speed > 0) Serial.println("Positive (IN2)");
    else if(speed < 0) Serial.println("Negative (IN1)");
    else Serial.println("Stop");
    lastDebugPrintTime = millis();
  }
  
  // --- FIN DE LÍNEAS DE DEPURACIÓN ---

  if(speed > 0) { // Mover en una dirección (ej. fader hacia arriba -> ADC alto)
    ledcWrite(0, 0);         // MOTOR_IN1 apagado
    ledcWrite(1, pwmValue);  // MOTOR_IN2 con PWM
  }
  else if(speed < 0) { // Mover en la dirección opuesta (ej. fader hacia abajo -> ADC bajo)
    ledcWrite(0, pwmValue);  // MOTOR_IN1 con PWM
    ledcWrite(1, 0);         // MOTOR_IN2 apagado
  }
  else { // Detener el motor (freno)
    ledcWrite(0, 0);
    ledcWrite(1, 0);
  }
}

// --- FUNCIONES DE CALIBRACIÓN ---
void calibrateSystem() {
  Serial.println("\n=== INICIANDO CALIBRACION AUTOMATICA ===");
  driverEnable(true); 
  Serial.println(">> Moviendo el fader hacia LIMITE SUPERIOR (MAX ADC)...");
  calibState = 0; // Estado 0: buscando el máximo
  // Mover en dirección positiva (hacia arriba) con la fuerza mínima garantizada
  driveMotorContinuously(MIN_PWM_DRIVE); 
  lastSensorStableTime = millis();
  lastSensorValue = readSmoothedADC(); // Leer valor inicial para comparación
}

void processCalibrationPhase() {
  int currentRawADC = readSmoothedADC();
  unsigned long currentTime = millis();
  
  // Si la lectura ADC ha estado estable por suficiente tiempo
  if(abs(currentRawADC - lastSensorValue) <= ADC_STABILITY_THRESHOLD) {
    if((currentTime - lastSensorStableTime) > CALIB_STABLE_TIME) {
      if(calibState == 0) { // Estábamos buscando el máximo
        maxPositionADC = currentRawADC;
        Serial.print("✔ LIMITE SUPERIOR (MAX ADC) encontrado: ");
        Serial.println(maxPositionADC);
        calibState = 1; // Cambiar a buscar el mínimo
        Serial.println("\n>> Moviendo el fader hacia LIMITE INFERIOR (MIN ADC)...");
        // Mover en dirección negativa (hacia abajo) con la fuerza mínima garantizada
        driveMotorContinuously(-MIN_PWM_DRIVE); 
        lastSensorStableTime = currentTime; // Reiniciar temporizador de estabilidad
      }
      else if(calibState == 1) { // Estábamos buscando el mínimo
        minPositionADC = currentRawADC;
        Serial.print("✔ LIMITE INFERIOR (MIN ADC) encontrado: ");
        Serial.println(minPositionADC);
        finishCalibration(); // Finalizar la calibración
      }
      return; // Salir de la función una vez detectada estabilidad
    }
  }
  else {
    // Si la lectura no es estable, reiniciar el temporizador
    lastSensorStableTime = currentTime;
    lastSensorValue = currentRawADC;
  }
  
  // Detectar Timeout general de calibración (si no se estabiliza a tiempo)
  if((currentTime - lastSensorStableTime) > CALIB_TIMEOUT) {
    calibrationFailed("TIMEOUT en el proceso de calibracion.");
  }
}

void driveMotorContinuously(int pwmSpeed) {
    driverEnable(true); // Asegurarse de que el driver esté habilitado
    driveMotor(pwmSpeed); // Iniciar movimiento
}

void finishCalibration() {
  driveMotor(0); // Detener el motor
  driverEnable(false); // Deshabilitar el driver
  isCalibrated = true;
  calibState = -1; // Marcar como no en calibración
  
  // --- VALIDACIONES DE RANGO MEJORADAS ---
  if(minPositionADC >= maxPositionADC) {
    calibrationFailed("Minimo ADC (" + String(minPositionADC) + ") es mayor o igual que Maximo ADC (" + String(maxPositionADC) + "). Esto indica una inversion del movimiento o fader atascado.");
    return;
  }
  if((maxPositionADC - minPositionADC) < MIN_RANGE_DIFF_ADC) {
     calibrationFailed("Rango ADC detectado (" + String(maxPositionADC - minPositionADC) + ") es demasiado estrecho. Minimo esperado: " + String(MIN_RANGE_DIFF_ADC) + "). El fader no recorre suficiente.");
     return;
  }
  if(minPositionADC > MAX_ADC_DEVIATION_FROM_0) {
      calibrationFailed("Minimo ADC detectado (" + String(minPositionADC) + ") esta muy lejos de 0. Max. desviacion permitida: " + String(MAX_ADC_DEVIATION_FROM_0) + ". Verifique el ajuste del fader.");
      return;
  }
  if(maxPositionADC < ADC_MAX_VALUE - MAX_ADC_DEVIATION_FROM_4095) {
      calibrationFailed("Maximo ADC detectado (" + String(maxPositionADC) + ") esta muy lejos de " + String(ADC_MAX_VALUE) + ". Min. valor esperado: " + String(ADC_MAX_VALUE - MAX_ADC_DEVIATION_FROM_4095) + ". Verifique el ajuste del fader.");
      return;
  }
  // --- FIN DE VALIDACIONES DE RANGO ---

  Serial.print("✅ CALIBRACION COMPLETA. Rango ADC: [");
  Serial.print(minPositionADC);
  Serial.print("-");
  Serial.print(maxPositionADC);
  Serial.println("]");
  
  Serial.println(">> Moviendo a posicion central...");
  moveToPosition(map(50, 0, 100, minPositionADC, maxPositionADC)); 
  
  blinkStatus(3, 200); 
  Serial.println("\n>> SISTEMA LISTO PARA OPERAR!");
}

// Handler de fallos de calibracion
void calibrationFailed(const String& reason) {
    Serial.print("❌ FALLO DE CALIBRACION: ");
    Serial.println(reason);
    Serial.println(">> Acciones recomendadas:");
    Serial.println("   - Verifique que el fader no este atascado.");
    Serial.println("   - Asegure que el motor no este sobrecargado.");
    Serial.println("   - Revise el cableado del motor al DRV8833 y los pines de datos. Puede que esten invertidos.");
    Serial.println("   - Confirme la alimentacion del motor y DRV8833.");
    Serial.println(">> Puede intentar recalibrar con el comando 'calib'.");
    stopSystemEmergently(); // Bloquea el programa
}


// --- FUNCIONES DE MOVIMIENTO EXTERNO ---
void moveToPosition(int targetADC) {
  if (!isCalibrated) {
    Serial.println("⚠️ Sistema no calibrado. No se puede mover.");
    return;
  }
  // Restringir el setpoint al rango ADC calibrado
  targetADC = constrain(targetADC, minPositionADC, maxPositionADC);

  Serial.print("Moviendo a posicion ADC: ");
  Serial.println(targetADC);
  
  driverEnable(true); // Habilitar el driver para el movimiento
  pidSetpoint = targetADC; // Establecer el setpoint directamente en valor ADC
  
  unsigned long lastPrintTime = 0; // Para no saturar el serial con actualizaciones
  unsigned long startTime = millis(); // Para implementar un timeout en el movimiento
  
  // Bucle de movimiento hasta que la posición se acerque al setpoint o haya un timeout
  while (abs(pidInput - pidSetpoint) > 10) { // Tolerancia de 10 unidades ADC
    if (digitalRead(ENABLE_PIN) == LOW) { // Si el driver se deshabilitó manualmente
        Serial.println("Movimiento interrumpido: Driver deshabilitado.");
        break;
    }
    if (millis() - startTime > CALIB_TIMEOUT * 2) { // Timeout doble si el movimiento tarda demasiado
        Serial.println("Movimiento interrumpido: Timeout.");
        break;
    }
    
    pidInput = readSmoothedADC(); // Obtener la lectura más reciente para el PID
    
    if(positionPID.Compute()) { // Recalcular la salida del PID
      int motorPWM = static_cast<int>(pidOutput);
      
      // Aplicar el umbral MIN_PWM_DRIVE
      if (abs(motorPWM) > 0 && abs(motorPWM) < MIN_PWM_DRIVE) {
          motorPWM = (motorPWM > 0) ? MIN_PWM_DRIVE : -MIN_PWM_DRIVE;
      }
      
      driveMotor(motorPWM); // Mover el motor
    }
    
    delay(PID_SAMPLE_RATE); // Pequeña pausa para permitir que el PID actúe y estabilice
    
    // Imprimir el progreso cada cierto tiempo para feedback
    if (millis() - lastPrintTime > 200) {
      Serial.print("  Actual ADC: "); Serial.print(pidInput); Serial.print(" / Objetivo ADC: ");
      Serial.println(pidSetpoint);
      lastPrintTime = millis();
    }
  }
  
  driveMotor(0); // Detener el motor al alcanzar la posición
  driverEnable(false); // Deshabilitar el driver para ahorrar energía/reducir calor
  Serial.println("✔ Posicion alcanzada.");
}

// --- OTRAS FUNCIONES ---
// Lectura del potenciómetro con filtro de media móvil
int readSmoothedADC() {
  adcBuffer[adcIndex] = analogRead(POT_PIN);
  adcIndex = (adcIndex + 1) % ADC_BUFFER_SIZE;
  
  long sum = 0;
  for(int i = 0; i < ADC_BUFFER_SIZE; i++) sum += adcBuffer[i];
  return sum / ADC_BUFFER_SIZE;
}

// Procesamiento de comandos seriales
void readSerialCommands() {
  static String commandBuffer;
  while(Serial.available()) {
    char c = Serial.read();
    if(c == '\n') {
      executeCommand(commandBuffer); 
      commandBuffer = ""; 
    } else if(c != '\r') {
      commandBuffer += c;
    }
  }
}

// Ejecuta un comando basado en la cadena recibida
void executeCommand(String cmd) {
  cmd.trim(); // Eliminar espacios en blanco
  
  Serial.print(">> Comando recibido: "); Serial.println(cmd);
  if(cmd.equalsIgnoreCase("enable")) {
    driverEnable(true);
  }
  else if(cmd.equalsIgnoreCase("disable")) {
    driverEnable(false);
  }
  else if(cmd.equalsIgnoreCase("calib")) {
    calibState = -1;      
    isCalibrated = false; 
    calibrateSystem();    
  }
  else if(cmd.startsWith("goto ")) {
    String valStr = cmd.substring(5);
    if (valStr.length() == 0 || (!valStr.toInt() && valStr != "0")) { 
        Serial.println("❌ Formato incorrecto. Uso: goto [valor_ADC entre 0-4095]");
        return;
    }
    int targetADC = valStr.toInt(); 
    
    if(!isCalibrated) {
      Serial.println("❌ Error: Calibre el sistema primero con 'calib'.");
      return;
    }
    targetADC = constrain(targetADC, minPositionADC, maxPositionADC);
    
    moveToPosition(targetADC); 
  }
  else if(cmd.startsWith("gotopct ")) { 
    String valStr = cmd.substring(8);
     if (valStr.length() == 0 || (!valStr.toFloat() && valStr != "0")) { 
        Serial.println("❌ Formato incorrecto. Uso: gotopct [0-100]");
        return;
    }
    float posPct = valStr.toFloat(); 
    posPct = constrain(posPct, 0.0, 100.0); 
    
    if(!isCalibrated) {
      Serial.println("❌ Error: Calibre el sistema primero con 'calib'.");
      return;
    }
    int targetADC = map(posPct, 0.0, 100.0, minPositionADC, maxPositionADC);
    moveToPosition(targetADC);
  }
  else if(cmd.equalsIgnoreCase("min")) {
    if(isCalibrated) moveToPosition(minPositionADC); else Serial.println("❌ Error: Sistema no calibrado.");
  }
  else if(cmd.equalsIgnoreCase("max")) {
    if(isCalibrated) moveToPosition(maxPositionADC); else Serial.println("❌ Error: Sistema no calibrado.");
  }
  else if(cmd.equalsIgnoreCase("center")) {
    if(isCalibrated) moveToPosition(map(50, 0, 100, minPositionADC, maxPositionADC)); else Serial.println("❌ Error: Sistema no calibrado.");
  }
  else if(cmd.equalsIgnoreCase("stop")) {
    driveMotor(0);       
    driverEnable(false); 
    Serial.println("✔ Motor detenido y driver deshabilitado.");
  }
  else if(cmd.equalsIgnoreCase("status")) {
      Serial.print("  Calibrado: "); Serial.println(isCalibrated ? "SI" : "NO");
      if (isCalibrated) {
          Serial.print("  Rango ADC: [");Serial.print(minPositionADC);
          Serial.print("-"); Serial.print(maxPositionADC); Serial.println("]");
      }
      Serial.print("  Driver: "); Serial.println(digitalRead(ENABLE_PIN) == HIGH ? "HABILITADO" : "DESHABILITADO");
      Serial.print("  Posicion (ADC): "); Serial.println(pidInput); 
      Serial.print("  Setpoint (ADC): "); Serial.println(pidSetpoint); 
      Serial.print("  Output (PID): "); Serial.print(pidOutput); Serial.println(" (PWM)");
  }
  else if(cmd.equalsIgnoreCase("help")) {
    Serial.println("\n📌 COMANDOS DISPONIBLES:");
    Serial.println("  enable      - Habilita el driver del motor");
    Serial.println("  disable     - Deshabilita el driver del motor (ahorro energia)");
    Serial.println("  calib       - Inicia calibracion automatica del fader");
    Serial.println("  goto [0-4095]- Mueve el fader a una posicion ADC deseada (valor absoluto)");
    Serial.println("  gotopct [0-100]- Mueve el fader a una posicion porcentual (con rango calibrado)");
    Serial.println("  min / max   - Mueve el fader a los limites (minPositionADC / maxPositionADC)");
    Serial.println("  center      - Mueve el fader al centro del rango calibrado");
    Serial.println("  stop        - Detiene el motor y deshabilita el driver");
    Serial.println("  status      - Muestra el estado actual del sistema");
    Serial.println("  help        - Muestra esta lista de comandos");
  }
  else {
    Serial.print("❌ Comando no reconocido: "); Serial.println(cmd);
  }
}
void stopSystemEmergently() {
  driveMotor(0);       
  driverEnable(false); 
  
  Serial.println("\n!!! PARADA DE EMERGENCIA ACTIVADA !!!");
  Serial.println(">> Por favor, revise el sistema o reinicie.");
  
  for(int i=0; i<10; i++) {
    digitalWrite(STATUS_LED, HIGH); delay(100);
    digitalWrite(STATUS_LED, LOW);  delay(100);
  }
  while(1); 
}
void blinkStatus(int count, int duration) {
  for(int i=0; i<count; i++) {
    digitalWrite(STATUS_LED, HIGH); delay(duration);
    digitalWrite(STATUS_LED, LOW);  delay(duration);
  }
}