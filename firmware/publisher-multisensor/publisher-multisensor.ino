/* ===========================================================================
 * Activity 2 — Multisensor publisher (ESP32-S3): BME280 + MQ-2
 *
 * BME280 (I2C 0x76) -> environmental JSON:
 *   - unmsm/iot2025/lab-a/g1/sensores/ambiental  (JSON: temp, hum, pressure)
 *   - unmsm/iot2025/lab-a/g1/sensores/presion    (plain pressure value)
 *
 * MQ-2 gas sensor -> gas level + alarm:
 *   - unmsm/iot2025/lab-a/g1/sensores/gas        (raw ADC level)
 *   - unmsm/iot2025/lab-a/g1/alarmas/gas         ("ALERT" when over threshold)
 *
 * MQ-2 warm-up: the heater needs ~20 s to stabilize. The gate below is fully
 * NON-BLOCKING (millis()-based) — the sketch NEVER calls delay(20000). No gas
 * value is published until warm-up completes; a warm-up status is published to
 * .../status so observers know the node is alive but not yet reporting gas.
 *
 * Client ID: esp32-g1-bme280
 *
 * Libraries: WiFi, PubSubClient, Adafruit BME280, Adafruit Unified Sensor,
 *            ArduinoJson.
 *
 * NOTE on QoS: PubSubClient publishes at QoS 0 only (no QoS param on publish()).
 * The "gas alarm QoS 1" requirement is a publisher-side limitation — see README.
 * =========================================================================== */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// --- Identity & namespace (EXACT lab namespace) -----------------------------
static const char* DEVICE_ID      = "esp32-g1-bme280";
static const char* TOPIC_AMBIENT  = "unmsm/iot2025/lab-a/g1/sensores/ambiental";
static const char* TOPIC_PRESION  = "unmsm/iot2025/lab-a/g1/sensores/presion";
static const char* TOPIC_GAS      = "unmsm/iot2025/lab-a/g1/sensores/gas";
static const char* TOPIC_ALARM    = "unmsm/iot2025/lab-a/g1/alarmas/gas";
static const char* TOPIC_STATUS   = "unmsm/iot2025/lab-a/g1/status";

// --- Hardware ---------------------------------------------------------------
#define BME280_ADDR  0x76    // I2C address (SCL=GPIO22, SDA=GPIO21)
#define MQ2_AO_PIN   36      // analog out (ADC1_CH0)
#define MQ2_DO_PIN   35      // digital out (threshold comparator on the module)

// --- Tuning -----------------------------------------------------------------
static const unsigned long WARMUP_MS           = 20000; // MQ-2 heater warm-up
static const unsigned long PUBLISH_INTERVAL_MS = 5000;  // publish every 5 s
static const int           GAS_THRESHOLD       = 2000;  // raw ADC alarm level
static const uint16_t      MQTT_PORT           = 1883;

Adafruit_BME280 bme;
WiFiClient      wifiClient;
PubSubClient    mqtt(wifiClient);

unsigned long warmupStart = 0;     // set once WiFi+MQTT are up
bool          warmedUp    = false;
bool          bmeOk       = false;
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

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting to broker...");
    bool ok = mqtt.connect(
        DEVICE_ID,
        nullptr, nullptr,
        TOPIC_STATUS, 0, true, "offline");   // LWT
    if (ok) {
      Serial.println(" connected.");
      mqtt.publish(TOPIC_STATUS, "online", true);
    } else {
      Serial.printf(" failed (rc=%d). Retry in 2s.\n", mqtt.state());
      delay(2000);
    }
  }
}

void publishEnvironment() {
  if (!bmeOk) return;

  float temperatura = bme.readTemperature();          // °C
  float humedad     = bme.readHumidity();             // %RH
  float presion     = bme.readPressure() / 100.0F;    // hPa

  StaticJsonDocument<160> doc;
  doc["dispositivo"] = DEVICE_ID;
  doc["temperatura"] = temperatura;
  doc["humedad"]     = humedad;
  doc["presion"]     = presion;

  char json[160];
  size_t n = serializeJson(doc, json);
  mqtt.publish(TOPIC_AMBIENT, json, n);

  char buf[16];
  dtostrf(presion, 0, 2, buf);
  mqtt.publish(TOPIC_PRESION, buf);

  Serial.printf("[PUB] ambiental %s\n", json);
}

void publishGas() {
  // Gate: do nothing until the MQ-2 heater has warmed up.
  if (!warmedUp) {
    if (millis() - warmupStart >= WARMUP_MS) {
      warmedUp = true;
      mqtt.publish(TOPIC_STATUS, "online", true);   // gas reporting now active
      Serial.println("[MQ-2] Warm-up complete — gas reporting enabled.");
    } else {
      // Publish a warm-up heartbeat so observers know the node is alive.
      unsigned long remaining = (WARMUP_MS - (millis() - warmupStart)) / 1000;
      char msg[40];
      snprintf(msg, sizeof(msg), "warming:%lus", remaining);
      mqtt.publish(TOPIC_STATUS, msg);
      Serial.printf("[MQ-2] Warming up, %lus left...\n", remaining);
    }
    return;   // never publish gas before warm-up
  }

  int gas = analogRead(MQ2_AO_PIN);   // 0..4095

  char buf[16];
  itoa(gas, buf, 10);
  mqtt.publish(TOPIC_GAS, buf);

  // Threshold crossing -> gas alarm. Node-RED reacts and drives the relay.
  if (gas >= GAS_THRESHOLD || digitalRead(MQ2_DO_PIN) == LOW) {
    mqtt.publish(TOPIC_ALARM, "ALERT");
    Serial.printf("[ALARM] gas=%d >= %d -> ALERT\n", gas, GAS_THRESHOLD);
  } else {
    Serial.printf("[PUB] gas=%d (ok)\n", gas);
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(MQ2_DO_PIN, INPUT);
  analogReadResolution(12);   // 0..4095 on ESP32

  Wire.begin(21, 22);         // SDA=GPIO21, SCL=GPIO22
  bmeOk = bme.begin(BME280_ADDR);
  if (!bmeOk) {
    Serial.println("[BME280] Not found at 0x76 — check wiring. Continuing.");
  }

  connectWiFi();
  mqtt.setServer(SECRET_MQTT_BROKER, MQTT_PORT);
  connectMQTT();

  warmupStart = millis();     // start the non-blocking MQ-2 warm-up window
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected())             connectMQTT();
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishEnvironment();
    publishGas();
  }
}
