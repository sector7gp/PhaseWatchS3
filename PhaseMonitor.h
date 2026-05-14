#pragma once
#include "Config.h"

class PhaseMonitor {
public:
    static void init();
    static void update();
    
    static bool l1_ok;
    static bool l2_ok;
    static bool l3_ok;
    
    static PhaseCriticality getCriticality();
    static bool hasChanged();
    static void clearChanged();

private:
    static bool readPhase(uint8_t pin);
    
    static unsigned long lastDebounceTimeL1;
    static unsigned long lastDebounceTimeL2;
    static unsigned long lastDebounceTimeL3;
    
    static bool lastReadingL1;
    static bool lastReadingL2;
    static bool lastReadingL3;
    
    static bool changedFlag;
};
