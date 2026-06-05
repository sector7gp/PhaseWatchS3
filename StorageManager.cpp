#include "StorageManager.h"
#include "Logger.h"

Preferences StorageManager::preferences;
SystemConfig StorageManager::config;

void StorageManager::init() {
    load();
}

void StorageManager::load() {
    preferences.begin("phasewatch", true); // true = read-only
    
    String ssid = preferences.getString("wifiSSID", "");
    String pass = preferences.getString("wifiPass", "");
    
    String mHost = preferences.getString("mqttHost", "");
    int mPort = preferences.getInt("mqttPort", 1883);
    String mUser = preferences.getString("mqttUser", "");
    String mPass = preferences.getString("mqttPass", "");
    
    config.configured = preferences.getBool("configured", false);
    
    safeStrCopy(config.wifiSSID, MAX_SSID_LEN, ssid.c_str());
    safeStrCopy(config.wifiPass, MAX_PASS_LEN, pass.c_str());
    safeStrCopy(config.mqttHost, MAX_MQTT_HOST_LEN, mHost.c_str());
    config.mqttPort = mPort;
    safeStrCopy(config.mqttUser, MAX_MQTT_USER_LEN, mUser.c_str());
    safeStrCopy(config.mqttPass, MAX_MQTT_PASS_LEN, mPass.c_str());
    
    for (int i = 0; i < MAX_PHONE_NUMBERS; i++) {
        String p = preferences.getString(("phone" + String(i)).c_str(), "");
        safeStrCopy(config.phones[i], MAX_PHONE_LEN, p.c_str());
    }
    
    preferences.end();
    Logger::info("Configuration loaded from NVS");
}

void StorageManager::save() {
    preferences.begin("phasewatch", false); // false = read-write
    
    preferences.putString("wifiSSID", config.wifiSSID);
    preferences.putString("wifiPass", config.wifiPass);
    preferences.putString("mqttHost", config.mqttHost);
    preferences.putInt("mqttPort", config.mqttPort);
    preferences.putString("mqttUser", config.mqttUser);
    preferences.putString("mqttPass", config.mqttPass);
    
    preferences.putBool("configured", true);
    config.configured = true;
    
    for (int i = 0; i < MAX_PHONE_NUMBERS; i++) {
        preferences.putString(("phone" + String(i)).c_str(), config.phones[i]);
    }
    
    preferences.end();
    Logger::info("Configuration saved to NVS");
}

void StorageManager::factoryReset() {
    preferences.begin("phasewatch", false);
    preferences.clear();
    preferences.end();
    Logger::warn("Factory reset performed. All settings cleared.");
    ESP.restart();
}
