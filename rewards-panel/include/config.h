#pragma once

#include <Arduino.h>

struct KidProfile {
  const char *name;
  const char *slug;
};

// Keep in sync with the jobs panel kids list.
static const KidProfile KIDS[] = {
  {"Xavier",  "xavier"},
  {"Presley", "presley"},
  {"Jax",     "jax"},
  {"Banx",    "banx"},
  {"Hazel",   "hazel"},
};
static const size_t KID_COUNT = sizeof(KIDS) / sizeof(KIDS[0]);

struct PrizeDef {
  char id[24];
  char name[48];
  uint16_t cost;
};

static const size_t PRIZE_MAX = 16;

static const char *MQTT_BASE = "kids";
static const char *MQTT_STATUS_TOPIC = "kids/rewards-panel/status";
static const char *MQTT_CATALOG_TOPIC = "kids/rewards/catalog";
static const char *MQTT_CATALOG_SET = "kids/rewards/catalog/set";
static const char *MQTT_CLAIM_TOPIC = "kids/rewards/claim";
static const char *MQTT_BONUS_TOPIC = "kids/rewards/bonus";
static const char *MQTT_BONUS_SET = "kids/rewards/bonus/set";
static const char *MQTT_CELEBRATE_TOPIC = "kids/rewards/celebrate";

static const uint32_t MQTT_REPUBLISH_MS = 60000;
static const uint32_t CAROUSEL_MS = 4500;
static const uint32_t SPLASH_MS = 7000;
static const uint32_t CLAIM_DEBOUNCE_MS = 600;

// Exciting shop-window palette (not purple-glow generic)
static const uint32_t COL_BG = 0x0B1F2A;
static const uint32_t COL_PANEL = 0x123447;
static const uint32_t COL_ACCENT = 0xF4B942;   // gold
static const uint32_t COL_WIN = 0x2DD4A8;      // mint win
static const uint32_t COL_BONUS = 0xFF6B4A;    // coral bonus
static const uint32_t COL_TEXT = 0xF7F4EF;
static const uint32_t COL_MUTED = 0x8FA6B5;
static const uint32_t COL_BTN = 0x1C4A5E;
