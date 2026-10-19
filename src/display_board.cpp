#include "display_board.h"

#include <Arduino.h>
#include <esp_display_panel.hpp>

using namespace esp_panel::board;
using namespace esp_panel::drivers;

namespace {

Board *g_board = nullptr;
lv_disp_draw_buf_t draw_buf;
lv_disp_drv_t disp_drv;
lv_indev_drv_t indev_drv;
lv_color_t *buf1 = nullptr;
lv_color_t *buf2 = nullptr;

// Simple mutex via portMUX for LVGL from Arduino loop only — single-threaded draw.
portMUX_TYPE lvgl_mux = portMUX_INITIALIZER_UNLOCKED;

void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  auto *lcd = static_cast<LCD *>(drv->user_data);
  const int x = area->x1;
  const int y = area->y1;
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  lcd->drawBitmap(x, y, x + w, y + h, reinterpret_cast<const uint8_t *>(color_p));
  lv_disp_flush_ready(drv);
}

void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  auto *touch = static_cast<Touch *>(drv->user_data);
  TouchPoint point{};
  // readPoints(points, max_points, timeout_ms) — returns number of points
  const int count = touch->readPoints(&point, 1, 0);
  if (count > 0) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = point.x;
    data->point.y = point.y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

}  // namespace

bool display_board_init() {
  Serial.println("[display] init Waveshare ESP32-S3-Touch-LCD-7");

  g_board = new Board();
  g_board->init();
  // Full refresh draw avoids tearing on RGB panels when using LVGL partial buffers
  g_board->getLCD()->configFrameBufferNumber(2);
  if (!g_board->begin()) {
    Serial.println("[display] board begin failed");
    return false;
  }
  Serial.println("[display] board began");

  if (g_board->getBacklight()) {
    g_board->getBacklight()->on();
    // Start dim — UI will raise brightness after "Lets Go"
    g_board->getBacklight()->setBrightness(8);
  }
  Serial.println("[display] backlight on (dim)");

  LCD *lcd = g_board->getLCD();
  Touch *touch = g_board->getTouch();
  if (!lcd) {
    Serial.println("[display] LCD missing");
    return false;
  }

  lv_init();
  Serial.println("[display] lvgl init");

  const size_t buf_pixels = LCD_H_RES * 40;
  buf1 = static_cast<lv_color_t *>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  buf2 = static_cast<lv_color_t *>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (!buf1) {
    buf1 = static_cast<lv_color_t *>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
  }
  if (!buf2) {
    buf2 = static_cast<lv_color_t *>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM));
  }
  if (!buf1) {
    Serial.println("[display] LVGL buffer alloc failed");
    return false;
  }
  Serial.printf("[display] lvgl buffers ok (buf2=%s)\n", buf2 ? "yes" : "single");

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_pixels);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.user_data = lcd;
  lv_disp_drv_register(&disp_drv);

  if (touch) {
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    indev_drv.user_data = touch;
    lv_indev_drv_register(&indev_drv);
  } else {
    Serial.println("[display] warning: no touch controller");
  }

  Serial.println("[display] ready 800x480");
  return true;
}

void display_board_lvgl_lock() {
  portENTER_CRITICAL(&lvgl_mux);
}

void display_board_lvgl_unlock() {
  portEXIT_CRITICAL(&lvgl_mux);
}

void display_board_loop() {
  display_board_lvgl_lock();
  lv_timer_handler();
  display_board_lvgl_unlock();
}

void display_board_set_backlight(uint8_t percent) {
  if (!g_board || !g_board->getBacklight()) {
    return;
  }
  if (percent > 100) {
    percent = 100;
  }
  g_board->getBacklight()->setBrightness(percent);
  g_board->getBacklight()->on();
}

bool display_board_touch_pressed() {
  if (!g_board || !g_board->getTouch()) {
    return false;
  }
  TouchPoint point{};
  return g_board->getTouch()->readPoints(&point, 1, 0) > 0;
}
