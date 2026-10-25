#include "ui.h"

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

// Kill theme chrome so full-bleed layers aren't inset (BOTTOM_MID then sits mid-screen).
void strip_chrome(lv_obj_t *o) {
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
}

lv_obj_t *make_full_layer(lv_obj_t *parent, uint32_t bg) {
  lv_obj_t *layer = lv_obj_create(parent);
  strip_chrome(layer);
  lv_obj_set_size(layer, LCD_H_RES, LCD_V_RES);
  lv_obj_set_pos(layer, 0, 0);
  lv_obj_set_style_bg_color(layer, c(bg), 0);
  lv_obj_set_style_bg_opa(layer, LV_OPA_COVER, 0);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
  return layer;
}

// Rotate prize-card accent so each ad feels fresh
const uint32_t AD_BANDS[] = {0xFDE68A, 0xFBCFE8, 0xBBF7D0, 0xBAE6FD, 0xDDD6FE};
const size_t AD_BAND_COUNT = sizeof(AD_BANDS) / sizeof(AD_BANDS[0]);

// Absolute layout for 800×480 — no BOTTOM_MID (breaks if parent has theme pad).
constexpr int SKY_H = 64;
constexpr int CARD_Y = 76;
constexpr int CARD_H = 200;
constexpr int WANT_Y = 292;
constexpr int WANT_H = 70;
constexpr int SCORE_Y = 386;
constexpr int SCORE_H = 72;

}  // namespace

void AppUi::begin(RewardState *state, MqttBridge *mqtt) {
  self_ = this;
  state_ = state;
  mqtt_ = mqtt;
  seen_revision_ = state_->revision();
  build();
  showAds();
  last_carousel_ms_ = millis();
}

void AppUi::build() {
  screen_ = lv_obj_create(nullptr);
  strip_chrome(screen_);
  lv_obj_set_style_bg_color(screen_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

  // ========== AD MODE ==========
  ad_layer_ = make_full_layer(screen_, COL_BG);

  lv_obj_t *sky = lv_obj_create(ad_layer_);
  strip_chrome(sky);
  lv_obj_set_size(sky, LCD_H_RES, SKY_H);
  lv_obj_set_pos(sky, 0, 0);
  lv_obj_set_style_bg_color(sky, c(COL_SKY), 0);
  lv_obj_set_style_bg_opa(sky, LV_OPA_COVER, 0);
  lv_obj_clear_flag(sky, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *brand = make_label(ad_layer_, "Prize Shop!", &lv_font_montserrat_28, COL_TEXT);
  lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 16);

  prize_card_ = lv_obj_create(ad_layer_);
  lv_obj_set_size(prize_card_, 720, CARD_H);
  lv_obj_set_pos(prize_card_, (LCD_H_RES - 720) / 2, CARD_Y);
  lv_obj_set_style_bg_color(prize_card_, c(COL_CARD), 0);
  lv_obj_set_style_border_width(prize_card_, 0, 0);
  lv_obj_set_style_outline_width(prize_card_, 0, 0);
  lv_obj_set_style_shadow_width(prize_card_, 0, 0);
  lv_obj_set_style_radius(prize_card_, 28, 0);
  lv_obj_set_style_pad_all(prize_card_, 16, 0);
  lv_obj_clear_flag(prize_card_, LV_OBJ_FLAG_SCROLLABLE);

  prize_name_ = make_label(prize_card_, "", &lv_font_montserrat_28, COL_TEXT);
  lv_label_set_long_mode(prize_name_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(prize_name_, 660);
  lv_obj_set_style_text_align(prize_name_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(prize_name_, LV_ALIGN_TOP_MID, 0, 28);

  prize_cost_ = make_label(prize_card_, "", &lv_font_montserrat_28, COL_ACCENT);
  lv_obj_align(prize_cost_, LV_ALIGN_BOTTOM_MID, 0, -20);

  want_btn_ = lv_btn_create(ad_layer_);
  lv_obj_set_size(want_btn_, 360, WANT_H);
  lv_obj_set_pos(want_btn_, (LCD_H_RES - 360) / 2, WANT_Y);
  lv_obj_set_style_bg_color(want_btn_, c(COL_ACCENT), 0);
  lv_obj_set_style_bg_color(want_btn_, c(0xFB923C), LV_STATE_PRESSED);
  lv_obj_set_style_radius(want_btn_, 35, 0);
  lv_obj_set_style_shadow_width(want_btn_, 0, 0);
  lv_obj_set_style_border_width(want_btn_, 0, 0);
  lv_obj_add_event_cb(want_btn_, onWantClick, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *want_lbl = make_label(want_btn_, "I want this!", &lv_font_montserrat_28, 0xFFFFFF);
  lv_obj_center(want_lbl);

  // Score chips along the bottom — sized to fit five kids on 800px
  score_row_ = lv_obj_create(ad_layer_);
  strip_chrome(score_row_);
  lv_obj_set_size(score_row_, LCD_H_RES - 16, SCORE_H);
  lv_obj_set_pos(score_row_, 8, SCORE_Y);
  lv_obj_set_style_bg_opa(score_row_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(score_row_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(score_row_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(score_row_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(score_row_, 6, 0);

  const int chip_w = 148;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    score_chips_[i] = lv_obj_create(score_row_);
    lv_obj_set_size(score_chips_[i], chip_w, 56);
    lv_obj_set_style_bg_color(score_chips_[i], c(KIDS[i].color), 0);
    lv_obj_set_style_border_width(score_chips_[i], 0, 0);
    lv_obj_set_style_outline_width(score_chips_[i], 0, 0);
    lv_obj_set_style_shadow_width(score_chips_[i], 0, 0);
    lv_obj_set_style_radius(score_chips_[i], 28, 0);
    lv_obj_set_style_pad_all(score_chips_[i], 2, 0);
    lv_obj_clear_flag(score_chips_[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *chip = make_label(score_chips_[i], "", &lv_font_montserrat_14, 0xFFFFFF);
    lv_label_set_long_mode(chip, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(chip, chip_w - 8);
    lv_obj_set_style_text_align(chip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(chip);
    lv_obj_set_user_data(score_chips_[i], chip);
  }

  // ========== PICKER ==========
  picker_layer_ = make_full_layer(screen_, COL_BG);
  lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);

  picker_title_ = make_label(picker_layer_, "Whose prize?", &lv_font_montserrat_28, COL_TEXT);
  lv_obj_align(picker_title_, LV_ALIGN_TOP_MID, 0, 16);

  picker_sub_ = make_label(picker_layer_, "", &lv_font_montserrat_18, COL_MUTED);
  lv_obj_align(picker_sub_, LV_ALIGN_TOP_MID, 0, 52);

  const int cols = 3;
  const int btn_w = 230;
  const int btn_h = 96;
  const int gap_x = 18;
  const int gap_y = 14;
  const int start_x = (LCD_H_RES - (cols * btn_w + (cols - 1) * gap_x)) / 2;
  const int start_y = 90;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    kid_btns_[i] = lv_btn_create(picker_layer_);
    lv_obj_set_size(kid_btns_[i], btn_w, btn_h);
    const int row = static_cast<int>(i) / cols;
    const int col = static_cast<int>(i) % cols;
    lv_obj_set_pos(kid_btns_[i], start_x + col * (btn_w + gap_x), start_y + row * (btn_h + gap_y));
    lv_obj_set_style_bg_color(kid_btns_[i], c(KIDS[i].color), 0);
    lv_obj_set_style_radius(kid_btns_[i], 24, 0);
    lv_obj_set_style_shadow_width(kid_btns_[i], 0, 0);
    lv_obj_set_style_border_width(kid_btns_[i], 0, 0);
    lv_obj_add_event_cb(kid_btns_[i], onKidClick, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
    kid_lbls_[i] = make_label(kid_btns_[i], KIDS[i].name, &lv_font_montserrat_20, 0xFFFFFF);
    lv_obj_set_style_text_align(kid_lbls_[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(kid_lbls_[i]);
  }

  lv_obj_t *cancel = lv_btn_create(picker_layer_);
  lv_obj_set_size(cancel, 200, 56);
  lv_obj_set_pos(cancel, (LCD_H_RES - 200) / 2, LCD_V_RES - 72);
  lv_obj_set_style_bg_color(cancel, c(COL_BTN_BACK), 0);
  lv_obj_set_style_radius(cancel, 28, 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_set_style_border_width(cancel, 0, 0);
  lv_obj_add_event_cb(cancel, onCancelClick, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cl = make_label(cancel, "Go back", &lv_font_montserrat_18, COL_TEXT);
  lv_obj_center(cl);

  // ========== SPLASH ==========
  splash_layer_ = make_full_layer(screen_, COL_WIN);
  lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);

  splash_stars_ = make_label(splash_layer_, "* * * * *", &lv_font_montserrat_28, 0xFFFFFF);
  lv_obj_align(splash_stars_, LV_ALIGN_TOP_MID, 0, 70);

  splash_title_ = make_label(splash_layer_, "YOU WIN!", &lv_font_montserrat_28, 0xFFFFFF);
  lv_obj_align(splash_title_, LV_ALIGN_CENTER, 0, -40);

  splash_kid_ = make_label(splash_layer_, "", &lv_font_montserrat_28, COL_TEXT);
  lv_obj_align(splash_kid_, LV_ALIGN_CENTER, 0, 20);

  splash_sub_ = make_label(splash_layer_, "", &lv_font_montserrat_20, COL_TEXT);
  lv_obj_set_width(splash_sub_, LCD_H_RES - 80);
  lv_label_set_long_mode(splash_sub_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(splash_sub_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(splash_sub_, LV_ALIGN_CENTER, 0, 80);

  lv_scr_load(screen_);
}

void AppUi::setMode(Mode m) {
  mode_ = m;
  if (m == Mode::Ads) {
    lv_obj_clear_flag(ad_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  } else if (m == Mode::Picker) {
    lv_obj_add_flag(ad_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ad_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  }
}

void AppUi::cycleAdBg() {
  const uint32_t band = AD_BANDS[ad_index_ % AD_BAND_COUNT];
  lv_obj_set_style_bg_color(prize_card_, c(band), 0);
}

void AppUi::refreshScores() {
  for (size_t i = 0; i < KID_COUNT; ++i) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%s\n%lu", KIDS[i].name, static_cast<unsigned long>(state_->tally(i)));
    auto *lbl = static_cast<lv_obj_t *>(lv_obj_get_user_data(score_chips_[i]));
    if (lbl) {
      lv_label_set_text(lbl, buf);
      lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    }
  }
}

void AppUi::refreshPickerButtons() {
  const uint16_t cost =
      (pending_prize_ < state_->prizeCount()) ? state_->prize(pending_prize_).cost : 0;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    const uint32_t pts = state_->tally(i);
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

void AppUi::refreshAd() {
  if (state_->prizeCount() == 0) {
    lv_label_set_text(prize_name_, "No prizes yet");
    lv_label_set_text(prize_cost_, "Ask a grown-up!");
    lv_obj_add_state(want_btn_, LV_STATE_DISABLED);
    refreshScores();
    return;
  }
  lv_obj_clear_state(want_btn_, LV_STATE_DISABLED);
  if (ad_index_ >= state_->prizeCount()) {
    ad_index_ = 0;
  }
  const PrizeDef &p = state_->prize(ad_index_);
  lv_label_set_text(prize_name_, p.name);
  char cost[48];
  snprintf(cost, sizeof(cost), "%u points", p.cost);
  lv_label_set_text(prize_cost_, cost);
  cycleAdBg();

  lv_obj_set_style_opa(prize_card_, LV_OPA_0, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, prize_card_);
  lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
  lv_anim_set_time(&a, 380);
  lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(v), 0);
  });
  lv_anim_start(&a);

  refreshScores();
}

void AppUi::showAds() {
  setMode(Mode::Ads);
  refreshAd();
}

void AppUi::showPicker(size_t prize_index) {
  pending_prize_ = prize_index;
  if (prize_index < state_->prizeCount()) {
    const PrizeDef &p = state_->prize(prize_index);
    char sub[80];
    snprintf(sub, sizeof(sub), "%s  ·  %u points", p.name, p.cost);
    lv_label_set_text(picker_sub_, sub);
  } else {
    lv_label_set_text(picker_sub_, "");
  }
  lv_label_set_text(picker_title_, "Tap your name!");
  refreshPickerButtons();
  setMode(Mode::Picker);
}

void AppUi::showSplashFromState() {
  const auto &s = state_->pendingSplash();
  if (s.kind == RewardState::SplashKind::None) {
    return;
  }

  uint32_t bg = COL_WIN;
  const char *stars = "* * * YOU WIN! * * *";
  if (s.kind == RewardState::SplashKind::Bonus) {
    bg = COL_BONUS;
    stars = "* * * BONUS! * * *";
  } else if (s.kind == RewardState::SplashKind::KeepGoing) {
    bg = COL_KEEP;
    stars = "Almost there!";
  } else {
    stars = "* * * * *";
  }

  lv_obj_set_style_bg_color(splash_layer_, c(bg), 0);
  lv_label_set_text(splash_stars_, stars);
  // Friendlier titles on glass
  if (s.kind == RewardState::SplashKind::Winner) {
    lv_label_set_text(splash_title_, "YOU WIN!");
  } else if (s.kind == RewardState::SplashKind::Bonus) {
    lv_label_set_text(splash_title_, "BONUS POINTS!");
  } else {
    lv_label_set_text(splash_title_, "Keep earning!");
  }
  lv_label_set_text(splash_sub_, s.subtitle);
  if (s.kid < KID_COUNT) {
    lv_label_set_text(splash_kid_, KIDS[s.kid].name);
  } else {
    lv_label_set_text(splash_kid_, "");
  }

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, splash_title_);
  lv_anim_set_values(&a, -40, 0);
  lv_anim_set_time(&a, 420);
  lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
  lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(obj), v, 0);
  });
  lv_anim_start(&a);

  setMode(Mode::Splash);
  splash_until_ms_ = millis() + (s.kind == RewardState::SplashKind::KeepGoing ? 3000 : SPLASH_MS);
  state_->clearSplash();
}

void AppUi::tick() {
  if (state_->revision() != seen_revision_) {
    seen_revision_ = state_->revision();
    refreshScores();
    if (state_->pendingSplash().kind != RewardState::SplashKind::None) {
      showSplashFromState();
    } else if (mode_ == Mode::Ads) {
      refreshAd();
    } else if (mode_ == Mode::Picker) {
      refreshPickerButtons();
    }
  }

  if (mode_ == Mode::Splash && millis() >= splash_until_ms_) {
    showAds();
  }

  if (mode_ == Mode::Ads && state_->prizeCount() > 0) {
    if (millis() - last_carousel_ms_ >= CAROUSEL_MS) {
      last_carousel_ms_ = millis();
      ad_index_ = (ad_index_ + 1) % state_->prizeCount();
      refreshAd();
    }
  }
}

void AppUi::onWantClick(lv_event_t *e) {
  (void)e;
  if (!self_ || self_->mode_ != Mode::Ads) {
    return;
  }
  if (self_->state_->prizeCount() == 0) {
    return;
  }
  self_->showPicker(self_->ad_index_);
}

void AppUi::onCancelClick(lv_event_t *e) {
  (void)e;
  if (self_) {
    self_->showAds();
  }
}

void AppUi::onKidClick(lv_event_t *e) {
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
  const bool ok = self_->state_->tryClaim(kid, prize, &before, &after);
  if (ok) {
    self_->mqtt_->publishTally(kid);
    self_->mqtt_->publishClaim(kid, prize, before, after);
    self_->mqtt_->publishCelebrateWinner(kid, prize, after);
  }
  self_->showSplashFromState();
}
