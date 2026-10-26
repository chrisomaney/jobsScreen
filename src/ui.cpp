#include "ui.h"

#include <WiFi.h>
#include <string.h>

#include "clock_util.h"
#include "display_board.h"
#include "job_icons.h"

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

void strip_chrome(lv_obj_t *o) {
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
}

void style_space_bg(lv_obj_t *o) {
  strip_chrome(o);
  lv_obj_set_style_bg_color(o, c(COL_PRIZE_BG), 0);
  lv_obj_set_style_bg_grad_color(o, c(COL_PRIZE_BG2), 0);
  lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_stop(o, 0, 0);
  lv_obj_set_style_bg_grad_stop(o, 255, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, lv_opa_t opa) {
  lv_obj_t *bar = lv_obj_create(parent);
  strip_chrome(bar);
  lv_obj_set_size(bar, w, h);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_style_bg_color(bar, c(color), 0);
  lv_obj_set_style_bg_opa(bar, opa, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return bar;
}

void paint_hud_backdrop(lv_obj_t *parent) {
  make_bar(parent, 0, 0, LCD_H_RES, 64, COL_PRIZE_SKY, LV_OPA_COVER);
  constexpr int floor_y = 400;
  make_bar(parent, 0, floor_y, LCD_H_RES, 2, COL_PRIZE_ACCENT, LV_OPA_50);
  for (int i = 0; i < 9; ++i) {
    make_bar(parent, 40 + i * 90, floor_y, 1, LCD_V_RES - floor_y, COL_PRIZE_LINE, LV_OPA_50);
  }
  make_bar(parent, 16, 12, 36, 3, COL_PRIZE_ACCENT, LV_OPA_COVER);
  make_bar(parent, 16, 12, 3, 28, COL_PRIZE_ACCENT, LV_OPA_COVER);
  make_bar(parent, LCD_H_RES - 52, 12, 36, 3, COL_PRIZE_ACCENT, LV_OPA_COVER);
  make_bar(parent, LCD_H_RES - 19, 12, 3, 28, COL_PRIZE_ACCENT, LV_OPA_COVER);
}

const uint32_t AD_RIMS[] = {0x22D3EE, 0xA78BFA, 0x34D399, 0xF472B6, 0xFBBF24};
const size_t AD_RIM_COUNT = sizeof(AD_RIMS) / sizeof(AD_RIMS[0]);

// 800×480 prize idle layout (jobs button is the big CTA)
constexpr int CARD_Y = 58;
constexpr int CARD_H = 160;
constexpr int WANT_Y = 232;
constexpr int WANT_H = 56;
constexpr int JOBS_Y = 300;
constexpr int JOBS_H = 72;
constexpr int SCORE_Y = 388;
constexpr int SCORE_H = 72;

size_t countJobsInCategory(size_t kid, JobCategory cat) {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JobState::jobCategory(j) == cat && JobState::isAssigned(kid, j)) {
      ++n;
    }
  }
  return n;
}

size_t countDoneInCategory(const JobState *state, size_t kid, JobCategory cat) {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JobState::jobCategory(j) == cat && state->isDone(kid, j)) {
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

void AppUi::begin(JobState *state, MqttBridge *mqtt, RewardEngine *rewards) {
  self_ = this;
  state_ = state;
  mqtt_ = mqtt;
  rewards_ = rewards;
  seen_revision_ = state_->revision();
  seen_config_revision_ = state_->configRevision();
  seen_rewards_revision_ = rewards_ ? rewards_->revision() : 0;
  buildIdle();
  buildHome();
  buildKidScaffold();
  buildNotDoneScaffold();
  buildDoerPicker();
  buildMessageOverlay();
  last_carousel_ms_ = millis();
  showIdle();
}

bool AppUi::debounce(uint32_t *last_ms) {
  const uint32_t now = millis();
  if (*last_ms != 0 && now - *last_ms < DONE_DEBOUNCE_MS) {
    return false;
  }
  *last_ms = now;
  return true;
}

void AppUi::publishKid(size_t kid) {
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    mqtt_->publishJob(kid, j);
  }
  mqtt_->publishKidSummary(kid);
}

bool AppUi::isActiveSession() const {
  return screen_kind_ != ScreenKind::Idle;
}

void AppUi::applyBacklightForScreen() {
  display_board_set_backlight(100);
}

void AppUi::styleTab(lv_obj_t *btn, bool active, uint32_t accent) {
  lv_obj_set_style_bg_color(btn, c(active ? accent : COL_BTN), 0);
  lv_obj_set_style_border_width(btn, active ? 0 : 2, 0);
  lv_obj_set_style_border_color(btn, c(accent), 0);
}

void AppUi::buildIdle() {
  screen_idle_ = lv_obj_create(nullptr);
  style_space_bg(screen_idle_);
  lv_obj_clear_flag(screen_idle_, LV_OBJ_FLAG_SCROLLABLE);

  ad_layer_ = lv_obj_create(screen_idle_);
  style_space_bg(ad_layer_);
  lv_obj_set_size(ad_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_pos(ad_layer_, 0, 0);
  lv_obj_clear_flag(ad_layer_, LV_OBJ_FLAG_SCROLLABLE);
  paint_hud_backdrop(ad_layer_);

  lv_obj_t *brand = make_label(ad_layer_, "PRIZE STATION", &lv_font_montserrat_24, COL_PRIZE_ACCENT);
  lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 18);

  prize_card_ = lv_obj_create(ad_layer_);
  lv_obj_set_size(prize_card_, 720, CARD_H);
  lv_obj_set_pos(prize_card_, (LCD_H_RES - 720) / 2, CARD_Y);
  lv_obj_set_style_bg_color(prize_card_, c(COL_PRIZE_CARD), 0);
  lv_obj_set_style_border_width(prize_card_, 3, 0);
  lv_obj_set_style_border_color(prize_card_, c(COL_PRIZE_ACCENT), 0);
  lv_obj_set_style_outline_width(prize_card_, 0, 0);
  lv_obj_set_style_shadow_width(prize_card_, 0, 0);
  lv_obj_set_style_radius(prize_card_, 18, 0);
  lv_obj_set_style_pad_all(prize_card_, 12, 0);
  lv_obj_clear_flag(prize_card_, LV_OBJ_FLAG_SCROLLABLE);
  make_bar(prize_card_, 0, 0, 720, 5, COL_PRIZE_ACCENT, LV_OPA_COVER);

  prize_name_ = make_label(prize_card_, "", &lv_font_montserrat_28, COL_TEXT);
  lv_label_set_long_mode(prize_name_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(prize_name_, 680);
  lv_obj_set_style_text_align(prize_name_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(prize_name_, LV_ALIGN_TOP_MID, 0, 28);

  prize_cost_ = make_label(prize_card_, "", &lv_font_montserrat_24, COL_PRIZE_COST);
  lv_obj_align(prize_cost_, LV_ALIGN_BOTTOM_MID, 0, -16);

  want_btn_ = lv_btn_create(ad_layer_);
  lv_obj_set_size(want_btn_, 300, WANT_H);
  lv_obj_set_pos(want_btn_, (LCD_H_RES - 300) / 2, WANT_Y);
  lv_obj_set_style_bg_color(want_btn_, c(COL_PRIZE_ACCENT), 0);
  lv_obj_set_style_bg_color(want_btn_, c(COL_PRIZE_ACCENT2), LV_STATE_PRESSED);
  lv_obj_set_style_radius(want_btn_, 12, 0);
  lv_obj_set_style_shadow_width(want_btn_, 0, 0);
  lv_obj_set_style_border_width(want_btn_, 0, 0);
  lv_obj_add_event_cb(want_btn_, onWantPrize, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *want_lbl = make_label(want_btn_, "I WANT THIS", &lv_font_montserrat_24, COL_PRIZE_BG);
  lv_obj_center(want_lbl);

  // Big jobs entry button
  jobs_btn_ = lv_btn_create(ad_layer_);
  lv_obj_set_size(jobs_btn_, 520, JOBS_H);
  lv_obj_set_pos(jobs_btn_, (LCD_H_RES - 520) / 2, JOBS_Y);
  lv_obj_set_style_bg_color(jobs_btn_, c(0xF8FAFC), 0);
  lv_obj_set_style_bg_color(jobs_btn_, c(COL_PRIZE_ACCENT2), LV_STATE_PRESSED);
  lv_obj_set_style_radius(jobs_btn_, 16, 0);
  lv_obj_set_style_shadow_width(jobs_btn_, 0, 0);
  lv_obj_set_style_border_width(jobs_btn_, 3, 0);
  lv_obj_set_style_border_color(jobs_btn_, c(COL_PRIZE_ACCENT), 0);
  lv_obj_add_event_cb(jobs_btn_, onLetsGo, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *jobs_lbl = make_label(jobs_btn_, "JOBS  →", &lv_font_montserrat_28, COL_PRIZE_BG);
  lv_obj_center(jobs_lbl);

  score_row_ = lv_obj_create(ad_layer_);
  strip_chrome(score_row_);
  lv_obj_set_size(score_row_, LCD_H_RES - 16, SCORE_H);
  lv_obj_set_pos(score_row_, 8, SCORE_Y);
  lv_obj_set_style_bg_opa(score_row_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(score_row_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(score_row_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(score_row_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  const int chip_w = 148;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    score_chips_[i] = lv_obj_create(score_row_);
    lv_obj_set_size(score_chips_[i], chip_w, 56);
    lv_obj_set_style_bg_color(score_chips_[i], c(0x0B1220), 0);
    lv_obj_set_style_border_width(score_chips_[i], 2, 0);
    lv_obj_set_style_border_color(score_chips_[i], c(KIDS[i].color), 0);
    lv_obj_set_style_radius(score_chips_[i], 10, 0);
    lv_obj_set_style_pad_all(score_chips_[i], 2, 0);
    lv_obj_clear_flag(score_chips_[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *chip = make_label(score_chips_[i], "", &lv_font_montserrat_14, COL_TEXT);
    lv_label_set_long_mode(chip, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(chip, chip_w - 8);
    lv_obj_set_style_text_align(chip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(chip);
    lv_obj_set_user_data(score_chips_[i], chip);
  }

  picker_layer_ = lv_obj_create(screen_idle_);
  style_space_bg(picker_layer_);
  lv_obj_set_size(picker_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_pos(picker_layer_, 0, 0);
  lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(picker_layer_, LV_OBJ_FLAG_SCROLLABLE);
  paint_hud_backdrop(picker_layer_);

  picker_title_ = make_label(picker_layer_, "SELECT OPERATOR", &lv_font_montserrat_28, COL_PRIZE_ACCENT);
  lv_obj_align(picker_title_, LV_ALIGN_TOP_MID, 0, 16);
  picker_sub_ = make_label(picker_layer_, "", &lv_font_montserrat_18, COL_MUTED);
  lv_obj_align(picker_sub_, LV_ALIGN_TOP_MID, 0, 52);

  const int cols = 3;
  const int btn_w = 230;
  const int btn_h = 96;
  const int gap_x = 18;
  const int gap_y = 14;
  const int start_x = (LCD_H_RES - (cols * btn_w + (cols - 1) * gap_x)) / 2;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    kid_btns_[i] = lv_btn_create(picker_layer_);
    lv_obj_set_size(kid_btns_[i], btn_w, btn_h);
    const int row = static_cast<int>(i) / cols;
    const int col = static_cast<int>(i) % cols;
    lv_obj_set_pos(kid_btns_[i], start_x + col * (btn_w + gap_x), 90 + row * (btn_h + gap_y));
    lv_obj_set_style_bg_color(kid_btns_[i], c(0x0B1220), 0);
    lv_obj_set_style_border_width(kid_btns_[i], 3, 0);
    lv_obj_set_style_border_color(kid_btns_[i], c(KIDS[i].color), 0);
    lv_obj_set_style_radius(kid_btns_[i], 12, 0);
    lv_obj_set_style_shadow_width(kid_btns_[i], 0, 0);
    lv_obj_add_event_cb(kid_btns_[i], onPrizeKid, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
    kid_lbls_[i] = make_label(kid_btns_[i], KIDS[i].name, &lv_font_montserrat_20, COL_TEXT);
    lv_obj_set_style_text_align(kid_lbls_[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(kid_lbls_[i]);
  }

  lv_obj_t *cancel = lv_btn_create(picker_layer_);
  lv_obj_set_size(cancel, 200, 56);
  lv_obj_set_pos(cancel, (LCD_H_RES - 200) / 2, LCD_V_RES - 72);
  lv_obj_set_style_bg_color(cancel, c(COL_PRIZE_BTN_BACK), 0);
  lv_obj_set_style_radius(cancel, 10, 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_set_style_border_width(cancel, 2, 0);
  lv_obj_set_style_border_color(cancel, c(COL_MUTED), 0);
  lv_obj_add_event_cb(cancel, onPrizeCancel, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cl = make_label(cancel, "ABORT", &lv_font_montserrat_18, COL_TEXT);
  lv_obj_center(cl);

  splash_layer_ = lv_obj_create(screen_idle_);
  style_space_bg(splash_layer_);
  lv_obj_set_size(splash_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_pos(splash_layer_, 0, 0);
  lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(splash_layer_, LV_OBJ_FLAG_SCROLLABLE);

  splash_stars_ = make_label(splash_layer_, "* * * * *", &lv_font_montserrat_28, COL_PRIZE_ACCENT);
  lv_obj_align(splash_stars_, LV_ALIGN_TOP_MID, 0, 70);
  splash_title_ = make_label(splash_layer_, "YOU WIN!", &lv_font_montserrat_28, COL_TEXT);
  lv_obj_align(splash_title_, LV_ALIGN_CENTER, 0, -40);
  splash_kid_ = make_label(splash_layer_, "", &lv_font_montserrat_28, COL_PRIZE_ACCENT);
  lv_obj_align(splash_kid_, LV_ALIGN_CENTER, 0, 20);
  splash_sub_ = make_label(splash_layer_, "", &lv_font_montserrat_20, COL_MUTED);
  lv_obj_set_width(splash_sub_, LCD_H_RES - 80);
  lv_label_set_long_mode(splash_sub_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(splash_sub_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(splash_sub_, LV_ALIGN_CENTER, 0, 80);
}

void AppUi::buildMessageOverlay() {
  // Top layer so parent messages appear over prize or jobs screens.
  msg_layer_ = lv_obj_create(lv_layer_top());
  strip_chrome(msg_layer_);
  lv_obj_set_size(msg_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_pos(msg_layer_, 0, 0);
  lv_obj_set_style_bg_color(msg_layer_, c(0x0B1220), 0);
  lv_obj_set_style_bg_opa(msg_layer_, LV_OPA_80, 0);
  lv_obj_add_flag(msg_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(msg_layer_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = lv_obj_create(msg_layer_);
  lv_obj_set_size(card, 640, 260);
  lv_obj_center(card);
  lv_obj_set_style_bg_color(card, c(COL_PRIZE_CARD), 0);
  lv_obj_set_style_border_width(card, 3, 0);
  lv_obj_set_style_border_color(card, c(COL_PRIZE_ACCENT), 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_pad_all(card, 20, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  msg_title_ = make_label(card, "", &lv_font_montserrat_28, COL_PRIZE_ACCENT);
  lv_obj_align(msg_title_, LV_ALIGN_TOP_MID, 0, 8);
  msg_body_ = make_label(card, "", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_width(msg_body_, 580);
  lv_label_set_long_mode(msg_body_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(msg_body_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(msg_body_, LV_ALIGN_CENTER, 0, 20);
}

void AppUi::refreshMessageOverlay() {
  if (!rewards_ || !msg_layer_) {
    return;
  }
  const auto &m = rewards_->panelMessage();
  if (!m.active) {
    lv_obj_add_flag(msg_layer_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_text(msg_title_, m.title);
  lv_label_set_text(msg_body_, m.text);
  lv_obj_clear_flag(msg_layer_, LV_OBJ_FLAG_HIDDEN);
}

void AppUi::buildHome() {
  screen_home_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen_home_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_home_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_home_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *home_title = make_label(screen_home_, "Jobs Sign-off", &lv_font_montserrat_28, COL_TEXT);
  lv_obj_align(home_title, LV_ALIGN_TOP_MID, 0, 8);

  status_label_ = make_label(screen_home_, "Connecting...", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 16, 46);

  clock_label_ = make_label(screen_home_, "", &lv_font_montserrat_14, COL_MUTED);
  lv_obj_align(clock_label_, LV_ALIGN_TOP_RIGHT, -16, 46);

  // 3-across grid so five kids fit above the leftover bar on 800×480
  const int cols = 3;
  const int card_w = 236;
  const int card_h = 110;
  const int gap_x = 14;
  const int gap_y = 12;
  const int origin_x = 32;
  const int origin_y = 74;

  for (size_t i = 0; i < KID_COUNT; ++i) {
    const int col = static_cast<int>(i % cols);
    const int row = static_cast<int>(i / cols);
    lv_obj_t *card = lv_btn_create(screen_home_);
    lv_obj_set_size(card, card_w, card_h);
    lv_obj_set_pos(card, origin_x + col * (card_w + gap_x), origin_y + row * (card_h + gap_y));
    lv_obj_set_style_bg_color(card, c(COL_CARD), 0);
    lv_obj_set_style_bg_color(card, c(0x2F445C), LV_STATE_PRESSED);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_add_event_cb(card, onKidCard, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));

    lv_obj_t *name = make_label(card, KIDS[i].name, &lv_font_montserrat_24, COL_TEXT);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 18);

    home_remain_labels_[i] = make_label(card, "", &lv_font_montserrat_14, COL_MUTED);
    lv_obj_align(home_remain_labels_[i], LV_ALIGN_BOTTOM_MID, 0, -14);
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
  lv_obj_add_event_cb(reset, onResetKid, LV_EVENT_LONG_PRESSED, nullptr);
  lv_obj_t *reset_lbl = make_label(reset, "Hold: reset", &lv_font_montserrat_14, COL_TEXT);
  lv_obj_center(reset_lbl);

  internet_badge_ = lv_obj_create(screen_kid_);
  lv_obj_set_size(internet_badge_, 150, 36);
  lv_obj_align(internet_badge_, LV_ALIGN_TOP_RIGHT, -12, 10);
  lv_obj_set_style_radius(internet_badge_, 18, 0);
  lv_obj_set_style_border_width(internet_badge_, 0, 0);
  lv_obj_set_style_bg_color(internet_badge_, c(COL_BTN), 0);
  lv_obj_clear_flag(internet_badge_, LV_OBJ_FLAG_SCROLLABLE);
  badge_label_ = make_label(internet_badge_, "Internet OFF", &lv_font_montserrat_14, COL_TEXT);
  lv_obj_center(badge_label_);

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

  lv_obj_t *nd_title = make_label(screen_not_done_, "Jobs left to do", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(nd_title, LV_ALIGN_TOP_MID, 0, 12);

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

  // 3-across so five names fit with room for Cancel
  const int cols = 3;
  const int btn_w = 230;
  const int btn_h = 96;
  const int gap_x = 16;
  const int gap_y = 14;
  const int origin_x = 47;
  const int origin_y = 100;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    const int col = static_cast<int>(i % cols);
    const int row = static_cast<int>(i / cols);
    lv_obj_t *btn = lv_btn_create(screen_doer_);
    doer_btns_[i] = btn;
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, origin_x + col * (btn_w + gap_x), origin_y + row * (btn_h + gap_y));
    lv_obj_set_style_bg_color(btn, c(COL_CARD), 0);
    lv_obj_set_style_bg_color(btn, c(0x2F445C), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, onDoerChosen, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
    lv_obj_t *n = make_label(btn, KIDS[i].name, &lv_font_montserrat_24, COL_TEXT);
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
    lv_label_set_text(leftover_hint_, "anyone can help - then tap who did it (rewards)");
  } else {
    lv_obj_set_style_bg_color(leftover_btn_, c(0x3F3F46), 0);
    lv_obj_add_state(leftover_btn_, LV_STATE_DISABLED);
    lv_obj_clear_flag(leftover_btn_, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(leftover_title_, "Jobs left to do");
    const int mins = clock_minutes_until_leftover_open();
    char buf[64];
    if (!clock_is_synced()) {
      snprintf(buf, sizeof(buf), "opens at %02d:%02d - waiting for clock sync", LEFTOVER_OPENS_HOUR,
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
    const size_t left = state_->remainingCount(i);
    const bool ready = state_->internetEffective(i);
    char buf[64];
    if (left == 0) {
      snprintf(buf, sizeof(buf), "all done - internet on  " LV_SYMBOL_BULLET "  rewards %lu",
               static_cast<unsigned long>(state_->rewardTally(i)));
      lv_obj_set_style_text_color(home_remain_labels_[i], c(COL_DONE), 0);
    } else if (ready) {
      snprintf(buf, sizeof(buf), "internet on  " LV_SYMBOL_BULLET "  %u optional left", static_cast<unsigned>(left));
      lv_obj_set_style_text_color(home_remain_labels_[i], c(COL_DONE), 0);
    } else {
      snprintf(buf, sizeof(buf), "%u left  " LV_SYMBOL_BULLET "  rewards %lu", static_cast<unsigned>(left),
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
    if (JobState::jobCategory(j) != cat || !JobState::isAssigned(active_kid_, j)) {
      continue;
    }

    lv_obj_t *row = lv_obj_create(job_list_);
    job_rows_[j] = row;
    // Taller rows so Jax can spot the line icon next to each job / expectation
    lv_obj_set_size(row, 760, 64);
    lv_obj_set_style_bg_color(row, c(COL_CARD), 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(row, 4, 0);
    lv_obj_set_style_border_color(row, c(accent), 0);
    lv_obj_set_style_pad_left(row, 10, 0);
    lv_obj_set_style_pad_right(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = job_icon_create(row, JobState::jobId(j));
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *name = make_label(row, JobState::jobLabel(j), &lv_font_montserrat_18, COL_TEXT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 56, JobState::jobDetail(j)[0] ? -9 : 0);

    if (JobState::jobDetail(j)[0]) {
      lv_obj_t *det = make_label(row, JobState::jobDetail(j), &lv_font_montserrat_14, COL_MUTED);
      lv_label_set_long_mode(det, LV_LABEL_LONG_DOT);
      lv_obj_set_width(det, 470);
      lv_obj_align(det, LV_ALIGN_LEFT_MID, 56, 12);
    }

    lv_obj_t *btn = lv_btn_create(row);
    done_btns_[j] = btn;
    lv_obj_set_size(btn, 120, 42);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    // Tap = mark done. Hold = undo (so a stray tap can't switch internet off).
    // SHORT_CLICKED is not sent after a long press, so the two never clash.
    lv_obj_add_event_cb(btn, onDone, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void *>(j));
    lv_obj_add_event_cb(btn, onUndo, LV_EVENT_LONG_PRESSED, reinterpret_cast<void *>(j));
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
      lv_label_set_text(lbl, LV_SYMBOL_OK " Done");
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
  const size_t cat_total = countJobsInCategory(active_kid_, cat);
  const int pct = cat_total ? static_cast<int>((cat_done * 100) / cat_total) : 0;
  lv_bar_set_value(progress_bar_, pct, LV_ANIM_OFF);

  char buf[64];
  snprintf(buf, sizeof(buf), "%u/%u done - hold green to undo", static_cast<unsigned>(cat_done),
           static_cast<unsigned>(cat_total));
  lv_label_set_text(progress_label_, buf);

  const bool ready = state_->internetEffective(active_kid_);
  lv_obj_set_style_bg_color(internet_badge_, c(ready ? COL_DONE : COL_BTN), 0);
  lv_label_set_text(badge_label_, ready ? "Internet ON" : "Internet OFF");
}

void AppUi::rebuildNotDoneList() {
  lv_obj_clean(not_done_list_);

  size_t total_left = 0;
  for (size_t k = 0; k < KID_COUNT; ++k) {
    total_left += state_->remainingCount(k);
  }

  char summary[80];
  if (total_left == 0) {
    snprintf(summary, sizeof(summary), "Everyone is clear - nothing left to do");
  } else {
    snprintf(summary, sizeof(summary),
             "%u left - tap Done, then who did it (rewards). Unlock = own list clear.",
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
    lv_obj_t *msg = make_label(ok, "Nothing left - nice work", &lv_font_montserrat_18, COL_DONE);
    lv_obj_center(msg);
    return;
  }

  for (size_t k = 0; k < KID_COUNT; ++k) {
    const size_t left = state_->remainingCount(k);
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
    snprintf(hdr_txt, sizeof(hdr_txt), "%s's list - %u left (%s)", KIDS[k].name,
             static_cast<unsigned>(left), state_->internetEffective(k) ? "internet on" : "internet locked");
    lv_obj_t *hl = make_label(hdr, hdr_txt, &lv_font_montserrat_16, COL_TEXT);
    lv_obj_align(hl, LV_ALIGN_LEFT_MID, 12, 0);

    auto add_open = [&](JobCategory cat, uint32_t accent, const char *tag) {
      for (size_t j = 0; j < JOB_COUNT; ++j) {
        if (JobState::jobCategory(j) != cat || !JobState::isAssigned(k, j) || state_->isDone(k, j)) {
          continue;
        }
        lv_obj_t *row = lv_obj_create(not_done_list_);
        lv_obj_set_size(row, 760, 60);
        lv_obj_set_style_bg_color(row, c(COL_CARD), 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(row, 4, 0);
        lv_obj_set_style_border_color(row, c(accent), 0);
        lv_obj_set_style_pad_left(row, 10, 0);
        lv_obj_set_style_pad_right(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon = job_icon_create(row, JobState::jobId(j));
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

        char line[96];
        snprintf(line, sizeof(line), "[%s]  %s", tag, JobState::jobLabel(j));
        lv_obj_t *lbl = make_label(row, line, &lv_font_montserrat_16, COL_TEXT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 56, 0);

        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_set_size(btn, 120, 40);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn, c(COL_BTN), 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, onLeftoverDone, LV_EVENT_SHORT_CLICKED, packOwnerJob(k, j));
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
  // The owner can't claim a helper reward for their own job
  for (size_t i = 0; i < KID_COUNT; ++i) {
    if (i == owner) {
      lv_obj_add_flag(doer_btns_[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(doer_btns_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  char buf[96];
  snprintf(buf, sizeof(buf), "Who helped with %s's \"%s\"?", KIDS[owner].name, JobState::jobLabel(job));
  lv_label_set_text(doer_prompt_, buf);
  applyBacklightForScreen();
  lv_scr_load(screen_doer_);
}

void AppUi::completeLeftover(size_t doer) {
  const size_t owner = pending_owner_;
  const size_t job = pending_job_;
  awaiting_doer_ = false;
  if (doer == owner || doer >= KID_COUNT || !JobState::isAssigned(owner, job) ||
      state_->isDone(owner, job)) {
    showNotDone();  // owner can't self-credit; job may have been done meanwhile
    return;
  }

  state_->setDone(owner, job, true);
  state_->addReward(doer);

  mqtt_->publishJob(owner, job);
  mqtt_->publishRewardEvent(doer, owner, job);  // also publishes doer's tally
  mqtt_->publishKidSummary(owner);              // owner's internet on/off + progress

  showNotDone();
}

void AppUi::setIdleMode(IdleMode m) {
  idle_mode_ = m;
  if (m == IdleMode::Ads) {
    lv_obj_clear_flag(ad_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  } else if (m == IdleMode::Picker) {
    lv_obj_add_flag(ad_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ad_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  }
}

void AppUi::cyclePrizeRim() {
  const uint32_t rim = AD_RIMS[ad_index_ % AD_RIM_COUNT];
  lv_obj_set_style_border_color(prize_card_, c(rim), 0);
}

void AppUi::refreshPrizeScores() {
  for (size_t i = 0; i < KID_COUNT; ++i) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%s\n%lu", KIDS[i].name,
             static_cast<unsigned long>(state_->rewardTally(i)));
    auto *lbl = static_cast<lv_obj_t *>(lv_obj_get_user_data(score_chips_[i]));
    if (lbl) {
      lv_label_set_text(lbl, buf);
      lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    }
  }
}

void AppUi::refreshPrizePicker() {
  const uint16_t cost =
      (pending_prize_ < state_->prizeCount()) ? state_->prize(pending_prize_).cost : 0;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    const uint32_t pts = state_->rewardTally(i);
    char buf[48];
    if (cost == 0) {
      snprintf(buf, sizeof(buf), "%s\n%lu pts", KIDS[i].name, static_cast<unsigned long>(pts));
      lv_obj_set_style_bg_opa(kid_btns_[i], LV_OPA_COVER, 0);
    } else if (pts >= cost) {
      snprintf(buf, sizeof(buf), "%s\n%lu pts  OK!", KIDS[i].name, static_cast<unsigned long>(pts));
      lv_obj_set_style_bg_opa(kid_btns_[i], LV_OPA_COVER, 0);
    } else {
      snprintf(buf, sizeof(buf), "%s\nneed %u more", KIDS[i].name,
               static_cast<unsigned>(cost - pts));
      lv_obj_set_style_bg_opa(kid_btns_[i], LV_OPA_60, 0);
    }
    lv_label_set_text(kid_lbls_[i], buf);
    lv_obj_set_style_text_align(kid_lbls_[i], LV_TEXT_ALIGN_CENTER, 0);
  }
}

void AppUi::refreshPrizeAd() {
  if (state_->prizeCount() == 0) {
    lv_label_set_text(prize_name_, "No prizes yet");
    lv_label_set_text(prize_cost_, "Ask a grown-up!");
    lv_obj_add_state(want_btn_, LV_STATE_DISABLED);
    refreshPrizeScores();
    return;
  }
  lv_obj_clear_state(want_btn_, LV_STATE_DISABLED);
  if (ad_index_ >= state_->prizeCount()) {
    ad_index_ = 0;
  }
  const PrizeDef &p = state_->prize(ad_index_);
  lv_label_set_text(prize_name_, p.name);
  char cost[48];
  snprintf(cost, sizeof(cost), "%u CREDITS", p.cost);
  lv_label_set_text(prize_cost_, cost);
  cyclePrizeRim();
  refreshPrizeScores();
}

void AppUi::showPrizePicker(size_t prize_index) {
  pending_prize_ = prize_index;
  if (prize_index < state_->prizeCount()) {
    const PrizeDef &p = state_->prize(prize_index);
    char sub[80];
    snprintf(sub, sizeof(sub), "%s  //  %u credits", p.name, p.cost);
    lv_label_set_text(picker_sub_, sub);
  } else {
    lv_label_set_text(picker_sub_, "");
  }
  refreshPrizePicker();
  setIdleMode(IdleMode::Picker);
}

void AppUi::showPrizeSplashFromState() {
  const auto &s = state_->pendingSplash();
  if (s.kind == JobState::SplashKind::None) {
    return;
  }
  uint32_t tint = COL_PRIZE_WIN;
  const char *stars = "* * * JACKPOT * * *";
  if (s.kind == JobState::SplashKind::Bonus) {
    tint = COL_PRIZE_BONUS;
    stars = "* * * BONUS DROP * * *";
  } else if (s.kind == JobState::SplashKind::KeepGoing) {
    tint = COL_PRIZE_KEEP;
    stars = "SIGNAL WEAK — KEEP GOING";
  }
  lv_obj_set_style_bg_color(splash_layer_, c(COL_PRIZE_BG), 0);
  lv_obj_set_style_bg_grad_color(splash_layer_, c(tint), 0);
  lv_obj_set_style_bg_grad_dir(splash_layer_, LV_GRAD_DIR_VER, 0);
  lv_label_set_text(splash_stars_, stars);
  lv_label_set_text(splash_title_, s.title);
  lv_label_set_text(splash_sub_, s.subtitle);
  if (s.kid < KID_COUNT) {
    lv_label_set_text(splash_kid_, KIDS[s.kid].name);
  } else {
    lv_label_set_text(splash_kid_, "");
  }
  setIdleMode(IdleMode::Splash);
  splash_until_ms_ = millis() + (s.kind == JobState::SplashKind::KeepGoing ? 3000 : SPLASH_MS);
  state_->clearSplash();
}

void AppUi::showIdle() {
  awaiting_doer_ = false;
  screen_kind_ = ScreenKind::Idle;
  applyBacklightForScreen();
  setIdleMode(IdleMode::Ads);
  refreshPrizeAd();
  last_carousel_ms_ = millis();
  lv_scr_load(screen_idle_);
}

void AppUi::showHome() {
  awaiting_doer_ = false;
  screen_kind_ = ScreenKind::Home;
  applyBacklightForScreen();
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
  applyBacklightForScreen();
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
  applyBacklightForScreen();
  rebuildNotDoneList();
  lv_scr_load(screen_not_done_);
}

void AppUi::refreshActiveScreen() {
  switch (screen_kind_) {
    case ScreenKind::Idle:
      if (idle_mode_ == IdleMode::Ads) {
        refreshPrizeAd();
      } else if (idle_mode_ == IdleMode::Picker) {
        refreshPrizePicker();
      }
      break;
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
  // LVGL tracks time since the last touch, so no extra GT911 reads needed
  if (isActiveSession() && display_board_inactive_ms() >= IDLE_TIMEOUT_MS) {
    showIdle();
  }

  if (rewards_ && rewards_->revision() != seen_rewards_revision_) {
    seen_rewards_revision_ = rewards_->revision();
    refreshMessageOverlay();
  }

  if (screen_kind_ == ScreenKind::Idle) {
    if (idle_mode_ == IdleMode::Splash && millis() >= splash_until_ms_) {
      setIdleMode(IdleMode::Ads);
      refreshPrizeAd();
    }
    if (idle_mode_ == IdleMode::Ads && state_->prizeCount() > 0 &&
        millis() - last_carousel_ms_ >= CAROUSEL_MS) {
      last_carousel_ms_ = millis();
      ad_index_ = (ad_index_ + 1) % state_->prizeCount();
      refreshPrizeAd();
    }
  }

  const uint32_t rev = state_->revision();
  const uint32_t cfg_rev = state_->configRevision();
  if (rev != seen_revision_ || cfg_rev != seen_config_revision_) {
    const bool cfg_changed = cfg_rev != seen_config_revision_;
    seen_revision_ = rev;
    seen_config_revision_ = cfg_rev;
    if (screen_kind_ == ScreenKind::Idle) {
      if (state_->pendingSplash().kind != JobState::SplashKind::None) {
        showPrizeSplashFromState();
      } else {
        refreshActiveScreen();
      }
    } else if (!awaiting_doer_) {
      if (cfg_changed && screen_kind_ == ScreenKind::Kid) {
        rebuildJobList();  // job list for this kid changed (web app)
      }
      refreshActiveScreen();
    }
  }

  static uint32_t last_clock_ms = 0;
  if (millis() - last_clock_ms > 1000) {
    last_clock_ms = millis();
    if (clock_label_ && screen_kind_ == ScreenKind::Home) {
      char tbuf[24];
      clock_format_now(tbuf, sizeof(tbuf));
      lv_label_set_text(clock_label_, tbuf);
    }
    if (screen_kind_ == ScreenKind::Home) {
      updateLeftoverHomeButton();
    }
    if (screen_kind_ == ScreenKind::NotDone && !clock_leftover_screen_open() && !awaiting_doer_) {
      showHome();
    }
  }

  if (status_label_ && screen_kind_ == ScreenKind::Home) {
    const char *msg = mqtt_->connected() ? "MQTT online" : "MQTT connecting...";
    if (WiFi.status() != WL_CONNECTED) {
      msg = "Wi-Fi connecting...";
    }
    if (strcmp(lv_label_get_text(status_label_), msg) != 0) {
      lv_label_set_text(status_label_, msg);
    }
  }
}

void AppUi::onWantPrize(lv_event_t *e) {
  (void)e;
  if (!self_ || self_->screen_kind_ != ScreenKind::Idle || self_->idle_mode_ != IdleMode::Ads) {
    return;
  }
  if (self_->state_->prizeCount() == 0) {
    return;
  }
  self_->showPrizePicker(self_->ad_index_);
}

void AppUi::onPrizeCancel(lv_event_t *e) {
  (void)e;
  if (!self_) {
    return;
  }
  self_->setIdleMode(IdleMode::Ads);
  self_->refreshPrizeAd();
}

void AppUi::onPrizeKid(lv_event_t *e) {
  if (!self_) {
    return;
  }
  const uint32_t now = millis();
  if (now - self_->last_claim_ms_ < CLAIM_DEBOUNCE_MS) {
    return;
  }
  self_->last_claim_ms_ = now;
  const size_t kid = reinterpret_cast<size_t>(lv_event_get_user_data(e));
  const size_t prize = self_->pending_prize_;
  uint32_t before = 0;
  uint32_t after = 0;
  const bool ok = self_->state_->tryClaimPrize(kid, prize, &before, &after);
  if (ok) {
    self_->mqtt_->publishRewardTally(kid);
    self_->mqtt_->publishClaim(kid, prize, before, after);
    self_->mqtt_->publishCelebrateWinner(kid, prize, after);
  }
  self_->showPrizeSplashFromState();
}

void AppUi::onLetsGo(lv_event_t *e) {
  (void)e;
  if (self_) {
    self_->showHome();
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
  const size_t job = reinterpret_cast<size_t>(lv_event_get_user_data(e));
  const size_t kid = self_->active_kid_;
  if (job >= JOB_COUNT || self_->state_->isDone(kid, job) || !self_->debounce(&self_->last_done_ms_[job])) {
    return;  // already done: tapping again does nothing (hold to undo)
  }
  self_->state_->setDone(kid, job, true);
  self_->mqtt_->publishJob(kid, job);
  self_->mqtt_->publishKidSummary(kid);
  self_->updateJobRow(job);
  self_->updateKidHeader();
}

void AppUi::onUndo(lv_event_t *e) {
  const size_t job = reinterpret_cast<size_t>(lv_event_get_user_data(e));
  const size_t kid = self_->active_kid_;
  if (job >= JOB_COUNT || !self_->state_->isDone(kid, job)) {
    return;
  }
  self_->state_->setDone(kid, job, false);
  self_->last_done_ms_[job] = millis();
  self_->mqtt_->publishJob(kid, job);
  self_->mqtt_->publishKidSummary(kid);
  self_->updateJobRow(job);
  self_->updateKidHeader();
}

void AppUi::onResetKid(lv_event_t *e) {
  (void)e;
  const size_t kid = self_->active_kid_;
  self_->state_->resetKid(kid);
  self_->publishKid(kid);
  self_->rebuildJobList();
  self_->updateKidHeader();
}

void AppUi::onLeftoverDone(lv_event_t *e) {
  if (!self_->debounce(&self_->last_leftover_ms_)) {
    return;
  }

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
