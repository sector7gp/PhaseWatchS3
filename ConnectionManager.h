#pragma once
#include "Config.h"
#include <WiFi.h>
#include <PubSubClient.h>

// TINY_GSM_DEBUG desactivado: inunda USB y satura UART (usar consola AT del portal)
#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

class GsmUartGuard;

class ConnectionManager {
public:
    static void init();
    static void update();
    
    static NetworkMode getCurrentNetwork();
    static bool isConnected();
    static bool isMqttConnected();
    static bool isGsmDetected();
    static bool isGsmRegistered();
    static int getRSSI();
    static int getGsmSignal();

    static String getDeviceId();
    static String getMqttTopicBase();
    static String getMqttTopicStatus();
    static String getMqttTopicEvents();
    static String getMqttTopicLwt();
    static bool getBatteryStatus(int16_t& milliVolts, int8_t& percent);

    static void publishEvent(const char* eventType, const char* message);
    static void sendTestNotification();

    static bool retryGsmConnection();
    static String getGsmDebugInfo();
    static String sendAtCommand(const char* cmd);

private:
    friend class GsmUartGuard;

    enum BootPhase { BOOT_WAIT_GSM, BOOT_PROBE_GSM, BOOT_GSM_REG, BOOT_WIFI, BOOT_DONE };

    static void feedWdt();
    static bool lockGsmUart();
    static void unlockGsmUart();
    static void processBootstrap();
    static void initGsmSerial();
    static void updateGsmDebugStatus(bool quick = true);
    static void logGsmDiagnostics(bool quick = false);
    static void startGsmRegistration(uint32_t timeout_ms);
    static void tickGsmRegistration();
    static void completeGprsConnect();
    static bool probeGsmModem(bool verbose);
    static void connectWiFiBlocking();
    static void startWiFiConnection();
    static void pollWiFiConnection();
    static void connectGSM();
    static void disconnectMQTT();
    static void maintainConnection();
    static void maintainMQTT();
    static void publishHeartbeat();
    static void sendSMS(const char* message);
    
    static NetworkMode currentNetwork;
    static unsigned long lastHeartbeatTime;
    static unsigned long lastReconnectAttempt;
    static unsigned long wifiConnectStartTime;
    static bool wifiConnectPending;
    static bool gsmModemReady;
    static unsigned long lastGsmProbe;
    static unsigned long lastBattRead;
    static int16_t cachedBattMv;
    static int8_t cachedBattPct;
    static bool gsmSerialStarted;
    static String gsmDebugInfo;
    static BootPhase bootPhase;
    static unsigned long bootPhaseStart;
    static bool gsmRegPending;
    static bool gsmGprsPending;
    static unsigned long gsmRegStart;
    static uint32_t gsmRegTimeoutMs;
    static unsigned long gsmRegLastLog;
    static unsigned long gsmRegLastTick;
    static volatile bool gsmUartLocked;

    static String generateJsonPayload(const char* msgType, const char* event, const char* message);
};
