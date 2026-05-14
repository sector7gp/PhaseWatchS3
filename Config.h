#pragma once

#include <Arduino.h>

// --- GPIO DEFINITIONS ---
// Phase inputs
#define PIN_PHASE_1 4
#define PIN_PHASE_2 5
#define PIN_PHASE_3 6

// LEDs (Common cathode assumed, HIGH = ON)
#define PIN_LED_R 15
#define PIN_LED_G 16
#define PIN_LED_B 17

// SIM800L
#define PIN_GSM_RX 18 // ESP RX, connect to SIM TX
#define PIN_GSM_TX 19 // ESP TX, connect to SIM RX
#define GSM_BAUD_RATE 9600

// --- CONFIGURATION LIMITS ---
#define MAX_PHONE_NUMBERS 3
#define MAX_PHONE_LEN 20
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_MQTT_HOST_LEN 64
#define MAX_MQTT_USER_LEN 32
#define MAX_MQTT_PASS_LEN 32
#define MAX_MQTT_PORT 65535

// --- TIMINGS ---
#define DEBOUNCE_DELAY_MS 50       // 50ms to filter 50Hz noise/flicker
#define MQTT_KEEPALIVE_MS 30000    // 30 seconds
#define WDT_TIMEOUT_MS 30000       // 30 seconds watchdog

// --- ENUMS ---
enum SystemState {
  STATE_AP_CONFIG,
  STATE_CONNECTING_WIFI,
  STATE_CONNECTING_GSM,
  STATE_NORMAL_OP
};

enum PhaseCriticality {
  CRIT_NORMAL,
  CRIT_PARTIAL_FAIL,
  CRIT_TOTAL_FAIL
};

enum NetworkMode {
  NET_NONE,
  NET_WIFI,
  NET_GSM
};
