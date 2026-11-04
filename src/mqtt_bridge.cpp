#include "mqtt_bridge.h"

#include <ArduinoJson.h>
#include <string.h>

#include "app_secrets.h"

namespace {

MqttBridge *g_mqtt = nullptr;

void mqttTrampoline(char *topic, byte *payload, unsigned int length) {
  if (g_mqtt) {
    g_mqtt->handleMessage(topic, payload, length);
  }
}

}  // namespace

void MqttBridge::begin(JobState *state, RewardEngine *rewards) {
  state_ = state;
  rewards_ = rewards;
  g_mqtt = this;
  client_.setClient(wifi_);
  client_.setServer(MQTT_HOST, MQTT_PORT);
  client_.setBufferSize(6144);  // config JSON grows with custom jobs
  client_.setCallback(mqttTrampoline);
  pending_config_buf_[0] = '\0';
  pending_catalog_buf_[0] = '\0';
  pending_rules_buf_[0] = '\0';
  pending_net_sched_buf_[0] = '\0';
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

void MqttBridge::queuePayload(char *dest, size_t *dest_len, bool *flag, const byte *payload,
                              unsigned int length) {
  if (!dest || !dest_len || !flag) {
    return;
  }
  const size_t n = length < (kPendingPayloadMax - 1) ? length : (kPendingPayloadMax - 1);
  memcpy(dest, payload, n);
  dest[n] = '\0';
  *dest_len = n;
  *flag = true;
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
  client_.subscribe(MQTT_RULES_SET);
  client_.subscribe(MQTT_PANEL_MESSAGE_SET);
  client_.subscribe(MQTT_NET_SCHEDULES_SET);
  for (size_t k = 0; k < KID_COUNT; ++k) {
    client_.subscribe(topicRewardTally(k).c_str());
    client_.subscribe((topicInternet(k) + "/set").c_str());
  }
  // Shared MikroTik profile — admin can force on/off; panel does not auto-manage it.
  client_.subscribe((String(MQTT_BASE) + "/common/internet/set").c_str());
  requestPublishAll();
  Serial.println("[mqtt] connected");
}

void MqttBridge::handleMessage(char *topic, byte *payload, unsigned int length) {
  if (length == 0) {
    return;  // empty payload = our own "clear retained" publish; ignore
  }
  const String t(topic);
  Serial.printf("[mqtt] cmd %s\n", t.c_str());

  if (t == MQTT_CONFIG_SET) {
    // Defer JSON/NVS — applying + republishing here freezes the RGB panel.
    queuePayload(pending_config_buf_, &pending_config_len_, &pending_config_apply_, payload, length);
    return;
  }

  // Parent internet override: kids/{slug}/internet/set  payload on|off|auto
  for (size_t k = 0; k < KID_COUNT; ++k) {
    if (t == topicInternet(k) + "/set") {
      String body;
      body.reserve(length + 1);
      for (unsigned int i = 0; i < length; ++i) {
        body += static_cast<char>(payload[i]);
      }
      body.trim();
      body.toLowerCase();
      JobState::InternetOverride mode = JobState::InternetOverride::Auto;
      if (body == "on" || body == "1" || body == "true") {
        mode = JobState::InternetOverride::On;
      } else if (body == "off" || body == "0" || body == "false") {
        mode = JobState::InternetOverride::Off;
      } else {
        mode = JobState::InternetOverride::Auto;
      }
      state_->setInternetOverride(k, mode);
      pending_publish_summaries_ = true;
      Serial.printf("[mqtt] internet override %s = %s\n", KIDS[k].slug, body.c_str());
      return;
    }
  }
  if (t == String(MQTT_BASE) + "/common/internet/set") {
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) {
      body += static_cast<char>(payload[i]);
    }
    body.trim();
    body.toLowerCase();
    const char *val = (body == "on" || body == "1" || body == "true") ? "on" : "off";
    client_.publish((String(MQTT_BASE) + "/common/internet").c_str(), val, true);
    Serial.printf("[mqtt] common internet = %s\n", val);
    return;
  }
  if (t == String(MQTT_BASE) + "/cmd/reset_all") {
    state_->resetAll();
    pending_publish_all_ = true;
    return;
  }
  if (t == String(MQTT_BASE) + "/cmd/reset_rewards") {
    state_->resetRewards();
    pending_publish_summaries_ = true;
    return;
  }

  if (t == MQTT_CATALOG_SET) {
    queuePayload(pending_catalog_buf_, &pending_catalog_len_, &pending_catalog_apply_, payload, length);
    return;
  }

  if (t == MQTT_RULES_SET && rewards_) {
    queuePayload(pending_rules_buf_, &pending_rules_len_, &pending_rules_apply_, payload, length);
    return;
  }

  if (t == MQTT_NET_SCHEDULES_SET) {
    queuePayload(pending_net_sched_buf_, &pending_net_sched_len_, &pending_net_sched_apply_, payload,
                length);
    return;
  }

  if (t == MQTT_PANEL_MESSAGE_SET && rewards_) {
    String body;
    body.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) {
      body += static_cast<char>(payload[i]);
    }
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
      return;
    }
    const char *title = doc["title"] | "Message";
    const char *text = doc["text"] | "";
    const uint32_t seconds = doc["seconds"] | 12;
    rewards_->setPanelMessage(title, text, seconds);
    Serial.println("[mqtt] panel message set");
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
    const int32_t points = doc["points"] | 0;
    const char *title = doc["title"] | "";
    const char *message = doc["message"] | "";
    const bool behaviour = doc["behaviour"] | false;
    const bool show_panel = doc["panel"] | behaviour;
    const bool show_tv = doc["tv"] | behaviour;
    const uint32_t seconds = doc["seconds"] | 12;
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
    // Splash on panel for plain ±; Behaviour uses custom title/message overlays.
    state_->adjustPoints(kid, points, !behaviour);
    const uint32_t after = state_->rewardTally(kid);
    // Keep MQTT traffic tiny in-callback — one tally + one event is OK.
    publishRewardTally(kid);
    publishBonus(kid, points, before, after);
    if (behaviour) {
      if (show_panel && message[0] && rewards_) {
        const char *pt = title[0] ? title : (points > 0 ? "Great behaviour!" : "Behaviour note");
        rewards_->setPanelMessage(pt, message, seconds);
      }
      if (show_tv) {
        publishCelebrateBehaviour(kid, points, title, message, after);
      }
    } else if (points > 0) {
      publishCelebrateBonus(kid, static_cast<uint32_t>(points), after);
    }
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
      pending_publish_all_ = true;  // republish outside the callback
      return;
    }
  }
}

bool MqttBridge::processDeferredApplies() {
  // One heavy apply per loop() so LVGL can paint between Saves.
  if (pending_config_apply_) {
    pending_config_apply_ = false;
    if (state_->applyConfigJson(pending_config_buf_, pending_config_len_)) {
      Serial.println("[mqtt] job config updated from web app");
      // Assignments change counts/internet — do NOT blast every job topic.
      requestPublishConfigAndSummaries();
    } else {
      pending_publish_config_ = true;  // echo so the web app can confirm
    }
    return true;
  }

  if (pending_catalog_apply_) {
    pending_catalog_apply_ = false;
    if (state_->setCatalogFromJson(pending_catalog_buf_)) {
      pending_publish_catalog_ = true;
      Serial.println("[mqtt] prize catalog updated");
    }
    return true;
  }

  if (pending_rules_apply_ && rewards_) {
    pending_rules_apply_ = false;
    if (rewards_->applyRulesJson(pending_rules_buf_)) {
      pending_publish_rules_ = true;
      Serial.println("[mqtt] reward rules updated");
    }
    return true;
  }

  if (pending_net_sched_apply_) {
    pending_net_sched_apply_ = false;
    if (state_->applyInternetSchedulesJson(pending_net_sched_buf_)) {
      pending_publish_net_sched_ = true;
      pending_publish_summaries_ = true;  // internet may flip
      Serial.println("[mqtt] internet schedules updated");
    } else {
      pending_publish_net_sched_ = true;  // echo current
    }
    return true;
  }
  return false;
}

void MqttBridge::publishJob(size_t kid, size_t job) {
  if (!client_.connected() || !JobState::isAssigned(kid, job)) {
    return;
  }
  const char *val = state_->isDone(kid, job) ? "done" : "todo";
  client_.publish(topicJob(kid, job).c_str(), val, true);
}

void MqttBridge::publishKidInternet(size_t kid) {
  if (!client_.connected() || kid >= KID_COUNT) {
    return;
  }
  const bool ready = state_->internetEffective(kid);
  client_.publish(topicInternet(kid).c_str(), ready ? "on" : "off", true);
}

void MqttBridge::publishKidProgress(size_t kid) {
  if (!client_.connected() || kid >= KID_COUNT) {
    return;
  }
  JsonDocument doc;
  const size_t done = state_->doneCount(kid);
  const size_t req_done = state_->requiredDoneCount(kid);
  const size_t req_total = state_->requiredTotal(kid);
  const bool ready = state_->internetEffective(kid);
  doc["done"] = done;
  doc["total"] = state_->assignedCount(kid);
  doc["required_done"] = req_done;
  doc["required_total"] = req_total;
  doc["pct"] = req_total ? (req_done * 100) / req_total : 100;
  doc["internet"] = ready;
  doc["jobs_ready"] = state_->internetReady(kid);
  doc["scheduled_off"] = state_->internetScheduledOff(kid);
  const auto ov = state_->internetOverride(kid);
  doc["override"] = ov == JobState::InternetOverride::On    ? "on"
                    : ov == JobState::InternetOverride::Off ? "off"
                                                            : "auto";
  doc["name"] = KIDS[kid].name;
  doc["rewards"] = state_->rewardTally(kid);

  String json;
  serializeJson(doc, json);
  client_.publish(topicProgress(kid).c_str(), json.c_str(), true);
}

void MqttBridge::publishKidSummary(size_t kid) {
  publishKidInternet(kid);
  publishKidProgress(kid);
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

void MqttBridge::publishRules() {
  if (!client_.connected() || !rewards_) {
    return;
  }
  const String json = rewards_->rulesStateJson();
  client_.publish(MQTT_RULES_STATE, json.c_str(), true);
}

void MqttBridge::publishInternetSchedules() {
  if (!client_.connected()) {
    return;
  }
  const String json = state_->internetSchedulesJson();
  client_.publish(MQTT_NET_SCHEDULES_STATE, json.c_str(), true);
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

void MqttBridge::publishBonus(size_t kid, int32_t points, uint32_t before, uint32_t after) {
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

void MqttBridge::publishCelebrateBehaviour(size_t kid, int32_t points, const char *title,
                                           const char *message, uint32_t after) {
  if (!client_.connected()) {
    return;
  }
  JsonDocument doc;
  doc["type"] = points >= 0 ? "behaviour" : "penalty";
  doc["kid"] = KIDS[kid].slug;
  if (title && title[0]) {
    doc["title"] = title;
  } else {
    doc["title"] = points >= 0 ? "GREAT BEHAVIOUR!" : "BEHAVIOUR NOTE";
  }
  char sub[160];
  if (message && message[0]) {
    snprintf(sub, sizeof(sub), "%s  (%+ld pts)", message, static_cast<long>(points));
  } else {
    snprintf(sub, sizeof(sub), "%+ld points", static_cast<long>(points));
  }
  doc["subtitle"] = sub;
  doc["points"] = points;
  doc["tally_after"] = after;
  doc["ts"] = millis();
  String json;
  serializeJson(doc, json);
  client_.publish(MQTT_CELEBRATE_TOPIC, json.c_str(), false);
}

void MqttBridge::publishSummaries() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    publishKidSummary(k);
  }
}

void MqttBridge::startPublishJob(bool include_jobs, bool include_catalog, bool include_rules,
                                 bool include_net_sched, bool include_summaries) {
  pub_include_jobs_ = include_jobs;
  pub_include_catalog_ = include_catalog;
  pub_include_rules_ = include_rules;
  pub_include_net_sched_ = include_net_sched;
  pub_include_summaries_ = include_summaries;
  pub_kid_ = 0;
  pub_job_ = 0;
  pub_summary_step_ = 0;
  pub_phase_ = PubPhase::Config;
  Serial.printf("[mqtt] publish start (jobs=%d catalog=%d rules=%d sched=%d sums=%d)\n",
                include_jobs ? 1 : 0, include_catalog ? 1 : 0, include_rules ? 1 : 0,
                include_net_sched ? 1 : 0, include_summaries ? 1 : 0);
}

void MqttBridge::pumpPublish() {
  if (!client_.connected()) {
    pub_phase_ = PubPhase::Idle;
    return;
  }

  // Start a new multi-step job when idle.
  if (pub_phase_ == PubPhase::Idle) {
    if (pending_publish_all_) {
      pending_publish_all_ = false;
      pending_publish_config_ = false;
      pending_publish_catalog_ = false;
      pending_publish_rules_ = false;
      pending_publish_net_sched_ = false;
      pending_publish_summaries_ = false;
      startPublishJob(true, true, true, true, true);
    } else if (pending_publish_config_ || pending_publish_catalog_ || pending_publish_rules_ ||
               pending_publish_net_sched_ || pending_publish_summaries_) {
      const bool cfg = pending_publish_config_;
      const bool cat = pending_publish_catalog_;
      const bool rules = pending_publish_rules_;
      const bool sched = pending_publish_net_sched_;
      const bool sums = pending_publish_summaries_;
      pending_publish_config_ = false;
      pending_publish_catalog_ = false;
      pending_publish_rules_ = false;
      pending_publish_net_sched_ = false;
      pending_publish_summaries_ = false;
      // Reuse the machine; skip unused stages inside pump.
      pub_include_jobs_ = false;
      pub_include_catalog_ = cat;
      pub_include_rules_ = rules;
      pub_include_net_sched_ = sched;
      pub_include_summaries_ = sums;
      pub_kid_ = 0;
      pub_job_ = 0;
      pub_summary_step_ = 0;
      if (cfg) {
        pub_phase_ = PubPhase::Config;
      } else if (cat) {
        pub_phase_ = PubPhase::Catalog;
      } else if (rules) {
        pub_phase_ = PubPhase::Rules;
      } else if (sched) {
        pub_phase_ = PubPhase::NetSched;
      } else if (sums) {
        pub_phase_ = PubPhase::Summaries;
      }
    } else {
      return;
    }
  }

  auto afterMeta = [this]() {
    if (pub_include_jobs_) {
      pub_phase_ = PubPhase::Jobs;
      pub_kid_ = 0;
      pub_job_ = 0;
    } else if (pub_include_summaries_) {
      pub_phase_ = PubPhase::Summaries;
      pub_kid_ = 0;
      pub_summary_step_ = 0;
    } else {
      pub_phase_ = PubPhase::Idle;
      Serial.println("[mqtt] publish done");
    }
  };

  uint8_t budget = kPubsPerPump;
  while (budget > 0 && pub_phase_ != PubPhase::Idle) {
    switch (pub_phase_) {
      case PubPhase::Config:
        publishConfig();
        --budget;
        if (pub_include_catalog_) {
          pub_phase_ = PubPhase::Catalog;
        } else if (pub_include_rules_) {
          pub_phase_ = PubPhase::Rules;
        } else if (pub_include_net_sched_) {
          pub_phase_ = PubPhase::NetSched;
        } else {
          afterMeta();
        }
        break;

      case PubPhase::Catalog:
        publishCatalog();
        --budget;
        if (pub_include_rules_) {
          pub_phase_ = PubPhase::Rules;
        } else if (pub_include_net_sched_) {
          pub_phase_ = PubPhase::NetSched;
        } else {
          afterMeta();
        }
        break;

      case PubPhase::Rules:
        publishRules();
        --budget;
        if (pub_include_net_sched_) {
          pub_phase_ = PubPhase::NetSched;
        } else {
          afterMeta();
        }
        break;

      case PubPhase::NetSched:
        publishInternetSchedules();
        --budget;
        afterMeta();
        break;

      case PubPhase::Jobs: {
        // Advance through the matrix; skip unassigned quickly without counting
        // against the publish budget so we finish in reasonable time.
        uint8_t scanned = 0;
        while (budget > 0 && pub_phase_ == PubPhase::Jobs && scanned < 32) {
          ++scanned;
          if (JobState::isAssigned(pub_kid_, pub_job_)) {
            publishJob(pub_kid_, pub_job_);
            --budget;
          }
          ++pub_job_;
          if (pub_job_ >= JOB_COUNT) {
            pub_job_ = 0;
            ++pub_kid_;
            if (pub_kid_ >= KID_COUNT) {
              if (pub_include_summaries_) {
                pub_phase_ = PubPhase::Summaries;
                pub_kid_ = 0;
                pub_summary_step_ = 0;
              } else {
                pub_phase_ = PubPhase::Idle;
                Serial.println("[mqtt] publish done");
              }
              break;
            }
          }
        }
        break;
      }

      case PubPhase::Summaries:
        // One MQTT message per slice (internet / progress / tally).
        if (pub_summary_step_ == 0) {
          publishKidInternet(pub_kid_);
        } else if (pub_summary_step_ == 1) {
          publishKidProgress(pub_kid_);
        } else {
          publishRewardTally(pub_kid_);
        }
        --budget;
        ++pub_summary_step_;
        if (pub_summary_step_ >= 3) {
          pub_summary_step_ = 0;
          ++pub_kid_;
          if (pub_kid_ >= KID_COUNT) {
            pub_phase_ = PubPhase::Idle;
            Serial.println("[mqtt] publish done");
          }
        }
        break;

      case PubPhase::Idle:
        break;
    }
  }
}

void MqttBridge::publishAll() {
  // Compatibility for callers that still want a "full sync" — always chunked.
  pending_publish_all_ = true;
}

void MqttBridge::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  ensureConnected();
  if (!client_.connected()) {
    return;
  }

  client_.loop();
  // Never apply + publish in the same slice — RGB panel needs a paint between.
  if (processDeferredApplies()) {
    return;
  }
  pumpPublish();

  const uint32_t now = millis();
  // Light keepalive for HA after broker restarts — never full job-state blast
  // on a timer (that freezes the panel). Job topics still update on real changes.
  if (pub_phase_ == PubPhase::Idle && now - last_repub_ms_ >= MQTT_REPUBLISH_MS) {
    last_repub_ms_ = now;
    client_.publish(MQTT_STATUS_TOPIC, "online", true);
    pending_publish_config_ = true;
    pending_publish_catalog_ = true;
    pending_publish_rules_ = true;
    pending_publish_net_sched_ = true;
    pending_publish_summaries_ = true;
  }
}
