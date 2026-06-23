# Guía paso a paso — Levantar el ESP32 desde cero hasta el broker

Esta guía cubre el lado del **dispositivo**: instalar el entorno, cablear los sensores,
configurar las credenciales y la clave pública, **flashear** el ESP32-S3 y confirmar que
se conecta al broker MQTT. El lado del servidor (contenedores, nodos y **generación de
las claves**) está en [`guia-stack.md`](./guia-stack.md).

> Sirve para cualquiera de los cuatro sketches de `firmware/`. Los pasos específicos del
> sketch cifrado **`publisher-ecc`** (Lab 08) van marcados como **[Lab 08]**.

---

## 0. Lo que necesitas

- Una placa **ESP32-S3** (DevKit) y un cable **USB de datos** (no de solo carga).
- El **kit de sensores** del laboratorio: DHT22, BME280, MQ-2 y un módulo relay.
- Una red **Wi-Fi 2.4 GHz** (el ESP32 no se conecta a 5 GHz).
- El **servidor ya levantado** y la **IP del broker** (la Raspberry Pi). Si aún no está,
  sigue primero [`guia-stack.md`](./guia-stack.md).

> **[Lab 08]** El sketch `publisher-ecc` necesita el header `ecc_public_key.h`, que
> genera el script `scripts/ecc-keygen.sh` **en el servidor** (ver
> [`guia-stack.md`](./guia-stack.md), paso 2). El ESP32 solo lleva la mitad **pública**;
> la clave **privada** nunca sale de la Pi.

---

## Lo que debes pedir al equipo de infraestructura

El servidor (broker, Node-RED, contenedores y **las claves**) lo maneja el **otro
equipo** ([`guia-stack.md`](./guia-stack.md)). Tú **no** generas claves ni levantas
Docker. Antes de empezar, **pídeles**:

| Pide al equipo del servidor | Para qué | Lo usas en |
|---|---|---|
| **IP del broker** (la Raspberry Pi) | apuntar el ESP32 al MQTT | paso 4 (`SECRET_MQTT_BROKER`) |
| **SSID y clave Wi-Fi** (**2.4 GHz**) | conectar el ESP32 a la red | paso 4 |
| **[Lab 08]** archivo `ecc_public_key.h` | compilar `publisher-ecc` (clave pública) | paso 5 |

> 🔒 Nunca te darán (ni necesitas) la clave **privada** `ecc_privada.pem`: esa se queda
> en el servidor. Tú solo recibes el header con la mitad **pública**.

Y **avísales a ellos**:

- Cuando el ESP32 ya esté **publicando** (paso 7), para que verifiquen el descifrado en
  Node-RED y el guardado en Google Sheets.
- Si ves errores de conexión al broker (`rc=-2`, etc.), para descartar que el contenedor
  Mosquitto o la IP estén mal.

> Si te entregan un `ecc_public_key.h` **nuevo** (porque regeneraron las claves), tienes
> que **re-flashear** el ESP32 con el header actualizado.

---

## 1. Instalar el Arduino IDE y el soporte para ESP32

1. Descarga e instala el **Arduino IDE 2.x** desde <https://www.arduino.cc/en/software>.
2. Abre **Archivo → Preferencias** y, en *"Gestor de URLs Adicionales de Tarjetas"*,
   pega:

   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. Abre **Herramientas → Placa → Gestor de tarjetas**, busca **esp32** e instala
   **"esp32 by Espressif Systems"**.
4. Selecciona la placa en **Herramientas → Placa → esp32 → "ESP32S3 Dev Module"**.

---

## 2. Instalar las librerías

Abre **Herramientas → Gestionar Bibliotecas** e instala:

| Librería | Autor | Para qué |
|----------|-------|----------|
| **PubSubClient** | Nick O'Leary (`knolleary`) | Cliente MQTT |
| **DHT sensor library** | Adafruit | DHT22 (temperatura/humedad) |
| **Adafruit Unified Sensor** | Adafruit | Dependencia de la anterior |
| **Adafruit BME280 Library** | Adafruit | BME280 (presión) |
| **ArduinoJson** | Benoit Blanchon | Serializar el JSON |
| **micro-ecc** *(Ken MacKay)* | — | **[Lab 08]** ECDH P-256 (incluye `<uECC.h>`) |

> `WiFi` ya viene con el core del ESP32. SHA-256 y AES los aporta **mbedtls**, también
> incluido en el core — no instalas nada extra para eso.
>
> **[Lab 08]** Si no encuentras *micro-ecc* en el gestor, descárgala como ZIP desde
> <https://github.com/kmackay/micro-ecc> e instálala con
> **Programa → Incluir biblioteca → Añadir biblioteca .ZIP**.

---

## 3. Cablear los sensores (kit del aula)

| Sensor / Actuador | Pin / Bus | GPIO ESP32-S3 | Notas |
|-------------------|-----------|---------------|-------|
| DHT22 | DATA | **GPIO4** | resistencia pull-up de 10 kΩ a 3.3 V |
| BME280 (I2C 0x76) | SDA | **GPIO21** | |
|  | SCL | **GPIO22** | |
| MQ-2 | AO (analógico) | **GPIO36** | nivel de gas |
|  | DO (digital) | **GPIO35** | umbral |
| Relay (4 canales) | IN | **GPIO26** | **active-LOW**; arranca en HIGH en `setup()` |

Alimentación: DHT22 y BME280 a **3.3 V**; el calefactor del **MQ-2 necesita 5 V (VIN)**;
GND común para todo.

> `publisher-dht22` solo usa el DHT22; `publisher-multisensor` y `publisher-ecc` usan
> DHT22 + BME280 + MQ-2; `subscriber-relay` usa el relay.

---

## 4. Configurar las credenciales — `arduino_secrets.h`

Ninguna credencial va escrita en el `.ino`. Cada carpeta de sketch necesita su propio
`arduino_secrets.h` (está en `.gitignore`, nunca se sube). Copia la plantilla a la
carpeta del sketch que vas a flashear, por ejemplo para **[Lab 08]**:

```bash
cd firmware
cp arduino_secrets.example.h publisher-ecc/arduino_secrets.h
```

(Para los otros: `publisher-dht22/`, `publisher-multisensor/`, `subscriber-relay/`.)

Edita ese archivo con tus datos:

```cpp
#define SECRET_WIFI_SSID   "UNMSM-IoT"        // tu Wi-Fi (2.4 GHz)
#define SECRET_WIFI_PASS   "tu-clave-wifi"
#define SECRET_MQTT_BROKER "192.168.1.100"    // IP de la Raspberry Pi (broker)
```

La IP del broker la obtienes **en la Pi** con:

```bash
hostname -I
```

---

## 5. **[Lab 08]** Colocar la clave pública en el sketch

El sketch `publisher-ecc` hace `#include "ecc_public_key.h"`. Ese header lo **generó el
servidor** con `scripts/ecc-keygen.sh` y quedó en `firmware/publisher-ecc/`. Antes de
compilar, confirma que existe:

```bash
ls firmware/publisher-ecc/ecc_public_key.h
```

- Si trabajas en la **misma máquina** que el servidor, ya está ahí (paso 2 de
  [`guia-stack.md`](./guia-stack.md)).
- Si flasheas desde **otra PC**, copia ese archivo (es la clave **pública**, seguro de
  copiar) a `firmware/publisher-ecc/` antes de abrir el sketch.

> El header **no** se sube a git (está en `.gitignore`), así que cada quien lo obtiene
> del servidor. La clave **privada** (`ecc_privada.pem`) nunca se copia al ESP32.

---

## 6. Compilar y flashear

1. Conecta el ESP32-S3 por USB.
2. Abre el sketch: **Archivo → Abrir →** `firmware/publisher-ecc/publisher-ecc.ino`
   (o el sketch que toque).
3. Selecciona el puerto en **Herramientas → Puerto** (algo como `/dev/ttyACM0`,
   `/dev/ttyUSB0` o `COMx`).
4. En **Herramientas**, con la placa "ESP32S3 Dev Module", deja los valores por defecto;
   si tu placa no enumera el puerto, activa **"USB CDC On Boot: Enabled"**.
5. Pulsa **Subir** (la flecha →). Si la subida no arranca, mantén pulsado el botón
   **BOOT** de la placa mientras empieza a subir y suéltalo al ver "Connecting...".

---

## 7. Verificar la conexión al broker

Abre el **Monitor Serie** (lupa arriba a la derecha) a **115200 baudios**. Deberías ver
algo así:

```
[WiFi] Connecting to UNMSM-IoT.....
[WiFi] Connected. IP: 192.168.1.42
[MQTT] Connecting to broker... connected.
[ECC] published  plain=24.50C/62.3%  ct=121B
```

Confirma desde el **host del broker** que los mensajes llegan:

```bash
# [Lab 08] payload cifrado (Base64) en el topic json_enc:
mosquitto_sub -h <ip-del-broker> -t unmsm/iot2025/lab-a/g1/sensores/json_enc -v

# o toda la jerarquía del grupo:
mosquitto_sub -h <ip-del-broker> -t 'unmsm/iot2025/lab-a/g1/#' -v
```

Si ves los mensajes llegando, **el ESP32 ya está conectado al broker**. El descifrado y
el guardado en Google Sheets ocurren en Node-RED (lado servidor: ver
[`guia-stack.md`](./guia-stack.md), pasos 5–7).

---

## Problemas frecuentes

| Síntoma | Causa probable / solución |
|---------|---------------------------|
| El puerto no aparece en **Herramientas → Puerto** | Cable USB de solo carga, o falta el driver USB-serie (CP210x / CH340). Prueba otro cable; instala el driver. |
| `[WiFi] Connecting...` no termina nunca | SSID/clave mal, o red de **5 GHz**. Usa 2.4 GHz y revisa `arduino_secrets.h`. |
| `[MQTT] failed (rc=-2)` y reintenta | IP del broker incorrecta, o el contenedor Mosquitto no está arriba. Verifica con `hostname -I` en la Pi y `docker compose ps`. |
| La subida falla en "Connecting..." | Mantén **BOOT** pulsado al iniciar la subida; revisa que el puerto sea el correcto. |
| **[Lab 08]** No compila: `ecc_public_key.h: No such file` | Falta el header. Genera las claves en el servidor (`scripts/ecc-keygen.sh`) y copia el header a `firmware/publisher-ecc/`. Ver [`guia-stack.md`](./guia-stack.md). |
| **[Lab 08]** No compila: `uECC.h: No such file` | Falta la librería **micro-ecc** (paso 2). |
| Llegan bytes pero Node-RED no descifra | El par de claves del ESP32 y de Node-RED no coincide: regenera, recopia la privada y **re-flashea**. |
