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
const int MIN_PWM_DRIVE = 640;   
const unsigned long CALIB_TIMEOUT = 10000;  
const int ADC_BUFFER_SIZE = 30;         // AUMENTADO PARA MÁS SUAVIZADO
const int ADC_STABILITY_THRESHOLD = 50; // AUMENTADO PARA MAYOR TOLERANCIA
const unsigned long CALIB_STABLE_TIME = 450;  
const int ADC_MAX_VALUE = 4095;   
          
// --- PARÁMETROS DE CONTROL DE POSICIÓN HÍBRIDO ---
const int PWM_MAX_DRIVE_HARDCODE = 950;
const int PID_PULSE_THRESHOLD = 300;    
const int POSITION_TOLERANCE = 50;      // <-- Volver a un valor más ajustado (50)
const int HOLD_PWM_MAGNITUDE = 100;     // <-- NUEVO: Fuerza PWM base para mantener la posición.