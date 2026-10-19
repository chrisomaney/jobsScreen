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
  client_.setBufferSize(1024);
  client_.setCallback(mqttTrampoline);
}

String MqttBridge::topicJob(size_t kid, size_t job) const {
  return String(MQTT_BASE) + "/" + KIDS[kid].slug + "/job/" + JOBS[job].id + "/state";
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
  String cmds[KID_COUNT + 2];
  cmds[0] = String(MQTT_BASE) + "/cmd/reset_all";
  cmds[1] = String(MQTT_BASE) + "/cmd/reset_rewards";
  for (size_t k = 0; k < KID_COUNT; ++k) {
    cmds[k + 2] = String(MQTT_BASE) + "/" + KIDS[k].slug + "/cmd/reset";
  }
  for (const String &t : cmds) {
    client_.publish(t.c_str(), "", true);
  }
  for (const String &t : cmds) {
    client_.subscribe(t.c_str());
  }
  publishAll();
  Serial.println("[mqtt] connected");
}

void MqttBridge::handleMessage(char *topic, byte *payload, unsigned int length) {
  (void)payload;
  if (length == 0) {
    return;  // empty payload = our own "clear retained" publish; ignore
  }
  const String t(topic);
  Serial.printf("[mqtt] cmd %s\n", t.c_str());

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
  doc["job"] = JOBS[job].id;
  doc["job_label"] = JOBS[job].label;
  doc["tally"] = state_->rewardTally(doer);
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_REWARDS_EVENT, json.c_str(), false);
  publishRewardTally(doer);
}

void MqttBridge::publishAll() {
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
