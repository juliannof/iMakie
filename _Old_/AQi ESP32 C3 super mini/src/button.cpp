// button.cpp
#include "button.h"
#include "Config.h"

ButtonHandler buttonHandler; // Instancia global

void ButtonHandler::begin() {
    button.begin(BUTTON_PIN);
    button.setClickHandler(handlePress);
    button.setLongClickHandler(handlePress);
}

void ButtonHandler::update() {
    button.loop();
}

bool ButtonHandler::wasPressed() {
    if (pressed) {
        pressed = false;
        return true;
    }
    return false;
}

void ButtonHandler::handlePress(Button2& btn) {
    buttonHandler.pressed = true;
}