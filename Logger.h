#pragma once

#include <Arduino.h>
#include <vector>
#include <String.h>

class Logger {
public:
    static void init();
    static void log(const char* level, const char* msg);
    static void info(const char* msg);
    static void warn(const char* msg);
    static void error(const char* msg);
    
    // Returns the full log as a single string (HTML formatted or plain text)
    static String getLogs();
    static void clearLogs();

private:
    static std::vector<String> logLines;
    static const size_t MAX_LOG_LINES = 50;
};
