#pragma once

#include "Config.h"

class WebPortal {
public:
    static void init();
    static void update();
    static bool isAPMode();

private:
    static void handleRoot();
    static void handleApiStatus();
    static void handleApiLogs();
    static void handleApiConfig();
    static void handleSaveConfig();
    static void handleTest();
    static void handleGsmRetry();
    static void handleGsmAt();
    static void handleFactoryReset();
    static void handleNotFound();
    static void prepareRestart();
    static void applyConfigFromRequest();
    static String jsonEscape(const String& input);
    static String criticalityLabel(PhaseCriticality c);
    
    static bool apMode;
};
