# Guía paso a paso — Levantar el stack, los nodos y generar las claves

Esta guía cubre el lado **servidor** del laboratorio (la Raspberry Pi 4 o tu PC de
desarrollo): cómo **levantar los contenedores**, cargar **los nodos de Node-RED**, y
**generar las claves ECC** del Lab 08. El lado del ESP32 (firmware, flasheo, conexión al
broker) está en [`guia-esp32.md`](./guia-esp32.md).

> Sigue los pasos **en orden**. Las claves se generan en el servidor; el ESP32 solo
> recibe la mitad **pública**.

---

## Coordinación con el equipo del ESP32

Este proyecto lo arman **dos equipos**: el de **infraestructura/servidor** (esta guía) y
el del **dispositivo** ([`guia-esp32.md`](./guia-esp32.md)). El equipo del ESP32 **no**
tiene acceso al servidor ni genera claves, así que tú debes **entregarles**:

| Le entregas al equipo del ESP32 | De dónde sale | Cuándo |
|---|---|---|
| **IP del broker** (la Raspberry Pi) | `hostname -I` en la Pi | apenas levantes el stack (paso 3) |
| **Credenciales Wi-Fi** (SSID y clave, **2.4 GHz**) | la red del aula que tú configuras | antes de que flasheen |
| **`firmware/publisher-ecc/ecc_public_key.h`** *(clave pública)* | lo genera `ecc-keygen.sh` (paso 2) | **antes** de que compilen `publisher-ecc` |

> 🔒 La clave **privada** (`ecc_privada.pem`) **nunca** se comparte: se queda en el
> servidor. Solo entregas el header con la clave **pública**.

Y necesitas que **ellos te avisen**:

- Cuándo el ESP32 empieza a **publicar**, para que verifiques el descifrado y el guardado
  en Sheets (paso 7).
- Si ven errores de conexión al broker (`rc=-2`, etc.), para descartar que Mosquitto o la
  IP estén mal.
- El `client id` / topic que usan — en este repo ya están fijados: **`esp32-g1-ecc`** y
  **`unmsm/iot2025/lab-a/g1/sensores/json_enc`**.

> **Orden importante:** genera las claves (paso 2) y entrega el header **antes** de que
> el otro equipo pueda flashear `publisher-ecc`. Si regeneras las claves después, tienen
> que re-flashear y tú debes reiniciar Node-RED.

---

## 0. Requisitos previos

- **Docker Engine** + **Docker Compose v2** (se usa `docker compose`, no `docker-compose`).
- **OpenSSL** (para generar las claves; viene preinstalado en Raspberry Pi OS y Arch).
- **git** para clonar el repositorio.
- Host: Raspberry Pi OS de 64 bits, o cualquier Linux x86_64. La **misma**
  `docker-compose.yml` funciona en ambas arquitecturas (imágenes multi-arch).

Verifica que Docker está instalado y corriendo:

```bash
docker --version
docker compose version
systemctl is-active docker      # debe decir: active
```

Si dice `inactive`, arráncalo: `sudo systemctl start docker`.

---

## 1. Clonar el repositorio y crear el archivo de entorno

```bash
git clone https://github.com/<tu-usuario>/iot-mqtt-stack.git
cd iot-mqtt-stack

cp .env.example .env
```

Edita `.env` (está en `.gitignore`, nunca se sube):

| Variable | Qué poner |
|----------|-----------|
| `DATA_PATH` | Dónde viven los volúmenes. En la Pi, apunta a un SSD USB (`/mnt/ssd/iot-data`) para no desgastar la microSD. En tu PC, deja `./data`. |
| `TZ` | Zona horaria, p. ej. `America/Lima`. |
| `INFLUXDB_DB` | Deja `iot_lab` (debe coincidir con el esquema del laboratorio). |
| `GF_SECURITY_ADMIN_USER` / `_PASSWORD` | Usuario/clave de Grafana. Cámbialos antes de cualquier uso fuera de la LAN. |
| `APPS_SCRIPT_URL` | La URL del endpoint de Google Sheets. **La completarás en el paso 6.** Por ahora déjala como está. |

---

## 2. Generar el par de claves ECC (una sola vez)

El Lab 08 cifra la telemetría con **ECIES** (ECDH efímero P-256 + AES-128-CTR). El
suscriptor (Node-RED) tiene una clave **privada** fija `d`; el ESP32 lleva embebida solo
la clave **pública** `Q`. El script genera ambas mitades:

```bash
./scripts/ecc-keygen.sh
```

Esto produce tres archivos:

| Archivo | Qué es | ¿Se sube a git? |
|---------|--------|-----------------|
| `crypto/ecc_privada.pem` | Clave **privada** `d`. Solo la usa Node-RED. | **NO** (está en `.gitignore`, `chmod 600`) |
| `crypto/ecc_publica.pem` | Clave **pública** `Q`. Distribuible. | Sí (es pública) |
| `firmware/publisher-ecc/ecc_public_key.h` | `Q` como header C para el ESP32. | **NO** (es específica del par de claves; trátala como `arduino_secrets.h`) |

> ⚠️ **Nunca** subas `ecc_privada.pem` ni `ecc_public_key.h`. Si vuelves a generar las
> claves, tendrás que **re-flashear el ESP32** (paso de la otra guía) y **reiniciar
> Node-RED**. El script se niega a sobrescribir una clave privada existente: si quieres
> regenerar, borra `crypto/ecc_privada.pem` a mano primero.

El header `ecc_public_key.h` queda dentro de `firmware/publisher-ecc/`, justo donde el
sketch lo necesita. Esa parte la usa la [guía del ESP32](./guia-esp32.md).

---

## 3. Levantar los contenedores

Desde la raíz del proyecto:

```bash
docker compose up -d
```

La primera vez Docker **construye** la imagen de Node-RED (base 3.1 +
`node-red-contrib-influxdb`) y descarga Mosquitto, InfluxDB 1.8 y Grafana. Tarda unos
minutos. Luego comprueba el estado:

```bash
docker compose ps
```

Los **cuatro** servicios deben aparecer como `running` y, tras el `start_period`, como
`healthy`:

| Servicio | Contenedor | Puerto |
|----------|------------|--------|
| Mosquitto (broker MQTT) | `iot-mosquitto` | `1883` |
| Node-RED (ETL) | `iot-nodered` | `1880` |
| InfluxDB 1.8 | `iot-influxdb` | `8086` |
| Grafana | `iot-grafana` | `3000` |

### Si algún contenedor se reinicia en bucle (Grafana / Node-RED)

Docker crea las carpetas de datos bajo `${DATA_PATH}` como `root`, pero Grafana corre
como uid `472` y Node-RED como uid `1000`, así que no pueden escribir y reinician en
bucle (logs: `is not writable` o `EACCES ... /data/settings.js`). Corrige los permisos
con un contenedor desechable (no hace falta `sudo` en el host):

```bash
docker compose down
docker run --rm -v "$(pwd)/data:/data" alpine sh -c \
  'chown -R 472:472 /data/grafana && chown -R 1000:1000 /data/node-red'
docker compose up -d
docker compose ps        # los cuatro deben quedar (healthy)
```

(Si cambiaste `DATA_PATH` a otra ruta, aplica el `chown` a esa ruta.)

---

## 4. Colocar la clave privada en el volumen de Node-RED

Node-RED lee la clave privada **dentro del contenedor** en `/data/ecc_privada.pem`. Esa
ruta corresponde a `${DATA_PATH}/node-red/` en el host (se crea al levantar el stack en
el paso 3). Copia la clave ahí y reinicia Node-RED:

```bash
cp crypto/ecc_privada.pem "${DATA_PATH:-./data}/node-red/ecc_privada.pem"
docker compose restart nodered
```

> Si `${DATA_PATH}/node-red/` aún no existe, créala con
> `mkdir -p "${DATA_PATH:-./data}/node-red"` y vuelve a copiar.

---

## 5. Revisar los nodos de Node-RED

Los flujos se cargan **solos** desde `node-red/flows.json` (montado en el contenedor) y
`settings.js` ya expone los módulos `crypto`/`fs` a los nodos de función. Abre el editor:

```
http://<ip-del-host>:1880
```

(En la Pi, halla la IP con `hostname -I`, p. ej. `http://192.168.1.100:1880`.)

Verás una pestaña **"UNMSM IoT — Grupo g1"** con tres flujos ya armados:

1. **Flujo 1 — persistencia de telemetría:**
   `mqtt in (sensores/dht22)` → `json` → `function` → `influxdb batch (iot_lab)` → `debug`.
2. **Flujo 2 — alarma de gas:**
   `mqtt in (alarmas/gas)` → `switch` → `function (ON)` → `mqtt out (actuadores/relay)`.
3. **Flujo 3 — descifrado ECC + Google Sheets (Lab 08):**
   `mqtt in (sensores/json_enc, utf8)` → `Descifrado ECC` → `Preparar payload Sheets` →
   `http request (POST)` → `debug` (+ un nodo `catch`).

No necesitas conectar nada a mano. Si el editor muestra el aviso de cambios sin
desplegar, pulsa **Deploy** (botón rojo, arriba a la derecha). Dentro de Docker, los
servicios se comunican **por nombre**: el broker es `mosquitto:1883` y la base es
`influxdb:8086` — **nunca** `localhost`.

> El nodo **Descifrado ECC** lee `/data/ecc_privada.pem`. Si ves un error
> `ENOENT /data/ecc_privada.pem` en el nodo `catch`, repite el paso 4 (clave no copiada)
> y reinicia: `docker compose restart nodered`.

Otros accesos:

| Servicio | URL |
|----------|-----|
| Editor Node-RED | `http://<ip>:1880` |
| Grafana (dashboards) | `http://<ip>:3000` (por defecto `admin` / `admin`) |
| API InfluxDB | `http://<ip>:8086` |
| Broker MQTT | `<ip>:1883` |

---

## 6. Desplegar el endpoint de Google Sheets y completar `APPS_SCRIPT_URL`

El Flujo 3 hace `POST` de cada lectura descifrada a un **Google Apps Script** que añade
una fila a una Hoja de cálculo. El procedimiento completo (crear la hoja con la pestaña
`Datos_IoT_UNMSM_G1` y sus 8 columnas, pegar `Codigo.gs`, e implementar como aplicación
web con acceso **"Cualquiera"**) está en [`../apps-script/README.md`](../apps-script/README.md).

Cuando tengas la URL `/exec`, ponla en `.env`:

```bash
# .env
APPS_SCRIPT_URL=https://script.google.com/macros/s/XXXXXXXX/exec
```

Y recarga Node-RED para que tome la variable:

```bash
docker compose up -d nodered
```

---

## 7. Verificación de extremo a extremo

Con el ESP32 `publisher-ecc` ya publicando (ver [`guia-esp32.md`](./guia-esp32.md)):

```bash
# 1. El broker solo ve bytes opacos (Base64):
mosquitto_sub -h <ip-del-host> -t unmsm/iot2025/lab-a/g1/sensores/json_enc -v

# 2. Logs de Node-RED: el nodo "Descifrado ECC" no debe dar error
docker compose logs -f nodered
```

- En el editor de Node-RED, el panel de **debug** (barra lateral) muestra el JSON ya
  **descifrado** y la respuesta `{ status: 'ok', fila: N }` del Apps Script.
- En la Hoja de cálculo debería aparecer una fila nueva cada ~10 s.
- La telemetría del DHT22 (Flujo 1) se puede ver en Grafana, dashboard
  **"Lab IoT UNMSM – Grupo g1"**.

Prueba rápida sin hardware (telemetría en claro del Lab 07):

```bash
mosquitto_pub -h <ip-del-host> -t unmsm/iot2025/lab-a/g1/sensores/dht22 \
  -m '{"dispositivo":"esp32-g1-dht22","temperatura":24.5,"humedad":55.0}'
```

---

## Resumen del orden correcto

1. `cp .env.example .env` y editar.
2. `./scripts/ecc-keygen.sh` → genera claves (privada, pública, header del firmware).
3. `docker compose up -d` → levanta los 4 contenedores (corrige permisos si reinician).
4. Copiar `crypto/ecc_privada.pem` a `${DATA_PATH}/node-red/` y `docker compose restart nodered`.
5. Revisar los flujos en `http://<ip>:1880` (se cargan solos).
6. Desplegar el Apps Script, poner `APPS_SCRIPT_URL` en `.env`, `docker compose up -d nodered`.
7. Flashear el ESP32 (ver [`guia-esp32.md`](./guia-esp32.md)) y verificar el flujo completo.

> Versión corta en inglés (checklist) en [`lab08-runbook.md`](./lab08-runbook.md).
