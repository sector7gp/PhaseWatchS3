#include "Logger.h"

#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include "hal/usb_serial_jtag_ll.h"
#endif

#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE
#include "HWCDC.h"
#endif

std::vector<String> Logger::logLines;
bool Logger::usbReplayDone = false;

#if SOC_USB_SERIAL_JTAG_SUPPORTED
// Escritura directa al FIFO USB-JTAG: visible en /dev/cu.usbmodem* sin depender
// de HWCDC::isConnected() ni de CONFIG_ESP_CONSOLE (que apunta a UART0).
static void writeUsbJtag(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int timeout = 2000;
        while (!usb_serial_jtag_ll_txfifo_writable() && --timeout > 0) {
            delayMicroseconds(10);
        }
        if (timeout == 0) {
            return;
        }
        usb_serial_jtag_ll_write_txfifo(reinterpret_cast<const uint8_t*>(&data[i]), 1);
        usb_serial_jtag_ll_txfifo_flush();
    }
}
#endif

void Logger::setupUsbReplay() {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE
    HWCDCSerial.onEvent(ARDUINO_HW_CDC_CONNECTED_EVENT, [](void*, esp_event_base_t, int32_t, void*) {
        delay(50);
        usbReplayDone = false;
        replayToSerial();
        usbReplayDone = true;
    });
#endif
}

void Logger::pollUsbReplay() {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE
    if (!usbReplayDone && HWCDCSerial.isConnected()) {
        replayToSerial();
        usbReplayDone = true;
    }
#endif
}

void Logger::init() {
    logLines.reserve(MAX_LOG_LINES);
    setupUsbReplay();
    info("Logger initialized");
}

static void writeSerialLine(const String& line) {
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    writeUsbJtag(line.c_str(), line.length());
    writeUsbJtag("\n", 1);
#endif
#if DEBUG_UART_HEADER
    Serial0.println(line);
#endif
}

void Logger::log(const char* level, const char* msg) {
    String logEntry = String("[") + String(millis() / 1000) + "s] " + String(level) + ": " + String(msg);
    writeSerialLine(logEntry);

    if (logLines.size() >= MAX_LOG_LINES) {
        logLines.erase(logLines.begin());
    }
    logLines.push_back(logEntry);
}

void Logger::info(const char* msg) {
    log("INFO", msg);
}

void Logger::warn(const char* msg) {
    log("WARN", msg);
}

void Logger::error(const char* msg) {
    log("ERROR", msg);
}

String Logger::getLogs() {
    String fullLog = "";
    for (const String& line : logLines) {
        fullLog += line + "\n";
    }
    return fullLog;
}

void Logger::clearLogs() {
    logLines.clear();
    info("Logs cleared");
}

void Logger::replayToSerial() {
    writeSerialLine("");
    writeSerialLine("=== PhaseWatch S3 — log buffer (USB conectado) ===");
    for (const String& line : logLines) {
        writeSerialLine(line);
    }
    writeSerialLine("=== fin buffer — nuevos eventos debajo ===");
}
