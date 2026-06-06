#include "ConnectionManager.h"
#include "StorageManager.h"
#include "PhaseMonitor.h"
#include "Logger.h"
#include <esp_task_wdt.h>
#include <esp_mac.h>

// UART0 en pines TX/RX del header — USB CDC es el debug (Serial)
HardwareSerial SerialGSM(GSM_UART_PORT);
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
bool ConnectionManager::gsmSerialStarted = false;
String ConnectionManager::gsmDebugInfo = "Sin inicializar";
ConnectionManager::BootPhase ConnectionManager::bootPhase = ConnectionManager::BOOT_DONE;
unsigned long ConnectionManager::bootPhaseStart = 0;
bool ConnectionManager::gsmRegPending = false;
bool ConnectionManager::gsmGprsPending = false;
unsigned long ConnectionManager::gsmRegStart = 0;
uint32_t ConnectionManager::gsmRegTimeoutMs = 0;
unsigned long ConnectionManager::gsmRegLastLog = 0;
unsigned long ConnectionManager::gsmRegLastTick = 0;
volatile bool ConnectionManager::gsmUartLocked = false;
String macStr = "";

class GsmUartGuard {
public:
    explicit GsmUartGuard(bool acquired) : ok(acquired) {}
    ~GsmUartGuard() { if (ok) ConnectionManager::unlockGsmUart(); }
    bool active() const { return ok; }
private:
    bool ok;
};

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define RECONNECT_INTERVAL_MS 30000
#define GSM_PROBE_INTERVAL_MS 30000
#define GSM_BOOT_DELAY_MS 3000
#define GSM_REG_TIMEOUT_MS 120000
#define GSM_REG_TICK_MS 3000
#define GSM_BOOT_REG_WAIT_MS 60000
#define GSM_DIAG_LOG_INTERVAL_MS 30000

static const char* simStatusLabel(SimStatus s) {
    switch (s) {
        case SIM_READY: return "READY";
        case SIM_LOCKED: return "PIN/PUK";
        default: return "ERROR/NO SIM";
    }
}

static const char* regStatusLabel(SIM800RegStatus r) {
    switch (r) {
        case REG_OK_HOME: return "HOME";
        case REG_OK_ROAMING: return "ROAMING";
        case REG_SEARCHING: return "BUSCANDO";
        case REG_DENIED: return "DENEGADO";
        case REG_UNREGISTERED: return "SIN REGISTRO";
        default: return "?";
    }
}

void ConnectionManager::updateGsmDebugStatus(bool quick) {
    feedWdt();
    int csq = modem.getSignalQuality();
    SIM800RegStatus reg = modem.getRegistrationStatus();
    String simStr;
    if (quick) {
        simStr = gsmModemReady ? "READY" : "?";
    } else {
        simStr = String(simStatusLabel(modem.getSimStatus(1000)));
    }
    gsmDebugInfo = "SIM=" + simStr +
                   " CREG=" + String(regStatusLabel(reg)) +
                   "(" + String((int)reg) + ")" +
                   " CSQ=" + String(csq) + "/31";
}

void ConnectionManager::logGsmDiagnostics(bool quick) {
    updateGsmDebugStatus(quick);
    Logger::info(("GSM diag: " + gsmDebugInfo).c_str());
}

void ConnectionManager::startGsmRegistration(uint32_t timeout_ms) {
    gsmRegPending = true;
    gsmRegStart = millis();
    gsmRegTimeoutMs = timeout_ms;
    gsmRegLastLog = 0;
    gsmDebugInfo = "SIM OK. Registrando en red (fondo)...";
    Logger::info("GSM: registro en segundo plano iniciado.");
}

void ConnectionManager::completeGprsConnect() {
    if (!lockGsmUart()) {
        Logger::warn("GSM: GPRS diferido (UART ocupado).");
        gsmGprsPending = true;
        return;
    }

    Logger::info("Connecting to GPRS...");
    feedWdt();
    if (!modem.gprsConnect("internet", "", "")) {
        feedWdt();
        Logger::error("GPRS Failed.");
        currentNetwork = NET_NONE;
        unlockGsmUart();
        return;
    }

    feedWdt();
    Logger::info("GPRS Connected.");
    currentNetwork = NET_GSM;
    mqtt = &mqttGSM;
    mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
    unlockGsmUart();
}

void ConnectionManager::tickGsmRegistration() {
    if (!gsmRegPending || !gsmModemReady || gsmUartLocked) return;
    if (millis() - gsmRegLastTick < GSM_REG_TICK_MS) return;
    gsmRegLastTick = millis();

    feedWdt();
    yield();

    if (modem.isNetworkConnected()) {
        logGsmDiagnostics(true);
        gsmRegPending = false;
        Logger::info("GSM: registrado en red celular.");
        if (gsmGprsPending) {
            gsmGprsPending = false;
            completeGprsConnect();
        }
        return;
    }

    SIM800RegStatus reg = modem.getRegistrationStatus();
    if (reg == REG_DENIED) {
        logGsmDiagnostics(true);
        gsmRegPending = false;
        gsmGprsPending = false;
        Logger::error("GSM: registro denegado (plan/SIM).");
        return;
    }

    if (millis() - gsmRegStart > gsmRegTimeoutMs) {
        logGsmDiagnostics(true);
        gsmRegPending = false;
        gsmGprsPending = false;
        Logger::warn("GSM: timeout registro en fondo.");
        return;
    }

    updateGsmDebugStatus(true);
    if (gsmRegLastLog == 0 || millis() - gsmRegLastLog > GSM_DIAG_LOG_INTERVAL_MS) {
        gsmRegLastLog = millis();
        Logger::info(("GSM diag: " + gsmDebugInfo).c_str());
        if (reg == REG_SEARCHING) {
            Logger::info("GSM: buscando operador (fondo)...");
        }
    }
}

void ConnectionManager::feedWdt() {
    esp_err_t err = esp_task_wdt_reset();
    if (err == ESP_ERR_NOT_FOUND) {
        esp_task_wdt_add(NULL);
    }
    yield();
}

bool ConnectionManager::lockGsmUart() {
    if (gsmUartLocked) return false;
    gsmUartLocked = true;
    feedWdt();
    return true;
}

void ConnectionManager::unlockGsmUart() {
    gsmUartLocked = false;
}

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

void ConnectionManager::initGsmSerial() {
    if (gsmSerialStarted) return;

    SerialGSM.begin(GSM_BAUD_RATE, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
    gsmSerialStarted = true;

    String uartInfo = "GSM UART" + String(GSM_UART_PORT) +
                      ": pin RX=GPIO" + String(PIN_GSM_RX) +
                      " (<- SIM TX), pin TX=GPIO" + String(PIN_GSM_TX) +
                      " (-> SIM RX), " + String(GSM_BAUD_RATE) + " baud";
    Logger::info(uartInfo.c_str());
    Logger::info("AT debug: consola en portal web (pestaña Logs)");
    gsmDebugInfo = uartInfo;
}

bool ConnectionManager::probeGsmModem(bool verbose) {
    GsmUartGuard guard(lockGsmUart());
    if (!guard.active()) {
        if (verbose) Logger::warn("GSM: UART ocupado, probe omitido.");
        return gsmModemReady;
    }

    initGsmSerial();
    feedWdt();

    while (SerialGSM.available()) {
        SerialGSM.read();
    }

    if (verbose) {
        Logger::info(("GSM: enviando AT via UART" + String(GSM_UART_PORT) +
                      " (GPIO" + String(PIN_GSM_TX) + " TX)...").c_str());
    }
    gsmDebugInfo = "Enviando AT por UART" + String(GSM_UART_PORT) + "...";

    feedWdt();
    modem.init();
    feedWdt();

    for (int attempt = 1; attempt <= 3; attempt++) {
        if (verbose) {
            Logger::info(("GSM: testAT intento " + String(attempt) + "/3").c_str());
        }
        gsmDebugInfo = "testAT intento " + String(attempt) + "/3";

        feedWdt();
        if (modem.testAT(3000)) {
            feedWdt();
            gsmModemReady = true;
            String info = modem.getModemInfo();
            gsmDebugInfo = "Detectado: " + info;
            Logger::info(("GSM detectado — " + info).c_str());
            logGsmDiagnostics(true);

            if (modem.isNetworkConnected()) {
                Logger::info("GSM: registrado en red celular.");
            } else {
                startGsmRegistration(GSM_REG_TIMEOUT_MS);
            }
            return true;
        }

        feedWdt();
        delay(500);
        feedWdt();
    }

    gsmModemReady = false;
    gsmDebugInfo = "Sin respuesta AT. Cableado: RX(GPIO" + String(PIN_GSM_RX) +
                   ")<-SIM-TX, TX(GPIO" + String(PIN_GSM_TX) +
                   ")->SIM-RX, 4V/2A, GND comun";
    Logger::error("GSM: sin respuesta AT tras 3 intentos.");
    return false;
}

bool ConnectionManager::retryGsmConnection() {
    Logger::warn("GSM: reintento manual iniciado.");
    feedWdt();

    gsmRegPending = false;
    gsmGprsPending = false;

    GsmUartGuard guard(lockGsmUart());
    if (!guard.active()) {
        Logger::warn("GSM: UART ocupado, reintento omitido.");
        return false;
    }

    gsmModemReady = false;
    gsmDebugInfo = "Reiniciando UART GSM...";

    if (gsmSerialStarted) {
        SerialGSM.end();
        gsmSerialStarted = false;
        delay(200);
        feedWdt();
    }

    gsmDebugInfo = "Esperando arranque SIM800 (3s)...";
    delay(GSM_BOOT_DELAY_MS);
    feedWdt();

    bool detected = probeGsmModem(true);
    lastGsmProbe = millis();

    if (!detected) {
        return false;
    }

    if (!modem.isNetworkConnected()) {
        feedWdt();
        Logger::warn("GSM: reiniciando radio (reintento manual)...");
        gsmDebugInfo = "Reiniciando radio...";
        modem.restart();
        feedWdt();
        startGsmRegistration(GSM_REG_TIMEOUT_MS);
    }

    return true;
}

String ConnectionManager::getGsmDebugInfo() {
    return gsmDebugInfo;
}

String ConnectionManager::sendAtCommand(const char* cmd) {
    GsmUartGuard guard(lockGsmUart());
    if (!guard.active()) {
        return "ERROR: GSM ocupado (modem en uso, reintenta)";
    }

    bool resumeReg = gsmRegPending;
    gsmRegPending = false;

    String command = String(cmd);
    command.trim();
    if (command.length() == 0 || command.length() > 128) {
        gsmRegPending = resumeReg;
        return "ERROR: comando invalido (max 128 chars)";
    }

    for (size_t i = 0; i < command.length(); i++) {
        char c = command.charAt(i);
        if (c < 32 || c == 127) {
            gsmRegPending = resumeReg;
            return "ERROR: caracteres no permitidos";
        }
    }

    command.toUpperCase();
    if (!command.startsWith("AT")) {
        if (command.startsWith("+")) {
            command = "AT" + command;
        } else {
            command = "AT+" + command;
        }
    }

    initGsmSerial();
    feedWdt();

    while (SerialGSM.available()) {
        SerialGSM.read();
    }

    SerialGSM.print(command);
    SerialGSM.print("\r\n");

    String response = "";
    response.reserve(256);
    unsigned long lastData = millis();
    unsigned long start = millis();

    while (millis() - start < 4000) {
        feedWdt();
        while (SerialGSM.available() && response.length() < 480) {
            char c = SerialGSM.read();
            response += c;
            lastData = millis();
        }

        if (response.length() > 0) {
            if (response.indexOf("OK") >= 0 || response.indexOf("ERROR") >= 0) {
                if (millis() - lastData > 200) break;
            }
        }
        delay(10);
    }

    response.trim();
    if (response.length() == 0) {
        response = "(sin respuesta en 4s)";
    }

    gsmRegPending = resumeReg;
    Logger::info(("AT manual [" + command + "]: " + response).c_str());
    return response;
}

void ConnectionManager::init() {
    if (!StorageManager::config.configured) {
        Logger::info("Network init skipped (AP configuration mode).");
        bootPhase = BOOT_DONE;
        return;
    }

    macStr = getDeviceId();
    // UART GSM se abre en processBootstrap (da tiempo a enumerar USB CDC)

    bootPhase = BOOT_WAIT_GSM;
    bootPhaseStart = millis();
    gsmDebugInfo = "Esperando arranque SIM800 (3s)...";
    Logger::info("Bootstrap red/GSM diferido al loop (no bloquea setup).");
}

void ConnectionManager::processBootstrap() {
    feedWdt();

    switch (bootPhase) {
        case BOOT_WAIT_GSM:
            if (millis() - bootPhaseStart < GSM_BOOT_DELAY_MS) return;
            bootPhase = BOOT_PROBE_GSM;
            bootPhaseStart = millis();
            probeGsmModem(true);
            lastGsmProbe = millis();
            if (gsmModemReady && !modem.isNetworkConnected()) {
                bootPhase = BOOT_GSM_REG;
                bootPhaseStart = millis();
                Logger::info("Bootstrap: GSM registrando antes de WiFi...");
            } else {
                bootPhase = BOOT_WIFI;
                bootPhaseStart = millis();
                startWiFiConnection();
                Logger::info("Bootstrap: WiFi en progreso...");
            }
            return;

        case BOOT_GSM_REG:
            if (modem.isNetworkConnected()) {
                gsmRegPending = false;
                Logger::info("Bootstrap: GSM registrado, iniciando WiFi...");
                bootPhase = BOOT_WIFI;
                bootPhaseStart = millis();
                startWiFiConnection();
                return;
            }
            if (millis() - bootPhaseStart > GSM_BOOT_REG_WAIT_MS) {
                Logger::warn("Bootstrap: GSM sin registro en 60s, iniciando WiFi...");
                bootPhase = BOOT_WIFI;
                bootPhaseStart = millis();
                startWiFiConnection();
            }
            return;

        case BOOT_WIFI:
            feedWdt();
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnectPending = false;
                currentNetwork = NET_WIFI;
                mqtt = &mqttWiFi;
                mqtt->setServer(StorageManager::config.mqttHost, StorageManager::config.mqttPort);
                bootPhase = BOOT_DONE;
                Logger::info("Bootstrap completo: WiFi conectado.");
                return;
            }
            if (millis() - bootPhaseStart > WIFI_CONNECT_TIMEOUT_MS) {
                Logger::warn("Bootstrap: WiFi timeout, probando GPRS...");
                wifiConnectPending = false;
                connectGSM();
                bootPhase = BOOT_DONE;
            }
            return;

        case BOOT_DONE:
        default:
            return;
    }
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
        feedWdt();
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
    feedWdt();

    if (!gsmModemReady) {
        Logger::warn("GSM: modem no detectado, reintentando antes de GPRS...");
        if (!probeGsmModem(true)) {
            currentNetwork = NET_NONE;
            return;
        }
        lastGsmProbe = millis();
    }

    if (!modem.isNetworkConnected()) {
        Logger::info("GSM: sin registro, iniciando attach en fondo...");
        gsmGprsPending = true;
        startGsmRegistration(GSM_REG_TIMEOUT_MS);
        currentNetwork = NET_NONE;
        return;
    }

    completeGprsConnect();
}

void ConnectionManager::update() {
    if (!StorageManager::config.configured) return;

    tickGsmRegistration();

    if (bootPhase != BOOT_DONE) {
        processBootstrap();
        return;
    }

    if (millis() - lastGsmProbe > GSM_PROBE_INTERVAL_MS) {
        lastGsmProbe = millis();
        if (gsmModemReady) {
            if (gsmUartLocked) return;
            GsmUartGuard guard(lockGsmUart());
            if (!guard.active()) return;
            feedWdt();
            gsmModemReady = modem.testAT(1000);
            if (gsmModemReady) {
                updateGsmDebugStatus(true);
            } else {
                Logger::warn("GSM: modem dejo de responder.");
                gsmDebugInfo = "Modem dejo de responder AT";
            }
        } else {
            if (gsmUartLocked) return;
            GsmUartGuard guard(lockGsmUart());
            if (!guard.active()) return;
            Logger::info("GSM: reintento automatico (testAT rapido)...");
            initGsmSerial();
            feedWdt();
            if (modem.testAT(2000)) {
                gsmModemReady = true;
                gsmDebugInfo = "Detectado en reintento: " + modem.getModemInfo();
                Logger::info(("GSM: detectado en reintento — " + gsmDebugInfo).c_str());
            } else {
                gsmDebugInfo = "Sin respuesta AT (auto-retry)";
            }
        }
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
    feedWdt();
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
