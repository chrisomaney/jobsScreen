#include "ui.h"

#include <WiFi.h>

#include "clock_util.h"
#include "display_board.h"

AppUi *AppUi::self_ = nullptr;

namespace {

lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, c(color), 0);
  return lbl;
}

size_t countJobsInCategory(JobCategory cat) {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JOBS[j].category == cat) {
      ++n;
    }
  }
  return n;
}

size_t countDoneInCategory(const JobState *state, size_t kid, JobCategory cat) {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JOBS[j].category == cat && state->isDone(kid, j)) {
      ++n;
    }
  }
  return n;
}

// Pack owner+job into pointer-sized user data
void *packOwnerJob(size_t owner, size_t job) {
  return reinterpret_cast<void *>((owner << 16) | (job & 0xFFFF));
}

void unpackOwnerJob(void *p, size_t *owner, size_t *job) {
  const uintptr_t v = reinterpret_cast<uintptr_t>(p);
  *owner = static_cast<size_t>(v >> 16);
  *job = static_cast<size_t>(v & 0xFFFF);
}

}  // namespace

void AppUi::begin(JobState *state, MqttBridge *mqtt) {
  self_ = this;
  state_ = state;
  mqtt_ = mqtt;
  seen_revision_ = state_->revision();
  buildHome();
  buildKidScaffold();
  buildNotDoneScaffold();
  buildDoerPicker();
  showHome();
}

void AppUi::styleTab(lv_obj_t *btn, bool active, uint32_t accent) {
  lv_obj_set_style_bg_color(btn, c(active ? accent : COL_BTN), 0);
  lv_obj_set_style_border_width(btn, active ? 0 : 2, 0);
  lv_obj_set_style_border_color(btn, c(accent), 0);
}

void AppUi::buildHome() {
  screen_home_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen_home_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_home_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_home_, LV_OBJ_FLAG_SCROLLABLE);

  make_label(screen_home_, "Jobs Sign-off", &lv_font_montserrat_28, COL_TEXT);
  lv_obj_align(lv_obj_get_child(screen_home_, 0), LV_ALIGN_TOP_MID, 0, 8);

  status_label_ = make_label(screen_home_, "Connecting…", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 16, 46);

  clock_label_ = make_label(screen_home_, "", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(clock_label_, LV_ALIGN_TOP_RIGHT, -16, 46);

  const int card_w = 360;
  const int card_h = 130;
  const int gap = 16;
  const int origin_y = 74;

  for (size_t i = 0; i < KID_COUNT; ++i) {
    const int col = static_cast<int>(i % 2);
    const int row = static_cast<int>(i / 2);
    lv_obj_t *card = lv_btn_create(screen_home_);
    lv_obj_set_size(card, card_w, card_h);
    lv_obj_set_pos(card, 32 + col * (card_w + gap), origin_y + row * (card_h + gap));
    lv_obj_set_style_bg_color(card, c(COL_CARD), 0);
    lv_obj_set_style_bg_color(card, c(0x2F445C), LV_STATE_PRESSED);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_add_event_cb(card, onKidCard, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));

    lv_obj_t *name = make_label(card, KIDS[i].name, &lv_font_montserrat_28, COL_TEXT);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 22);

    home_remain_labels_[i] = make_label(card, "", &lv_font_montserrat_14, COL_MUTED);
    lv_obj_align(home_remain_labels_[i], LV_ALIGN_BOTTOM_MID, 0, -18);
  }

  leftover_btn_ = lv_btn_create(screen_home_);
  lv_obj_set_size(leftover_btn_, 736, 72);
  lv_obj_align(leftover_btn_, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_radius(leftover_btn_, 14, 0);
  lv_obj_set_style_shadow_width(leftover_btn_, 0, 0);
  lv_obj_set_style_border_width(leftover_btn_, 0, 0);
  lv_obj_add_event_cb(leftover_btn_, onNotDoneCard, LV_EVENT_CLICKED, nullptr);

  leftover_title_ = make_label(leftover_btn_, "Jobs left to do", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_align(leftover_title_, LV_ALIGN_LEFT_MID, 24, -8);
  leftover_hint_ = make_label(leftover_btn_, "", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(leftover_hint_, LV_ALIGN_LEFT_MID, 24, 16);

  updateHomeRemainders();
  updateLeftoverHomeButton();
}

void AppUi::buildKidScaffold() {
  screen_kid_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen_kid_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_kid_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_kid_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back = lv_btn_create(screen_kid_);
  lv_obj_set_size(back, 96, 40);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 10, 8);
  lv_obj_set_style_bg_color(back, c(COL_BTN), 0);
  lv_obj_set_style_radius(back, 10, 0);
  lv_obj_set_style_shadow_width(back, 0, 0);
  lv_obj_add_event_cb(back, onBackHome, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_lbl = make_label(back, "< Home", &lv_font_montserrat_14, COL_TEXT);
  lv_obj_center(back_lbl);

  kid_title_ = make_label(screen_kid_, "", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_align(kid_title_, LV_ALIGN_TOP_MID, 0, 14);

  lv_obj_t *reset = lv_btn_create(screen_kid_);
  lv_obj_set_size(reset, 100, 36);
  lv_obj_align(reset, LV_ALIGN_TOP_RIGHT, -176, 10);
  lv_obj_set_style_bg_color(reset, c(0x7F1D1D), 0);
  lv_obj_set_style_radius(reset, 10, 0);
  lv_obj_set_style_shadow_width(reset, 0, 0);
  lv_obj_add_event_cb(reset, onResetKid, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *reset_lbl = make_label(reset, "Reset day", &lv_font_montserrat_14, COL_TEXT);
  lv_obj_center(reset_lbl);

  internet_badge_ = lv_obj_create(screen_kid_);
  lv_obj_set_size(internet_badge_, 150, 36);
  lv_obj_align(internet_badge_, LV_ALIGN_TOP_RIGHT, -12, 10);
  lv_obj_set_style_radius(internet_badge_, 18, 0);
  lv_obj_set_style_border_width(internet_badge_, 0, 0);
  lv_obj_set_style_bg_color(internet_badge_, c(COL_BTN), 0);
  lv_obj_clear_flag(internet_badge_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *badge_lbl = make_label(internet_badge_, "Internet OFF", &lv_font_montserrat_14, COL_TEXT);
  lv_obj_center(badge_lbl);

  tab_expect_ = lv_btn_create(screen_kid_);
  lv_obj_set_size(tab_expect_, 240, 42);
  lv_obj_align(tab_expect_, LV_ALIGN_TOP_LEFT, 10, 56);
  lv_obj_set_style_radius(tab_expect_, 10, 0);
  lv_obj_set_style_shadow_width(tab_expect_, 0, 0);
  lv_obj_add_event_cb(tab_expect_, onTabExpect, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *te = make_label(tab_expect_, "Expectations", &lv_font_montserrat_16, COL_TEXT);
  lv_obj_center(te);

  tab_jobs_ = lv_btn_create(screen_kid_);
  lv_obj_set_size(tab_jobs_, 240, 42);
  lv_obj_align(tab_jobs_, LV_ALIGN_TOP_LEFT, 260, 56);
  lv_obj_set_style_radius(tab_jobs_, 10, 0);
  lv_obj_set_style_shadow_width(tab_jobs_, 0, 0);
  lv_obj_add_event_cb(tab_jobs_, onTabJobs, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *tj = make_label(tab_jobs_, "Jobs", &lv_font_montserrat_16, COL_TEXT);
  lv_obj_center(tj);

  progress_bar_ = lv_bar_create(screen_kid_);
  lv_obj_set_size(progress_bar_, 270, 14);
  lv_obj_align(progress_bar_, LV_ALIGN_TOP_RIGHT, -12, 62);
  lv_bar_set_range(progress_bar_, 0, 100);
  lv_obj_set_style_bg_color(progress_bar_, c(COL_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress_bar_, c(COL_DONE), LV_PART_INDICATOR);
  lv_obj_set_style_radius(progress_bar_, 7, LV_PART_MAIN);
  lv_obj_set_style_radius(progress_bar_, 7, LV_PART_INDICATOR);

  progress_label_ = make_label(screen_kid_, "", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(progress_label_, LV_ALIGN_TOP_RIGHT, -12, 80);

  job_list_ = lv_obj_create(screen_kid_);
  lv_obj_set_size(job_list_, 776, 360);
  lv_obj_align(job_list_, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(job_list_, c(COL_BG), 0);
  lv_obj_set_style_border_width(job_list_, 0, 0);
  lv_obj_set_style_pad_all(job_list_, 4, 0);
  lv_obj_set_flex_flow(job_list_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(job_list_, 6, 0);
  lv_obj_set_scrollbar_mode(job_list_, LV_SCROLLBAR_MODE_AUTO);
}

void AppUi::buildNotDoneScaffold() {
  screen_not_done_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen_not_done_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_not_done_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_not_done_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back = lv_btn_create(screen_not_done_);
  lv_obj_set_size(back, 96, 40);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 10, 8);
  lv_obj_set_style_bg_color(back, c(COL_BTN), 0);
  lv_obj_set_style_radius(back, 10, 0);
  lv_obj_set_style_shadow_width(back, 0, 0);
  lv_obj_add_event_cb(back, onBackHome, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_lbl = make_label(back, "< Home", &lv_font_montserrat_14, COL_TEXT);
  lv_obj_center(back_lbl);

  make_label(screen_not_done_, "Jobs left to do", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(lv_obj_get_child(screen_not_done_, 1), LV_ALIGN_TOP_MID, 0, 12);

  not_done_summary_ = make_label(screen_not_done_, "", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(not_done_summary_, LV_ALIGN_TOP_MID, 0, 46);

  not_done_list_ = lv_obj_create(screen_not_done_);
  lv_obj_set_size(not_done_list_, 776, 400);
  lv_obj_align(not_done_list_, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(not_done_list_, c(COL_BG), 0);
  lv_obj_set_style_border_width(not_done_list_, 0, 0);
  lv_obj_set_style_pad_all(not_done_list_, 4, 0);
  lv_obj_set_flex_flow(not_done_list_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(not_done_list_, 6, 0);
  lv_obj_set_scrollbar_mode(not_done_list_, LV_SCROLLBAR_MODE_AUTO);
}

void AppUi::buildDoerPicker() {
  screen_doer_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen_doer_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_doer_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_doer_, LV_OBJ_FLAG_SCROLLABLE);

  doer_prompt_ = make_label(screen_doer_, "Who did this job?", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(doer_prompt_, LV_ALIGN_TOP_MID, 0, 40);

  const int btn_w = 340;
  const int btn_h = 110;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    const int col = static_cast<int>(i % 2);
    const int row = static_cast<int>(i / 2);
    lv_obj_t *btn = lv_btn_create(screen_doer_);
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, 50 + col * (btn_w + 20), 110 + row * (btn_h + 20));
    lv_obj_set_style_bg_color(btn, c(COL_CARD), 0);
    lv_obj_set_style_bg_color(btn, c(0x2F445C), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, onDoerChosen, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
    lv_obj_t *n = make_label(btn, KIDS[i].name, &lv_font_montserrat_28, COL_TEXT);
    lv_obj_center(n);
  }

  lv_obj_t *cancel = lv_btn_create(screen_doer_);
  lv_obj_set_size(cancel, 200, 48);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_set_style_bg_color(cancel, c(COL_BTN), 0);
  lv_obj_set_style_radius(cancel, 12, 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_add_event_cb(cancel, onDoerCancel, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cl = make_label(cancel, "Cancel", &lv_font_montserrat_16, COL_TEXT);
  lv_obj_center(cl);
}

void AppUi::updateLeftoverHomeButton() {
  if (!leftover_btn_) {
    return;
  }
  const bool open = clock_leftover_screen_open();
  if (open) {
    lv_obj_set_style_bg_color(leftover_btn_, c(0x7C2D12), 0);
    lv_obj_clear_state(leftover_btn_, LV_STATE_DISABLED);
    lv_obj_add_flag(leftover_btn_, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(leftover_title_, "Jobs left to do");
    lv_label_set_text(leftover_hint_, "anyone can help — then tap who did it (rewards)");
  } else {
    lv_obj_set_style_bg_color(leftover_btn_, c(0x3F3F46), 0);
    lv_obj_add_state(leftover_btn_, LV_STATE_DISABLED);
    lv_obj_clear_flag(leftover_btn_, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(leftover_title_, "Jobs left to do");
    const int mins = clock_minutes_until_leftover_open();
    char buf[64];
    if (!clock_is_synced()) {
      snprintf(buf, sizeof(buf), "opens at %02d:%02d — waiting for clock sync", LEFTOVER_OPENS_HOUR,
               LEFTOVER_OPENS_MINUTE);
    } else if (mins > 0) {
      snprintf(buf, sizeof(buf), "locked until %02d:%02d (%d min)", LEFTOVER_OPENS_HOUR,
               LEFTOVER_OPENS_MINUTE, mins);
    } else {
      snprintf(buf, sizeof(buf), "opens at %02d:%02d", LEFTOVER_OPENS_HOUR, LEFTOVER_OPENS_MINUTE);
    }
    lv_label_set_text(leftover_hint_, buf);
  }
}

void AppUi::updateHomeRemainders() {
  for (size_t i = 0; i < KID_COUNT; ++i) {
    if (!home_remain_labels_[i]) {
      continue;
    }
    const size_t left = JOB_COUNT - state_->doneCount(i);
    char buf[64];
    if (left == 0) {
      snprintf(buf, sizeof(buf), "all done — internet on");
      lv_obj_set_style_text_color(home_remain_labels_[i], c(COL_DONE), 0);
    } else {
      snprintf(buf, sizeof(buf), "%u left  •  rewards %lu", static_cast<unsigned>(left),
               static_cast<unsigned long>(state_->rewardTally(i)));
      lv_obj_set_style_text_color(home_remain_labels_[i], c(COL_MUTED), 0);
    }
    lv_label_set_text(home_remain_labels_[i], buf);
  }
}

void AppUi::rebuildJobList() {
  lv_obj_clean(job_list_);
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    done_btns_[j] = nullptr;
    job_rows_[j] = nullptr;
  }

  const JobCategory cat =
      (active_page_ == KidPage::Expectations) ? JobCategory::Expectation : JobCategory::Paid;
  const uint32_t accent = (active_page_ == KidPage::Expectations) ? COL_EXPECT : COL_PAID;

  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JOBS[j].category != cat) {
      continue;
    }

    lv_obj_t *row = lv_obj_create(job_list_);
    job_rows_[j] = row;
    lv_obj_set_size(row, 760, 56);
    lv_obj_set_style_bg_color(row, c(COL_CARD), 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(row, 4, 0);
    lv_obj_set_style_border_color(row, c(accent), 0);
    lv_obj_set_style_pad_left(row, 14, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = make_label(row, JOBS[j].label, &lv_font_montserrat_18, COL_TEXT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, JOBS[j].detail[0] ? -9 : 0);

    if (JOBS[j].detail[0]) {
      lv_obj_t *det = make_label(row, JOBS[j].detail, &lv_font_montserrat_14, COL_MUTED);
      lv_label_set_long_mode(det, LV_LABEL_LONG_DOT);
      lv_obj_set_width(det, 520);
      lv_obj_align(det, LV_ALIGN_LEFT_MID, 0, 12);
    }

    lv_obj_t *btn = lv_btn_create(row);
    done_btns_[j] = btn;
    lv_obj_set_size(btn, 120, 42);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, onDone, LV_EVENT_CLICKED, reinterpret_cast<void *>(j));
    lv_obj_t *bl = make_label(btn, "Done", &lv_font_montserrat_16, COL_TEXT);
    lv_obj_center(bl);

    updateJobRow(j);
  }
}

void AppUi::updateJobRow(size_t job) {
  if (job >= JOB_COUNT || !done_btns_[job]) {
    return;
  }
  const bool done = state_->isDone(active_kid_, job);
  lv_obj_t *btn = done_btns_[job];
  lv_obj_t *lbl = lv_obj_get_child(btn, 0);
  if (done) {
    lv_obj_set_style_bg_color(btn, c(COL_BTN_ACTIVE), 0);
    if (lbl) {
      lv_label_set_text(lbl, "Done");
    }
    if (job_rows_[job]) {
      lv_obj_set_style_bg_color(job_rows_[job], c(0x1F3A2E), 0);
    }
  } else {
    lv_obj_set_style_bg_color(btn, c(COL_BTN), 0);
    if (lbl) {
      lv_label_set_text(lbl, "Done");
    }
    if (job_rows_[job]) {
      lv_obj_set_style_bg_color(job_rows_[job], c(COL_CARD), 0);
    }
  }
}

void AppUi::updateKidHeader() {
  const bool expectations = (active_page_ == KidPage::Expectations);
  lv_label_set_text(kid_title_, KIDS[active_kid_].name);

  styleTab(tab_expect_, expectations, COL_EXPECT);
  styleTab(tab_jobs_, !expectations, COL_PAID);

  const JobCategory cat = expectations ? JobCategory::Expectation : JobCategory::Paid;
  const size_t cat_done = countDoneInCategory(state_, active_kid_, cat);
  const size_t cat_total = countJobsInCategory(cat);
  const int pct = cat_total ? static_cast<int>((cat_done * 100) / cat_total) : 0;
  lv_bar_set_value(progress_bar_, pct, LV_ANIM_OFF);

  char buf[64];
  snprintf(buf, sizeof(buf), "%u/%u on this screen", static_cast<unsigned>(cat_done),
           static_cast<unsigned>(cat_total));
  lv_label_set_text(progress_label_, buf);

  const bool ready = state_->internetReady(active_kid_);
  lv_obj_set_style_bg_color(internet_badge_, c(ready ? COL_DONE : COL_BTN), 0);
  lv_obj_t *badge_lbl = lv_obj_get_child(internet_badge_, 0);
  if (badge_lbl) {
    lv_label_set_text(badge_lbl, ready ? "Internet ON" : "Internet OFF");
  }
}

void AppUi::rebuildNotDoneList() {
  lv_obj_clean(not_done_list_);

  size_t total_left = 0;
  for (size_t k = 0; k < KID_COUNT; ++k) {
    total_left += (JOB_COUNT - state_->doneCount(k));
  }

  char summary[80];
  if (total_left == 0) {
    snprintf(summary, sizeof(summary), "Everyone is clear — internet unlocked for all");
  } else {
    snprintf(summary, sizeof(summary),
             "%u left — tap Done, then who did it (rewards). Unlock = own list clear.",
             static_cast<unsigned>(total_left));
  }
  lv_label_set_text(not_done_summary_, summary);

  if (total_left == 0) {
    lv_obj_t *ok = lv_obj_create(not_done_list_);
    lv_obj_set_size(ok, 760, 80);
    lv_obj_set_style_bg_color(ok, c(0x1F3A2E), 0);
    lv_obj_set_style_radius(ok, 12, 0);
    lv_obj_set_style_border_width(ok, 0, 0);
    lv_obj_clear_flag(ok, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *msg = make_label(ok, "Nothing left — nice work", &lv_font_montserrat_18, COL_DONE);
    lv_obj_center(msg);
    return;
  }

  for (size_t k = 0; k < KID_COUNT; ++k) {
    const size_t left = JOB_COUNT - state_->doneCount(k);
    if (left == 0) {
      continue;
    }

    lv_obj_t *hdr = lv_obj_create(not_done_list_);
    lv_obj_set_size(hdr, 760, 36);
    lv_obj_set_style_bg_color(hdr, c(COL_CARD), 0);
    lv_obj_set_style_radius(hdr, 8, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    char hdr_txt[64];
    snprintf(hdr_txt, sizeof(hdr_txt), "%s's list — %u left (internet locked)", KIDS[k].name,
             static_cast<unsigned>(left));
    lv_obj_t *hl = make_label(hdr, hdr_txt, &lv_font_montserrat_16, COL_TEXT);
    lv_obj_align(hl, LV_ALIGN_LEFT_MID, 12, 0);

    auto add_open = [&](JobCategory cat, uint32_t accent, const char *tag) {
      for (size_t j = 0; j < JOB_COUNT; ++j) {
        if (JOBS[j].category != cat || state_->isDone(k, j)) {
          continue;
        }
        lv_obj_t *row = lv_obj_create(not_done_list_);
        lv_obj_set_size(row, 760, 52);
        lv_obj_set_style_bg_color(row, c(COL_CARD), 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(row, 4, 0);
        lv_obj_set_style_border_color(row, c(accent), 0);
        lv_obj_set_style_pad_left(row, 12, 0);
        lv_obj_set_style_pad_right(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        char line[96];
        snprintf(line, sizeof(line), "[%s]  %s", tag, JOBS[j].label);
        lv_obj_t *lbl = make_label(row, line, &lv_font_montserrat_16, COL_TEXT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_set_size(btn, 120, 40);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn, c(COL_BTN), 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, onLeftoverDone, LV_EVENT_CLICKED, packOwnerJob(k, j));
        lv_obj_t *bl = make_label(btn, "Done", &lv_font_montserrat_16, COL_TEXT);
        lv_obj_center(bl);
      }
    };

    add_open(JobCategory::Expectation, COL_EXPECT, "Expectation");
    add_open(JobCategory::Paid, COL_PAID, "Job");
  }
}

void AppUi::showDoerPicker(size_t owner, size_t job) {
  awaiting_doer_ = true;
  pending_owner_ = owner;
  pending_job_ = job;
  char buf[96];
  snprintf(buf, sizeof(buf), "Who did %s's \"%s\"?", KIDS[owner].name, JOBS[job].label);
  lv_label_set_text(doer_prompt_, buf);
  lv_scr_load(screen_doer_);
}

void AppUi::completeLeftover(size_t doer) {
  const size_t owner = pending_owner_;
  const size_t job = pending_job_;
  awaiting_doer_ = false;

  const bool was_ready = state_->internetReady(owner);
  state_->setDone(owner, job, true);
  state_->addReward(doer);

  mqtt_->publishJob(owner, job);
  mqtt_->publishRewardEvent(doer, owner, job);
  mqtt_->publishKidSummary(owner);
  if (!was_ready && state_->internetReady(owner)) {
    // Edge: owner just unlocked — HA listens to kids/{owner}/internet = on
    mqtt_->publishKidSummary(owner);
  }

  showNotDone();
}

void AppUi::showHome() {
  awaiting_doer_ = false;
  screen_kind_ = ScreenKind::Home;
  updateHomeRemainders();
  updateLeftoverHomeButton();
  lv_scr_load(screen_home_);
}

void AppUi::showKidPage(size_t kid, KidPage page) {
  if (kid >= KID_COUNT) {
    return;
  }
  awaiting_doer_ = false;
  active_kid_ = kid;
  active_page_ = page;
  screen_kind_ = ScreenKind::Kid;
  rebuildJobList();
  updateKidHeader();
  lv_scr_load(screen_kid_);
}

void AppUi::showNotDone() {
  if (!clock_leftover_screen_open()) {
    showHome();
    return;
  }
  awaiting_doer_ = false;
  screen_kind_ = ScreenKind::NotDone;
  rebuildNotDoneList();
  lv_scr_load(screen_not_done_);
}

void AppUi::refreshActiveScreen() {
  switch (screen_kind_) {
    case ScreenKind::Home:
      updateHomeRemainders();
      updateLeftoverHomeButton();
      break;
    case ScreenKind::Kid:
      updateKidHeader();
      for (size_t j = 0; j < JOB_COUNT; ++j) {
        updateJobRow(j);
      }
      break;
    case ScreenKind::NotDone:
      rebuildNotDoneList();
      break;
  }
}

void AppUi::tick() {
  const uint32_t rev = state_->revision();
  if (rev != seen_revision_) {
    seen_revision_ = rev;
    if (!awaiting_doer_) {
      refreshActiveScreen();
    }
  }

  static uint32_t last_clock_ms = 0;
  if (millis() - last_clock_ms > 1000) {
    last_clock_ms = millis();
    if (clock_label_) {
      char tbuf[24];
      clock_format_now(tbuf, sizeof(tbuf));
      lv_label_set_text(clock_label_, tbuf);
    }
    if (screen_kind_ == ScreenKind::Home) {
      updateLeftoverHomeButton();
    }
    // Kick viewers off leftover if somehow still open before 7:40 (e.g. clock jumped)
    if (screen_kind_ == ScreenKind::NotDone && !clock_leftover_screen_open() && !awaiting_doer_) {
      showHome();
    }
  }

  if (status_label_) {
    const char *msg = mqtt_->connected() ? "MQTT online" : "MQTT connecting…";
    if (WiFi.status() != WL_CONNECTED) {
      msg = "Wi-Fi connecting…";
    }
    lv_label_set_text(status_label_, msg);
  }
}

void AppUi::onKidCard(lv_event_t *e) {
  const size_t kid = reinterpret_cast<size_t>(lv_event_get_user_data(e));
  self_->showKidPage(kid, KidPage::Expectations);
}

void AppUi::onNotDoneCard(lv_event_t *e) {
  (void)e;
  if (!clock_leftover_screen_open()) {
    return;
  }
  self_->showNotDone();
}

void AppUi::onBackHome(lv_event_t *e) {
  (void)e;
  self_->showHome();
}

void AppUi::onTabExpect(lv_event_t *e) {
  (void)e;
  self_->showKidPage(self_->active_kid_, KidPage::Expectations);
}

void AppUi::onTabJobs(lv_event_t *e) {
  (void)e;
  self_->showKidPage(self_->active_kid_, KidPage::Jobs);
}

void AppUi::onDone(lv_event_t *e) {
  static uint32_t last_ms = 0;
  const uint32_t now = millis();
  if (now - last_ms < DONE_DEBOUNCE_MS) {
    return;
  }
  last_ms = now;

  const size_t job = reinterpret_cast<size_t>(lv_event_get_user_data(e));
  const size_t kid = self_->active_kid_;
  const bool now_done = !self_->state_->isDone(kid, job);
  self_->state_->setDone(kid, job, now_done);
  self_->mqtt_->publishJob(kid, job);
  self_->mqtt_->publishKidSummary(kid);
  self_->updateJobRow(job);
  self_->updateKidHeader();
}

void AppUi::onResetKid(lv_event_t *e) {
  (void)e;
  const size_t kid = self_->active_kid_;
  self_->state_->resetKid(kid);
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    self_->mqtt_->publishJob(kid, j);
  }
  self_->mqtt_->publishKidSummary(kid);
  self_->rebuildJobList();
  self_->updateKidHeader();
}

void AppUi::onLeftoverDone(lv_event_t *e) {
  static uint32_t last_ms = 0;
  const uint32_t now = millis();
  if (now - last_ms < DONE_DEBOUNCE_MS) {
    return;
  }
  last_ms = now;

  size_t owner = 0;
  size_t job = 0;
  unpackOwnerJob(lv_event_get_user_data(e), &owner, &job);
  self_->showDoerPicker(owner, job);
}

void AppUi::onDoerChosen(lv_event_t *e) {
  const size_t doer = reinterpret_cast<size_t>(lv_event_get_user_data(e));
  if (!self_->awaiting_doer_) {
    return;
  }
  self_->completeLeftover(doer);
}

void AppUi::onDoerCancel(lv_event_t *e) {
  (void)e;
  self_->awaiting_doer_ = false;
  self_->showNotDone();
}
