#pragma once

#include <Arduino.h>

class NetworkServices {
public:
    static void init();
    static void update();
    static void stop();

    static bool isMdnsActive();
    static bool isOtaActive();
    static String getMdnsHostname();
    static String getMdnsFqdn();

private:
    static void start();
    static String sanitizeHostname(const char* raw);

    static bool servicesStarted;
    static String activeHostname;
};
