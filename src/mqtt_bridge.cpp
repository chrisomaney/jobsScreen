#include "mqtt_bridge.h"

#include <ArduinoJson.h>

#include "app_secrets.h"

namespace {

MqttBridge *g_mqtt = nullptr;

void mqttTrampoline(char *topic, byte *payload, unsigned int length) {
  if (g_mqtt) {
    g_mqtt->handleMessage(topic, payload, length);
  }
}

}  // namespace

void MqttBridge::begin(JobState *state) {
  state_ = state;
  g_mqtt = this;
  client_.setClient(wifi_);
  client_.setServer(MQTT_HOST, MQTT_PORT);
  client_.setBufferSize(6144);  // config JSON grows with custom jobs
  client_.setCallback(mqttTrampoline);
}

String MqttBridge::topicJob(size_t kid, size_t job) const {
  return String(MQTT_BASE) + "/" + KIDS[kid].slug + "/job/" + JobState::jobId(job) + "/state";
}

String MqttBridge::topicInternet(size_t kid) const {
  return String(MQTT_BASE) + "/" + KIDS[kid].slug + "/internet";
}

String MqttBridge::topicProgress(size_t kid) const {
  return String(MQTT_BASE) + "/" + KIDS[kid].slug + "/progress";
}

String MqttBridge::topicRewardTally(size_t kid) const {
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

  // Clear any *retained* command first (empty retained payload deletes it),
  // otherwise a retained reset would wipe everything on every reconnect.
  String cmds[KID_COUNT + 3];
  cmds[0] = String(MQTT_BASE) + "/cmd/reset_all";
  cmds[1] = String(MQTT_BASE) + "/cmd/reset_rewards";
  cmds[2] = MQTT_CONFIG_SET;
  for (size_t k = 0; k < KID_COUNT; ++k) {
    cmds[k + 3] = String(MQTT_BASE) + "/" + KIDS[k].slug + "/cmd/reset";
  }
  for (const String &t : cmds) {
    client_.publish(t.c_str(), "", true);
  }
  for (const String &t : cmds) {
    client_.subscribe(t.c_str());
  }
  client_.subscribe(MQTT_CATALOG_SET);
  client_.subscribe(MQTT_BONUS_SET);
  for (size_t k = 0; k < KID_COUNT; ++k) {
    client_.subscribe(topicRewardTally(k).c_str());
  }
  publishAll();
  Serial.println("[mqtt] connected");
}

void MqttBridge::handleMessage(char *topic, byte *payload, unsigned int length) {
  if (length == 0) {
    return;  // empty payload = our own "clear retained" publish; ignore
  }
  const String t(topic);
  Serial.printf("[mqtt] cmd %s\n", t.c_str());

  if (t == MQTT_CONFIG_SET) {
    if (state_->applyConfigJson(reinterpret_cast<const char *>(payload), length)) {
      Serial.println("[mqtt] job config updated from web app");
      publishAll();  // assignments change counts + internet on/off
    } else {
      publishConfig();  // nothing changed — still echo so the web app can confirm
    }
    return;
  }
  if (t == String(MQTT_BASE) + "/cmd/reset_all") {
    state_->resetAll();
    publishAll();
    return;
  }
  if (t == String(MQTT_BASE) + "/cmd/reset_rewards") {
    state_->resetRewards();
    for (size_t k = 0; k < KID_COUNT; ++k) {
      publishRewardTally(k);
    }
    return;
  }

  if (t == MQTT_CATALOG_SET) {
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) {
      body += static_cast<char>(payload[i]);
    }
    if (state_->setCatalogFromJson(body.c_str())) {
      publishCatalog();
      Serial.println("[mqtt] prize catalog updated");
    }
    return;
  }

  if (t == MQTT_BONUS_SET) {
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) {
      body += static_cast<char>(payload[i]);
    }
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
      return;
    }
    const char *slug = doc["kid"] | "";
    const uint32_t points = doc["points"] | 0;
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
    const uint32_t before = state_->rewardTally(kid);
    state_->addBonus(kid, points);
    const uint32_t after = state_->rewardTally(kid);
    publishRewardTally(kid);
    publishBonus(kid, points, before, after);
    publishCelebrateBonus(kid, points, after);
    return;
  }

  for (size_t k = 0; k < KID_COUNT; ++k) {
    if (t == topicRewardTally(k)) {
      String body;
      body.reserve(length + 1);
      for (unsigned int i = 0; i < length; ++i) {
        body += static_cast<char>(payload[i]);
      }
      const uint32_t v = static_cast<uint32_t>(body.toInt());
      if (state_->rewardTally(k) != v) {
        state_->setRewardTally(k, v);
        Serial.printf("[mqtt] tally %s = %lu\n", KIDS[k].slug, static_cast<unsigned long>(v));
      }
      return;
    }
  }

  for (size_t k = 0; k < KID_COUNT; ++k) {
    String expect = String(MQTT_BASE) + "/" + KIDS[k].slug + "/cmd/reset";
    if (t == expect) {
      state_->resetKid(k);
      for (size_t j = 0; j < JOB_COUNT; ++j) {
        publishJob(k, j);
      }
      publishKidSummary(k);
      return;
    }
  }
}

void MqttBridge::publishJob(size_t kid, size_t job) {
  if (!client_.connected() || !JobState::isAssigned(kid, job)) {
    return;
  }
  const char *val = state_->isDone(kid, job) ? "done" : "todo";
  client_.publish(topicJob(kid, job).c_str(), val, true);
}

void MqttBridge::publishKidSummary(size_t kid) {
  if (!client_.connected() || kid >= KID_COUNT) {
    return;
  }

  const bool ready = state_->internetReady(kid);
  client_.publish(topicInternet(kid).c_str(), ready ? "on" : "off", true);

  JsonDocument doc;
  const size_t done = state_->doneCount(kid);
  const size_t req_done = state_->requiredDoneCount(kid);
  const size_t req_total = state_->requiredTotal(kid);
  doc["done"] = done;
  doc["total"] = state_->assignedCount(kid);
  doc["required_done"] = req_done;
  doc["required_total"] = req_total;
  doc["pct"] = req_total ? (req_done * 100) / req_total : 100;
  doc["internet"] = ready;
  doc["name"] = KIDS[kid].name;
  doc["rewards"] = state_->rewardTally(kid);

  String json;
  serializeJson(doc, json);
  client_.publish(topicProgress(kid).c_str(), json.c_str(), true);
  publishRewardTally(kid);
}

void MqttBridge::publishRewardTally(size_t kid) {
  if (!client_.connected() || kid >= KID_COUNT) {
    return;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(state_->rewardTally(kid)));
  client_.publish(topicRewardTally(kid).c_str(), buf, true);
}

void MqttBridge::publishRewardEvent(size_t doer, size_t owner, size_t job) {
  if (!client_.connected() || doer >= KID_COUNT || owner >= KID_COUNT || job >= JOB_COUNT) {
    return;
  }
  JsonDocument doc;
  doc["doer"] = KIDS[doer].slug;
  doc["doer_name"] = KIDS[doer].name;
  doc["owner"] = KIDS[owner].slug;
  doc["owner_name"] = KIDS[owner].name;
  doc["job"] = JobState::jobId(job);
  doc["job_label"] = JobState::jobLabel(job);
  doc["tally"] = state_->rewardTally(doer);
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_REWARDS_EVENT, json.c_str(), false);
  publishRewardTally(doer);
}

void MqttBridge::publishConfig() {
  if (!client_.connected()) {
    return;
  }
  const String json = state_->configStateJson();
  client_.publish(MQTT_CONFIG_STATE, json.c_str(), true);
}

void MqttBridge::publishCatalog() {
  if (!client_.connected()) {
    return;
  }
  const String json = state_->catalogToJson();
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
  publishConfig();
  publishCatalog();
  for (size_t k = 0; k < KID_COUNT; ++k) {
    for (size_t j = 0; j < JOB_COUNT; ++j) {
      publishJob(k, j);
    }
    publishKidSummary(k);
  }
}

void MqttBridge::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
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
