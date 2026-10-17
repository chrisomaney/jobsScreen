#pragma once

#include <lvgl.h>
#include <stdint.h>

// Waveshare ESP32-S3-Touch-LCD-7 — 800x480 RGB + GT911
static const uint16_t LCD_H_RES = 800;
static const uint16_t LCD_V_RES = 480;

bool display_board_init();
void display_board_lvgl_lock();
void display_board_lvgl_unlock();
void display_board_loop();
