#pragma once

// LVGL custom allocator: prefer PSRAM, fall back to internal RAM.
// Plain C — this header is included from LVGL's C sources.

#include <stddef.h>
#include "esp_heap_caps.h"

static inline void *lv_psram_alloc(size_t size) {
  void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

static inline void *lv_psram_realloc(void *ptr, size_t size) {
  void *p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
}

static inline void lv_psram_free(void *ptr) { heap_caps_free(ptr); }
