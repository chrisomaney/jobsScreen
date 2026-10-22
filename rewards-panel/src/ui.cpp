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
  lv_obj_set_style_bg_color(screen_, c(COL_BG), 0);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

  // --- Ad layer (full bleed) ---
  ad_layer_ = lv_obj_create(screen_);
  lv_obj_set_size(ad_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_style_bg_color(ad_layer_, c(COL_BG), 0);
  lv_obj_set_style_border_width(ad_layer_, 0, 0);
  lv_obj_set_style_pad_all(ad_layer_, 0, 0);
  lv_obj_set_style_radius(ad_layer_, 0, 0);
  lv_obj_clear_flag(ad_layer_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ad_layer_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ad_layer_, onAdClick, LV_EVENT_CLICKED, nullptr);

  // Accent wash at top
  lv_obj_t *wash = lv_obj_create(ad_layer_);
  lv_obj_set_size(wash, LCD_H_RES, 120);
  lv_obj_align(wash, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(wash, c(COL_PANEL), 0);
  lv_obj_set_style_bg_opa(wash, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(wash, 0, 0);
  lv_obj_set_style_radius(wash, 0, 0);
  lv_obj_clear_flag(wash, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *brand = make_label(ad_layer_, "REWARDS", &lv_font_montserrat_20, COL_ACCENT);
  lv_obj_align(brand, LV_ALIGN_TOP_LEFT, 28, 28);

  hint_ = make_label(ad_layer_, "Tap to claim", &lv_font_montserrat_16, COL_MUTED);
  lv_obj_align(hint_, LV_ALIGN_TOP_RIGHT, -28, 32);

  prize_name_ = make_label(ad_layer_, "", &lv_font_montserrat_28, COL_TEXT);
  lv_label_set_long_mode(prize_name_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(prize_name_, LCD_H_RES - 80);
  lv_obj_align(prize_name_, LV_ALIGN_CENTER, 0, -24);

  prize_cost_ = make_label(ad_layer_, "", &lv_font_montserrat_28, COL_ACCENT);
  lv_obj_align(prize_cost_, LV_ALIGN_CENTER, 0, 48);

  score_ticker_ = make_label(ad_layer_, "", &lv_font_montserrat_14, COL_MUTED);
  lv_label_set_long_mode(score_ticker_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(score_ticker_, LCD_H_RES - 48);
  lv_obj_align(score_ticker_, LV_ALIGN_BOTTOM_MID, 0, -20);

  // --- Picker ---
  picker_layer_ = lv_obj_create(screen_);
  lv_obj_set_size(picker_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_style_bg_color(picker_layer_, c(COL_BG), 0);
  lv_obj_set_style_border_width(picker_layer_, 0, 0);
  lv_obj_set_style_radius(picker_layer_, 0, 0);
  lv_obj_add_flag(picker_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(picker_layer_, LV_OBJ_FLAG_SCROLLABLE);

  picker_title_ = make_label(picker_layer_, "Who's claiming?", &lv_font_montserrat_24, COL_TEXT);
  lv_obj_align(picker_title_, LV_ALIGN_TOP_MID, 0, 28);

  const int cols = 3;
  const int btn_w = 220;
  const int btn_h = 72;
  const int gap = 16;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    lv_obj_t *btn = lv_btn_create(picker_layer_);
    lv_obj_set_size(btn, btn_w, btn_h);
    const int row = static_cast<int>(i) / cols;
    const int col = static_cast<int>(i) % cols;
    const int x = 40 + col * (btn_w + gap);
    const int y = 100 + row * (btn_h + gap);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, c(COL_BTN), 0);
    lv_obj_set_style_bg_color(btn, c(COL_ACCENT), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, onKidClick, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
    lv_obj_t *lbl = make_label(btn, KIDS[i].name, &lv_font_montserrat_20, COL_TEXT);
    lv_obj_center(lbl);
  }

  lv_obj_t *cancel = lv_btn_create(picker_layer_);
  lv_obj_set_size(cancel, 180, 52);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_set_style_bg_color(cancel, c(COL_PANEL), 0);
  lv_obj_set_style_radius(cancel, 12, 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_add_event_cb(cancel, onCancelClick, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cl = make_label(cancel, "Cancel", &lv_font_montserrat_18, COL_MUTED);
  lv_obj_center(cl);

  // --- Splash ---
  splash_layer_ = lv_obj_create(screen_);
  lv_obj_set_size(splash_layer_, LCD_H_RES, LCD_V_RES);
  lv_obj_set_style_bg_color(splash_layer_, c(COL_WIN), 0);
  lv_obj_set_style_border_width(splash_layer_, 0, 0);
  lv_obj_set_style_radius(splash_layer_, 0, 0);
  lv_obj_add_flag(splash_layer_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(splash_layer_, LV_OBJ_FLAG_SCROLLABLE);

  splash_title_ = make_label(splash_layer_, "WINNER!", &lv_font_montserrat_28, COL_BG);
  lv_obj_align(splash_title_, LV_ALIGN_CENTER, 0, -50);

  splash_kid_ = make_label(splash_layer_, "", &lv_font_montserrat_24, COL_BG);
  lv_obj_align(splash_kid_, LV_ALIGN_CENTER, 0, 10);

  splash_sub_ = make_label(splash_layer_, "", &lv_font_montserrat_20, COL_PANEL);
  lv_obj_set_width(splash_sub_, LCD_H_RES - 80);
  lv_label_set_long_mode(splash_sub_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(splash_sub_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(splash_sub_, LV_ALIGN_CENTER, 0, 60);

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

void AppUi::refreshTicker() {
  String line;
  for (size_t i = 0; i < KID_COUNT; ++i) {
    if (i) {
      line += "   ·   ";
    }
    line += KIDS[i].name;
    line += " ";
    line += String(state_->tally(i));
  }
  lv_label_set_text(score_ticker_, line.c_str());
}

void AppUi::refreshAd() {
  if (state_->prizeCount() == 0) {
    lv_label_set_text(prize_name_, "No prizes yet");
    lv_label_set_text(prize_cost_, "Ask a parent to add some");
    return;
  }
  if (ad_index_ >= state_->prizeCount()) {
    ad_index_ = 0;
  }
  const PrizeDef &p = state_->prize(ad_index_);
  lv_label_set_text(prize_name_, p.name);
  char cost[48];
  snprintf(cost, sizeof(cost), "%u points", p.cost);
  lv_label_set_text(prize_cost_, cost);

  // Subtle zoom-in feel via opacity animation
  lv_obj_set_style_opa(prize_name_, LV_OPA_0, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, prize_name_);
  lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
  lv_anim_set_time(&a, 420);
  lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(v), 0);
  });
  lv_anim_start(&a);

  refreshTicker();
}

void AppUi::showAds() {
  setMode(Mode::Ads);
  refreshAd();
}

void AppUi::showPicker(size_t prize_index) {
  pending_prize_ = prize_index;
  char buf[80];
  if (prize_index < state_->prizeCount()) {
    snprintf(buf, sizeof(buf), "Claim: %s", state_->prize(prize_index).name);
  } else {
    snprintf(buf, sizeof(buf), "Who's claiming?");
  }
  lv_label_set_text(picker_title_, buf);
  setMode(Mode::Picker);
}

void AppUi::showSplashFromState() {
  const auto &s = state_->pendingSplash();
  if (s.kind == RewardState::SplashKind::None) {
    return;
  }
  uint32_t bg = COL_WIN;
  if (s.kind == RewardState::SplashKind::Bonus) {
    bg = COL_BONUS;
  } else if (s.kind == RewardState::SplashKind::KeepGoing) {
    bg = COL_PANEL;
  }
  lv_obj_set_style_bg_color(splash_layer_, c(bg), 0);
  lv_label_set_text(splash_title_, s.title);
  lv_label_set_text(splash_sub_, s.subtitle);
  if (s.kid < KID_COUNT) {
    lv_label_set_text(splash_kid_, KIDS[s.kid].name);
  } else {
    lv_label_set_text(splash_kid_, "");
  }

  // Pop-in title
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, splash_title_);
  lv_anim_set_values(&a, 0, 20);
  lv_anim_set_time(&a, 350);
  lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
  lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(obj), -50 + v, 0);
  });
  lv_anim_start(&a);

  setMode(Mode::Splash);
  splash_until_ms_ = millis() + (s.kind == RewardState::SplashKind::KeepGoing ? 2800 : SPLASH_MS);
  state_->clearSplash();
}

void AppUi::tick() {
  if (state_->revision() != seen_revision_) {
    seen_revision_ = state_->revision();
    refreshTicker();
    if (state_->pendingSplash().kind != RewardState::SplashKind::None) {
      showSplashFromState();
    } else if (mode_ == Mode::Ads) {
      refreshAd();
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

void AppUi::onAdClick(lv_event_t *e) {
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
