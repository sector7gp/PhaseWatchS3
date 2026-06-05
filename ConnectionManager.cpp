#include "ConnectionManager.h"
#include "StorageManager.h"
#include "PhaseMonitor.h"
#include "Logger.h"
#include <esp_task_wdt.h>
#include <esp_mac.h>

// Hardware Serial for GSM
HardwareSerial SerialGSM(1);
TinyGsm modem(SerialGSM);
TinyGsmClient gsmClient(modem);
WiFiClient wifiClient;

PubSubClient mqttWiFi(wifiClient);
PubSubClient mqttGSM(gsmClient);
PubSubClient* mqtt = nullptr;

NetworkMode ConnectionManager::currentNetwork = NET_NONE;
unsigned long ConnectionManager::lastHeartbeatTime = 0;
unsigned long ConnectionManager::lastReconnectAttempt = 0;
unsigned long ConnectionManager::wifiConnectStartTime = 0;
bool ConnectionManager::wifiConnectPending = false;
bool ConnectionManager::gsmModemReady = false;
unsigned long ConnectionManager::lastGsmProbe = 0;
unsigned long ConnectionManager::lastBattRead = 0;
int16_t ConnectionManager::cachedBattMv = -1;
int8_t ConnectionManager::cachedBattPct = -1;
String macStr = "";

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define RECONNECT_INTERVAL_MS 30000

void ConnectionManager::disconnectMQTT() {
    if (mqtt != nullptr && mqtt->connected()) {
        mqtt->disconnect();
    }
}

String ConnectionManager::getDeviceId() {
    if (macStr.length() > 0) return macStr;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[13];
    snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void ConnectionManager::init() {
    if (!StorageManager::config.configured) {
        Logger::info("Network init skipped (AP configuration mode).");
        return;
    }

    macStr = getDeviceId();
    
    SerialGSM.begin(GSM_BAUD_RATE, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
    Logger::info("Initializing Modem...");
    modem.init();
    gsmModemReady = modem.testAT(2000);
    lastGsmProbe = millis();

    if (gsmModemReady) {
        Logger::info("GSM modem detected.");
    } else {
        Logger::warn("GSM modem not detected.");
    }
    
    connectWiFiBlocking();
}

void ConnectionManager::startWiFiConnection() {
    Logger::info("Connecting to WiFi...");
    disconnectMQTT();
    WiFi.mode(WIFI_STA);
    WiFi.begin(StorageManager::config.wifiSSID, StorageManager::config.wifiPass);
    wifiConnectPending = true;
    wifiConnectStartTime = millis();
}

void ConnectionManager::pollWiFiConnection() {
    if (!wifiConnectPending) return;

    if (WiFi.status() == WL_CONNECTED) {
        Logger::info("WiFi Connected.");
        wifiConnectPending = false;
        currentNetwork = NET_WIFI;
        mqtt = &mqttWiFi;
        mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
        return;
    }

    if (millis() - wifiConnectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        Logger::warn("WiFi Failed. Fallback to GSM.");
        wifiConnectPending = false;
        connectGSM();
    }
}

void ConnectionManager::connectWiFiBlocking() {
    startWiFiConnection();

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 15) {
        delay(500);
        esp_task_wdt_reset();
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnectPending = false;
        currentNetwork = NET_WIFI;
        mqtt = &mqttWiFi;
        mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
    } else {
        wifiConnectPending = false;
        connectGSM();
    }
}

void ConnectionManager::connectGSM() {
    disconnectMQTT();

    Logger::info("Connecting to GSM network...");
    if (!modem.waitForNetwork(10000)) {
        Logger::error("GSM Network failed.");
        currentNetwork = NET_NONE;
        return;
    }
    
    if (!modem.isNetworkConnected()) {
        Logger::error("GSM not registered.");
        currentNetwork = NET_NONE;
        return;
    }
    
    Logger::info("Connecting to GPRS...");
    if (!modem.gprsConnect("internet", "", "")) {
        Logger::error("GPRS Failed.");
        currentNetwork = NET_NONE;
        return;
    }
    
    Logger::info("GPRS Connected.");
    currentNetwork = NET_GSM;
    mqtt = &mqttGSM;
    mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
}

void ConnectionManager::update() {
    if (!StorageManager::config.configured) return;

    if (gsmModemReady && millis() - lastGsmProbe > 30000) {
        gsmModemReady = modem.testAT(1000);
        lastGsmProbe = millis();
    }
    
    maintainConnection();
    maintainMQTT();
    
    if (millis() - lastHeartbeatTime > MQTT_KEEPALIVE_MS) {
        publishHeartbeat();
        lastHeartbeatTime = millis();
    }
}

void ConnectionManager::maintainConnection() {
    if (wifiConnectPending) {
        pollWiFiConnection();
        return;
    }

    if (currentNetwork == NET_WIFI && WiFi.status() != WL_CONNECTED) {
        Logger::warn("WiFi lost. Switching to GSM...");
        currentNetwork = NET_NONE;
        lastReconnectAttempt = millis();
        connectGSM();
    } else if (currentNetwork == NET_GSM && WiFi.status() == WL_CONNECTED) {
        Logger::info("WiFi restored. Switching back...");
        disconnectMQTT();
        modem.gprsDisconnect();
        WiFi.mode(WIFI_STA);
        currentNetwork = NET_WIFI;
        mqtt = &mqttWiFi;
        mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
    } else if (currentNetwork == NET_NONE) {
        if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = millis();
            startWiFiConnection();
        }
    }
}

void ConnectionManager::maintainMQTT() {
    if (currentNetwork == NET_NONE || mqtt == nullptr) return;
    
    if (!mqtt->connected()) {
        Logger::info("Connecting to MQTT...");
        String clientId = "PhaseWatch-" + macStr;
        String lwtTopic = "phasewatch/" + macStr + "/lwt";
        
        if (mqtt->connect(clientId.c_str(), 
                          StorageManager::config.mqttUser, 
                          StorageManager::config.mqttPass, 
                          lwtTopic.c_str(), 0, true, "{\"status\":\"offline\"}")) {
            Logger::info("MQTT Connected.");
            mqtt->publish(lwtTopic.c_str(), "{\"status\":\"online\"}", true);
        } else {
            Logger::error("MQTT Connect failed.");
        }
    } else {
        mqtt->loop();
    }
}

String ConnectionManager::generateJsonPayload(const char* msgType, const char* event, const char* message) {
    String crit = "";
    PhaseCriticality c = PhaseMonitor::getCriticality();
    if (c == CRIT_NORMAL) crit = "NORMAL";
    else if (c == CRIT_PARTIAL_FAIL) crit = "PARTIAL_FAIL";
    else crit = "TOTAL_FAIL";
    
    String net = (currentNetwork == NET_WIFI) ? "WIFI" : ((currentNetwork == NET_GSM) ? "GPRS" : "NONE");
    
    String json = "{";
    json += "\"type\":\"" + String(msgType) + "\",";
    if (event != nullptr) json += "\"event\":\"" + String(event) + "\",";
    json += "\"timestamp\":" + String(millis()/1000) + ",";
    json += "\"l1\":" + String(PhaseMonitor::l1_ok ? "true" : "false") + ",";
    json += "\"l2\":" + String(PhaseMonitor::l2_ok ? "true" : "false") + ",";
    json += "\"l3\":" + String(PhaseMonitor::l3_ok ? "true" : "false") + ",";
    json += "\"criticality\":\"" + crit + "\",";
    json += "\"network\":\"" + net + "\",";
    json += "\"rssi\":" + String(getRSSI());
    if (message != nullptr) json += ",\"message\":\"" + String(message) + "\"";
    json += "}";
    return json;
}

void ConnectionManager::publishEvent(const char* eventType, const char* message) {
    if (mqtt && mqtt->connected()) {
        String topic = "phasewatch/" + macStr + "/events";
        String payload = generateJsonPayload("event", eventType, message);
        mqtt->publish(topic.c_str(), payload.c_str());
        Logger::info(("MQTT Published: " + payload).c_str());
    }
    
    if (String(eventType) == "PHASE_LOST" || String(eventType) == "PHASE_RESTORED" || String(eventType) == "TEST") {
        sendSMS(message);
    }
}

void ConnectionManager::publishHeartbeat() {
    if (mqtt && mqtt->connected()) {
        String topic = "phasewatch/" + macStr + "/status";
        String payload = generateJsonPayload("heartbeat", nullptr, nullptr);
        mqtt->publish(topic.c_str(), payload.c_str());
    }
}

void ConnectionManager::sendSMS(const char* message) {
    if (!modem.isNetworkConnected()) {
        Logger::warn("SMS skipped: GSM not registered.");
        return;
    }

    for (int i=0; i<MAX_PHONE_NUMBERS; i++) {
        String phone = StorageManager::config.phones[i];
        if (phone.length() > 5) {
            Logger::info(("Sending SMS to " + phone).c_str());
            if (modem.sendSMS(phone, String(message))) {
                Logger::info("SMS sent.");
            } else {
                Logger::error("SMS failed.");
            }
        }
    }
}

NetworkMode ConnectionManager::getCurrentNetwork() { return currentNetwork; }

bool ConnectionManager::isConnected() {
    return currentNetwork != NET_NONE && mqtt != nullptr && mqtt->connected();
}

bool ConnectionManager::isMqttConnected() {
    return mqtt != nullptr && mqtt->connected();
}

bool ConnectionManager::isGsmDetected() {
    return gsmModemReady;
}

bool ConnectionManager::isGsmRegistered() {
    return gsmModemReady && modem.isNetworkConnected();
}

int ConnectionManager::getGsmSignal() {
    if (!gsmModemReady) return 0;
    return modem.getSignalQuality();
}

String ConnectionManager::getMqttTopicBase() {
    return "phasewatch/" + getDeviceId();
}

String ConnectionManager::getMqttTopicStatus() {
    return getMqttTopicBase() + "/status";
}

String ConnectionManager::getMqttTopicEvents() {
    return getMqttTopicBase() + "/events";
}

String ConnectionManager::getMqttTopicLwt() {
    return getMqttTopicBase() + "/lwt";
}

bool ConnectionManager::getBatteryStatus(int16_t& milliVolts, int8_t& percent) {
    milliVolts = cachedBattMv;
    percent = cachedBattPct;

    if (!gsmModemReady) return false;

    if (millis() - lastBattRead < 60000 && cachedBattMv > 0) {
        return true;
    }

    int8_t chargeState = 0;
    int16_t mv = 0;
    if (modem.getBattStats(chargeState, percent, mv) && mv > 0) {
        cachedBattMv = mv;
        cachedBattPct = percent;
        lastBattRead = millis();
        milliVolts = mv;
        return true;
    }

    return false;
}

int ConnectionManager::getRSSI() {
    if (currentNetwork == NET_WIFI) {
        return WiFi.RSSI();
    } else if (currentNetwork == NET_GSM) {
        return modem.getSignalQuality();
    }
    return 0;
}

void ConnectionManager::sendTestNotification() {
    publishEvent("TEST", "Notificacion de prueba desde PhaseWatch");
}
