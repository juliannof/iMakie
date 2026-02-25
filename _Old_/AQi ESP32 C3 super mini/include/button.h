// button.h
#pragma once

#include <Button2.h>

class ButtonHandler {
public:
    void begin();
    void update();
    bool wasPressed();
    
private:
    Button2 button;
    bool pressed = false;
    
    static void handlePress(Button2& btn);
};