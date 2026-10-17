#include "idle_photo.h"

#include "display_board.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

namespace {

lv_img_dsc_t dsc_{};
uint8_t *pixels_ = nullptr;
bool ready_ = false;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void fill_fallback_backdrop(uint16_t *buf) {
  // Soft morning window light — full-bleed backdrop until a real photo is uploaded.
  for (int y = 0; y < LCD_V_RES; ++y) {
    for (int x = 0; x < LCD_H_RES; ++x) {
      const float u = static_cast<float>(x) / static_cast<float>(LCD_H_RES - 1);
      const float v = static_cast<float>(y) / static_cast<float>(LCD_V_RES - 1);
      // Cool slate floor → warm window glow toward top-right
      const int r = static_cast<int>(28 + 70 * u + 40 * (1.0f - v));
      const int g = static_cast<int>(36 + 55 * u + 25 * (1.0f - v));
      const int b = static_cast<int>(48 + 35 * (1.0f - u) + 20 * v);
      const int vignette = static_cast<int>(18 * ((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)));
      buf[y * LCD_H_RES + x] = rgb565(
          static_cast<uint8_t>(constrain(r - vignette, 0, 255)),
          static_cast<uint8_t>(constrain(g - vignette, 0, 255)),
          static_cast<uint8_t>(constrain(b - vignette / 2, 0, 255)));
    }
  }
}

bool load_from_littlefs(uint16_t *buf) {
  if (!LittleFS.begin(true)) {
    Serial.println("[idle] LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists("/idle.rgb565")) {
    Serial.println("[idle] no /idle.rgb565 — using generated backdrop");
    return false;
  }
  File f = LittleFS.open("/idle.rgb565", "r");
  if (!f) {
    return false;
  }
  const size_t need = static_cast<size_t>(LCD_H_RES) * LCD_V_RES * sizeof(uint16_t);
  const size_t got = f.read(reinterpret_cast<uint8_t *>(buf), need);
  f.close();
  if (got != need) {
    Serial.printf("[idle] /idle.rgb565 size mismatch (%u / %u)\n", static_cast<unsigned>(got),
                  static_cast<unsigned>(need));
    return false;
  }
  Serial.println("[idle] loaded /idle.rgb565");
  return true;
}

}  // namespace

bool idle_photo_begin(lv_img_dsc_t *out_dsc) {
  if (ready_ && pixels_) {
    *out_dsc = dsc_;
    return true;
  }

  const size_t bytes = static_cast<size_t>(LCD_H_RES) * LCD_V_RES * sizeof(uint16_t);
  pixels_ = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!pixels_) {
    pixels_ = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_DEFAULT));
  }
  if (!pixels_) {
    Serial.println("[idle] pixel buffer alloc failed");
    return false;
  }

  auto *buf = reinterpret_cast<uint16_t *>(pixels_);
  if (!load_from_littlefs(buf)) {
    fill_fallback_backdrop(buf);
  }

  dsc_.header.always_zero = 0;
  dsc_.header.w = LCD_H_RES;
  dsc_.header.h = LCD_V_RES;
  dsc_.header.cf = LV_IMG_CF_TRUE_COLOR;
  dsc_.data_size = bytes;
  dsc_.data = pixels_;
  ready_ = true;
  *out_dsc = dsc_;
  return true;
}

void idle_photo_end() {
  if (pixels_) {
    free(pixels_);
    pixels_ = nullptr;
  }
  ready_ = false;
}
