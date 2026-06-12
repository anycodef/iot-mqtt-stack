// =============================================================================
// arduino_secrets.h — TEMPLATE (safe to commit, contains no real secrets)
//
// 1. Copy this file to `arduino_secrets.h` in the SAME firmware/ folder:
//        cp arduino_secrets.example.h arduino_secrets.h
// 2. Fill in your classroom Wi-Fi credentials and the Raspberry Pi IP.
// 3. `arduino_secrets.h` is GIT-IGNORED — never commit it.
//
// Find the Raspberry Pi IP on the Pi with:  hostname -I
// =============================================================================
#ifndef ARDUINO_SECRETS_H
#define ARDUINO_SECRETS_H

#define SECRET_WIFI_SSID   ""              // e.g. "UNMSM-IoT"
#define SECRET_WIFI_PASS   ""              // e.g. "supersecret"
#define SECRET_MQTT_BROKER "192.168.1.100" // RPi IP from `hostname -I`

#endif // ARDUINO_SECRETS_H
