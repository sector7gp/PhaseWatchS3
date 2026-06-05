# PhaseWatchS3

**Versión:** v0.1

Sistema de monitoreo de fases (L1, L2, L3) para ESP32-S3 con doble conectividad (Wi-Fi y GPRS), notificaciones MQTT y alertas por SMS.

## Changelog

### v0.1

Correcciones de estabilidad y conectividad sobre el commit inicial:

- **`isConnected()`** ahora requiere red activa **y** sesión MQTT conectada, para que el LED verde refleje el estado real del dispositivo.
- **Cambio de red (Wi-Fi ↔ GPRS):** se desconecta MQTT antes de alternar entre clientes, evitando sesiones colgadas.
- **Reconexión no bloqueante:** el `loop` principal ya no se congela reintentando Wi-Fi; los reintentos se limitan a cada 30 s. En el arranque se mantiene conexión bloqueante con alimentación del watchdog.
- **SMS:** solo se envía si el módem GSM está registrado en red; si el dispositivo opera solo por Wi-Fi, se omite con un log de advertencia.
- **Copia segura de strings:** `safeStrCopy()` garantiza terminador nulo en la configuración guardada desde el portal web y al cargar desde NVS.

## Esquema de Conexiones

### Entradas de Fase (Aisladas)
Se recomienda utilizar optoacopladores (ej. PC817) conectados a las líneas de red a través de un circuito de reducción (capacitivo/resistivo y puente de diodos) para obtener una señal digital segura de 3.3V al microcontrolador.
*   **L1 (Fase 1):** GPIO 4 (PULLUP interno, la señal debe llevar a GND cuando hay tensión).
*   **L2 (Fase 2):** GPIO 5 (PULLUP interno)
*   **L3 (Fase 3):** GPIO 6 (PULLUP interno)

### Módulo Celular (SIM800L)
*   **VCC:** Fuente externa (3.7V - 4.2V, 2A). El ESP32 no provee corriente suficiente.
*   **GND:** Común con ESP32 y fuente.
*   **SIM800L TX:** GPIO 18 (RX en el ESP32). Nivel lógico 2.8V (el ESP32 de 3.3V lo lee bien).
*   **SIM800L RX:** GPIO 19 (TX en el ESP32).
    *   *Nota Divisor de Tensión:* El SIM800L tiene nivel lógico de 2.8V. La señal TX del ESP32 (3.3V) podría requerir un divisor resistivo simple (Ej: 10k en serie, 20k a GND) para no dañar el pin RX del SIM800L si la placa no trae adaptación de niveles.

### LED de Estado (RGB o individuales)
*   **LED Rojo (Alertas):** GPIO 15 — parpadea cuando hay falla de fase.
*   **LED Verde (Conexión):** GPIO 16 — fijo cuando las fases están OK y MQTT está conectado.
*   **LED Azul (Modo AP/Config):** GPIO 17 — fijo en modo configuración (Access Point).
*   **Amarillo (R+G):** parpadea cuando las fases están OK pero no hay conexión MQTT activa.

## Documentación de Tópicos MQTT

### Estructura General

*   **Tópico Base:** `phasewatch/<client_id>` (donde `<client_id>` es la dirección MAC del ESP32, ej: `phasewatch/AABBCCDDEEFF`).

### Publicaciones (Del ESP32 al Broker)

1.  **Estado (Heartbeat / Keep-alive)**
    *   **Tópico:** `phasewatch/<client_id>/status`
    *   **Frecuencia:** Cada 30 segundos (configurable).
    *   **Payload (JSON):**
        ```json
        {
          "type": "heartbeat",
          "timestamp": 3600,
          "l1": true,
          "l2": true,
          "l3": true,
          "criticality": "NORMAL", 
          "network": "WIFI", 
          "rssi": -65
        }
        ```
        *(Valores de criticality: "NORMAL", "PARTIAL_FAIL", "TOTAL_FAIL" | network: "WIFI", "GPRS", "NONE" | timestamp: segundos desde el arranque del dispositivo)*

2.  **Eventos (Caída o Restauración de Fase)**
    *   **Tópico:** `phasewatch/<client_id>/events`
    *   **Frecuencia:** Inmediata al detectar y confirmar (con debouncing) un cambio de estado.
    *   **Payload (JSON):**
        ```json
        {
          "type": "event",
          "event": "PHASE_LOST",
          "l1": true,
          "l2": false,
          "l3": true,
          "criticality": "PARTIAL_FAIL",
          "message": "Falla en fase L2"
        }
        ```

3.  **Last Will and Testament (LWT)**
    *   **Tópico:** `phasewatch/<client_id>/lwt`
    *   **Mensaje enviado en conexión:** `{"status": "online"}`
    *   **Mensaje enviado por el broker si se desconecta offline:** `{"status": "offline"}`
