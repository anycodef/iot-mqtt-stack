/* ===========================================================================
 * Activity 1 — DHT22 telemetry publisher (ESP32-S3)
 *
 * Reads temperature & humidity from a DHT22 and publishes to the broker:
 *   - unmsm/iot2025/lab-a/g1/sensores/temperatura   (plain value, QoS 0)
 *   - unmsm/iot2025/lab-a/g1/sensores/humedad       (plain value, QoS 0)
 *   - unmsm/iot2025/lab-a/g1/sensores/dht22         (JSON, consumed by Node-RED)
 *
 * Connection lifecycle:
 *   - Last Will & Testament (LWT): broker publishes "offline" (retained) to
 *     .../status if this node drops unexpectedly.
 *   - On connect: publishes "online" (retained) to .../status.
 *
 * Client ID: esp32-g1-dht22
 *
 * Libraries (Library Manager):
 *   WiFi (ESP32 core), PubSubClient (knolleary),
 *   DHT sensor library (Adafruit), Adafruit Unified Sensor, ArduinoJson.
 *
 * NOTE on QoS: PubSubClient::publish() has no QoS argument — it always
 * publishes at QoS 0. See the project README "PubSubClient QoS-0 note".
 * =========================================================================== */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"   // copied from firmware/arduino_secrets.example.h

// --- Identity & namespace (EXACT lab namespace — do not change) -------------
static const char* DEVICE_ID    = "esp32-g1-dht22";
static const char* TOPIC_TEMP   = "unmsm/iot2025/lab-a/g1/sensores/temperatura";
static const char* TOPIC_HUM    = "unmsm/iot2025/lab-a/g1/sensores/humedad";
static const char* TOPIC_DHT22  = "unmsm/iot2025/lab-a/g1/sensores/dht22";
static const char* TOPIC_STATUS = "unmsm/iot2025/lab-a/g1/status";

// --- Hardware ---------------------------------------------------------------
#define DHT_PIN   4          // DATA on GPIO4 (10k pull-up to 3.3V)
#define DHT_TYPE  DHT22

// --- Timing -----------------------------------------------------------------
static const unsigned long PUBLISH_INTERVAL_MS = 5000;  // publish every 5 s
static const uint16_t      MQTT_PORT           = 1883;

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastPublish = 0;

// ---------------------------------------------------------------------------
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s", SECRET_WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print('.');
  }
  Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

// Connect (or reconnect) to the MQTT broker, registering the LWT first.
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting to broker...");
    // LWT: retained "offline" on .../status if the connection drops.
    bool ok = mqtt.connect(
        DEVICE_ID,
        nullptr, nullptr,         // no username/password (LAN anonymous)
        TOPIC_STATUS, 0, true, "offline");
    if (ok) {
      Serial.println(" connected.");
      // Announce presence: retained "online".
      mqtt.publish(TOPIC_STATUS, "online", true);
    } else {
      Serial.printf(" failed (rc=%d). Retry in 2s.\n", mqtt.state());
      delay(2000);
    }
  }
}

void publishReadings() {
  float temperatura = dht.readTemperature();   // °C
  float humedad     = dht.readHumidity();      // %RH

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("[DHT22] Read failed — skipping this cycle.");
    return;
  }

  // Plain per-metric topics (QoS 0).
  char buf[16];
  dtostrf(temperatura, 0, 2, buf);
  mqtt.publish(TOPIC_TEMP, buf);
  dtostrf(humedad, 0, 2, buf);
  mqtt.publish(TOPIC_HUM, buf);

  // JSON payload consumed by the Node-RED telemetry-persistence flow.
  StaticJsonDocument<128> doc;
  doc["dispositivo"] = DEVICE_ID;
  doc["temperatura"] = temperatura;
  doc["humedad"]     = humedad;

  char json[128];
  size_t n = serializeJson(doc, json);
  mqtt.publish(TOPIC_DHT22, json, n);

  Serial.printf("[PUB] %s\n", json);
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  dht.begin();
  connectWiFi();
  mqtt.setServer(SECRET_MQTT_BROKER, MQTT_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected())             connectMQTT();
  mqtt.loop();   // keep-alive + LWT bookkeeping

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishReadings();
  }
}
