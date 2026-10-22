#pragma once

#include <Arduino.h>

struct KidProfile {
  const char *name;
  const char *slug;
  uint32_t color;  // big button colour on the picker
};

// Keep in sync with the jobs panel kids list.
static const KidProfile KIDS[] = {
  {"Xavier",  "xavier",  0x3B82F6},  // bright blue
  {"Presley", "presley", 0xEC4899},  // pink
  {"Jax",     "jax",     0xF59E0B},  // orange
  {"Banx",    "banx",    0x22C55E},  // green
  {"Hazel",   "hazel",   0xA855F7},  // violet
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
static const uint32_t CAROUSEL_MS = 5000;
static const uint32_t SPLASH_MS = 7500;
static const uint32_t CLAIM_DEBOUNCE_MS = 600;

// Bright playground palette
static const uint32_t COL_BG = 0xFFF4E0;       // warm cream
static const uint32_t COL_SKY = 0x7DD3FC;      // sky blue band
static const uint32_t COL_CARD = 0xFFFFFF;
static const uint32_t COL_ACCENT = 0xF97316;   // juicy orange
static const uint32_t COL_COST = 0xEAB308;     // sunny yellow
static const uint32_t COL_WIN = 0x4ADE80;      // lime win
static const uint32_t COL_BONUS = 0xFB7185;    // candy pink bonus
static const uint32_t COL_KEEP = 0x60A5FA;     // soft blue keep-going
static const uint32_t COL_TEXT = 0x1E293B;
static const uint32_t COL_MUTED = 0x64748B;
static const uint32_t COL_BTN_BACK = 0xCBD5E1;
