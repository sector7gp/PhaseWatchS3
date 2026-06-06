#include "StorageManager.h"
#include "Logger.h"
#include <cstring>

Preferences StorageManager::preferences;
SystemConfig StorageManager::config;

static void loadDefaults() {
    memset(&StorageManager::config, 0, sizeof(StorageManager::config));
    StorageManager::config.mqttPort = 1883;
    safeStrCopy(StorageManager::config.hostname, MAX_HOSTNAME_LEN, DEFAULT_HOSTNAME);
    safeStrCopy(StorageManager::config.otaPass, MAX_OTA_PASS_LEN, DEFAULT_OTA_PASS);
}

// Buffers fijos: evita VLA en Preferences::getString() y logs ESP_ERR en claves ausentes.
static void readPrefString(Preferences& prefs, const char* key, char* dest, size_t destSize,
                           const char* defaultValue) {
    if (destSize == 0) return;
    if (!prefs.isKey(key)) {
        safeStrCopy(dest, destSize, defaultValue);
        return;
    }
    size_t len = prefs.getString(key, dest, destSize);
    if (len == 0) {
        safeStrCopy(dest, destSize, defaultValue);
    }
}

void StorageManager::init() {
    load();
}

void StorageManager::load() {
    loadDefaults();

    if (!preferences.begin("phasewatch", true)) {
        Logger::warn("NVS namespace 'phasewatch' no disponible — modo AP.");
        config.configured = false;
        return;
    }

    readPrefString(preferences, "wifiSSID", config.wifiSSID, MAX_SSID_LEN, "");
    readPrefString(preferences, "wifiPass", config.wifiPass, MAX_PASS_LEN, "");
    readPrefString(preferences, "mqttHost", config.mqttHost, MAX_MQTT_HOST_LEN, "");
    config.mqttPort = preferences.getInt("mqttPort", 1883);
    readPrefString(preferences, "mqttUser", config.mqttUser, MAX_MQTT_USER_LEN, "");
    readPrefString(preferences, "mqttPass", config.mqttPass, MAX_MQTT_PASS_LEN, "");

    config.configured = preferences.getBool("configured", false);

    for (int i = 0; i < MAX_PHONE_NUMBERS; i++) {
        char key[12];
        snprintf(key, sizeof(key), "phone%d", i);
        readPrefString(preferences, key, config.phones[i], MAX_PHONE_LEN, "");
    }

    readPrefString(preferences, "hostname", config.hostname, MAX_HOSTNAME_LEN, DEFAULT_HOSTNAME);
    readPrefString(preferences, "otaPass", config.otaPass, MAX_OTA_PASS_LEN, DEFAULT_OTA_PASS);

    preferences.end();

    if (config.configured && config.wifiSSID[0] == '\0') {
        Logger::warn("NVS corrupto (configured sin SSID) — entrando en modo AP.");
        config.configured = false;
    }

    Logger::info(config.configured
        ? "Configuration loaded from NVS (device configured)."
        : "No configuration found — AP setup mode will start.");
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
        char key[12];
        snprintf(key, sizeof(key), "phone%d", i);
        preferences.putString(key, config.phones[i]);
    }

    preferences.putString("hostname", config.hostname);
    preferences.putString("otaPass", config.otaPass);
    
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
