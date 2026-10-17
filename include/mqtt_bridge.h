#pragma once

#include "job_state.h"
#include <PubSubClient.h>
#include <WiFi.h>

class MqttBridge {
public:
  void begin(JobState *state);
  void loop();
  void publishJob(size_t kid, size_t job);
  void publishKidSummary(size_t kid);
  void publishRewardTally(size_t kid);
  void publishRewardEvent(size_t doer, size_t owner, size_t job);
  void publishAll();
  bool connected() { return client_.connected(); }
  void handleMessage(char *topic, byte *payload, unsigned int length);

private:
  WiFiClient wifi_;
  PubSubClient client_;
  JobState *state_ = nullptr;
  uint32_t last_reconnect_ms_ = 0;
  uint32_t last_repub_ms_ = 0;

  void ensureConnected();
  String topicJob(size_t kid, size_t job) const;
  String topicInternet(size_t kid) const;
  String topicProgress(size_t kid) const;
  String topicRewardTally(size_t kid) const;
};
