#pragma once

#include "mqtt_bridge.h"
#include "reward_state.h"
#include <lvgl.h>

class AppUi {
public:
  void begin(RewardState *state, MqttBridge *mqtt);
  void tick();

private:
  enum class Mode : uint8_t { Ads, Picker, Splash };

  RewardState *state_ = nullptr;
  MqttBridge *mqtt_ = nullptr;
  uint32_t seen_revision_ = 0;
  Mode mode_ = Mode::Ads;
  size_t ad_index_ = 0;
  size_t pending_prize_ = 0;
  uint32_t last_carousel_ms_ = 0;
  uint32_t splash_until_ms_ = 0;
  uint32_t last_claim_ms_ = 0;

  lv_obj_t *screen_ = nullptr;
  lv_obj_t *ad_layer_ = nullptr;
  lv_obj_t *prize_name_ = nullptr;
  lv_obj_t *prize_cost_ = nullptr;
  lv_obj_t *score_ticker_ = nullptr;
  lv_obj_t *hint_ = nullptr;

  lv_obj_t *picker_layer_ = nullptr;
  lv_obj_t *picker_title_ = nullptr;

  lv_obj_t *splash_layer_ = nullptr;
  lv_obj_t *splash_title_ = nullptr;
  lv_obj_t *splash_sub_ = nullptr;
  lv_obj_t *splash_kid_ = nullptr;

  void build();
  void showAds();
  void showPicker(size_t prize_index);
  void showSplashFromState();
  void refreshAd();
  void refreshTicker();
  void setMode(Mode m);

  static void onAdClick(lv_event_t *e);
  static void onKidClick(lv_event_t *e);
  static void onCancelClick(lv_event_t *e);
  static AppUi *self_;
};
