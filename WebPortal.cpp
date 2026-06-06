#include "WebPortal.h"
#include "WebPortalHTML.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "StorageManager.h"
#include "PhaseMonitor.h"
#include "ConnectionManager.h"
#include "NetworkServices.h"
#include "Logger.h"

WebServer server(80);
DNSServer dnsServer;

bool WebPortal::apMode = false;
const byte DNS_PORT = 53;

String WebPortal::jsonEscape(const String& input) {
    String out;
    out.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

String WebPortal::criticalityLabel(PhaseCriticality c) {
    if (c == CRIT_NORMAL) return "NORMAL";
    if (c == CRIT_PARTIAL_FAIL) return "PARTIAL_FAIL";
    return "TOTAL_FAIL";
}

void WebPortal::init() {
    if (!StorageManager::config.configured) {
        apMode = true;
        Logger::info("Starting in AP Mode...");

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(
            IPAddress(192, 168, 4, 1),
            IPAddress(192, 168, 4, 1),
            IPAddress(255, 255, 255, 0)
        );

        bool apStarted = WiFi.softAP("PhaseWatch_Config", "12345678", 1, 0, 4);
        if (apStarted) {
            Logger::info(("AP ready — SSID: PhaseWatch_Config | IP: " + WiFi.softAPIP().toString()).c_str());
        } else {
            Logger::error("Failed to start configuration AP.");
        }

        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    } else {
        apMode = false;
        Logger::info("Starting in Station Mode Web Server...");
        // WebServer requiere stack WiFi inicializado (evita assert xQueueSemaphoreTake)
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        delay(100);
    }

    server.on("/", handleRoot);
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.on("/api/logs", HTTP_GET, handleApiLogs);
    server.on("/api/config", HTTP_GET, handleApiConfig);
    server.on("/api/save", HTTP_POST, handleSaveConfig);
    server.on("/api/test", HTTP_POST, handleTest);
    server.on("/api/gsm/retry", HTTP_POST, handleGsmRetry);
    server.on("/api/reset", HTTP_POST, handleFactoryReset);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.on("/test", HTTP_POST, handleTest);
    server.on("/reset", HTTP_POST, handleFactoryReset);
    server.onNotFound(handleNotFound);

    server.begin();
    Logger::info("Web Server Started.");
}

void WebPortal::update() {
    if (apMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

bool WebPortal::isAPMode() {
    return apMode;
}

void WebPortal::handleRoot() {
    server.send_P(200, "text/html", PORTAL_HTML);
}

void WebPortal::handleApiStatus() {
    PhaseCriticality crit = PhaseMonitor::getCriticality();

    String network = "NONE";
    if (ConnectionManager::getCurrentNetwork() == NET_WIFI) network = "WIFI";
    else if (ConnectionManager::getCurrentNetwork() == NET_GSM) network = "GPRS";

    int16_t battMv = -1;
    int8_t battPct = -1;
    bool battOk = ConnectionManager::getBatteryStatus(battMv, battPct);

    String json = "{";
    json += "\"device_id\":\"" + ConnectionManager::getDeviceId() + "\",";
    json += "\"uptime_s\":" + String(millis() / 1000) + ",";
    json += "\"config_mode\":" + String(apMode ? "true" : "false") + ",";
    json += "\"l1\":" + String(PhaseMonitor::l1_ok ? "true" : "false") + ",";
    json += "\"l2\":" + String(PhaseMonitor::l2_ok ? "true" : "false") + ",";
    json += "\"l3\":" + String(PhaseMonitor::l3_ok ? "true" : "false") + ",";
    json += "\"criticality\":\"" + criticalityLabel(crit) + "\",";
    json += "\"network\":\"" + network + "\",";
    json += "\"rssi\":" + String(ConnectionManager::getRSSI()) + ",";
    json += "\"mqtt_connected\":" + String(ConnectionManager::isMqttConnected() ? "true" : "false") + ",";
    json += "\"gsm_detected\":" + String(ConnectionManager::isGsmDetected() ? "true" : "false") + ",";
    json += "\"gsm_registered\":" + String(ConnectionManager::isGsmRegistered() ? "true" : "false") + ",";
    json += "\"gsm_signal\":" + String(ConnectionManager::getGsmSignal()) + ",";
    json += "\"gsm_debug\":\"" + jsonEscape(ConnectionManager::getGsmDebugInfo()) + "\",";
    json += "\"battery_available\":" + String(battOk ? "true" : "false") + ",";
    json += "\"battery_mv\":" + String(battMv) + ",";
    json += "\"battery_pct\":" + String(battPct) + ",";
    json += "\"mdns_active\":" + String(NetworkServices::isMdnsActive() ? "true" : "false") + ",";
    json += "\"mdns_hostname\":\"" + jsonEscape(NetworkServices::getMdnsHostname()) + "\",";
    json += "\"mdns_fqdn\":\"" + jsonEscape(NetworkServices::getMdnsFqdn()) + "\",";
    json += "\"ota_active\":" + String(NetworkServices::isOtaActive() ? "true" : "false") + ",";
    json += "\"mqtt_topics\":{";
    json += "\"base\":\"" + ConnectionManager::getMqttTopicBase() + "\",";
    json += "\"status\":\"" + ConnectionManager::getMqttTopicStatus() + "\",";
    json += "\"events\":\"" + ConnectionManager::getMqttTopicEvents() + "\",";
    json += "\"lwt\":\"" + ConnectionManager::getMqttTopicLwt() + "\"";
    json += "}}";

    server.send(200, "application/json", json);
}

void WebPortal::handleApiLogs() {
    String json = "{\"logs\":\"" + jsonEscape(Logger::getLogs()) + "\"}";
    server.send(200, "application/json", json);
}

void WebPortal::handleApiConfig() {
    String json = "{";
    json += "\"wifiSSID\":\"" + jsonEscape(String(StorageManager::config.wifiSSID)) + "\",";
    json += "\"mqttHost\":\"" + jsonEscape(String(StorageManager::config.mqttHost)) + "\",";
    json += "\"mqttPort\":" + String(StorageManager::config.mqttPort) + ",";
    json += "\"mqttUser\":\"" + jsonEscape(String(StorageManager::config.mqttUser)) + "\",";
    json += "\"hostname\":\"" + jsonEscape(String(StorageManager::config.hostname)) + "\",";
    json += "\"configured\":" + String(StorageManager::config.configured ? "true" : "false") + ",";
    json += "\"phones\":[";
    for (int i = 0; i < MAX_PHONE_NUMBERS; i++) {
        if (i > 0) json += ",";
        json += "\"" + jsonEscape(String(StorageManager::config.phones[i])) + "\"";
    }
    json += "]}";

    server.send(200, "application/json", json);
}

void WebPortal::applyConfigFromRequest() {
    if (server.hasArg("wifiSSID")) safeStrCopy(StorageManager::config.wifiSSID, MAX_SSID_LEN, server.arg("wifiSSID").c_str());
    if (server.hasArg("wifiPass") && server.arg("wifiPass").length() > 0) {
        safeStrCopy(StorageManager::config.wifiPass, MAX_PASS_LEN, server.arg("wifiPass").c_str());
    }

    if (server.hasArg("mqttHost")) safeStrCopy(StorageManager::config.mqttHost, MAX_MQTT_HOST_LEN, server.arg("mqttHost").c_str());
    if (server.hasArg("mqttPort")) StorageManager::config.mqttPort = server.arg("mqttPort").toInt();
    if (server.hasArg("mqttUser")) safeStrCopy(StorageManager::config.mqttUser, MAX_MQTT_USER_LEN, server.arg("mqttUser").c_str());
    if (server.hasArg("mqttPass") && server.arg("mqttPass").length() > 0) {
        safeStrCopy(StorageManager::config.mqttPass, MAX_MQTT_PASS_LEN, server.arg("mqttPass").c_str());
    }

    if (server.hasArg("phone0")) safeStrCopy(StorageManager::config.phones[0], MAX_PHONE_LEN, server.arg("phone0").c_str());
    if (server.hasArg("phone1")) safeStrCopy(StorageManager::config.phones[1], MAX_PHONE_LEN, server.arg("phone1").c_str());
    if (server.hasArg("phone2")) safeStrCopy(StorageManager::config.phones[2], MAX_PHONE_LEN, server.arg("phone2").c_str());

    if (server.hasArg("hostname")) {
        String host = server.arg("hostname");
        host.trim();
        if (host.length() == 0) {
            safeStrCopy(StorageManager::config.hostname, MAX_HOSTNAME_LEN, DEFAULT_HOSTNAME);
        } else {
            safeStrCopy(StorageManager::config.hostname, MAX_HOSTNAME_LEN, host.c_str());
        }
    } else if (!StorageManager::config.configured) {
        safeStrCopy(StorageManager::config.hostname, MAX_HOSTNAME_LEN, DEFAULT_HOSTNAME);
    }

    if (server.hasArg("otaPass") && server.arg("otaPass").length() > 0) {
        safeStrCopy(StorageManager::config.otaPass, MAX_OTA_PASS_LEN, server.arg("otaPass").c_str());
    } else if (!StorageManager::config.configured) {
        safeStrCopy(StorageManager::config.otaPass, MAX_OTA_PASS_LEN, DEFAULT_OTA_PASS);
    }
}

void WebPortal::prepareRestart() {
    NetworkServices::stop();
    server.stop();
    if (apMode) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
    }
    WiFi.mode(WIFI_OFF);
    delay(200);
}

void WebPortal::handleSaveConfig() {
    applyConfigFromRequest();
    StorageManager::save();

    String body = "{\"ok\":true,\"restarting\":true,\"message\":\"Configuracion guardada. Reiniciando...\"}";
    server.send(200, "application/json", body);
    server.client().stop();
    delay(500);
    prepareRestart();
    ESP.restart();
}

void WebPortal::handleTest() {
    ConnectionManager::sendTestNotification();
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Notificacion de prueba enviada\"}");
}

void WebPortal::handleGsmRetry() {
    bool detected = ConnectionManager::retryGsmConnection();
    String body = "{\"ok\":" + String(detected ? "true" : "false") +
                  ",\"detected\":" + String(ConnectionManager::isGsmDetected() ? "true" : "false") +
                  ",\"registered\":" + String(ConnectionManager::isGsmRegistered() ? "true" : "false") +
                  ",\"debug\":\"" + jsonEscape(ConnectionManager::getGsmDebugInfo()) + "\"";
    if (detected) {
        body += ",\"message\":\"Modulo GSM detectado. Ver logs para detalle.\"}";
    } else {
        body += ",\"message\":\"No se detecto el SIM800L. Ver logs y cableado.\"}";
    }
    server.send(200, "application/json", body);
}

void WebPortal::handleFactoryReset() {
    server.send(200, "application/json", "{\"ok\":true,\"message\":\"Reseteando dispositivo...\"}");
    server.client().stop();
    delay(500);
    prepareRestart();
    StorageManager::factoryReset();
}

void WebPortal::handleNotFound() {
    if (apMode) {
        server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "404: Not found");
    }
}
