#include "display_board.h"

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <esp_heap_caps.h>

using namespace esp_panel::board;
using namespace esp_panel::drivers;

namespace {

Board *g_board = nullptr;
lv_disp_draw_buf_t draw_buf;
lv_disp_drv_t disp_drv;
lv_indev_drv_t indev_drv;
lv_color_t *buf1 = nullptr;
int g_backlight_state = -1;  // -1 unknown, 0 off, 1 on

void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  auto *lcd = static_cast<LCD *>(drv->user_data);
  const int x = area->x1;
  const int y = area->y1;
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  // ESP32_Display_Panel 1.x: drawBitmap(x_start, y_start, WIDTH, HEIGHT, data).
  // (Passing x+w / y+h here made every strip below the top of the screen fail
  // the bounds check or read past the end of the LVGL buffer.)
  lcd->drawBitmap(x, y, w, h, reinterpret_cast<const uint8_t *>(color_p));
  lv_disp_flush_ready(drv);
}

void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  auto *touch = static_cast<Touch *>(drv->user_data);
  TouchPoint point{};
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
  if (!g_board->init()) {
    Serial.println("[display] board init failed");
    return false;
  }
  // Note: keep the default single RGB frame buffer. With partial LVGL buffers,
  // a second frame buffer only wastes ~770 KB of PSRAM (and with esp_lcd's
  // copy mode can draw into the buffer that is not being scanned out).
  if (!g_board->begin()) {
    Serial.println("[display] board begin failed");
    return false;
  }

  LCD *lcd = g_board->getLCD();
  Touch *touch = g_board->getTouch();
  if (!lcd) {
    Serial.println("[display] LCD missing");
    return false;
  }

  display_board_set_backlight(100);

  lv_init();

  // One partial draw buffer is enough: the RGB flush is a synchronous memcpy
  // into the panel frame buffer, so a second buffer gives no overlap.
  // Prefer internal RAM (fast), fall back to PSRAM; Wi-Fi needs internal RAM too,
  // so keep this modest (800 x 40 x 2 B = 64 KB).
  const size_t buf_pixels = LCD_H_RES * 40;
  const size_t buf_bytes = buf_pixels * sizeof(lv_color_t);
  buf1 = static_cast<lv_color_t *>(heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!buf1) {
    buf1 = static_cast<lv_color_t *>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!buf1) {
    Serial.println("[display] LVGL buffer alloc failed");
    return false;
  }
  lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, buf_pixels);

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

  Serial.printf("[display] ready 800x480 (free internal %u, psram %u)\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  return true;
}

void display_board_loop() {
  lv_timer_handler();
}

void display_board_set_backlight(uint8_t percent) {
  if (!g_board || !g_board->getBacklight()) {
    return;
  }
  const int want = percent > 0 ? 1 : 0;
  if (want == g_backlight_state) {
    return;  // avoid an I2C expander write on every screen change
  }
  if (want) {
    g_board->getBacklight()->on();
  } else {
    g_board->getBacklight()->off();
  }
  g_backlight_state = want;
}

uint32_t display_board_inactive_ms() {
  return lv_disp_get_inactive_time(nullptr);
}
