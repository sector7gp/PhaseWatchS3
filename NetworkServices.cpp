#include "NetworkServices.h"
#include "StorageManager.h"
#include "Logger.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

bool NetworkServices::servicesStarted = false;
String NetworkServices::activeHostname = "";

String NetworkServices::sanitizeHostname(const char* raw) {
    String host = String(raw);
    host.trim();
    host.toLowerCase();

    if (host.endsWith(".local")) {
        host = host.substring(0, host.length() - 6);
    }

    String clean = "";
    for (size_t i = 0; i < host.length() && clean.length() < MAX_HOSTNAME_LEN - 1; i++) {
        char c = host.charAt(i);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
            clean += c;
        }
    }

    if (clean.length() == 0) {
        clean = DEFAULT_HOSTNAME;
    }

    return clean;
}

void NetworkServices::init() {
    servicesStarted = false;
    activeHostname = "";
}

void NetworkServices::start() {
    if (servicesStarted || WiFi.status() != WL_CONNECTED) return;

    activeHostname = sanitizeHostname(StorageManager::config.hostname);

    if (MDNS.begin(activeHostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("phasewatch", "tcp", 80);
        Logger::info(("mDNS started: " + activeHostname + ".local").c_str());
    } else {
        Logger::error("mDNS start failed.");
    }

    ArduinoOTA.setHostname(activeHostname.c_str());
    ArduinoOTA.setPassword(StorageManager::config.otaPass);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Logger::warn(("OTA start: " + type).c_str());
    });

    ArduinoOTA.onEnd([]() {
        Logger::info("OTA complete.");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        esp_task_wdt_reset();
        if (total > 0 && progress % (total / 10 + 1) == 0) {
            Logger::info(("OTA progress: " + String((progress * 100) / total) + "%").c_str());
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Logger::error(("OTA error: " + String(error)).c_str());
    });

    ArduinoOTA.begin();
    servicesStarted = true;
    Logger::info("Arduino OTA ready.");
}

void NetworkServices::stop() {
    if (!servicesStarted) return;

    MDNS.end();
    servicesStarted = false;
    activeHostname = "";
    Logger::info("Network services stopped.");
}

void NetworkServices::update() {
    if (!StorageManager::config.configured) return;

    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_OFF) {
        stop();
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!servicesStarted) {
            start();
        }
        ArduinoOTA.handle();
    } else {
        stop();
    }
}

bool NetworkServices::isMdnsActive() {
    return servicesStarted && WiFi.status() == WL_CONNECTED;
}

bool NetworkServices::isOtaActive() {
    return servicesStarted && WiFi.status() == WL_CONNECTED;
}

String NetworkServices::getMdnsHostname() {
    if (activeHostname.length() > 0) return activeHostname;
    return sanitizeHostname(StorageManager::config.hostname);
}

String NetworkServices::getMdnsFqdn() {
    return getMdnsHostname() + ".local";
}
