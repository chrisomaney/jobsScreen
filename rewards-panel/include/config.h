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
  {"Chris",   "chris",   0x06B6D4},  // cyan
  {"Stacy",   "stacy",   0xF43F5E},  // rose
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

// Neon arcade / futuristic palette (dark + cyan)
static const uint32_t COL_BG = 0x070B16;       // deep space
static const uint32_t COL_BG2 = 0x101A2E;      // gradient end
static const uint32_t COL_SKY = 0x0E1A33;      // header strip
static const uint32_t COL_CARD = 0x121C33;     // glass panel
static const uint32_t COL_ACCENT = 0x22D3EE;   // neon cyan
static const uint32_t COL_ACCENT2 = 0x67E8F9;  // bright cyan press
static const uint32_t COL_COST = 0xFDE047;     // electric gold
static const uint32_t COL_WIN = 0x10B981;      // emerald win
static const uint32_t COL_BONUS = 0xF43F5E;    // hot bonus
static const uint32_t COL_KEEP = 0x3B82F6;     // keep-going blue
static const uint32_t COL_TEXT = 0xF8FAFC;     // near-white
static const uint32_t COL_MUTED = 0x94A3B8;    // steel
static const uint32_t COL_BTN_BACK = 0x1E293B; // slate back
static const uint32_t COL_LINE = 0x1E3A5F;     // grid lines
static const uint32_t COL_GLOW = 0x0891B2;     // cyan rim
