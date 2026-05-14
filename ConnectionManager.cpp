#include "ConnectionManager.h"
#include "StorageManager.h"
#include "PhaseMonitor.h"
#include "Logger.h"

// Hardware Serial for GSM
HardwareSerial SerialGSM(1);
TinyGsm modem(SerialGSM);
TinyGsmClient gsmClient(modem);
WiFiClient wifiClient;

PubSubClient mqttWiFi(wifiClient);
PubSubClient mqttGSM(gsmClient);
PubSubClient* mqtt;

NetworkMode ConnectionManager::currentNetwork = NET_NONE;
unsigned long ConnectionManager::lastHeartbeatTime = 0;
String macStr = "";

void ConnectionManager::init() {
    macStr = WiFi.macAddress();
    macStr.replace(":", "");
    
    SerialGSM.begin(GSM_BAUD_RATE, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
    Logger::info("Initializing Modem...");
    modem.init();
    
    if (StorageManager::config.configured) {
        connectWiFi();
    }
}

void ConnectionManager::connectWiFi() {
    Logger::info("Connecting to WiFi...");
    WiFi.begin(StorageManager::config.wifiSSID, StorageManager::config.wifiPass);
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 15) {
        delay(500);
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Logger::info("WiFi Connected.");
        currentNetwork = NET_WIFI;
        mqtt = &mqttWiFi;
        mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
    } else {
        Logger::warn("WiFi Failed. Fallback to GSM.");
        connectGSM();
    }
}

void ConnectionManager::connectGSM() {
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
    
    // GPRS APN - Many standard SIMs work with generic or no APN, but better to set it if needed
    // Assuming automatic or basic internet APN. We can use "internet" as generic.
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
    
    maintainConnection();
    maintainMQTT();
    
    if (millis() - lastHeartbeatTime > MQTT_KEEPALIVE_MS) {
        publishHeartbeat();
        lastHeartbeatTime = millis();
    }
}

void ConnectionManager::maintainConnection() {
    if (currentNetwork == NET_WIFI && WiFi.status() != WL_CONNECTED) {
        Logger::warn("WiFi lost. Switching to GSM...");
        connectGSM();
    } else if (currentNetwork == NET_GSM && WiFi.status() == WL_CONNECTED) {
        // Option to switch back to WiFi if it comes back
        Logger::info("WiFi restored. Switching back...");
        modem.gprsDisconnect();
        currentNetwork = NET_WIFI;
        mqtt = &mqttWiFi;
        mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
    } else if (currentNetwork == NET_NONE) {
        // Try WiFi first
        connectWiFi();
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
    
    // Also send SMS
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
    for (int i=0; i<MAX_PHONE_NUMBERS; i++) {
        String phone = StorageManager::config.phones[i];
        if (phone.length() > 5) { // Basic validation
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
bool ConnectionManager::isConnected() { return currentNetwork != NET_NONE; }

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
