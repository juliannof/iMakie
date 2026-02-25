#pragma once
#include <Arduino.h>
#include "../config.h" 
#include "../FaderSensor/FaderSensor.h" 
#include "../MotorController/MotorController.h" // ¡Incluir MotorController!

class PositionController {
public:
    PositionController(FaderSensor* faderSensor, MotorController* motorCtrl); // <-- Modificado para aceptar MotorController
    void setTargetPosition(int targetADC); 
    void setTargetPositionPercent(int targetPercent, int minADC, int maxADC);
    int update(int currentADC); 
    bool isMoving() const { return _moving; }
    bool isHolding() const { return _holding; } 
    int getCurrentTarget() const { return _targetADC; }
    void setPulseDuration(int duration); // Si lo vas a usar
    void setPulseInterval(int interval); // Si lo vas a usar
    void resetControllerState();

private:
    int _targetADC;       
    bool _moving;         
    int _outputPWM;       
    bool _holding;        
    FaderSensor* _sensor; 
    MotorController* _motor; // <-- ¡Instancia de MotorController!
    unsigned long _lastPulseTime; 
    unsigned long _pulseEndTime;  
    int _currentPulseDuration;    
    int _currentPulseInterval;    
    float _kp, _ki, _kd;
    float _errorAnterior;
    float _integral;
    unsigned long _lastPIDTime;

    void controlPorPulsos(int currentADC); // Si lo vas a usar
    int  controlPorPID(int currentADC, float& outPidOutputFloat_raw); 
    void applyDeadzoneCompensation(int& pidOutput);
};
