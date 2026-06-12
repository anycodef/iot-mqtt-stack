/* ===========================================================================
 * Activity 3 — Relay subscriber / actuator (ESP32-S3)
 *
 * Subscribes to actuator commands and drives a relay (active-LOW):
 *   - SUB  unmsm/iot2025/lab-a/g1/actuadores/relay         (QoS 1)
 *   - PUB  unmsm/iot2025/lab-a/g1/actuadores/relay/status  (retained: ON/OFF)
 *
 * Payloads understood on the command topic: "ON" / "OFF" (also "1" / "0").
 * This node closes the bidirectional loop: a gas alarm published by the
 * multisensor is turned by Node-RED into a relay-ON command landing here.
 *
 * Client ID: esp32-g1-relay
 *
 * Relay is ACTIVE-LOW: the pin is driven HIGH in setup() BEFORE attaching it as
 * an output path, so the relay does not spuriously trigger at boot.
 *
 * loop() contains NO blocking delays — only millis()-based reconnect spacing.
 *
 * NOTE on QoS: subscribe() is requested at QoS 1 (at-least-once delivery of
 * commands). Publishing via PubSubClient is always QoS 0. See README.
 * =========================================================================== */

#include <WiFi.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"

// --- Identity & namespace (EXACT lab namespace) -----------------------------
static const char* DEVICE_ID           = "esp32-g1-relay";
static const char* TOPIC_RELAY_CMD     = "unmsm/iot2025/lab-a/g1/actuadores/relay";
static const char* TOPIC_RELAY_STATUS  = "unmsm/iot2025/lab-a/g1/actuadores/relay/status";
static const char* TOPIC_STATUS        = "unmsm/iot2025/lab-a/g1/status";

// --- Hardware (active-LOW relay) --------------------------------------------
#define RELAY_PIN     26
#define RELAY_ON      LOW    // active-LOW: LOW energizes the relay
#define RELAY_OFF     HIGH

// --- Tuning -----------------------------------------------------------------
static const unsigned long RECONNECT_INTERVAL_MS = 2000;
static const uint16_t      MQTT_PORT             = 1883;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastReconnectAttempt = 0;

// ---------------------------------------------------------------------------
void setRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
  mqtt.publish(TOPIC_RELAY_STATUS, on ? "ON" : "OFF", true);  // retained
  Serial.printf("[RELAY] -> %s\n", on ? "ON" : "OFF");
}

// MQTT message callback — runs whenever a command arrives.
void onMessage(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  Serial.printf("[MSG] %s = %s\n", topic, msg.c_str());

  if (String(topic) == TOPIC_RELAY_CMD) {
    if (msg.equalsIgnoreCase("ON") || msg == "1")  setRelay(true);
    else if (msg.equalsIgnoreCase("OFF") || msg == "0") setRelay(false);
  }
}

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

// Non-blocking single reconnect attempt. Returns true on success.
bool tryConnectMQTT() {
  Serial.print("[MQTT] Connecting to broker...");
  bool ok = mqtt.connect(
      DEVICE_ID,
      nullptr, nullptr,
      TOPIC_STATUS, 0, true, "offline");   // LWT
  if (ok) {
    Serial.println(" connected.");
    mqtt.publish(TOPIC_STATUS, "online", true);
    mqtt.subscribe(TOPIC_RELAY_CMD, 1);    // request QoS 1 for commands
    setRelay(false);                       // announce known OFF state (retained)
  } else {
    Serial.printf(" failed (rc=%d).\n", mqtt.state());
  }
  return ok;
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Drive the pin HIGH (relay OFF) BEFORE/at the moment it becomes an output,
  // so an active-LOW relay never clicks on at boot.
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  connectWiFi();
  mqtt.setServer(SECRET_MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  if (!mqtt.connected()) {
    // Non-blocking reconnect: attempt at most once per RECONNECT_INTERVAL_MS.
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
      lastReconnectAttempt = now;
      tryConnectMQTT();
    }
  } else {
    mqtt.loop();   // service incoming commands + keep-alive
  }
}
