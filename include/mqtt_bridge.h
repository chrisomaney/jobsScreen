#pragma once

#include "job_state.h"
#include "reward_engine.h"
#include <PubSubClient.h>
#include <WiFi.h>

class MqttBridge {
public:
  void begin(JobState *state, RewardEngine *rewards);
  void loop();
  void publishJob(size_t kid, size_t job);
  void publishKidSummary(size_t kid);
  void publishRewardTally(size_t kid);
  void publishRewardEvent(size_t doer, size_t owner, size_t job);
  void publishCatalog();
  void publishRules();
  void publishInternetSchedules();
  void publishClaim(size_t kid, size_t prize_index, uint32_t before, uint32_t after);
  void publishBonus(size_t kid, int32_t points, uint32_t before, uint32_t after);
  void publishCelebrateWinner(size_t kid, size_t prize_index, uint32_t after);
  void publishCelebrateBonus(size_t kid, uint32_t points, uint32_t after);
  void publishCelebrateBehaviour(size_t kid, int32_t points, const char *title, const char *message,
                                 uint32_t after);
  void publishAll();
  void publishConfig();
  // Queue a full HA republish from outside the MQTT callback (e.g. BLE config).
  // Work is chunked across loop() so the RGB/LVGL panel keeps painting.
  void requestPublishAll() { pending_publish_all_ = true; }
  // Config/assignment save: echo config + kid summaries only (not every job topic).
  void requestPublishConfigAndSummaries() {
    pending_publish_config_ = true;
    pending_publish_summaries_ = true;
  }
  bool connected() { return client_.connected(); }
  bool busyPublishing() const { return pub_phase_ != PubPhase::Idle; }
  void handleMessage(char *topic, byte *payload, unsigned int length);

private:
  enum class PubPhase : uint8_t {
    Idle = 0,
    Config,
    Catalog,
    Rules,
    NetSched,
    Jobs,
    Summaries,
  };

  static constexpr size_t kPendingPayloadMax = 6144;
  static constexpr uint8_t kPubsPerPump = 2;  // keep each loop() slice short

  WiFiClient wifi_;
  PubSubClient client_;
  JobState *state_ = nullptr;
  RewardEngine *rewards_ = nullptr;
  uint32_t last_reconnect_ms_ = 0;
  uint32_t last_repub_ms_ = 0;

  // Never publish or do heavy NVS/JSON work inside the PubSubClient callback —
  // that freezes the RGB panel (LVGL starved) and can leave the screen looking
  // mirrored/shifted until power cycle.
  bool pending_publish_all_ = false;
  bool pending_publish_config_ = false;
  bool pending_publish_catalog_ = false;
  bool pending_publish_rules_ = false;
  bool pending_publish_net_sched_ = false;
  bool pending_publish_summaries_ = false;

  PubPhase pub_phase_ = PubPhase::Idle;
  size_t pub_kid_ = 0;
  size_t pub_job_ = 0;
  bool pub_include_jobs_ = false;
  bool pub_include_catalog_ = false;
  bool pub_include_rules_ = false;
  bool pub_include_net_sched_ = false;

  bool pending_config_apply_ = false;
  bool pending_catalog_apply_ = false;
  bool pending_rules_apply_ = false;
  bool pending_net_sched_apply_ = false;
  size_t pending_config_len_ = 0;
  size_t pending_catalog_len_ = 0;
  size_t pending_rules_len_ = 0;
  size_t pending_net_sched_len_ = 0;
  char pending_config_buf_[kPendingPayloadMax];
  char pending_catalog_buf_[kPendingPayloadMax];
  char pending_rules_buf_[kPendingPayloadMax];
  char pending_net_sched_buf_[kPendingPayloadMax];

  void ensureConnected();
  void queuePayload(char *dest, size_t *dest_len, bool *flag, const byte *payload, unsigned int length);
  void processDeferredApplies();
  void startPublishJob(bool include_jobs, bool include_catalog, bool include_rules, bool include_net_sched);
  void pumpPublish();
  void publishSummaries();
  String topicJob(size_t kid, size_t job) const;
  String topicInternet(size_t kid) const;
  String topicProgress(size_t kid) const;
  String topicRewardTally(size_t kid) const;
};
