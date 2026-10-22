#pragma once

#include "job_state.h"
#include "mqtt_bridge.h"
#include <lvgl.h>

enum class KidPage : uint8_t {
  Expectations = 0,
  Jobs = 1,
};

class AppUi {
public:
  void begin(JobState *state, MqttBridge *mqtt);
  void showIdle();
  void showHome();
  void showKidPage(size_t kid, KidPage page);
  void showNotDone();
  void refreshActiveScreen();
  void tick();

private:
  enum class ScreenKind : uint8_t {
    Idle,
    Home,
    Kid,
    NotDone,
  };

  JobState *state_ = nullptr;
  MqttBridge *mqtt_ = nullptr;
  size_t active_kid_ = 0;
  KidPage active_page_ = KidPage::Expectations;
  ScreenKind screen_kind_ = ScreenKind::Idle;
  uint32_t seen_revision_ = 0;
  uint32_t seen_config_revision_ = 0;
  uint32_t last_done_ms_[JOB_COUNT]{};
  uint32_t last_leftover_ms_ = 0;

  bool awaiting_doer_ = false;
  size_t pending_owner_ = 0;
  size_t pending_job_ = 0;

  lv_obj_t *screen_idle_ = nullptr;
  lv_obj_t *screen_home_ = nullptr;
  lv_obj_t *screen_kid_ = nullptr;
  lv_obj_t *screen_not_done_ = nullptr;
  lv_obj_t *screen_doer_ = nullptr;

  lv_img_dsc_t idle_img_dsc_{};
  lv_obj_t *idle_img_ = nullptr;

  lv_obj_t *status_label_ = nullptr;
  lv_obj_t *clock_label_ = nullptr;
  lv_obj_t *home_remain_labels_[KID_COUNT]{};
  lv_obj_t *leftover_btn_ = nullptr;
  lv_obj_t *leftover_title_ = nullptr;
  lv_obj_t *leftover_hint_ = nullptr;

  lv_obj_t *kid_title_ = nullptr;
  lv_obj_t *progress_bar_ = nullptr;
  lv_obj_t *progress_label_ = nullptr;
  lv_obj_t *internet_badge_ = nullptr;
  lv_obj_t *tab_expect_ = nullptr;
  lv_obj_t *tab_jobs_ = nullptr;
  lv_obj_t *job_list_ = nullptr;

  lv_obj_t *not_done_list_ = nullptr;
  lv_obj_t *not_done_summary_ = nullptr;
  lv_obj_t *doer_prompt_ = nullptr;
  lv_obj_t *doer_btns_[KID_COUNT]{};
  lv_obj_t *badge_label_ = nullptr;

  lv_obj_t *done_btns_[JOB_COUNT]{};
  lv_obj_t *job_rows_[JOB_COUNT]{};

  void buildIdle();
  void buildHome();
  void buildKidScaffold();
  void buildNotDoneScaffold();
  void buildDoerPicker();
  void rebuildJobList();
  void rebuildNotDoneList();
  void updateJobRow(size_t job);
  void updateKidHeader();
  void updateHomeRemainders();
  void updateLeftoverHomeButton();
  void styleTab(lv_obj_t *btn, bool active, uint32_t accent);
  void showDoerPicker(size_t owner, size_t job);
  void completeLeftover(size_t doer);
  void applyBacklightForScreen();
  bool isActiveSession() const;
  void publishKid(size_t kid);
  bool debounce(uint32_t *last_ms);

  static void onLetsGo(lv_event_t *e);
  static void onKidCard(lv_event_t *e);
  static void onNotDoneCard(lv_event_t *e);
  static void onBackHome(lv_event_t *e);
  static void onTabExpect(lv_event_t *e);
  static void onTabJobs(lv_event_t *e);
  static void onDone(lv_event_t *e);
  static void onUndo(lv_event_t *e);
  static void onResetKid(lv_event_t *e);
  static void onLeftoverDone(lv_event_t *e);
  static void onDoerChosen(lv_event_t *e);
  static void onDoerCancel(lv_event_t *e);

  static AppUi *self_;
};
