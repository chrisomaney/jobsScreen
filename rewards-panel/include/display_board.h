#pragma once

#include <lvgl.h>
#include <stdint.h>

// Waveshare ESP32-S3-Touch-LCD-7 — 800x480 RGB + GT911
static const uint16_t LCD_H_RES = 800;
static const uint16_t LCD_V_RES = 480;

bool display_board_init();

// Runs LVGL. Call from loop() only — all LVGL + UI code runs on the Arduino
// loop task, so no locking is needed (and none must be added around drawing:
// disabling interrupts starves the RGB bounce-buffer ISR and trips the WDT).
void display_board_loop();

// This board's backlight is wired through the CH422G expander as on/off only
// (no PWM), so any percent > 0 means "on" and 0 means "off".
void display_board_set_backlight(uint8_t percent);

// Milliseconds since the last touch (LVGL tracks this; no extra GT911 reads).
uint32_t display_board_inactive_ms();
