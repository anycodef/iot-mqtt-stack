# Node-RED — ETL & automation

Node-RED is the **flow-based processing layer** of the stack (built on Node.js).
It subscribes to MQTT topics, parses the JSON payloads published by the ESP32
sketches, writes the resulting points into InfluxDB, and holds the
alarm/automation logic.

The editor is reachable at `http://<host>:1880` (use the Raspberry Pi IP from the
classroom laptops, e.g. `http://192.168.1.100:1880`).

## How it comes up "ready"

- The image is built from [`Dockerfile`](./Dockerfile): base `nodered/node-red:3.1`
  plus **`node-red-contrib-influxdb`** so the `influxdb out` node exists on first
  boot — no manual palette install needed.
- [`flows.json`](./flows.json) is mounted into `/data/flows.json` and auto-loaded
  via the `FLOWS=flows.json` environment variable in `docker-compose.yml`.

> Inside Docker, services talk **by service name**. The MQTT broker is
> `mosquitto:1883` and InfluxDB is `influxdb:8086` — **never** `localhost`.

## Flow 1 — telemetry persistence

```
[mqtt in: unmsm/iot2025/lab-a/g1/sensores/dht22]  (@ mosquitto:1883)
   -> [json]
   -> [function: build influx point]
   -> [influxdb out (v1): database iot_lab]        (@ influxdb:8086)
   -> [debug]
```

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

## Editing the flows

Edit in the browser editor and **Deploy**, or edit `flows.json` directly and
restart the container:

```bash
docker compose restart nodered
```
