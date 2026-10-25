#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>
#include <esp_system.h>

#include "app_secrets.h"
#include "clock_util.h"
#include "config.h"
#include "display_board.h"
#include "job_state.h"
#include "mqtt_bridge.h"
#include "ui.h"

namespace {

JobState g_state;
MqttBridge g_mqtt;
AppUi g_ui;

const char *resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "PANIC (crash)";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "other watchdog";
    case ESP_RST_BROWNOUT: return "brownout (power supply too weak?)";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    default: return "other";
  }
}

void startWifi() {
  // Non-blocking: the UI keeps running while Wi-Fi connects in the background.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("kids-jobs-panel");
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
}

void maintainWifi() {
  static bool was_connected = false;
  static uint32_t down_since = 0;
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !was_connected) {
    Serial.printf("[wifi] ok %s\n", WiFi.localIP().toString().c_str());
  }
  if (!up && was_connected) {
    Serial.println("[wifi] lost — auto-reconnecting");
    down_since = millis();
  }
  if (!up && !was_connected && down_since == 0) {
    down_since = millis();
  }
  // Auto-reconnect handles most drops; nudge it if we've been down a while.
  if (!up && millis() - down_since > 30000) {
    down_since = millis();
    Serial.println("[wifi] still down — reconnect");
    WiFi.reconnect();
  }
  if (up) {
    down_since = 0;
  }
  was_connected = up;
}

void maintainDailyReset() {
  static uint32_t last_check = 0;
  if (millis() - last_check < 10000) {
    return;
  }
  last_check = millis();
  if (g_state.rollDay(clock_job_day())) {
    Serial.println("[jobs] new day — all jobs reset");
    g_mqtt.publishAll();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  // USB-CDC on ESP32-S3 often drops the first lines unless we wait briefly.
  const uint32_t serial_wait_start = millis();
  while (!Serial && millis() - serial_wait_start < 2500) {
    delay(10);
  }
  delay(100);
  Serial.println();
  Serial.println("=== Kids Jobs Sign-off ===");
  Serial.printf("[boot] reset reason: %s\n", resetReasonText(esp_reset_reason()));
  Serial.printf("[boot] free heap=%u  psram=%u / %u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()),
                static_cast<unsigned>(ESP.getPsramSize()));

  g_state.begin();

  if (!display_board_init()) {
    Serial.println("FATAL: display init failed");
    while (true) {
      delay(1000);
    }
  }

  g_mqtt.begin(&g_state);
  g_ui.begin(&g_state, &g_mqtt);

  startWifi();
  clock_begin_ntp();  // SNTP keeps retrying until Wi-Fi is up
}

void loop() {
  maintainWifi();
  maintainDailyReset();
  g_mqtt.loop();
  g_ui.tick();
  display_board_loop();
  delay(5);
}
