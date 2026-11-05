#include "ota_bridge.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "app_secrets.h"

namespace {

bool g_started = false;
bool g_active = false;

}  // namespace

void ota_bridge_begin() {
  if (g_started) {
    return;
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPort(3232);
  if (OTA_PASSWORD[0] != '\0') {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  } else {
    Serial.println("[ota] WARNING: OTA_PASSWORD empty — set it in include/secrets.h");
  }

  ArduinoOTA.onStart([]() {
    g_active = true;
    Serial.println("[ota] update starting — UI may pause");
  });
  ArduinoOTA.onEnd([]() {
    g_active = false;
    Serial.println("[ota] update finished — rebooting");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned last_pct = 999;
    if (total == 0) {
      return;
    }
    const unsigned pct = (progress * 100u) / total;
    if (pct != last_pct && (pct % 10u) == 0u) {
      last_pct = pct;
      Serial.printf("[ota] %u%%\n", pct);
    }
  });
  ArduinoOTA.onError([](ota_error_t err) {
    g_active = false;
    Serial.printf("[ota] error %u\n", static_cast<unsigned>(err));
  });

  ArduinoOTA.begin();
  g_started = true;
  Serial.printf("[ota] listening as %s.local (TCP 3232) ip=%s\n", OTA_HOSTNAME,
                WiFi.localIP().toString().c_str());
}

void ota_bridge_loop() {
  if (!g_started) {
    if (WiFi.status() == WL_CONNECTED) {
      ota_bridge_begin();
    } else {
      return;
    }
  }
  ArduinoOTA.handle();
}

bool ota_bridge_active() { return g_active; }
