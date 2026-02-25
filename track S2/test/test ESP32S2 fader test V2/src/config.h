#pragma once

#include <Arduino.h>

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
const int ADC_BUFFER_SIZE = 10;
const int ADC_STABILITY_THRESHOLD = 15;
const unsigned long CALIB_STABLE_TIME = 450;
const int ADC_MAX_VALUE = 4095;

// --- CONTROL DE POSICIÓN ---
const int POSITION_TOLERANCE = 10;
const unsigned long MOVE_TIMEOUT = 5000;
const int POSITION_UPDATE_RATE = 50;

// --- PARÁMETROS PID/PULSOS ---
const int PID_PULSE_THRESHOLD = 20;
const int PWM_MAX_DRIVE_HARDCODE = 800;

// --- COMANDOS ---
const char EMERGENCY_STOP_CMD = 'x';
