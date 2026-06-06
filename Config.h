#pragma once

#include <Arduino.h>

// =============================================================================
// PINOUT — ESP32-S3 SuperMini (header accesible)
// =============================================================================
//
//  IZQUIERDA (arriba→abajo)     DERECHA (arriba→abajo)
//  ─────────────────────────    ─────────────────────
//  TX    GPIO43  → SIM800 RX     5V
//  RX    GPIO44  ← SIM800 TX     GND
//  GP1   LED Rojo                3V3
//  GP2   LED Verde               GP13
//  GP3   LED Azul                GP12
//  GP4   Fase L1                 GP11
//  GP5   Fase L2                 GP10
//  GP6   Fase L3                 GP9
//  GP7   (libre)                 GP8  (libre)
//
//  Onboard: LED azul en GPIO48 (no usado por este firmware)
//  NO USAR: GPIO18, GPIO19, GPIO20 (no en header / USB interno)
//
//  Debug USB: cable USB-C @115200 (CDC On Boot = Enabled)
//  GSM:       pines TX/RX del header @9600 (UART0)
// =============================================================================

// --- Fases (optoacopladores → GND cuando hay tensión) ---
#define PIN_PHASE_1 4   // GP4
#define PIN_PHASE_2 5   // GP5
#define PIN_PHASE_3 6   // GP6

// --- LEDs de estado (cátodo común, HIGH = ON; resistencia ~330Ω) ---
#define PIN_LED_R 1     // GP1 — rojo: alerta / falla de fase
#define PIN_LED_G 2     // GP2 — verde: fases OK + MQTT conectado
#define PIN_LED_B 3     // GP3 — azul: modo AP / configuración

// --- SIM800L (UART0 = pines TX/RX del header) ---
#define PIN_GSM_RX 44   // Pin RX ← SIM800 TX
#define PIN_GSM_TX 43   // Pin TX → SIM800 RX (divisor 3.3V→2.8V si hace falta)
#define GSM_UART_PORT 0
#define GSM_BAUD_RATE 9600
#define GSM_DEBUG_AT 1  // 1 = eco AT en monitor USB @115200

// --- CONFIGURATION LIMITS ---
#define MAX_PHONE_NUMBERS 3
#define MAX_PHONE_LEN 20
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_MQTT_HOST_LEN 64
#define MAX_MQTT_USER_LEN 32
#define MAX_MQTT_PASS_LEN 32
#define MAX_MQTT_PORT 65535
#define MAX_HOSTNAME_LEN 32
#define MAX_OTA_PASS_LEN 32

#define DEFAULT_HOSTNAME "phase"
#define DEFAULT_OTA_PASS "Phase123"

// --- TIMINGS ---
#define DEBOUNCE_DELAY_MS 50    // 50ms to filter 50Hz noise/flicker
#define MQTT_KEEPALIVE_MS 30000 // 30 seconds
#define WDT_TIMEOUT_MS 60000    // 60s — bootstrap GSM/WiFi puede tardar

// --- ENUMS ---
enum SystemState {
  STATE_AP_CONFIG,
  STATE_CONNECTING_WIFI,
  STATE_CONNECTING_GSM,
  STATE_NORMAL_OP
};

enum PhaseCriticality { CRIT_NORMAL, CRIT_PARTIAL_FAIL, CRIT_TOTAL_FAIL };

enum NetworkMode { NET_NONE, NET_WIFI, NET_GSM };

inline void safeStrCopy(char* dest, size_t destSize, const char* src) {
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}
