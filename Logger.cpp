#include "Logger.h"

std::vector<String> Logger::logLines;

void Logger::init() {
    logLines.reserve(MAX_LOG_LINES);
    info("Logger initialized");
}

void Logger::log(const char* level, const char* msg) {
    String logEntry = String("[") + String(millis() / 1000) + "s] " + String(level) + ": " + String(msg);
    Serial.println(logEntry);
    
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
