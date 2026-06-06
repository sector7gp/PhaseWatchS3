#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

struct SystemConfig {
    char wifiSSID[MAX_SSID_LEN];
    char wifiPass[MAX_PASS_LEN];
    
    char mqttHost[MAX_MQTT_HOST_LEN];
    int  mqttPort;
    char mqttUser[MAX_MQTT_USER_LEN];
    char mqttPass[MAX_MQTT_PASS_LEN];
    
    char phones[MAX_PHONE_NUMBERS][MAX_PHONE_LEN];

    char hostname[MAX_HOSTNAME_LEN];
    char otaPass[MAX_OTA_PASS_LEN];

    bool configured;
};

class StorageManager {
public:
    static void init();
    static SystemConfig config;
    
    static void load();
    static void save();
    static void factoryReset();

private:
    static Preferences preferences;
};
