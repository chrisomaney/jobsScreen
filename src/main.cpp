#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>
#include <esp_system.h>

#include "app_secrets.h"
#include "ble_bridge.h"
#include "clock_util.h"
#include "config.h"
#include "display_board.h"
#include "job_state.h"
#include "mqtt_bridge.h"
#include "reward_engine.h"
#include "ui.h"

namespace {

JobState g_state;
RewardEngine g_rewards;
MqttBridge g_mqtt;
BleBridge g_ble;
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
    // Chunked across loop() — never block the RGB panel with a full blast.
    g_mqtt.requestPublishAll();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== Kids Jobs Sign-off ===");
  Serial.printf("[boot] reset reason: %s\n", resetReasonText(esp_reset_reason()));
  Serial.printf("[boot] psram %u bytes\n", static_cast<unsigned>(ESP.getPsramSize()));

  g_state.begin();
  g_rewards.begin(&g_state);

  if (!display_board_init()) {
    Serial.println("FATAL: display init failed");
    while (true) {
      delay(1000);
    }
  }

  g_mqtt.begin(&g_state, &g_rewards);
  g_ui.begin(&g_state, &g_mqtt, &g_rewards);

  startWifi();
  // BLE after Wi-Fi mode is set — better coexistence on ESP32-S3.
  g_ble.begin(&g_state, &g_mqtt);
  clock_begin_ntp();  // SNTP keeps retrying until Wi-Fi is up
}

void loop() {
  maintainWifi();
  maintainDailyReset();
  g_rewards.tickMessage();
  const uint8_t awarded = g_rewards.tickSchedule();
  if (awarded) {
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if (awarded & (1u << k)) {
        g_mqtt.publishRewardTally(k);
      }
    }
  }
  static uint32_t last_net_sched_ms = 0;
  if (millis() - last_net_sched_ms > 15000) {
    last_net_sched_ms = millis();
    const uint8_t flipped = g_state.tickInternetSchedules();
    if (flipped) {
      for (size_t k = 0; k < KID_COUNT; ++k) {
        if (flipped & (1u << k)) {
          // Queue summaries instead of blasting 3×K publishes in this slice.
          g_mqtt.requestPublishConfigAndSummaries();
          break;
        }
      }
    }
  }

  g_mqtt.loop();
  // Leftover deferred NVS (usually empty — config/rules now persist synchronously).
  if (!g_state.loopPersist()) {
    g_rewards.loopPersist();
  }
  g_ble.loop();
  g_ui.tick();
  display_board_loop();
  delay(5);
}
