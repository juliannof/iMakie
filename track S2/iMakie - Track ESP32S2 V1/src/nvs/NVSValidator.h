#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define NVS_NS "ptxx"

enum class NVSStatus {
    VALID,
    CORRUPTED
};

class NVSValidator {
public:
    static NVSStatus validate();
    static void reset();
    static void displayError(const char* msg);
    static NVSStatus getLastStatus();

private:
    static bool checkNamespace();
    static bool checkCriticalKeys();
    static NVSStatus lastStatus;
};
