#pragma once

#include <Arduino.h>
#include "../config.h" 

#include "../FaderSensor/FaderSensor.h" 

class PositionController {
public:
    PositionController(FaderSensor* faderSensor); 

    void setTargetPosition(int targetADC); // <-- ¡SOLO 1 ARGUMENTO!
    void setTargetPositionPercent(int targetPercent, int minADC, int maxADC);
    int update(int currentADC); 
    bool isMoving() const { return _moving; }
    bool isHolding() const { return _holding; } 
    int getCurrentTarget() const { return _targetADC; }
    void setPulseDuration(int duration);
    void setPulseInterval(int interval);

    void resetControllerState();

private:
    int _targetADC;       
    bool _moving;         
    int _outputPWM;       
    bool _holding;        

    FaderSensor* _sensor; 

    unsigned long _lastPulseTime; 
    unsigned long _pulseEndTime;  
    int _currentPulseDuration;    
    int _currentPulseInterval;    

    float _kp, _ki, _kd;
    float _errorAnterior;
    float _integral;
    unsigned long _lastPIDTime; 

    void controlPorPulsos(int currentADC); 
    int  controlPorPID(int currentADC, float& outPidOutputFloat_raw); 
};
