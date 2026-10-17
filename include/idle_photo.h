#pragma once

#include <lvgl.h>
#include <stdint.h>

// Prepares an 800x480 RGB565 image for the always-on idle screen.
// Prefers LittleFS /idle.rgb565 (drop in via tools/make_idle_photo.py + uploadfs).
// Falls back to a soft generated backdrop if no file is present.
bool idle_photo_begin(lv_img_dsc_t *out_dsc);
void idle_photo_end();
