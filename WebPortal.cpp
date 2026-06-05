#include "WebPortal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "StorageManager.h"
#include "PhaseMonitor.h"
#include "ConnectionManager.h"
#include "Logger.h"

WebServer server(80);
DNSServer dnsServer;

bool WebPortal::apMode = false;
const byte DNS_PORT = 53;

void WebPortal::init() {
    if (!StorageManager::config.configured) {
        apMode = true;
        Logger::info("Starting in AP Mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("PhaseWatch_Config", "12345678"); // Default AP pass
        
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    } else {
        apMode = false;
        Logger::info("Starting in Station Mode Web Server...");
        // WiFi is already started by ConnectionManager
    }

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.on("/test", HTTP_POST, handleTest);
    server.on("/logs", handleLogs);
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
    String html = "<html><head><title>PhaseWatch S3</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif; margin:20px;} input, button{display:block; margin-bottom:10px; width:100%; padding:10px; box-sizing:border-box;}</style>";
    html += "</head><body>";
    
    html += "<h1>PhaseWatch S3</h1>";
    
    if (apMode) {
        html += "<h2>Configuracion</h2>";
        html += "<form action='/save' method='POST'>";
        html += "<label>WiFi SSID:</label><input type='text' name='wifiSSID' value='" + String(StorageManager::config.wifiSSID) + "'>";
        html += "<label>WiFi Password:</label><input type='password' name='wifiPass'>";
        
        html += "<label>MQTT Host:</label><input type='text' name='mqttHost' value='" + String(StorageManager::config.mqttHost) + "'>";
        html += "<label>MQTT Port:</label><input type='number' name='mqttPort' value='" + String(StorageManager::config.mqttPort) + "'>";
        html += "<label>MQTT User:</label><input type='text' name='mqttUser' value='" + String(StorageManager::config.mqttUser) + "'>";
        html += "<label>MQTT Pass:</label><input type='password' name='mqttPass'>";
        
        html += "<label>Telefono 1 (SMS):</label><input type='text' name='phone0' value='" + String(StorageManager::config.phones[0]) + "'>";
        html += "<label>Telefono 2 (SMS):</label><input type='text' name='phone1' value='" + String(StorageManager::config.phones[1]) + "'>";
        html += "<label>Telefono 3 (SMS):</label><input type='text' name='phone2' value='" + String(StorageManager::config.phones[2]) + "'>";
        
        html += "<button type='submit'>Guardar y Reiniciar</button>";
        html += "</form>";
    } else {
        html += "<h2>Dashboard Local</h2>";
        
        html += "<p><b>Fase L1:</b> <span style='color:" + String(PhaseMonitor::l1_ok ? "green" : "red") + "'>" + String(PhaseMonitor::l1_ok ? "OK" : "FALLA") + "</span></p>";
        html += "<p><b>Fase L2:</b> <span style='color:" + String(PhaseMonitor::l2_ok ? "green" : "red") + "'>" + String(PhaseMonitor::l2_ok ? "OK" : "FALLA") + "</span></p>";
        html += "<p><b>Fase L3:</b> <span style='color:" + String(PhaseMonitor::l3_ok ? "green" : "red") + "'>" + String(PhaseMonitor::l3_ok ? "OK" : "FALLA") + "</span></p>";
        
        String netStr = "Ninguna";
        if (ConnectionManager::getCurrentNetwork() == NET_WIFI) netStr = "WiFi";
        else if (ConnectionManager::getCurrentNetwork() == NET_GSM) netStr = "GSM/GPRS";
        
        html += "<p><b>Red Activa:</b> " + netStr + "</p>";
        html += "<p><b>RSSI (Señal):</b> " + String(ConnectionManager::getRSSI()) + "</p>";
        
        html += "<hr>";
        html += "<form action='/test' method='POST'><button type='submit'>Enviar Notificacion de Prueba</button></form>";
        html += "<a href='/logs'><button type='button'>Ver Logs de Sistema</button></a>";
    }
    
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void WebPortal::handleSaveConfig() {
    if (server.hasArg("wifiSSID")) safeStrCopy(StorageManager::config.wifiSSID, MAX_SSID_LEN, server.arg("wifiSSID").c_str());
    if (server.hasArg("wifiPass") && server.arg("wifiPass").length() > 0) safeStrCopy(StorageManager::config.wifiPass, MAX_PASS_LEN, server.arg("wifiPass").c_str());
    
    if (server.hasArg("mqttHost")) safeStrCopy(StorageManager::config.mqttHost, MAX_MQTT_HOST_LEN, server.arg("mqttHost").c_str());
    if (server.hasArg("mqttPort")) StorageManager::config.mqttPort = server.arg("mqttPort").toInt();
    if (server.hasArg("mqttUser")) safeStrCopy(StorageManager::config.mqttUser, MAX_MQTT_USER_LEN, server.arg("mqttUser").c_str());
    if (server.hasArg("mqttPass") && server.arg("mqttPass").length() > 0) safeStrCopy(StorageManager::config.mqttPass, MAX_MQTT_PASS_LEN, server.arg("mqttPass").c_str());
    
    if (server.hasArg("phone0")) safeStrCopy(StorageManager::config.phones[0], MAX_PHONE_LEN, server.arg("phone0").c_str());
    if (server.hasArg("phone1")) safeStrCopy(StorageManager::config.phones[1], MAX_PHONE_LEN, server.arg("phone1").c_str());
    if (server.hasArg("phone2")) safeStrCopy(StorageManager::config.phones[2], MAX_PHONE_LEN, server.arg("phone2").c_str());
    
    StorageManager::save();
    
    server.send(200, "text/html", "<html><body><h2>Configuracion Guardada. Reiniciando...</h2></body></html>");
    delay(2000);
    ESP.restart();
}

void WebPortal::handleTest() {
    ConnectionManager::sendTestNotification();
    server.sendHeader("Location", "/");
    server.send(303);
}

void WebPortal::handleLogs() {
    String html = "<html><body><h2>Logs del Sistema</h2><pre>";
    html += Logger::getLogs();
    html += "</pre><a href='/'>Volver</a></body></html>";
    server.send(200, "text/html", html);
}

void WebPortal::handleNotFound() {
    if (apMode) {
        // Redirect to captive portal root
        server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "404: Not found");
    }
}
