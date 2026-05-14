#pragma once
#include "Config.h"
#include <WiFi.h>
#include <PubSubClient.h>

#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>

class ConnectionManager {
public:
    static void init();
    static void update();
    
    static NetworkMode getCurrentNetwork();
    static bool isConnected();
    static int getRSSI();
    
    static void publishEvent(const char* eventType, const char* message);
    static void sendTestNotification();

private:
    static void connectWiFi();
    static void connectGSM();
    static void maintainConnection();
    static void maintainMQTT();
    static void publishHeartbeat();
    static void sendSMS(const char* message);
    
    static NetworkMode currentNetwork;
    static unsigned long lastHeartbeatTime;
    
    static String getMacAddress();
    static String generateJsonPayload(const char* msgType, const char* event, const char* message);
};
