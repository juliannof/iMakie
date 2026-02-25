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
const int PWM_MIN_MOTION_THRESHOLD = 640;     // Mínimo PWM para INICIAR movimiento continuo y controlable del Fader
const int PWM_HOLD_VALUE = 638;               // PWM para ACTIVAR el motor y anclar (justo por debajo del movimiento)
const int PWM_MAX_USABLE_DRIVE = 950;         // <-- CAMBIO: De 790 a 780, para un margen seguro.

const int DEFAULT_PULSE_DURATION = 15;
const int DEFAULT_PULSE_INTERVAL = 400;

// ======= NUEVOS PARÁMETROS PID ======= 
const float DEFAULT_KP = 0.5;   // Aumentamos KP para respuesta más rápida
const float DEFAULT_KI = 0.4;   // Mantenemos KI alto para corregir pequeños errores
const float DEFAULT_KD = 0.25;  // Mantenemos KD para estabilidad
// ============================================= 


//    O si tus pruebas determinaron que el límite es 790, déjalo en 790.
                                              //    Lo importante es que no sea 950.
const int PID_PULSE_THRESHOLD = 500;            // <-- CAMBIO: Deshabilita el modo de pulsos para esta prueba.
const int POSITION_TOLERANCE = 50;           // <-- CAMBIO: Aumenta la tolerancia final para la prueba.
const int RE_ACTIVATION_TOLERANCE = 100; // Puedes ajustar este valor
// const int HOLD_PWM_MAGNITUDE ya no existe, usamos PWM_HOLD_VALUE
const unsigned long CALIB_TIMEOUT = 10000;  
const int ADC_BUFFER_SIZE = 30;         // AUMENTADO PARA MÁS SUAVIZADO
const int ADC_STABILITY_THRESHOLD = 50; // AUMENTADO PARA MAYOR TOLERANCIA
const unsigned long CALIB_STABLE_TIME = 450;  
const int ADC_MAX_VALUE = 4095;   
          
// --- PARÁMETROS DE CONTROL DE POSICIÓN HÍBRIDO ---
const int PWM_MAX_DRIVE_HARDCODE = 750;
const int HOLD_PWM_MAGNITUDE = 100;     // <-- NUEVO: Fuerza PWM base para mantener la posición.