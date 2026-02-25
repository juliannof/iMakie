// src/main.cpp
#include <Arduino.h>
#include <PID_v1.h> 

// === PINES ===
// ADVERTENCIA ABOGADO DEL DIABLO:
// GPIO 16 (MOTOR_IN1) y GPIO 3 (FADER_PIN) pueden NO ser los ideales para PWM y ADC
// en un ESP32-S2 Lolin Mini. Especialmente GPIO3, que a menudo está atado a UART/boot.
// Se mantiene debido a la instrucción, pero tenlo en cuenta si los pines "no se mueven" o si hay ruido.
#define MOTOR_IN1 16
#define MOTOR_IN2 18  // GPIO 18 suele ser buen pin PWM en S2
#define nSLEEP 35     // Pin de salida general
#define FADER_PIN 3   // POSIBLE PUNTO DE FALLO: Verificar si GPIO3 tiene ADC funcional en tu ESP32-S2 Mini

// === ZONAS ESPECÍFICAS (Valores de REFERENCIA para los extremos del ADC 0-4095) ===
const int ZONA_MAX = 4095;       
const int ZONA_MIN = 0;          
const int TOLERANCIA_ZONA = 100; 

// === VARIABLES DE ESTADO ===
int fader_min = 0;
int fader_max = 4095;            
int fader_center = 2047;         
int current_position = 0;
int motor_power = 100;           // Potencia MAXIMA que el PID puede usar
bool calibration_done = false;   

// === CONFIGURACIÓN DE HARDWARE ===
const int PWM_FREQ = 20000;      
const int PWM_RESOLUTION = 8;    
const int CHANGE_THRESHOLD = 50; 
const int TARGET_ARRIVAL_TOLERANCE = 10; 
const int PID_SAMPLE_TIME_MS = 50; // ABOGADO DEL DIABLO: Asumiremos este valor del PID para los delays.

// --- VARIABLES PID ---
double Setpoint;      
double Input;         
double Output;        

double Kp = 0.5, Ki = 0.01, Kd = 0.1; // Coeficientes iniciales (ejemplo, requerirán sintonización)

PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT); 

// --- DECLARACIÓN DE FUNCIONES ---
void setupMotor();
void setMotor(bool enable, bool direction_forward, int power = -1); 
int readFader();
bool waitForZone(const char* zone_name, int target_zone, int timeout_ms = 20000); 
void testMotorToZone(int target_zone, const char* zone_name); 
void zoneCalibrationAutomated(); 
bool moveToPosition(int targetPosition, int maxMoveTime_ms = 15000); 
void showPIDTunings(); 
void setPIDTunings(double p, double i, double d); 

// === IMPLEMENTACIÓN DE FUNCIONES ===
void setupMotor() {
  pinMode(nSLEEP, OUTPUT);
  digitalWrite(nSLEEP, LOW); 

  bool setup_ch0_ok = ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
  bool setup_ch1_ok = ledcSetup(1, PWM_FREQ, PWM_RESOLUTION);
  
  ledcAttachPin(MOTOR_IN1, 0); 
  ledcAttachPin(MOTOR_IN2, 1);

  if (!setup_ch0_ok || !setup_ch1_ok) {
    Serial.println("❌ ERROR FATAL: Fallo en la configuración de PWM.");
    Serial.printf("   Canal 0 setup: %s\n", setup_ch0_ok ? "OK" : "FALLÓ");
    Serial.printf("   Canal 1 setup: %s\n", setup_ch1_ok ? "OK" : "FALLÓ");
    Serial.println("   Revisa logs de compilación, pinout del ESP32-S2 y documentación ledc.");
    while (true) { delay(1000); } 
  }

  ledcWrite(0, 0); 
  ledcWrite(1, 0);
}

void setMotor(bool enable, bool direction_forward, int power) { 
  if (power == -1) power = motor_power; 
  power = constrain(power, 0, 255); 

  if (enable) {
    digitalWrite(nSLEEP, HIGH); 
    if (direction_forward) {
      ledcWrite(0, power); 
      ledcWrite(1, 0);
    } else {
      ledcWrite(0, 0);
      ledcWrite(1, power); 
    }
  } else {
    ledcWrite(0, 0); 
    ledcWrite(1, 0);
    digitalWrite(nSLEEP, LOW); 
  }
}

int readFader() {
  return analogRead(FADER_PIN);
}

bool waitForZone(const char* zone_name, int target_zone, int timeout_ms) { 
  Serial.printf("\n🎯 Mueve el fader a la ZONA %s (alrededor de %d)\n", zone_name, target_zone);
  Serial.printf("   Rango aceptado: %d - %d\n",
                target_zone - TOLERANCIA_ZONA,
                target_zone + TOLERANCIA_ZONA);
  Serial.println("   Cuando esté en zona, presiona ESPACIO en el monitor serial.");

  unsigned long start_time = millis();
  int prev_position = -1; 
  unsigned long static_fader_start = 0; 

  char lineBuffer[100]; 

  while (millis() - start_time < timeout_ms) {
    current_position = readFader();
    int error = abs(current_position - target_zone);

    if (prev_position == current_position) {
        if (static_fader_start == 0) static_fader_start = millis();
        if (millis() - static_fader_start > 5000 && error > TOLERANCIA_ZONA) { 
            Serial.println("\n⚠️  ADVERTENCIA DE FADER: La lectura del fader no cambia, o el FADER_PIN puede ser incorrecto/desconectado.");
            Serial.printf("   Valor actual: %d. Error: %d. ¿Está el fader conectado al GPIO %d?\n", current_position, error, FADER_PIN);
            static_fader_start = 0; 
        }
    } else {
        static_fader_start = 0;
    }
    prev_position = current_position;

    if (error <= TOLERANCIA_ZONA) {
      snprintf(lineBuffer, sizeof(lineBuffer), "   ✅ EN ZONA: %d (error: %d)", current_position, error); 
    } else {
      snprintf(lineBuffer, sizeof(lineBuffer), "   🎯 Acercando... %d → %d (error: %d)",
                     current_position, target_zone, error); 
    }
    Serial.printf("\r%-80s", lineBuffer); 

    if (Serial.available()) {
      char cmd = Serial.read();
      if (cmd == ' ') {
        if (abs(current_position - target_zone) <= TOLERANCIA_ZONA) {
          Serial.printf("\n✅ Perfecto! Zona %s confirmada: %d\n", zone_name, current_position);
          return true;
        } else {
          Serial.printf("\n⚠️  ACTUALMENTE fuera de zona ideal (%d) - posición actual: %d (error: %d).\n",
                       target_zone, current_position, abs(current_position - target_zone));
          Serial.println("   ¿Desea FORZAR esta posición como válida? (s/N)");
          unsigned long confirm_timeout = millis();
          while (!Serial.available() && (millis() - confirm_timeout < 5000)); 

          if (Serial.available()) {
              char confirm_cmd = Serial.read();
              if (tolower(confirm_cmd) == 's') {
                  Serial.println("   Confirmado. Usando posición actual.");
                  return true;
              }
          }
          Serial.println("   No confirmado o timeout. Continúe moviendo el fader o pruebe de nuevo.");
          start_time = millis(); 
      }
      }
       while(Serial.available()) Serial.read();
    }
    delay(200);
  }
  Serial.println("\n❌ Timeout - no se alcanzó la zona o no se presionó ESPACIO a tiempo. Usando posición actual.");
  return false;
}

void testMotorToZone(int target_zone, const char* zone_name) {
  Serial.printf("\n🔧 TEST de motor hacia ZONA %s (~%d)\n", zone_name, target_zone);

  int initial_position = readFader(); 
  current_position = initial_position; 
  bool should_go_forward = (target_zone > initial_position);

  Serial.printf("   Posición actual: %d\n", initial_position);
  Serial.printf("   Objetivo: ~%d (%s)\n", target_zone, zone_name);
  Serial.printf("   Dirección: %s\n", should_go_forward ? "ADELANTE" : "ATRÁS");
  Serial.println("   Iniciando movimiento...");

  setMotor(true, should_go_forward, motor_power); // Usa la potencia máxima para este test
  unsigned long test_start = millis();
  int start_position_motor_test = initial_position; 

  bool movement_detected = false;
  bool wrong_direction = false;
  bool reached_zone = false;
  unsigned long last_movement_check = millis();
  int last_recorded_position = initial_position;
  char lineBuffer[100]; 

  while (millis() - test_start < 8000) { 
    current_position = readFader();
    int change_overall = current_position - start_position_motor_test; 
    int change_since_last_check = abs(current_position - last_recorded_position); 

    int error_to_zone = abs(current_position - target_zone);

    if (millis() - last_movement_check > 1000) { 
        if (change_since_last_check < 5 && !reached_zone && abs(change_overall) < CHANGE_THRESHOLD) { 
            Serial.println("\n⚠️  ADVERTENCIA: Fader estancado o motor no se mueve lo suficiente.");
            Serial.printf("   Posición actual: %d. Objetivo: %d. Último cambio importante: %d.\n",
                          current_position, target_zone, change_since_last_check);
        }
        last_movement_check = millis(); 
        last_recorded_position = current_position; 
    }

    if (error_to_zone <= TOLERANCIA_ZONA) { 
      snprintf(lineBuffer, sizeof(lineBuffer), "   ✅ EN ZONA: %d (Objetivo: %d)", current_position, target_zone); 
      reached_zone = true;
    } else {
      snprintf(lineBuffer, sizeof(lineBuffer), "   Movimiento... %d (Actual) → %d (Objetivo) | Cambio total: %d",
                   current_position, target_zone, change_overall); 
    }
    Serial.printf("\r%-80s", lineBuffer); 

    if (abs(change_overall) > CHANGE_THRESHOLD) {
      movement_detected = true;
      if ((should_go_forward && change_overall < -50) || (!should_go_forward && change_overall > 50)) {
        wrong_direction = true;
        break; 
      }
    }

    if (reached_zone) break;
    delay(200);
  }

  setMotor(false, should_go_forward); 
  Serial.println(); 

  if (wrong_direction) {
    Serial.println("❌ PROBLEMA GRAVE: El motor se mueve en dirección CONTRARIA a la esperada.");
    Serial.println("💡 SOLUCIÓN: Es casi seguro un error de cableado. Prueba a intercambiar los cables del motor (IN1 ↔ IN2)");
    Serial.println("   (Luego, si estás en modo automático, reinicia o espera la siguiente fase)"); 
    return; 
  }

  if (!movement_detected) {
    Serial.println("⚠️  ADVERTENCIA: El fader NO SE MOVIÓ significativamente con la potencia actual.");
    // Comentario corregido: Ya no se modifica motor_power aquí.
    Serial.printf("   Potencia máxima actual del PID es: %d/255. Considera ajustar Kp/Ki con 't' o la potencia máxima con 'p'.\n", motor_power); 
    return; 
  }

  if (reached_zone) {
    Serial.printf("✅ ÉXITO: El motor movió el fader a la zona %s (posición: %d).\n", zone_name, current_position);
  } else {
    Serial.printf("⚠️  ADVERTENCIA: El motor se movió, pero NO LLEGÓ a la zona %s. Posición final: %d.\n",
                 zone_name, current_position);
    Serial.println("   Esto podría indicar baja potencia, obstrucción, o una zona objetivo inalcanzable.");
  }
}

bool moveToPosition(int targetPosition, int maxMoveTime_ms) { 
    targetPosition = constrain(targetPosition, fader_min, fader_max); 

    Serial.printf("\n➡️ Moviendo fader a la posición objetivo: %d\n", targetPosition);
    unsigned long move_start_time = millis();
    int last_position_read = readFader(); 
    unsigned long last_movement_check_time = move_start_time;
    int prev_position_for_static_check = last_position_read;

    char lineBuffer[100]; 

    Setpoint = targetPosition; 
    
    myPID.SetMode(MANUAL);  
    Output = 0;             
    myPID.SetMode(AUTOMATIC); 
    
    Input = readFader();
    myPID.Compute(); 

    bool reached = false;
    while (!reached && (millis() - move_start_time < maxMoveTime_ms)) {
        Input = readFader(); 
        current_position = Input; 
        int error = Setpoint - current_position; 
        
        myPID.Compute(); 

        if (abs(error) <= TARGET_ARRIVAL_TOLERANCE) {
            setMotor(false, true); 
            reached = true;
            Serial.print("\r                                                                            \r"); 
            Serial.printf("✅ ALCANZADO: Fader en %d (Objetivo: %.0f, Error: %d)\n", current_position, Setpoint, error);
            break;
        } 
        
        if (Output > 0) { 
            setMotor(true, true, abs(static_cast<int>(Output))); 
        } else if (Output < 0) { 
            setMotor(true, false, abs(static_cast<int>(Output))); 
        } else { 
            setMotor(false, true); 
        }

        if (millis() - last_movement_check_time > 1000) { 
            if (abs(current_position - prev_position_for_static_check) < 5 && abs(error) > TARGET_ARRIVAL_TOLERANCE * 2) { 
                Serial.print("\r                                                                            \r"); 
                Serial.printf("⚠️  ADVERTENCIA: Motor estancado intentando alcanzar %.0f. Pos. actual: %d. Error: %d. Deteniendo.\n", Setpoint, current_position, error);
                setMotor(false, true); 
                return false; 
            }
            last_movement_check_time = millis();
            prev_position_for_static_check = current_position;
        }

        snprintf(lineBuffer, sizeof(lineBuffer), "   Moviendo... %d -> %.0f (Error: %d, PID Output: %.1f)", current_position, Setpoint, error, Output);
        Serial.printf("\r%-80s", lineBuffer); 
        
        delay(max(10, PID_SAMPLE_TIME_MS / 2)); 
    }

    setMotor(false, true); 
    if (!reached) {
        Serial.print("\r                                                                            \r"); 
        Serial.printf("❌ TIMEOUT: No se logró alcanzar la posición %.0f en %dms. Posición final: %d. Error: %d\n", Setpoint, maxMoveTime_ms, current_position, (static_cast<int>(Setpoint) - current_position));
    }
    return reached; 
}

void zoneCalibrationAutomated() {
  Serial.println("🎛️  INICIANDO CALIBRACIÓN AUTOMATIZADA");
  Serial.println("======================================");
  
  Serial.println("\n1. Moviendo a ZONA MÁXIMA (se irá hasta el tope superior)...");
  moveToPosition(ZONA_MAX + 1000, 15000); 
  fader_max = readFader(); 
  Serial.printf("✅ MÁXIMO establecido por tope: %d\n", fader_max);

  Serial.println("\n2. Moviendo a ZONA MÍNIMA (se irá hasta el tope inferior)...");
  moveToPosition(ZONA_MIN - 1000, 15000); 
  fader_min = readFader(); 
  Serial.printf("✅ MÍNIMO establecido por tope: %d\n", fader_min);

  if (fader_min >= fader_max) {
      Serial.println("⚠️  ADVERTENCIA CRÍTICA: Los valores de MÍNIMO y MÁXIMO se invirtieron o son iguales después de la calibración automática.");
      Serial.printf("   MÍNIMO: %d, MÁXIMO: %d. Verifica la conexión del motor y fader. Ajustando para evitar errores lógicos.\n", fader_min, fader_max);
      int temp_min = fader_min;
      int temp_max = fader_max;
      fader_min = min(temp_min, temp_max); 
      fader_max = max(temp_min, temp_max); 
      if (fader_min >= fader_max) fader_min = fader_max - 50; 
      if (fader_min < 0) fader_min = 0;
      if (fader_max > 4095) fader_max = 4095; 
      Serial.printf("   Valores corregidos: MÍNIMO: %d, MÁXIMO: %d\n", fader_min, fader_max);
  }

  fader_center = (fader_min + fader_max) / 2;

  Serial.println("\n3. TEST DE DIRECCIÓN del MOTOR: Moviendo brevemente hacia una zona media-alta...");
  int test_zone = fader_min + (fader_max - fader_min) * 2 / 3; 
  testMotorToZone(test_zone, "MEDIA-ALTA"); 

  Serial.println("\n4. MOVIENDO el fader automáticamente al CENTRO calculado.");
  Serial.printf("   Centro calculado: %d\n", fader_center);
  moveToPosition(fader_center, 10000); 
  
  calibration_done = true;
  Serial.println("\n🎉 CALIBRACIÓN AUTOMÁTICA COMPLETADA 🎉");
  Serial.printf("   Parámetros finales:\n");
  Serial.printf("   MÍNIMO detectado: %d (basado en exploración a ZONA_MIN)\n", fader_min); 
  Serial.printf("   MÁXIMO detectado: %d (basado en exploración a ZONA_MAX)\n", fader_max);
  Serial.printf("   CENTRO calculado: %d\n", fader_center);
  Serial.printf("   RANGO total útil: %d unidades (de %d a %d)\n", fader_max - fader_min, fader_min, fader_max);
  Serial.printf("   Potencia inicial del motor (Max PID): %d/255\n", motor_power);
}

void showPIDTunings() {
  Serial.printf("\n📊 --- PID TUNINGS ACTUALES --- \n");
  Serial.printf("   Kp: %.3f, Ki: %.3f, Kd: %.3f\n", Kp, Ki, Kd);
  Serial.println("-----------------------------------");
}

void setPIDTunings(double p, double i, double d) {
  Kp = p;
  Ki = i;
  Kd = d;
  myPID.SetTunings(Kp, Ki, Kd); 
  Serial.printf("✅ PID Tunings actualizados a: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", Kp, Ki, Kd);
}

// Función de configuración inicial
void setup() {
  Serial.begin(115200);
  delay(100); 
  Serial.println("\nEsperando conexión del Monitor Serial. Por favor, abre el monitor.");
  while (!Serial); 

  Serial.println("\n--- INICIO DEL SISTEMA DE FADER-MOTOR [MODO AUTOMÁTICO] ---");
  Serial.println("=========================================================");
  Serial.printf("   FADER_PIN en GPIO %d. Motores en GPIO %d y %d. nSLEEP en GPIO %d.\n", FADER_PIN, MOTOR_IN1, MOTOR_IN2, nSLEEP);
  delay(1000); 

  setupMotor();      
  
  myPID.SetMode(AUTOMATIC); 
  myPID.SetSampleTime(PID_SAMPLE_TIME_MS); 
  myPID.SetTunings(Kp, Ki, Kd); 

  zoneCalibrationAutomated(); 

  Serial.println("\n--- MODO DE OPERACIÓN PRINCIPAL ---");
  Serial.println("   Introduce un valor numérico (posición ADC) o un porcentaje (0-100%)");
  Serial.println("   para mover el fader a esa posición. También puedes usar los siguientes comandos:");
  Serial.println("   'c' = Re-calibrar (automáticamente)");
  Serial.println("   's' = Mostrar estado");
  Serial.println("   'p' = Ajustar potencia (80-200)");
  Serial.println("   't' = Establecer tunings PID (ej: t 0.5 0.01 0.1)"); 
  Serial.println("   'u' = Mostrar tunings PID actuales"); 
  Serial.println("   '0' = Parar motor");
  Serial.println("-----------------------------------");
  Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
}

// Bucle principal del programa
void loop() {
  current_position = readFader(); 

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); 
    input.trim(); 
    
    if (!input.isEmpty() && (input.toInt() != 0 || input.equals("0"))) { 
        int targetValueNum = input.toInt(); 
        
        bool is_percentage_input = false; 
        if (input.indexOf('%') != -1) {
             is_percentage_input = true;
        } else if (targetValueNum >= 0 && targetValueNum <= 100 && input.length() <= 3 && !input.startsWith("0x")) { 
            is_percentage_input = true;
        }

        int targetValueADC_Mapped;

        if (is_percentage_input) {
             targetValueNum = constrain(targetValueNum, 0, 100); 
             targetValueADC_Mapped = map(targetValueNum, 0, 100, fader_min, fader_max);
             Serial.printf("Comando porcentaje: %s (= %d%%) -> Posición ADC objetivo: %d\n", input.c_str(), targetValueNum, targetValueADC_Mapped);
        } else { 
            targetValueADC_Mapped = targetValueNum;
            Serial.printf("Comando posición ADC: %s -> Posición ADC objetivo: %d\n", input.c_str(), targetValueADC_Mapped);
        }
        
        if (targetValueADC_Mapped >= fader_min && targetValueADC_Mapped <= fader_max) { 
            myPID.SetTunings(Kp, Ki, Kd); 
            myPID.SetOutputLimits(-motor_power, motor_power); 
            if (!moveToPosition(targetValueADC_Mapped)) { 
                Serial.println("⚠️  ERROR: No se pudo mover el fader a la posición. Revisa la conexión o recalibra.");
            }
            Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
        } else {
            Serial.printf("❌ Valor '%s' (transformado a %d) fuera del rango calibrado (%d-%d ADC). Si fue un porcentaje, usa el símbolo '%%' o un número 0-100.\n", input.c_str(), targetValueADC_Mapped, fader_min, fader_max);
            Serial.println("   Comandos: 'c' = Recalibrar | 's' = estado | 'p' = potencia | 't' = tunings | 'u' = ver tunings | '0' = parar"); 
        }

    } else { 
      char cmd_char_processed = tolower(input.charAt(0)); 
      switch(cmd_char_processed) {
        case 'c':
          Serial.println("\n>>> INICIANDO RE-CALIBRACIÓN AUTOMÁTICA POR SOLICITUD DEL USUARIO <<<");
          zoneCalibrationAutomated(); 
          Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
          break;
        case 's':
          if (calibration_done) {
            int percentage = 0;
            if (fader_max != fader_min) { 
                percentage = map(current_position, fader_min, fader_max, 0, 100);
                percentage = constrain(percentage, 0, 100); 
            } else {
                Serial.println("⚠️  ADVERTENCIA: Rango de fader inválido (Max == Min). El porcentaje no es fiable.");
            }
            int error_to_ref_max = abs(current_position - ZONA_MAX); 
            int error_to_ref_min = abs(current_position - ZONA_MIN); 

            Serial.printf("\n📊 --- ESTADO ACTUAL DEL SISTEMA --- \n");
            Serial.printf("   Posición actual fader: %d (aproximadamente %d%% de su rango)\n", current_position, percentage);
            Serial.printf("   Límites calibrados: MIN %d - MAX %d | CENTRO: %d\n", fader_min, fader_max, fader_center);
            Serial.printf("   Distancia a REF_MAX (%d): %d | Distancia a REF_MIN (%d): %d\n", ZONA_MAX, error_to_ref_max, ZONA_MIN, error_to_ref_min);
            Serial.printf("   Potencia actual del motor (Max PID Output): %d/255\n", motor_power);
            showPIDTunings(); 
            if (!calibration_done) Serial.println("   ADVERTENCIA: La calibración aún no se ha completado.");
            Serial.println("-----------------------------------");
          } else {
            Serial.printf("📍 Posición actual fader: %d (Calibración aún no completada)\n", current_position);
          }
          Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
          break;
        
        case 'p': { 
          Serial.println("\n🔧 Ajustar potencia MÁXIMA del motor (Output límite del PID) (ingresa un número entre 80 y 200 y presiona ENTER):");
          Serial.println("   (Esperando el nuevo valor de potencia. Introduce solo el número.)");
          while (!Serial.available()) delay(100); 
          String power_input = Serial.readStringUntil('\n'); 
          power_input.trim(); 
          int new_power = power_input.toInt();

          if (new_power >= 80 && new_power <= 200) {
              motor_power = new_power; 
              Serial.printf("✅ Nueva potencia MÁXIMA del motor establecida a: %d/255\n", motor_power);
          } else {
              Serial.printf("❌ Entrada inválida o fuera de rango (80-200). La potencia sigue en %d/255.\n", motor_power);
          }
          Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
          break; 
        } 

        case 't': { 
            Serial.println("\n🔧 Ajustar PID Tunings (Sintaxis: t Kp Ki Kd. Ej: t 0.5 0.01 0.1 y ENTER):");
            while (!Serial.available()) delay(100); 
            String tunings_line = Serial.readStringUntil('\n');
            tunings_line.trim();

            double new_kp = 0.0, new_ki = 0.0, new_kd = 0.0;
            char buffer[50];
            tunings_line.toCharArray(buffer, sizeof(buffer)); 
            
            // Usar sscanf para parsear los 3 doubles
            // ABOGADO DEL DIABLO: sscanf es potente pero frágil a errores de formato.
            // Necesitamos que el usuario introduzca "t Kp Ki Kd" y que el input
            // se lea asì:
            // Por ejemplo, "t 0.5 0.01 0.1"
            // La String input contendrá "t 0.5 0.01 0.1".
            // Para sscanf, no debemos pasarle la 't'.
            
            // Primero, asegurarnos de que el primer carácter sea 't' (ya lo hemos hecho con cmd_char_processed)
            // Luego, saltemos el 't' y el espacio
            int start_parse = tunings_line.indexOf(' ');
            if (start_parse != -1) {
                String numbers_str = tunings_line.substring(start_parse + 1);
                numbers_str.toCharArray(buffer, sizeof(buffer));
                int num_read = sscanf(buffer, "%lf %lf %lf", &new_kp, &new_ki, &new_kd);
                
                if (num_read == 3) {
                    setPIDTunings(new_kp, new_ki, new_kd);
                    Serial.printf("✅ PID Tunings actualizados.\n");
                } else {
                    Serial.printf("❌ Formato incorrecto. No se pudieron leer 3 valores. Uso: t Kp Ki Kd (ej: t 0.5 0.01 0.1)\n");
                }
            } else {
                 Serial.printf("❌ Formato incorrecto. Uso: t Kp Ki Kd (ej: t 0.5 0.01 0.1)\n");
            }
            Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
            break;
        }

        case 'u': 
            showPIDTunings();
            Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
            break;


        case '0': 
          setMotor(false, true); 
          Serial.println("🛑 Motor parado por comando.");
          Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
          break;
        
        default: 
          Serial.printf("Comando '%s' no reconocido. Introduce un número (ADC o Porcentaje) o usa 'c', 's', 'p', 't', 'u', '0'.\n", (input.length() > 0 ? input.c_str() : "(vacío)"));
          Serial.printf("Fader actual: %d. Introduce una nueva posición (o comando):\n", readFader());
          break;
      }
    }
  // CORRECCIÓN: Falta la llave de cierre para if (Serial.available())
  } // <--- ¡AQUÍ ESTÁ LA LLAVE QUE FALTABA!
  delay(10); 
}