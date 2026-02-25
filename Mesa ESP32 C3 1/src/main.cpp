#include <Arduino.h>
#include <AccelStepper.h>

// Pines de conexión
const int trigPin = 10;     // Trigger del sensor ultrasónico
const int echoPin = 7;      // Echo del sensor ultrasónico
const int dirPin = 5;       // Dirección del motor
const int stepPin = 6;      // Paso del motor
const int enablePin = 20;   // Habilitación del motor
const int endstopPin = 21;  // Final de carrera

// Parámetros configurables
const int UMBRAL_DISTANCIA = 4;       // Distancia de activación en cm
const long MAX_TRAVEL_STEPS = 40000; // Máximo de pasos para ALEJARSE desde el Home (recorrido total)
const float MAX_SPEED = 1500.0;       // Velocidad máxima en pasos/seg
const float ACCELERATION = 500.0;     // Aceleración en pasos/seg^2
const long TIMEOUT_US = 25000;        // Timeout para el sensor ultrasónico (25ms)
const unsigned long SENSOR_DELAY = 100; // Tiempo entre lecturas del sensor
const float HOMING_SPEED = 1500.0;    // Velocidad de homing
const int HOMING_BACK_OFF_STEPS = 100; // Pasos para retroceder después del home

// Objeto AccelStepper
AccelStepper stepper(AccelStepper::DRIVER, stepPin, dirPin);

// --- Máquina de Estados ---
// Definición de los posibles estados del sistema
typedef enum {
    SYSTEM_INIT,               // Estado inicial de configuración en setup
    HOMING_START,              // Comenzar la secuencia de homing
    HOMING_SEARCHING,          // Movimiento para encontrar el final de carrera
    HOMING_BACKOFF,            // Retroceder después de encontrar el final de carrera
    IDLE_AT_HOME_POSITION,     // El motor está en la posición 0, esperando activación
    MOVING_AWAY_FROM_HOME,     // Movimiento desde 0 hacia MAX_TRAVEL_STEPS
    IDLE_AWAY_FROM_HOME,       // El motor está en MAX_TRAVEL_STEPS, esperando activación
    MOVING_TOWARDS_HOME,       // Movimiento desde MAX_TRAVEL_STEPS hacia 0
    ERROR_ENDSTOP_UNEXPECTED,  // Error: final de carrera activado en movimiento inesperado
    ERROR_HOME_NOT_FOUND,      // Error: no se encontró home en MOVING_TOWARDS_HOME
    ERROR_GENERAL              // Otros errores
} MotorState;

MotorState currentMotorState = SYSTEM_INIT; // Estado actual
MotorState lastStableState = IDLE_AT_HOME_POSITION; // Ayuda para recuperación de errores

// --- Variables para control de eventos y tiempos ---
volatile bool endstopTriggered = false; // Bandera de interrupción del final de carrera
unsigned long lastSensorRead = 0;       // Último tiempo de lectura del sensor
unsigned long errorStateStartTime = 0;  // Para gestionar estados de error (ej: timeout)

// --- Interrupción para final de carrera ---
void IRAM_ATTR handleEndstop() {
  endstopTriggered = true;
  // NO llamar a stepper.stop() aquí. Solo establece la bandera.
}

// --- Mide la distancia con el sensor ultrasónico ---
float measureDistance() {
  // Limpiar el pin Trig
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Establecer el pin Trig en HIGH durante 10 us
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Medir la duración del pulso en el pin Echo
  long duration = pulseIn(echoPin, HIGH, TIMEOUT_US);
  
  // Si pulseIn retorna 0, significa timeout o error
  if (duration == 0 || duration > TIMEOUT_US) {
    return -1; // Valor de error o fuera de rango
  }
  // Calcular la distancia en cm
  return (duration * 0.0343) / 2; // (velocidad del sonido 0.0343 cm/µs)
}

// --- Rutina de Homing ---
void performHoming() {
    Serial.println("Iniciando secuencia de Homing...");
    currentMotorState = HOMING_SEARCHING;
    endstopTriggered = false; // Resetear la bandera para una nueva búsqueda

    stepper.enableOutputs();
    stepper.setMaxSpeed(HOMING_SPEED);
    stepper.setAcceleration(ACCELERATION); // Puedes usar una aceleración diferente para homing si quieres.

    // Moverse lentamente en una dirección hasta que el final de carrera se active
    // Asumimos que el final de carrera está en la posición '0' del recorrido.
    // Usamos un valor muy grande para asegurarnos de que lo alcanzará.
    stepper.moveTo(-MAX_TRAVEL_STEPS * 2); // Mover hacia una dirección negativa
    
    // Bucle para esperar que se encuentre el final de carrera
    while(!endstopTriggered) {
        stepper.run();
        // Opcional: Implementar un timeout si el final de carrera nunca se encuentra
        // if (millis() - homingStartTime > HOMING_TIMEOUT_MS) {
        //   currentMotorState = ERROR_HOME_NOT_FOUND;
        //   Serial.println("Error: Timeout de homing!");
        //   stepper.disableOutputs();
        //   return;
        // }
    }

    // Final de carrera activado
    Serial.println("Final de carrera encontrado.");
    stepper.stop(); // Detener el motor con deceleración
    while(stepper.isRunning()) { // Esperar a que la deceleración termine
        stepper.run();
    }
    
    // Establecer la posición actual como el "cero"
    stepper.setCurrentPosition(0);
    Serial.println("Home establecido en posicion 0.");

    // Opcional: Retroceder unos pasos para liberar el final de carrera
    // y evitar que quede continuamente pulsado
    if (HOMING_BACK_OFF_STEPS > 0) {
        currentMotorState = HOMING_BACKOFF;
        Serial.print("Retrocediendo ");
        Serial.print(HOMING_BACK_OFF_STEPS);
        Serial.println(" pasos para liberar el final de carrera.");
        stepper.move(HOMING_BACK_OFF_STEPS); // Mover en dirección positiva
        while(stepper.isRunning()) {
            stepper.run();
        }
        Serial.println("Retroceso completado.");
    }

    stepper.disableOutputs(); // Desactivar el motor
    stepper.setMaxSpeed(MAX_SPEED); // Restaurar velocidad máxima normal
    currentMotorState = IDLE_AT_HOME_POSITION; // El sistema está listo
    Serial.println("Homing completado. Sistema en IDLE_AT_HOME_POSITION.");
    endstopTriggered = false; // Resetear la bandera de nuevo
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println("------------------------------------");
  Serial.println("Sistema de control con AccelStepper");
  Serial.println("------------------------------------");
  
  // Configura pines
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, HIGH);  // Desactiva el motor inicialmente (si AccelStepper no toma el control aún)
  
  // Configura final de carrera
  pinMode(endstopPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(endstopPin), handleEndstop, FALLING);
  
  // Configura sensor ultrasónico
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Configura AccelStepper
  stepper.setEnablePin(enablePin);
  stepper.setPinsInverted(true, false, true); // Invierte la lógica de ENABLE (activado en LOW)
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.disableOutputs(); // Asegura que el motor esté desactivado

  // Ejecutar la rutina de homing
  performHoming();
}

// --- LOOP ---
void loop() {
  // Comprobar y manejar la bandera del final de carrera primero.
  // La forma en que se maneja depende del estado actual del sistema.
  if (endstopTriggered) {
      endstopTriggered = false; // Resetear la bandera inmediatamente
      switch (currentMotorState) {
          case HOMING_SEARCHING:
              // Este caso ya se maneja directamente en performHoming() que es bloqueante.
              // En un loop() no bloqueante, se manejaría de forma diferente.
              break; // No requiere acción aquí, performHoming está en un while
          case MOVING_TOWARDS_HOME:
              // Se esperaba el final de carrera, es una condición normal de fin de movimiento.
              Serial.println("FDC durante MOVING_TOWARDS_HOME: encontrado.");
              stepper.stop(); // Detener el motor suavemente
              while(stepper.isRunning()) { stepper.run(); } // Esperar deceleración
              stepper.setCurrentPosition(0); // Restablecer la posición a 0
              if (HOMING_BACK_OFF_STEPS > 0) {
                  stepper.move(HOMING_BACK_OFF_STEPS);
                  Serial.println("FDC: Retrocediendo para liberar.");
                  while(stepper.isRunning()) { stepper.run(); }
              }
              stepper.disableOutputs();
              currentMotorState = IDLE_AT_HOME_POSITION;
              Serial.println("Motor en IDLE_AT_HOME_POSITION despues de FDC.");
              break;
          case MOVING_AWAY_FROM_HOME:
              // Error: el final de carrera se activó mientras nos alejábamos de él (comportamiento inesperado)
              Serial.println("ERROR: FDC activado mientras se movía lejos del Home!");
              stepper.stop(); // Detener inmediatamente
              while(stepper.isRunning()) { stepper.run(); } // Esperar deceleración
              stepper.disableOutputs();
              currentMotorState = ERROR_ENDSTOP_UNEXPECTED;
              errorStateStartTime = millis(); // Registrar tiempo de inicio del error
              break;
          case IDLE_AT_HOME_POSITION:
          case IDLE_AWAY_FROM_HOME:
              // FDC activado en estado inactivo. Podría ser un contacto accidental o un problema.
              // Podríamos simplemente registrarlo o entrar en un estado de error.
              Serial.println("ADVERTENCIA: FDC activado en estado IDLE.");
              // En este caso, simplemente reseteamos para que el sistema continúe
              // No cambiar el estado a error a menos que sea crítico
              break;
          default:
              Serial.println("ERROR: FDC activado en estado desconocido/inesperado!");
              stepper.stop();
              while(stepper.isRunning()) { stepper.run(); }
              stepper.disableOutputs();
              currentMotorState = ERROR_GENERAL;
              errorStateStartTime = millis();
              break;
      }
      return; // Salir temprano del loop para procesar la interrupción primero
  }

  // Ejecutar el motor si está en movimiento
  if (stepper.isRunning()) {
      stepper.run();
  }

  // --- Lógica del sensor y máquina de estados principal ---
  unsigned long currentMillis = millis();

  // Lectura no bloqueante del sensor ultrasónico
  if (currentMillis - lastSensorRead > SENSOR_DELAY) {
    lastSensorRead = currentMillis;
    float distance = measureDistance();
    
    // Serial.print("Distancia: "); Serial.print(distance); Serial.println(" cm"); // Debugging
    
    // Detectar activación del sensor solo si el sistema está en un estado IDLE
    if (distance > 0 && distance < UMBRAL_DISTANCIA) {
        if (currentMotorState == IDLE_AT_HOME_POSITION || currentMotorState == IDLE_AWAY_FROM_HOME) {
            Serial.print("Sensor activado a "); Serial.print(distance); Serial.println(" cm. ");
            Serial.print("Estado actual: "); Serial.println(currentMotorState == IDLE_AT_HOME_POSITION ? "IDLE_AT_HOME_POSITION" : "IDLE_AWAY_FROM_HOME");

            stepper.enableOutputs(); // Habilitar el motor para el movimiento
            
            // Decidir qué movimiento hacer basado en la posición actual
            if (currentMotorState == IDLE_AT_HOME_POSITION) {
                // Si estamos en 0, nos movemos hacia MAX_TRAVEL_STEPS
                Serial.println("Inicio de MOVING_AWAY_FROM_HOME.");
                currentMotorState = MOVING_AWAY_FROM_HOME;
                stepper.moveTo(MAX_TRAVEL_STEPS);
            } else if (currentMotorState == IDLE_AWAY_FROM_HOME) {
                // Si estamos en MAX_TRAVEL_STEPS, nos movemos hacia 0
                Serial.println("Inicio de MOVING_TOWARDS_HOME.");
                currentMotorState = MOVING_TOWARDS_HOME;
                stepper.moveTo(0); // Ir a la posición 0 (donde está el FDC)
            }
        } else {
            // Sensor activado pero el motor no está en estado IDLE (ya está moviéndose o falló)
            // Esto podría ser un problema si el sensor detecta algo y el motor ya está en movimiento,
            // Podríamos decidir ignorar, pausar el movimiento, o entrar en estado de error.
            // Por ahora, lo ignoramos.
            // Serial.println("Sensor activado pero motor no está en estado IDLE. Ignorando.");
        }
    }
  }

  // --- Lógica de la máquina de estados para movimientos completados ---
  switch (currentMotorState) {
      case MOVING_AWAY_FROM_HOME:
          if (!stepper.isRunning() && stepper.currentPosition() == MAX_TRAVEL_STEPS) {
              Serial.println("Movimiento MOVING_AWAY_FROM_HOME completado.");
              stepper.disableOutputs();
              currentMotorState = IDLE_AWAY_FROM_HOME;
              Serial.println("Motor en IDLE_AWAY_FROM_HOME.");
          } else if (!stepper.isRunning() && stepper.currentPosition() != MAX_TRAVEL_STEPS) {
              // Si no está corriendo pero no llegó al destino (ej. detuvo por error interno),
              // podríamos considerarlo un error, o simplemente lo dejamos donde está.
              // Para este ejemplo, lo consideramos un error.
              Serial.print("ERROR: Movimiento MOVING_AWAY_FROM_HOME detenido inesperadamente en pos ");
              Serial.println(stepper.currentPosition());
              stepper.disableOutputs();
              currentMotorState = ERROR_GENERAL; // Definir un error más específico si es necesario
              errorStateStartTime = millis();
          }
          break;

      case MOVING_TOWARDS_HOME:
          // NOTA: El FDC activará la interrupción y cambiará el estado a IDLE_AT_HOME_POSITION
          // Si llegamos aquí y !stepper.isRunning(), significa que moveTo(0) terminó sin FDC.
          // Esto implicaría que el FDC no funciona o que la distancia a 0 es menor que el FDC.
          if (!stepper.isRunning()) { // Implica que el motor ha llegado a la posición 0 sin activar el FDC.
              Serial.println("ERROR: Movimiento MOVING_TOWARDS_HOME completado a 0 sin FDC!");
              stepper.disableOutputs();
              currentMotorState = ERROR_HOME_NOT_FOUND;
              errorStateStartTime = millis();
          }
          break;
      
      case ERROR_ENDSTOP_UNEXPECTED:
      case ERROR_HOME_NOT_FOUND:
      case ERROR_GENERAL:
          // En estados de error, podríamos parpadear un LED, enviar mensajes de error
          // e intentar recuperarse si es posible, o esperar un reseteo manual.
          if (currentMillis - errorStateStartTime < 5000) { // Mostrar error por un tiempo
              // Serial.println("SISTEMA EN ESTADO DE ERROR. Por favor, verificar.");
              // digitalWrite(LED_BUILTIN, HIGH); delay(100); digitalWrite(LED_BUILTIN, LOW); delay(100);
          } else {
              // Después de un tiempo, podríamos intentar una recuperación o simplemente permanecer en error.
          }
          break;

      case IDLE_AT_HOME_POSITION:
      case IDLE_AWAY_FROM_HOME:
          // No hay lógica de movimiento activa en estos estados IDLE.
          // Solo esperan la activación del sensor.
          break;
      
      default:
          // Si llegamos a un estado desconocido, es un error crítico.
          Serial.println("ERROR: Estado del sistema desconocido. Reinicio necesario.");
          currentMotorState = ERROR_GENERAL;
          errorStateStartTime = millis();
          break;
  }
}