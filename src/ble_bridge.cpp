#include "ble_bridge.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <string.h>

#include "display_board.h"

namespace {

// Must match ios BlePanelClient.swift
static const char *kServiceUuid = "6B1D0001-5F8A-4B2E-9C71-0A1B2C3D4E5F";
static const char *kCmdUuid = "6B1D0002-5F8A-4B2E-9C71-0A1B2C3D4E5F";
static const char *kStatusUuid = "6B1D0003-5F8A-4B2E-9C71-0A1B2C3D4E5F";

JobState *g_ble_state = nullptr;
MqttBridge *g_ble_mqtt = nullptr;
NimBLECharacteristic *g_status_char = nullptr;

String g_rx_buf;
int g_expect_len = -1;
String g_payload;

void notifyJson(const JsonDocument &doc) {
  if (!g_status_char) {
    return;
  }
  String out;
  serializeJson(doc, out);
  g_status_char->setValue(out.c_str());
  g_status_char->notify();
}

void sendAck(bool ok, const char *message) {
  JsonDocument doc;
  doc["type"] = "ack";
  doc["ok"] = ok;
  doc["message"] = message ? message : "";
  notifyJson(doc);
}

void sendConfigState() {
  if (!g_ble_state) {
    return;
  }
  // Reuse the same JSON the web app expects under kids/config/state.
  const String cfg = g_ble_state->configStateJson();
  JsonDocument wrap;
  wrap["type"] = "config_state";
  JsonDocument payload;
  if (deserializeJson(payload, cfg)) {
    sendAck(false, "config serialize failed");
    return;
  }
  wrap["payload"] = payload.as<JsonVariant>();
  notifyJson(wrap);
}

void handleCompletePayload(const String &json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    sendAck(false, "bad JSON");
    return;
  }
  const char *type = doc["type"] | "";
  if (strcmp(type, "get_config") == 0) {
    sendConfigState();
    sendAck(true, "config sent");
    return;
  }
  if (strcmp(type, "config_set") == 0) {
    if (!doc["payload"].is<JsonObjectConst>() && !doc["payload"].is<JsonObject>()) {
      sendAck(false, "missing payload");
      return;
    }
    String body;
    serializeJson(doc["payload"], body);
    const bool ok = g_ble_state && g_ble_state->applyConfigJson(body.c_str(), body.length());
    if (ok) {
      if (g_ble_mqtt) {
        g_ble_mqtt->requestPublishAll();
      }
      sendConfigState();
      sendAck(true, "config applied");
    } else {
      // Still echo current config so the app can refresh.
      sendConfigState();
      sendAck(true, "no change");
    }
    return;
  }
  sendAck(false, "unknown type");
}

void ingestChunk(const uint8_t *data, size_t len) {
  g_rx_buf += String(reinterpret_cast<const char *>(data), len);

  // Keep UI alive while assembling large writes.
  display_board_loop();

  while (true) {
    if (g_expect_len < 0) {
      const int mark = g_rx_buf.indexOf("!LEN:");
      if (mark < 0) {
        if (g_rx_buf.length() > 64) {
          g_rx_buf.remove(0, g_rx_buf.length() - 64);
        }
        return;
      }
      if (mark > 0) {
        g_rx_buf.remove(0, mark);
      }
      const int nl = g_rx_buf.indexOf('\n');
      if (nl < 0) {
        return;
      }
      g_expect_len = g_rx_buf.substring(5, nl).toInt();
      g_rx_buf.remove(0, nl + 1);
      g_payload = "";
      if (g_expect_len <= 0 || g_expect_len > 12000) {
        g_expect_len = -1;
        sendAck(false, "bad length");
        return;
      }
    }

    if (static_cast<int>(g_payload.length()) < g_expect_len) {
      const int need = g_expect_len - static_cast<int>(g_payload.length());
      if (static_cast<int>(g_rx_buf.length()) < need) {
        g_payload += g_rx_buf;
        g_rx_buf = "";
        return;
      }
      g_payload += g_rx_buf.substring(0, need);
      g_rx_buf.remove(0, need);
    }

    // Expect !END\n after payload
    if (g_rx_buf.indexOf("!END\n") < 0) {
      // may arrive later
      if (g_rx_buf.length() > 16 && g_rx_buf.indexOf('!') < 0) {
        // garbage — reset
        g_expect_len = -1;
        g_payload = "";
        g_rx_buf = "";
        sendAck(false, "missing end");
      }
      return;
    }
    const int endAt = g_rx_buf.indexOf("!END\n");
    g_rx_buf.remove(0, endAt + 5);
    const String done = g_payload;
    g_expect_len = -1;
    g_payload = "";
    handleCompletePayload(done);
  }
}

class CmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {
    std::string v = c->getValue();
    if (v.empty()) {
      return;
    }
    ingestChunk(reinterpret_cast<const uint8_t *>(v.data()), v.size());
  }
};

CmdCallbacks g_cmd_callbacks;

}  // namespace

void BleBridge::begin(JobState *state, MqttBridge *mqtt) {
  state_ = state;
  mqtt_ = mqtt;
  g_ble_state = state;
  g_ble_mqtt = mqtt;

  // Wi‑Fi is already up (or connecting). NimBLE coexists on ESP32-S3.
  NimBLEDevice::init("KidsPanel");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  NimBLEDevice::setMTU(247);

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(kServiceUuid);
  NimBLECharacteristic *cmd = service->createCharacteristic(
      kCmdUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cmd->setCallbacks(&g_cmd_callbacks);
  g_status_char = service->createCharacteristic(
      kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  g_status_char->setValue("{}");
  service->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(kServiceUuid);
  adv->setName("KidsPanel");
  adv->setScanResponse(true);
  adv->start();
  started_ = true;
  Serial.println("[ble] advertising KidsPanel");
}

void BleBridge::loop() {
  if (!started_) {
    return;
  }
  // NimBLE is callback-driven; nudge advertising periodically.
  const uint32_t now = millis();
  if (now - last_keepalive_ms_ > 20000) {
    last_keepalive_ms_ = now;
    NimBLEDevice::startAdvertising();
  }
}
