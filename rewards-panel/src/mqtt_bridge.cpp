#include "mqtt_bridge.h"

#include <ArduinoJson.h>
#include <time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h missing — copy include/secrets.example.h to include/secrets.h"
#define WIFI_SSID "CHANGE_ME"
#define WIFI_PASSWORD "CHANGE_ME"
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "kids-rewards-panel"
#endif

namespace {
MqttBridge *g_bridge = nullptr;

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (g_bridge) {
    g_bridge->handleMessage(topic, payload, length);
  }
}
}  // namespace

void MqttBridge::begin(RewardState *state) {
  state_ = state;
  g_bridge = this;
  client_.setClient(wifi_);
  client_.setServer(MQTT_HOST, MQTT_PORT);
  client_.setBufferSize(2048);
  client_.setCallback(mqttCallback);
}

String MqttBridge::topicTally(size_t kid) const {
  return String(MQTT_BASE) + "/" + KIDS[kid].slug + "/rewards/tally";
}

void MqttBridge::ensureConnected() {
  if (client_.connected()) {
    return;
  }
  const uint32_t now = millis();
  if (now - last_reconnect_ms_ < 3000) {
    return;
  }
  last_reconnect_ms_ = now;
  Serial.printf("[mqtt] connecting to %s...\n", MQTT_HOST);
  const bool ok = (strlen(MQTT_USER) > 0)
                      ? client_.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                                        MQTT_STATUS_TOPIC, 1, true, "offline")
                      : client_.connect(MQTT_CLIENT_ID, MQTT_STATUS_TOPIC, 1, true, "offline");
  if (!ok) {
    Serial.printf("[mqtt] connect failed rc=%d\n", client_.state());
    return;
  }
  client_.publish(MQTT_STATUS_TOPIC, "online", true);
  client_.subscribe(MQTT_CATALOG_SET);
  client_.subscribe(MQTT_BONUS_SET);
  for (size_t k = 0; k < KID_COUNT; ++k) {
    client_.subscribe(topicTally(k).c_str());
  }
  Serial.println("[mqtt] connected");
  publishAll();
}

void MqttBridge::handleMessage(char *topic, byte *payload, unsigned int length) {
  String t(topic);
  String body;
  body.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) {
    body += static_cast<char>(payload[i]);
  }

  if (t == MQTT_CATALOG_SET) {
    if (state_->setCatalogFromJson(body.c_str())) {
      publishCatalog();
      Serial.println("[mqtt] catalog updated");
    }
    return;
  }

  if (t == MQTT_BONUS_SET) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
      return;
    }
    const char *slug = doc["kid"] | "";
    uint32_t points = doc["points"] | 0;
    size_t kid = KID_COUNT;
    for (size_t i = 0; i < KID_COUNT; ++i) {
      if (strcmp(KIDS[i].slug, slug) == 0) {
        kid = i;
        break;
      }
    }
    if (kid >= KID_COUNT || points == 0) {
      return;
    }
    const uint32_t before = state_->tally(kid);
    state_->addBonus(kid, points);
    const uint32_t after = state_->tally(kid);
    publishTally(kid);
    publishBonus(kid, points, before, after);
    publishCelebrateBonus(kid, points, after);
    return;
  }

  for (size_t k = 0; k < KID_COUNT; ++k) {
    if (t == topicTally(k)) {
      const uint32_t v = static_cast<uint32_t>(body.toInt());
      // Avoid echo loops: only apply if different
      if (state_->tally(k) != v) {
        state_->setTally(k, v);
        Serial.printf("[mqtt] tally %s = %lu\n", KIDS[k].slug, static_cast<unsigned long>(v));
      }
      return;
    }
  }
}

void MqttBridge::publishTally(size_t kid) {
  if (kid >= KID_COUNT || !client_.connected()) {
    return;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(state_->tally(kid)));
  client_.publish(topicTally(kid).c_str(), buf, true);
}

void MqttBridge::publishCatalog() {
  if (!client_.connected()) {
    return;
  }
  String json = state_->catalogToJson();
  client_.publish(MQTT_CATALOG_TOPIC, json.c_str(), true);
}

void MqttBridge::publishClaim(size_t kid, size_t prize_index, uint32_t before, uint32_t after) {
  if (!client_.connected() || prize_index >= state_->prizeCount()) {
    return;
  }
  JsonDocument doc;
  doc["kid"] = KIDS[kid].slug;
  doc["prize_id"] = state_->prize(prize_index).id;
  doc["prize_name"] = state_->prize(prize_index).name;
  doc["cost"] = state_->prize(prize_index).cost;
  doc["tally_before"] = before;
  doc["tally_after"] = after;
  doc["ts"] = millis();
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_CLAIM_TOPIC, json.c_str(), false);
}

void MqttBridge::publishBonus(size_t kid, uint32_t points, uint32_t before, uint32_t after) {
  if (!client_.connected()) {
    return;
  }
  JsonDocument doc;
  doc["kid"] = KIDS[kid].slug;
  doc["points"] = points;
  doc["tally_before"] = before;
  doc["tally_after"] = after;
  doc["ts"] = millis();
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_BONUS_TOPIC, json.c_str(), false);
}

void MqttBridge::publishCelebrateWinner(size_t kid, size_t prize_index, uint32_t after) {
  if (!client_.connected() || prize_index >= state_->prizeCount()) {
    return;
  }
  JsonDocument doc;
  doc["type"] = "winner";
  doc["kid"] = KIDS[kid].slug;
  doc["title"] = "WINNER!";
  doc["subtitle"] = state_->prize(prize_index).name;
  doc["prize_name"] = state_->prize(prize_index).name;
  doc["tally_after"] = after;
  doc["ts"] = millis();
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_CELEBRATE_TOPIC, json.c_str(), false);
}

void MqttBridge::publishCelebrateBonus(size_t kid, uint32_t points, uint32_t after) {
  if (!client_.connected()) {
    return;
  }
  JsonDocument doc;
  doc["type"] = "bonus";
  doc["kid"] = KIDS[kid].slug;
  doc["title"] = "BONUS!";
  char sub[48];
  snprintf(sub, sizeof(sub), "+%lu points", static_cast<unsigned long>(points));
  doc["subtitle"] = sub;
  doc["points"] = points;
  doc["tally_after"] = after;
  doc["ts"] = millis();
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_CELEBRATE_TOPIC, json.c_str(), false);
}

void MqttBridge::publishAll() {
  publishCatalog();
  for (size_t k = 0; k < KID_COUNT; ++k) {
    publishTally(k);
  }
}

void MqttBridge::loop() {
  ensureConnected();
  if (client_.connected()) {
    client_.loop();
    const uint32_t now = millis();
    if (now - last_repub_ms_ >= MQTT_REPUBLISH_MS) {
      last_repub_ms_ = now;
      publishAll();
    }
  }
}
