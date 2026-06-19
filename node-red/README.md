# Node-RED — ETL & automation

Node-RED is the **flow-based processing layer** of the stack (built on Node.js).
It subscribes to MQTT topics, parses the JSON payloads published by the ESP32
sketches, writes the resulting points into InfluxDB, and holds the
alarm/automation logic.

The editor is reachable at `http://<host>:1880` (use the Raspberry Pi IP from the
classroom laptops, e.g. `http://192.168.1.100:1880`).

## How it comes up "ready"

- The image is built from [`Dockerfile`](./Dockerfile): base `nodered/node-red:3.1`
  plus **`node-red-contrib-influxdb`** so the InfluxDB nodes exist on first boot —
  no manual palette install needed.
- [`flows.json`](./flows.json) is mounted into `/data/flows.json` and auto-loaded
  via the `FLOWS=flows.json` environment variable in `docker-compose.yml`.

> Inside Docker, services talk **by service name**. The MQTT broker is
> `mosquitto:1883` and InfluxDB is `influxdb:8086` — **never** `localhost`.

## Flow 1 — telemetry persistence

```
[mqtt in: unmsm/iot2025/lab-a/g1/sensores/dht22]  (@ mosquitto:1883)
   -> [json]
   -> [function: build influx point]
   -> [influxdb batch (v1): database iot_lab]      (@ influxdb:8086)
   -> [debug]
```

> The write node is **`influxdb batch`** (not `influxdb out`). The function emits an
> array of `{ measurement, tags, fields }` point objects, and `influxdb batch` is the
> node that consumes that shape — each point carries its own measurement, so none is
> set on the node itself.

The function node builds an InfluxDB v1 point array verbatim:

```javascript
var payload = msg.payload;
msg.payload = [
  {
    measurement: 'sensor_dht22',
    tags: { dispositivo: payload.dispositivo, aula: 'lab-a', grupo: 'g1' },
    fields: {
      temperatura: parseFloat(payload.temperatura),
      humedad: parseFloat(payload.humedad)
    }
  }
];
return msg;
```

Resulting schema in InfluxDB:

| element     | value                                |
|-------------|--------------------------------------|
| database    | `iot_lab`                            |
| measurement | `sensor_dht22`                       |
| tags        | `dispositivo`, `aula=lab-a`, `grupo=g1` |
| fields      | `temperatura`, `humedad`             |

## Flow 2 — gas alarm automation

```
[mqtt in: unmsm/iot2025/lab-a/g1/alarmas/gas]
   -> [switch: alerta?]
   -> [function: payload = "ON"]
   -> [mqtt out: unmsm/iot2025/lab-a/g1/actuadores/relay]
   -> (also) [debug: notification]
```

When the multisensor publishes a gas alarm, Node-RED reacts by commanding the
relay (`actuadores/relay`) ON. This closes the bidirectional loop: telemetry
flows **up** the pipeline, commands flow **down** to the actuator ESP32.

## Flow 3 — ECC decryption + Google Sheets (Lab 08)

```
[mqtt in: unmsm/iot2025/lab-a/g1/sensores/json_enc]  (datatype: utf8)
   -> [function: Descifrado ECC]            (reads /data/ecc_privada.pem)
   -> [function: Preparar payload Sheets]
   -> [http request: POST env APPS_SCRIPT_URL]   (follows the 302 redirect)
   -> [debug]                                (+ a catch node -> debug)
```

The ESP32 `publisher-ecc` sketch publishes an **ECIES-encrypted** payload (ephemeral
ECDH P-256 + SHA-256 KDF + AES-128-CTR, Base64-encoded). Node-RED is the only party that
can decrypt it, because it holds the ECC private key.

Two Docker-specific pieces make this flow work:

- **`settings.js`** (committed, mounted at `/data/settings.js`) exposes Node's core
  `crypto` and `fs` modules to function nodes via `functionGlobalContext`. Function nodes
  cannot `require()` core modules directly, so the decryption node reads them with
  `global.get('crypto')` / `global.get('fs')`. The compose file mounts it and keeps
  `FLOWS=flows.json` (consistent with `flowFile` inside `settings.js`).
- **The ECC private key** must live in the mounted Node-RED data volume so it is readable
  inside the container at **`/data/ecc_privada.pem`**. On the Pi:

  ```bash
  cp crypto/ecc_privada.pem "${DATA_PATH:-./data}/node-red/ecc_privada.pem"
  ```

> The `mqtt in` node uses **`datatype: "utf8"`** — the payload is a Base64 *string*;
> auto-detect/buffer would corrupt it before decryption. The `http request` node takes
> the method/URL from `msg` and **follows redirects** (Apps Script answers with a 302).
> The endpoint URL is read from the `APPS_SCRIPT_URL` env var (`env.get(...)`), never
> hardcoded in the committed flow.

The decryption KDF mirrors the firmware byte-for-byte: `SHA256(S.x || eph_pub_x)[:16]`,
where `S.x` is the ECDH shared-secret X coordinate. Malformed or forged messages throw
and are dropped (`return null`), and the `catch` node logs any error.

See [`../apps-script/README.md`](../apps-script/README.md) for the Google Sheet endpoint
and [`../docs/lab08-runbook.md`](../docs/lab08-runbook.md) for the full bring-up order.

## Editing the flows

Edit in the browser editor and **Deploy**, or edit `flows.json` directly and
restart the container:

```bash
docker compose restart nodered
```
