#pragma once

/*
 * Copy this file to secrets.h and fill in your values.
 * secrets.h is gitignored — do not commit real credentials.
 */

#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

#define MQTT_HOST     "192.168.1.10"   // Home Assistant / Mosquitto
#define MQTT_PORT     1883
#define MQTT_USER     ""               // leave empty if no auth
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "kids-jobs-panel"

// Network firmware updates (ArduinoOTA). Required for Cloud Agent / laptop
// pushes after the one-time USB bootstrap flash.
#define OTA_HOSTNAME "kids-jobs-panel"
#define OTA_PASSWORD "change-me-to-a-long-random-string"
