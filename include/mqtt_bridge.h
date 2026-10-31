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
  void publishClaim(size_t kid, size_t prize_index, uint32_t before, uint32_t after);
  void publishBonus(size_t kid, int32_t points, uint32_t before, uint32_t after);
  void publishCelebrateWinner(size_t kid, size_t prize_index, uint32_t after);
  void publishCelebrateBonus(size_t kid, uint32_t points, uint32_t after);
  void publishCelebrateBehaviour(size_t kid, int32_t points, const char *title, const char *message,
                                 uint32_t after);
  void publishAll();
  void publishConfig();
  bool connected() { return client_.connected(); }
  void handleMessage(char *topic, byte *payload, unsigned int length);

private:
  WiFiClient wifi_;
  PubSubClient client_;
  JobState *state_ = nullptr;
  RewardEngine *rewards_ = nullptr;
  uint32_t last_reconnect_ms_ = 0;
  uint32_t last_repub_ms_ = 0;
  // Never publish from inside the PubSubClient callback — it freezes the RGB
  // panel (LVGL starved) and can leave the screen looking mirrored/shifted.
  bool pending_publish_all_ = false;
  bool pending_publish_config_ = false;
  bool pending_publish_catalog_ = false;
  bool pending_publish_rules_ = false;

  void ensureConnected();
  void mqttYieldUi();
  void flushPendingPublishes();
  void publishSummaries();
  String topicJob(size_t kid, size_t job) const;
  String topicInternet(size_t kid) const;
  String topicProgress(size_t kid) const;
  String topicRewardTally(size_t kid) const;
};
