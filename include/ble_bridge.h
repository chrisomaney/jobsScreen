#pragma once

#include "job_state.h"
#include "mqtt_bridge.h"

// BLE fallback so the iOS Kids Admin app can set jobs when MQTT is down.
// Advertises as "KidsPanel" with a config CMD/STATUS GATT pair.
class BleBridge {
public:
  void begin(JobState *state, MqttBridge *mqtt);
  void loop();

private:
  JobState *state_ = nullptr;
  MqttBridge *mqtt_ = nullptr;
  bool started_ = false;
  uint32_t last_keepalive_ms_ = 0;
};
