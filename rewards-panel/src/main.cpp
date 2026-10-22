#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>

#include "display_board.h"
#include "mqtt_bridge.h"
#include "reward_state.h"
#include "ui.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"
#endif

namespace {

RewardState g_state;
MqttBridge g_mqtt;
AppUi g_ui;

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("kids-rewards-panel");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ok %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] failed — will keep retrying");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2500) {
    delay(10);
  }
  delay(100);
  Serial.println();
  Serial.println("=== Kids Rewards Panel ===");
  Serial.printf("[boot] heap=%u psram=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()));

  g_state.begin();
  if (!display_board_init()) {
    Serial.println("FATAL: display init failed");
    while (true) {
      delay(1000);
    }
  }
  display_board_set_backlight(100);

  g_mqtt.begin(&g_state);
  g_ui.begin(&g_state, &g_mqtt);

  connectWifi();
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
  }

  g_mqtt.loop();
  g_ui.tick();
  display_board_loop();
  delay(5);
}
