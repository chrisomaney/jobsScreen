#pragma once

#include "reward_state.h"
#include <PubSubClient.h>
#include <WiFi.h>

class MqttBridge {
public:
  void begin(RewardState *state);
  void loop();
  void publishAll();
  void publishTally(size_t kid);
  void publishCatalog();
  void publishClaim(size_t kid, size_t prize_index, uint32_t before, uint32_t after);
  void publishBonus(size_t kid, uint32_t points, uint32_t before, uint32_t after);
  void publishCelebrateWinner(size_t kid, size_t prize_index, uint32_t after);
  void publishCelebrateBonus(size_t kid, uint32_t points, uint32_t after);
  bool connected() { return client_.connected(); }
  void handleMessage(char *topic, byte *payload, unsigned int length);

private:
  WiFiClient wifi_;
  PubSubClient client_;
  RewardState *state_ = nullptr;
  uint32_t last_reconnect_ms_ = 0;
  uint32_t last_repub_ms_ = 0;

  void ensureConnected();
  String topicTally(size_t kid) const;
};
