#pragma once

class WebPortal {
public:
    static void init();
    static void update();
    static bool isAPMode();

private:
    static void handleRoot();
    static void handleSaveConfig();
    static void handleTest();
    static void handleLogs();
    static void handleNotFound();
    
    static bool apMode;
};
