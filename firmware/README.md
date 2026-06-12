# Firmware — ESP32-S3 sketches

Three Arduino sketches that exercise the full pipeline, one per lab activity:

| Sketch | Activity | Client ID | Role |
|--------|----------|-----------|------|
| [`publisher-dht22/`](./publisher-dht22) | 1 | `esp32-g1-dht22` | Publishes DHT22 temperature/humidity + JSON, LWT status |
| [`publisher-multisensor/`](./publisher-multisensor) | 2 | `esp32-g1-bme280` | Publishes BME280 environmental JSON + MQ-2 gas/alarm (non-blocking warm-up) |
| [`subscriber-relay/`](./subscriber-relay) | 3 | `esp32-g1-relay` | Subscribes to relay commands, drives an active-LOW relay |

## Required libraries (Arduino Library Manager)

- **WiFi** — bundled with the ESP32 Arduino core
- **PubSubClient** (by Nick O'Leary / `knolleary`)
- **DHT sensor library** (Adafruit) + **Adafruit Unified Sensor**
- **Adafruit BME280 Library**
- **ArduinoJson** (by Benoit Blanchon)

Board: any **ESP32-S3** dev board (install the *esp32 by Espressif Systems* board
package). The pinouts below are wired for the classroom kit.

## Credentials — `arduino_secrets.h` (git-ignored)

No credentials are ever hardcoded in a `.ino`. Each sketch does:

```cpp
#include "arduino_secrets.h"
```

Arduino compiles only files inside the sketch folder, so copy the template into
**each** of the three sketch folders and fill it in:

```bash
cp arduino_secrets.example.h publisher-dht22/arduino_secrets.h
cp arduino_secrets.example.h publisher-multisensor/arduino_secrets.h
cp arduino_secrets.example.h subscriber-relay/arduino_secrets.h
```

Then edit each copy:

```cpp
#define SECRET_WIFI_SSID   "UNMSM-IoT"
#define SECRET_WIFI_PASS   "your-wifi-password"
#define SECRET_MQTT_BROKER "192.168.1.100"   // Raspberry Pi IP
```

Find the Raspberry Pi IP **on the Pi**:

```bash
hostname -I
```

`arduino_secrets.h` matches `**/arduino_secrets.h` in `.gitignore`, so your real
credentials are never committed. Only `arduino_secrets.example.h` (empty values)
is tracked.

## Pinout (classroom kit)

| Sensor / Actuator | Pin / Bus | ESP32-S3 GPIO | Notes |
|-------------------|-----------|---------------|-------|
| DHT22             | VCC       | 3.3V          | |
|                   | DATA      | GPIO4         | 10 kΩ pull-up to 3.3V |
|                   | GND       | GND           | |
| BME280 (I2C 0x76) | VCC       | 3.3V          | |
|                   | GND       | GND           | |
|                   | SCL       | GPIO22        | |
|                   | SDA       | GPIO21        | |
| MQ-2              | VCC       | 5V (VIN)      | heater needs 5V |
|                   | GND       | GND           | |
|                   | AO        | GPIO36 (ADC)  | analog gas level |
|                   | DO        | GPIO35        | digital threshold |
| Relay (4-ch)      | IN        | GPIO26        | **active-LOW**, init HIGH in setup() |

## Important: PubSubClient publishes only at QoS 0

`PubSubClient::publish()` has **no QoS parameter** — every publish is QoS 0
("at most once"). Therefore the lab's *"gas alarm at QoS 1"* target is a
**publisher-side limitation**: QoS 1 only applies to **subscriptions**
(`subscribe(topic, 1)`, used by the relay node for command delivery).

If true at-least-once publishing is required, switch the firmware to
[**256dpi/arduino-mqtt**](https://github.com/256dpi/arduino-mqtt), whose
`publish()` accepts a QoS argument. The broker, Node-RED, and InfluxDB layers
are unaffected by this choice.

## Flashing

1. Select your ESP32-S3 board and port in the Arduino IDE.
2. Open one sketch folder, ensure its `arduino_secrets.h` exists.
3. Upload, then open the Serial Monitor at **115200 baud** to watch the logs.
4. Confirm telemetry arrives — e.g. on the broker host:

   ```bash
   mosquitto_sub -h localhost -t 'unmsm/iot2025/lab-a/g1/#' -v
   ```
