#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "Config.h"
#include "Logger.h"
#include "StorageManager.h"
#include "PhaseMonitor.h"
#include "ConnectionManager.h"
#include "NetworkServices.h"
#include "WebPortal.h"

unsigned long lastBlinkTime = 0;
bool ledState = false;

void setLED(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r ? HIGH : LOW);
    digitalWrite(PIN_LED_G, g ? HIGH : LOW);
    digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

void logPeriodicUsbStatus() {
#if USB_STATUS_INTERVAL_MS > 0
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus < USB_STATUS_INTERVAL_MS) return;
    lastStatus = millis();

    String status = "Status: ";
    if (WebPortal::isAPMode()) {
        status += "modo=AP";
    } else if (WiFi.status() == WL_CONNECTED) {
        status += "WiFi=OK IP=" + WiFi.localIP().toString();
    } else {
        status += "WiFi=--";
    }
    status += ConnectionManager::isMqttConnected() ? " MQTT=OK" : " MQTT=--";
    status += ConnectionManager::isGsmDetected() ? " GSM=OK" : " GSM=--";
    Logger::info(status.c_str());
#endif
}

void setup() {
    // Inicializa HWCDC/USB-JTAG (necesario para upload y FIFO USB)
    Serial.begin(115200);
    delay(300);
#if DEBUG_UART_HEADER
    Serial0.begin(DEBUG_BAUD_RATE, SERIAL_8N1, PIN_DEBUG_RX, PIN_DEBUG_TX);
#endif

    // Evita auto-reconnect NVS y carrera WiFi (error 12308) antes de setup
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // Config LEDs
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    setLED(false, false, false);
    
    Logger::init();
    Logger::info("Booting PhaseWatch S3...");

    // Arduino 3.x ya inicializa TWDT — reconfigurar sin llamar init (evita log de error)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    if (esp_task_wdt_reconfigure(&wdt_config) != ESP_OK) {
        esp_task_wdt_init(&wdt_config);
    }
    esp_task_wdt_add(NULL);
    
    StorageManager::init();
    PhaseMonitor::init();
    NetworkServices::init();

    if (StorageManager::config.configured) {
        ConnectionManager::init();
        WebPortal::init();
    } else {
        WebPortal::init();
        ConnectionManager::init();
    }
    
    Logger::info("Setup complete.");
}

void loop() {
    esp_task_wdt_reset(); // Feed WDT
    Logger::pollUsbReplay();
    
    PhaseMonitor::update();
    WebPortal::update();
    ConnectionManager::update();
    NetworkServices::update();
    
    handlePhaseEvents();
    updateIndicators();
    logPeriodicUsbStatus();
}

void handlePhaseEvents() {
    if (PhaseMonitor::hasChanged()) {
        PhaseCriticality crit = PhaseMonitor::getCriticality();
        
        String message = "";
        String eventType = "";
        
        if (crit == CRIT_NORMAL) {
            eventType = "PHASE_RESTORED";
            message = "Todas las fases han sido restauradas.";
        } else {
            eventType = "PHASE_LOST";
            message = "Falla detectada. L1:";
            message += PhaseMonitor::l1_ok ? "OK" : "FALLA";
            message += " L2:";
            message += PhaseMonitor::l2_ok ? "OK" : "FALLA";
            message += " L3:";
            message += PhaseMonitor::l3_ok ? "OK" : "FALLA";
        }
        
        Logger::warn(("Event Triggered: " + message).c_str());
        ConnectionManager::publishEvent(eventType.c_str(), message.c_str());
        
        PhaseMonitor::clearChanged();
    }
}

void updateIndicators() {
    if (WebPortal::isAPMode()) {
        // Blue fixed for AP Config Mode
        setLED(false, false, true);
        return;
    }
    
    PhaseCriticality crit = PhaseMonitor::getCriticality();
    
    if (crit != CRIT_NORMAL) {
        // Red blinking for Alert
        if (millis() - lastBlinkTime > 500) {
            ledState = !ledState;
            setLED(ledState, false, false);
            lastBlinkTime = millis();
        }
    } else {
        // Green fixed for Normal Operation + Connected
        // Let's blink green if connected, yellow if disconnected
        if (ConnectionManager::isConnected()) {
            setLED(false, true, false); // Solid green
        } else {
            // Yellow blinking for Normal Phase but no connection
            if (millis() - lastBlinkTime > 1000) {
                ledState = !ledState;
                setLED(ledState, ledState, false); // Yellow (R+G)
                lastBlinkTime = millis();
            }
        }
    }
}
