#pragma once

#include <Arduino.h>

// =============================================================================
// PINOUT — ESP32-S3 SuperMini (header accesible)
// =============================================================================
//
//  IZQUIERDA (arriba→abajo)     DERECHA (arriba→abajo)
//  ─────────────────────────    ─────────────────────
//  TX    GPIO43  → adaptador RX  5V
//  RX    GPIO44  ← adaptador TX  GND
//  GP1   LED Rojo                3V3
//  GP2   LED Verde               GP13
//  GP3   LED Azul                GP12
//  GP4   Fase L1                 GP11
//  GP5   Fase L2                 GP10
//  GP6   Fase L3                 GP9
//  GP7   GSM TX → SIM800 RX      GP8  GSM RX ← SIM800 TX
//
//  Onboard: LED azul en GPIO48 (no usado por este firmware)
//  NO USAR: GPIO18, GPIO19, GPIO20 (no en header / USB interno)
//
//  Debug USB-C:  /dev/cu.usbmodem* @115200 (USB-Serial/JTAG interno)
//  Debug header: Serial0 UART0 en TX/RX (GPIO43/44) @115200 — adaptador USB-TTL
//  GSM:          UART1 en GP7/GP8 @9600
//  NOTA: esp_log interno va a UART0; nuestro Logger usa USB-JTAG directo.
// =============================================================================

// --- Fases (optoacopladores → GND cuando hay tensión) ---
#define PIN_PHASE_1 4   // GP4
#define PIN_PHASE_2 5   // GP5
#define PIN_PHASE_3 6   // GP6

// --- LEDs de estado (cátodo común, PWM HIGH-side; resistencia ~330Ω) ---
#define PIN_LED_R 1     // GP1 — rojo: alerta / falla de fase
#define PIN_LED_G 2     // GP2 — verde: fases OK + MQTT conectado
#define PIN_LED_B 3     // GP3 — azul: modo AP / configuración
#define LED_PWM_FREQ_HZ 5000
#define LED_PWM_BITS 8
#define LED_BRIGHTNESS_PERCENT 25  // brillo ON (0-100 %)

// --- Consola debug en header TX/RX (UART0) ---
#define PIN_DEBUG_RX 44   // Header RX ← adaptador USB-TTL TX
#define PIN_DEBUG_TX 43   // Header TX → adaptador USB-TTL RX
#define DEBUG_UART_HEADER 1
#define DEBUG_BAUD_RATE 115200

// --- SIM800L (UART1 en GP7/GP8 — libera TX/RX del header) ---
#define PIN_GSM_RX 8    // GP8 ← SIM800 TX
#define PIN_GSM_TX 7    // GP7 → SIM800 RX (divisor 3.3V→2.8V si hace falta)
#define GSM_UART_PORT 1
#define GSM_BAUD_RATE 9600
#define GSM_DEBUG_AT 0  // 1 = eco TinyGSM en USB (desactivado; usar consola AT del portal)
#define USB_STATUS_INTERVAL_MS 30000  // heartbeat en consola USB (0 = desactivado)

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
