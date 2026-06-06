# PhaseWatchS3

**Versión:** v0.4

Sistema de monitoreo de fases (L1, L2, L3) para ESP32-S3 con doble conectividad (Wi-Fi y GPRS), notificaciones MQTT y alertas por SMS.

## Changelog

### v0.4

Estabilidad GSM y herramientas de depuración:

- **Registro GSM en segundo plano:** sin bloquear el `loop` ni el arranque; consultas AT limitadas a cada 3 s.
- **Bootstrap GSM → WiFi:** el módem registra en red hasta 60 s antes de iniciar Wi-Fi (menos contención RF/alimentación).
- **Diagnóstico GSM:** `SIM=… CREG=…(código) CSQ=…/31` en logs y dashboard (`gsm_debug`).
- **Consola AT en portal:** pestaña Logs con campo de texto, atajos (`CREG?`, `CSQ`, `CCID`, etc.) y API `POST /api/gsm/at`.
- **Mutex UART GSM:** evita colisiones entre registro automático y comandos AT manuales (fix panics/crashes).
- **Fix watchdog:** sin monitorear tareas idle; stack del loop ampliado a 16 KB; `yield()` en operaciones GSM.
- **TinyGSM debug desactivado** por defecto (`GSM_DEBUG_AT=0`); usar consola AT del portal para depurar.

### v0.3

Corrección de pinout y estabilidad para **ESP32-S3 SuperMini**:

- **Pinout SuperMini documentado** en `Config.h`: mapa completo del header accesible (TX/RX, GP1–GP13).
- **GSM en UART0** (pines **TX/RX** del header, GPIO 43/44) en lugar de GPIO 18/19 — que no salen al header y GPIO 19/20 son USB interno.
- **LEDs reasignados** a GP1/GP2/GP3 (GPIO 1/2/3), accesibles en el borde de la placa.
- **USB CDC estable:** compilación con `USBMode=hwcdc` + `CDCOnBoot=cdc`; debug por USB-C, GSM por pines TX/RX.
- **Fix watchdog:** `esp_task_wdt_reconfigure()` sin doble init; bootstrap GSM/WiFi no bloqueante con `feedWdt()`.
- **Fix crash post-config:** `WiFi.mode(WIFI_STA)` antes de `server.begin()` en modo estación.
- **GSM debug:** `GSM_DEBUG_AT`, probe automático cada 30 s, API `POST /api/gsm/retry`, campo `gsm_debug` en `/api/status`.
- **mDNS y OTA:** `NetworkServices` con hostname configurable (`phase.local` por defecto) y password OTA (`Phase123`).
- **Portal web:** botón reintentar GSM, reset de fábrica con modal en pestaña Configuración, campos mDNS/OTA.

### v0.2

Portal web renovado y mejoras de arranque/configuración:

- **SPA moderna** (HTML/CSS/JS en PROGMEM, sin dependencias externas) con pestañas: Dashboard, Configuración y Logs.
- **Dashboard en vivo:** se actualiza automáticamente cada 2 s al detectar cambios en fases, red, MQTT o GSM.
- **Estado ampliado:** conexión MQTT, módulo GSM detectado, registro en red, topics MQTT publicados y tensión del SIM800L.
- **API REST JSON:** `/api/status`, `/api/config`, `/api/logs`, `/api/save`, `/api/test`, `/api/reset`.
- **Modo AP mejorado:** IP fija `192.168.4.1`, logs claros en serial, cierre limpio de WiFi antes de reiniciar.
- **Fix crash al guardar config:** reinicio seguro del AP y orden de inicialización corregido (red antes del portal cuando hay config).
- **Reset de fábrica** desde el dashboard para volver al hotspot `PhaseWatch_Config`.

### v0.1

Correcciones de estabilidad y conectividad sobre el commit inicial:

- **`isConnected()`** ahora requiere red activa **y** sesión MQTT conectada, para que el LED verde refleje el estado real del dispositivo.
- **Cambio de red (Wi-Fi ↔ GPRS):** se desconecta MQTT antes de alternar entre clientes, evitando sesiones colgadas.
- **Reconexión no bloqueante:** el `loop` principal ya no se congela reintentando Wi-Fi; los reintentos se limitan a cada 30 s. En el arranque se mantiene conexión bloqueante con alimentación del watchdog.
- **SMS:** solo se envía si el módem GSM está registrado en red; si el dispositivo opera solo por Wi-Fi, se omite con un log de advertencia.
- **Copia segura de strings:** `safeStrCopy()` garantiza terminador nulo en la configuración guardada desde el portal web y al cargar desde NVS.

## Carga del firmware en ESP32-S3 SuperMini

### Requisitos

- [Arduino IDE 2](https://www.arduino.cc/en/software) o [arduino-cli](https://arduino.github.io/arduino-cli/)
- Paquete de placas **esp32** de Espressif Systems (v3.x recomendado)
- [esptool](https://docs.espressif.com/projects/esptool/) (`pip install esptool`)
- Cable USB-C de datos

**Librerías Arduino** (instalar desde el Gestor de librerías):

| Librería | Uso |
|----------|-----|
| [PubSubClient](https://github.com/knolleary/pubsubclient) | Cliente MQTT |
| [TinyGSM](https://github.com/vshymanskyy/TinyGSM) | Módem SIM800L |

### Configuración de la placa (Arduino IDE)

En **Herramientas**, seleccionar:

| Opción | Valor |
|--------|-------|
| Placa | `ESP32S3 Dev Module` |
| USB Mode | `Hardware CDC and JTAG` |
| USB CDC On Boot | `Enabled` |
| CPU Frequency | `240 MHz` |
| Flash Size | `4MB (32Mb)` |
| Flash Mode | `QIO 80MHz` |
| Partition Scheme | `Default 4MB with spiffs` |
| PSRAM | `Disabled` |
| Upload Speed | `921600` |
| Puerto | El puerto USB de la SuperMini (ver abajo) |

> La ESP32-S3 SuperMini no aparece como placa propia en el paquete esp32; se usa **ESP32S3 Dev Module** con los parámetros anteriores.

**URL del gestor de placas** (Archivo → Preferencias → URLs adicionales):

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

### Identificar el puerto serie

**macOS / Linux:**

```bash
ls /dev/cu.usb* /dev/tty.usb* 2>/dev/null
# Ejemplo: /dev/cu.usbmodem101
```

**Windows:** `COM3`, `COM4`, etc. en el Administrador de dispositivos.

Conectar la placa por USB-C. Si no aparece ningún puerto, mantener pulsado **BOOT**, pulsar y soltar **RESET**, y soltar **BOOT** (modo descarga).

### Compilar el firmware

Colocar todos los archivos del proyecto en una carpeta llamada `PhaseWatchS3` con `PhaseWatchS3.ino` en la raíz.

**Arduino IDE:** Verificar (✓). Los binarios quedan en una carpeta temporal; para obtener la ruta exacta, activar en Preferencias:

> *Mostrar salida detallada durante: subida*

**arduino-cli:**

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32

arduino-cli lib install "PubSubClient" "TinyGSM"

arduino-cli compile \
  --build-property build.extra_flags=-DARDUINO_LOOP_STACK_SIZE=16384 \
  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,FlashMode=qio,PartitionScheme=default \
  --output-dir build \
  PhaseWatchS3/
```

Tras compilar, en `build/` deberían generarse:

| Archivo | Descripción |
|---------|-------------|
| `PhaseWatchS3.ino.bootloader.bin` | Bootloader |
| `PhaseWatchS3.ino.partitions.bin` | Tabla de particiones |
| `PhaseWatchS3.ino.bin` | Firmware de la aplicación |

El archivo `boot_app0.bin` viene con el paquete esp32 (no se genera al compilar el sketch):

```bash
# macOS — ajustar la versión del paquete si es distinta
ls ~/Library/Arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin
```

### Subir con esptool

Reemplazar `PORT` por el puerto detectado y las rutas según dónde se compilaron los binarios.

```bash
python -m esptool \
  --chip esp32s3 \
  --port PORT \
  --baud 921600 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode qio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x0    build/PhaseWatchS3.ino.bootloader.bin \
  0x8000 build/PhaseWatchS3.ino.partitions.bin \
  0xe000 ~/Library/Arduino15/packages/esp32/hardware/esp32/3.0.7/tools/partitions/boot_app0.bin \
  0x10000 build/PhaseWatchS3.ino.bin
```

**Ejemplo en macOS** (puerto y versión del paquete esp32 reales):

```bash
python -m esptool \
  --chip esp32s3 \
  --port /dev/cu.usbmodem101 \
  --baud 921600 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode qio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x0    build/PhaseWatchS3.ino.bootloader.bin \
  0x8000 build/PhaseWatchS3.ino.partitions.bin \
  0xe000 ~/Library/Arduino15/packages/esp32/hardware/esp32/3.0.7/tools/partitions/boot_app0.bin \
  0x10000 build/PhaseWatchS3.ino.bin
```

Salida esperada al finalizar:

```
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

#### Obtener el comando exacto desde Arduino IDE

Si la compilación manual falla por rutas u offsets, subir una vez desde el IDE con la salida detallada activada. En la consola aparecerá la línea completa de `esptool` con todos los paths y direcciones correctos para tu entorno. Copiarla y reutilizarla para flasheos posteriores sin abrir el IDE.

#### Borrar flash completa (opcional)

Útil si la placa quedó en un estado inconsistente o al flashear por primera vez:

```bash
python -m esptool --chip esp32s3 --port PORT erase_flash
```

Luego ejecutar de nuevo el comando `write_flash`.

### Monitor serial

Con **USB CDC On Boot** habilitado, el puerto de carga es también el de depuración (115200 baud):

```bash
# arduino-cli
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200

# o con screen (macOS/Linux)
screen /dev/cu.usbmodem101 115200
```

Al arrancar debería verse:

```
[INFO]: Booting PhaseWatch S3...
```

### Primera configuración

Si el dispositivo no tiene configuración guardada en NVS, arranca en modo Access Point:

- **SSID:** `PhaseWatch_Config`
- **Contraseña:** `12345678`
- Conectarse y abrir `http://192.168.4.1` para cargar Wi-Fi, MQTT y teléfonos SMS.

## Portal Web Local

Accesible en `http://192.168.4.1` (modo AP) o en la IP LAN del dispositivo (modo operativo).

### Pestañas

| Pestaña | Contenido |
|---------|-----------|
| **Dashboard** | Fases L1/L2/L3, criticidad, red activa, RSSI, MQTT, GSM, topics y batería |
| **Configuración** | Wi-Fi, MQTT, teléfonos SMS. Guardar reinicia el dispositivo |
| **Logs** | Registro del sistema y **consola AT** para depurar el SIM800 |

### API REST

| Método | Ruta | Descripción |
|--------|------|-------------|
| `GET` | `/api/status` | Estado completo en JSON (fases, MQTT, GSM, topics, batería) |
| `GET` | `/api/config` | Configuración actual (sin contraseñas) |
| `GET` | `/api/logs` | Logs del sistema |
| `POST` | `/api/save` | Guardar configuración y reiniciar |
| `POST` | `/api/test` | Enviar notificación de prueba |
| `POST` | `/api/gsm/retry` | Reintentar detección/registro GSM |
| `POST` | `/api/gsm/at` | Enviar comando AT al SIM800 (`{"cmd":"AT+CREG?"}`) |
| `POST` | `/api/reset` | Reset de fábrica (vuelve al modo AP) |

### Batería (software)

La lectura de batería proviene del comando `AT+CBC` del **SIM800L** (tensión de alimentación del módulo). El ESP32-S3 SuperMini no incluye circuito de batería propio; sin GSM activo el dashboard muestra *no disponible*.

## Esquema de Conexiones (ESP32-S3 SuperMini)

Referencia completa en `Config.h`. Resumen del header:

| Pin placa | GPIO | Uso |
|-----------|------|-----|
| TX | 43 | → SIM800 RX |
| RX | 44 | ← SIM800 TX |
| GP1 | 1 | LED rojo |
| GP2 | 2 | LED verde |
| GP3 | 3 | LED azul |
| GP4 | 4 | Fase L1 |
| GP5 | 5 | Fase L2 |
| GP6 | 6 | Fase L3 |
| GP7–GP13 | 7–13 | Libre |
| Onboard LED | 48 | No usado por el firmware |

### Entradas de Fase (Aisladas)
Se recomienda utilizar optoacopladores (ej. PC817) conectados a las líneas de red a través de un circuito de reducción (capacitivo/resistivo y puente de diodos) para obtener una señal digital segura de 3.3V al microcontrolador.
*   **L1 (Fase 1):** GPIO 4 (PULLUP interno, la señal debe llevar a GND cuando hay tensión).
*   **L2 (Fase 2):** GPIO 5 (PULLUP interno)
*   **L3 (Fase 3):** GPIO 6 (PULLUP interno)

### Módulo Celular (SIM800L)
*   **VCC:** Fuente externa (3.7V - 4.2V, 2A). El ESP32 no provee corriente suficiente.
*   **GND:** Común con ESP32 y fuente.
*   **SIM800L TX** → **Pin RX** del ESP (GPIO 44). El ESP lee bien 2.8V del SIM800.
*   **SIM800L RX** → **Pin TX** del ESP (GPIO 43). Usar divisor 3.3V→2.8V si hace falta.
*   Son los pines **TX/RX impresos en el borde izquierdo** de la SuperMini (UART0).
*   Con **USB CDC On Boot = Enabled**, el monitor USB no compite con esos pines.
*   **No usar GPIO 18/19/20** — no están en el header o son USB interno.
    *   *Nota Divisor de Tensión:* El SIM800L tiene nivel lógico de 2.8V. La señal TX del ESP32 (3.3V) podría requerir un divisor resistivo simple (Ej: 10k en serie, 20k a GND) para no dañar el pin RX del SIM800L si la placa no trae adaptación de niveles.

### LED de Estado (RGB o individuales, resistencia ~330Ω)
*   **LED Rojo:** GP1 / GPIO 1 — parpadea cuando hay falla de fase.
*   **LED Verde:** GP2 / GPIO 2 — fijo cuando las fases están OK y MQTT está conectado.
*   **LED Azul:** GP3 / GPIO 3 — fijo en modo configuración (Access Point).
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
