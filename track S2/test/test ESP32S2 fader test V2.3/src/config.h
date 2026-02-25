#pragma once
#include <Arduino.h>

// --- CONFIGURACIÓN DE HARDWARE ---  
const int MOTOR_IN1 = 18;  
const int MOTOR_IN2 = 16;    
const int ENABLE_PIN = 34;  
const int POT_PIN = 10;  
const int STATUS_LED = 15;  
      
// --- PARÁMETROS OPERATIVOS ---  
const int PWM_FREQ = 20000;  
const int PWM_RESOLUTION = 10;  

// --- PARÁMETROS OPERATIVOS ---
const int PWM_MIN_MOTION_THRESHOLD = 640;     // Mínimo PWM para iniciar movimiento.
// const int PWM_HOLD_VALUE = 638;       // Ahora drive(0) lo hace, no se usa.
const int PWM_MAX_USABLE_DRIVE = 670;

// Valores iniciales sugeridos:
const float DEFAULT_KP = 0.8;    // Aumentado
const float DEFAULT_KI = 0.2;    // Reducido
const float DEFAULT_KD = 1.0;    // Ligera reducción

// --- PARÁMETROS DE TOLERANCIA Y REACTIVACIÓN ---
const int POSITION_TOLERANCE = 50;           // Tolerancia para declarar la posición asentada.
const int RE_ACTIVATION_TOLERANCE = 100;     // Tolerancia para reactivar el movimiento si está en holding.

// --- PARÁMETROS PARA EL MODO HÍBRIDO (PULSOS) ---
const int PID_PULSE_THRESHOLD = 150;       // Error absoluto para SALTAR al modo de control por pulsos.

// Nuevas constantes para el control de pulsos
const int PULSE_ERROR_THRESHOLD_HIGH = 150; // ErrorAbs > 150 -> duración y intervalo específicos.
const int PULSE_ERROR_THRESHOLD_MEDIUM = 50; // ErrorAbs > 50 -> duración y intervalo específicos.
// Si ErrorAbs <= 50, se usan los valores por defecto o los de la banda más baja.

// Duraciones de pulso (Reducir drasticamente)
const int PULSE_DURATION_HIGH_ERROR = 2;    // Era 5
const int PULSE_DURATION_MEDIUM_ERROR = 1;  // Era 2
const int PULSE_DURATION_LOW_ERROR = 1;     // Era 1 (mínimo, si 1ms aún mueve mucho, no hay más en duración)

// Intervalos de pulso (Ajustar según la duración)
const int PULSE_INTERVAL_HIGH_ERROR = 20;   // Reducir fuertemente. El motor no tiene que esperar tanto.
const int PULSE_INTERVAL_MEDIUM_ERROR = 40; // Ídem.
const int PULSE_INTERVAL_LOW_ERROR = 60;    // Ídem. Puedes probar con valores de 50 a 100.

// --- OTRAS CONSTANTES ---
const unsigned long CALIB_TIMEOUT = 10000;  
const int ADC_BUFFER_SIZE = 30;         
const int ADC_STABILITY_THRESHOLD = 50; 
const unsigned long CALIB_STABLE_TIME = 450;  
const int ADC_MAX_VALUE = 4095;   
          
// --- CONSTANTES PARA EL MOTOR CONTROLLER (aunque no estén en PositionController, es un buen lugar) ---
const int MOTOR_DIR_FORWARD = 1;
const int MOTOR_DIR_REVERSE = -1;
const int MOTOR_DIR_BRAKE = 0; 