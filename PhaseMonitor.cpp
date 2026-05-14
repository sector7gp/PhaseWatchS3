#include "PhaseMonitor.h"
#include <Arduino.h>

bool PhaseMonitor::l1_ok = true;
bool PhaseMonitor::l2_ok = true;
bool PhaseMonitor::l3_ok = true;

bool PhaseMonitor::lastReadingL1 = true;
bool PhaseMonitor::lastReadingL2 = true;
bool PhaseMonitor::lastReadingL3 = true;

unsigned long PhaseMonitor::lastDebounceTimeL1 = 0;
unsigned long PhaseMonitor::lastDebounceTimeL2 = 0;
unsigned long PhaseMonitor::lastDebounceTimeL3 = 0;

bool PhaseMonitor::changedFlag = false;

void PhaseMonitor::init() {
    pinMode(PIN_PHASE_1, INPUT_PULLUP);
    pinMode(PIN_PHASE_2, INPUT_PULLUP);
    pinMode(PIN_PHASE_3, INPUT_PULLUP);
    
    l1_ok = readPhase(PIN_PHASE_1);
    l2_ok = readPhase(PIN_PHASE_2);
    l3_ok = readPhase(PIN_PHASE_3);
    
    lastReadingL1 = l1_ok;
    lastReadingL2 = l2_ok;
    lastReadingL3 = l3_ok;
}

// Logic assumes LOW = Power Present (Optocoupler pulls down)
bool PhaseMonitor::readPhase(uint8_t pin) {
    return digitalRead(pin) == LOW; 
}

void PhaseMonitor::update() {
    bool currentL1 = readPhase(PIN_PHASE_1);
    bool currentL2 = readPhase(PIN_PHASE_2);
    bool currentL3 = readPhase(PIN_PHASE_3);
    
    // Debounce L1
    if (currentL1 != lastReadingL1) {
        lastDebounceTimeL1 = millis();
    }
    if ((millis() - lastDebounceTimeL1) > DEBOUNCE_DELAY_MS) {
        if (currentL1 != l1_ok) {
            l1_ok = currentL1;
            changedFlag = true;
        }
    }
    lastReadingL1 = currentL1;
    
    // Debounce L2
    if (currentL2 != lastReadingL2) {
        lastDebounceTimeL2 = millis();
    }
    if ((millis() - lastDebounceTimeL2) > DEBOUNCE_DELAY_MS) {
        if (currentL2 != l2_ok) {
            l2_ok = currentL2;
            changedFlag = true;
        }
    }
    lastReadingL2 = currentL2;
    
    // Debounce L3
    if (currentL3 != lastReadingL3) {
        lastDebounceTimeL3 = millis();
    }
    if ((millis() - lastDebounceTimeL3) > DEBOUNCE_DELAY_MS) {
        if (currentL3 != l3_ok) {
            l3_ok = currentL3;
            changedFlag = true;
        }
    }
    lastReadingL3 = currentL3;
}

PhaseCriticality PhaseMonitor::getCriticality() {
    int activePhases = (l1_ok ? 1 : 0) + (l2_ok ? 1 : 0) + (l3_ok ? 1 : 0);
    
    if (activePhases == 3) {
        return CRIT_NORMAL;
    } else if (activePhases > 0) {
        return CRIT_PARTIAL_FAIL;
    } else {
        return CRIT_TOTAL_FAIL;
    }
}

bool PhaseMonitor::hasChanged() {
    return changedFlag;
}

void PhaseMonitor::clearChanged() {
    changedFlag = false;
}
