#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>
#include <esp_system.h>

#include "clock_util.h"
#include "config.h"
#include "display_board.h"
#include "job_state.h"
#include "mqtt_bridge.h"
#include "ui.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"
#endif

namespace {

JobState g_state;
MqttBridge g_mqtt;
AppUi g_ui;

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("kids-jobs-panel");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ok %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] failed — will keep retrying in loop");
  }
}

}  // namespace

void setup() {
  // USB-CDC on ESP32-S3 re-enumerates after reset; wait so early logs are not lost.
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("=== Kids Jobs Sign-off ===");
  Serial.printf("[boot] reset reason %u  heap=%u  psram=%u\n",
                static_cast<unsigned>(esp_reset_reason()),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()));
  if (ESP.getFreePsram() == 0) {
    Serial.println("[boot] WARNING: PSRAM is 0 — RGB panel / idle photo will fail. "
                   "Need OPI PSRAM (qio_opi) in platformio.ini / Tools menu.");
  }

  g_state.begin();
  Serial.println("[boot] job state ready");

  if (!display_board_init()) {
    Serial.println("FATAL: display init failed — staying alive for serial debug");
    while (true) {
      delay(2000);
      Serial.println("[boot] display still failed (not reboot-looping)");
    }
  }
  Serial.println("[boot] display ready");

  g_mqtt.begin(&g_state);
  Serial.println("[boot] building UI…");
  g_ui.begin(&g_state, &g_mqtt);
  Serial.println("[boot] UI ready — idle screen");

  // Bring UI up before Wi-Fi so a bad secrets.h / long connect cannot look like a blank boot.
  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    clock_begin_ntp();
  }
  g_mqtt.publishAll();  // no-op until connected; reconnect path publishes
  Serial.println("[boot] setup complete");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t last_try = 0;
    if (millis() - last_try > 5000) {
      last_try = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.println("[wifi] retry…");
    }
  } else if (!clock_is_synced()) {
    static bool ntp_started = false;
    if (!ntp_started) {
      clock_begin_ntp();
      ntp_started = true;
    }
  }

  g_mqtt.loop();
  g_ui.tick();
  display_board_loop();
  delay(5);
}
