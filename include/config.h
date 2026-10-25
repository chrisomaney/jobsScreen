#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Kids — edit names / slugs to match your family
// Slugs are used in MQTT topics (lowercase, no spaces)
// ---------------------------------------------------------------------------
struct KidProfile {
  const char *name;   // shown on screen
  const char *slug;   // MQTT path segment
  uint32_t color;     // prize chips / picker rim
};

static const KidProfile KIDS[] = {
  {"Xavier",  "xavier",  0x3B82F6},
  {"Presley", "presley", 0xEC4899},
  {"Jax",     "jax",     0xF59E0B},
  {"Banx",    "banx",    0x22C55E},
  {"Hazel",   "hazel",   0xA855F7},
};

struct PrizeDef {
  char id[24];
  char name[48];
  uint16_t cost;
};
static const size_t PRIZE_MAX = 16;
static const size_t KID_COUNT = sizeof(KIDS) / sizeof(KIDS[0]);
static_assert(KID_COUNT <= 8, "kid masks are 8-bit");

// Bit masks for assigning jobs to kids (bit index = position in KIDS[]).
// Combine with |, e.g. KID_XAVIER | KID_JAX.
static const uint8_t KID_XAVIER  = 1u << 0;
static const uint8_t KID_PRESLEY = 1u << 1;
static const uint8_t KID_JAX     = 1u << 2;
static const uint8_t KID_BANX    = 1u << 3;
static const uint8_t KID_HAZEL   = 1u << 4;
static const uint8_t ALL_KIDS    = 0xFF;

// ---------------------------------------------------------------------------
// Jobs from your spreadsheet
// Last column = which kids have the job. Every job is currently on every
// kid's list (ALL_KIDS). To give a job to particular kids only, e.g.:
//   {"bins_out", ..., JobCategory::Paid, true, KID_XAVIER | KID_PRESLEY},
// ---------------------------------------------------------------------------
enum class JobCategory : uint8_t {
  Expectation = 0,
  Paid = 1,
};

struct JobDef {
  const char *id;          // stable MQTT key
  const char *label;       // short UI label
  const char *detail;      // optional longer hint (may be empty)
  JobCategory category;
  bool required_for_internet;  // must be done to unlock MikroTik profile
  uint8_t kids;                // who has this job on their list (ALL_KIDS or KID_* mask)
};

static const JobDef JOBS[] = {
  // Expectations
  {"make_bed",           "make bed",            "", JobCategory::Expectation, true, ALL_KIDS},
  {"tidy_floor",         "tidy floor",          "", JobCategory::Expectation, true, ALL_KIDS},
  {"washing_in_baskets", "washing in baskets",  "", JobCategory::Expectation, true, ALL_KIDS},
  {"towels_on_hooks",    "towels on hooks",     "", JobCategory::Expectation, true, ALL_KIDS},
  {"brush_teeth",        "brush teeth",         "", JobCategory::Expectation, true, ALL_KIDS},
  {"get_dressed",        "get dressed",         "", JobCategory::Expectation, true, ALL_KIDS},
  {"socks_on",           "socks on",            "", JobCategory::Expectation, true, ALL_KIDS},
  {"medication",         "medication",          "", JobCategory::Expectation, true, ALL_KIDS},
  {"loungeroom_tidy",    "loungeroom tidy",     "", JobCategory::Expectation, true, ALL_KIDS},
  {"table_cleared",      "table cleared",       "", JobCategory::Expectation, true, ALL_KIDS},
  {"lights_fans_off",    "lights and fans off", "", JobCategory::Expectation, true, ALL_KIDS},

  // Paid jobs
  {"feed_cat",     "feed cat",     "morning and night, check water every time", JobCategory::Paid, true, ALL_KIDS},
  {"feed_dog",     "feed dog",     "morning and night, check water every time", JobCategory::Paid, true, ALL_KIDS},
  {"dishes_away",  "dishes away",  "put all dishes away", JobCategory::Paid, true, ALL_KIDS},
  {"dishes_wash",  "dishes wash",  "wash all dishes", JobCategory::Paid, true, ALL_KIDS},
  {"water_plants", "water plants", "water all 4 plants upstairs", JobCategory::Paid, true, ALL_KIDS},
  {"bins_out",     "bins out",     "recycling + kitchen bin to street bin (every 2nd day / Sun night put bins out)", JobCategory::Paid, true, ALL_KIDS},
  {"bathroom",     "bathroom",     "wipe vanity, bath edges, mirror - put everything away", JobCategory::Paid, true, ALL_KIDS},
};
// Built-in jobs from the list above. Extra slots (custom_0 …) can be filled from the
// Jobs Chooser web app without reflashing — see MAX_CUSTOM_JOBS.
static const size_t BUILTIN_JOB_COUNT = sizeof(JOBS) / sizeof(JOBS[0]);
static const size_t MAX_CUSTOM_JOBS = 8;
static const size_t JOB_COUNT = BUILTIN_JOB_COUNT + MAX_CUSTOM_JOBS;
static const size_t JOB_LABEL_MAX = 40;
static const size_t JOB_DETAIL_MAX = 80;

// ---------------------------------------------------------------------------
// Local time — leftover / help screen unlocks at 7:40am
// POSIX TZ examples:
//   Queensland (no DST): "AEST-10"
//   NSW/VIC:             "AEST-10AEDT,M10.1.0,M4.1.0/3"
// ---------------------------------------------------------------------------
static const char *TIMEZONE_POSIX = "AEST-10";
static const int LEFTOVER_OPENS_HOUR = 7;
static const int LEFTOVER_OPENS_MINUTE = 40;

// The panel resets everyone's ticks once a day by itself at this local time
// (rewards tallies are kept). Works even if Home Assistant / MQTT is down.
static const int DAILY_RESET_HOUR = 3;
static const int DAILY_RESET_MINUTE = 0;

// ---------------------------------------------------------------------------
// MQTT topic layout (all retained so iOS admin + HA see last state)
//
//   kids/{slug}/job/{job_id}/state     payload: "done" | "todo"
//   kids/{slug}/internet               payload: "on" | "off"  (own list complete)
//   kids/{slug}/progress               payload: JSON {"done":n,"total":m,"pct":p}
//   kids/{slug}/rewards/tally          payload: integer count (help credits)
//   kids/rewards/event                 payload: JSON doer/owner/job (not retained)
//   kids/rewards/catalog               retained prize list JSON
//   kids/rewards/catalog/set           admin → update prizes
//   kids/rewards/bonus/set             admin → {kid, points}
//   kids/rewards/claim                 claim event (not retained)
//   kids/rewards/celebrate             phone/TV splash JSON
//   kids/panel/status                  payload: "online" / "offline" (LWT)
//
// Admin commands (subscribe):
//   kids/{slug}/cmd/reset              payload: any non-empty → clear that kid's jobs
//   kids/cmd/reset_all                 payload: any non-empty → clear everyone
//   kids/cmd/reset_rewards             payload: any non-empty → zero reward tallies
// (Retained messages on these topics are cleared on connect so a stray
//  retained reset can't wipe everything on every reconnect.)
// ---------------------------------------------------------------------------
static const char *MQTT_BASE = "kids";
static const char *MQTT_STATUS_TOPIC = "kids/panel/status";
static const char *MQTT_REWARDS_EVENT = "kids/rewards/event";
static const char *MQTT_CATALOG_TOPIC = "kids/rewards/catalog";
static const char *MQTT_CATALOG_SET = "kids/rewards/catalog/set";
static const char *MQTT_CLAIM_TOPIC = "kids/rewards/claim";
static const char *MQTT_BONUS_TOPIC = "kids/rewards/bonus";
static const char *MQTT_BONUS_SET = "kids/rewards/bonus/set";
static const char *MQTT_CELEBRATE_TOPIC = "kids/rewards/celebrate";
static const char *MQTT_RULES_STATE = "kids/rewards/rules/state";
static const char *MQTT_RULES_SET = "kids/rewards/rules/set";
static const char *MQTT_PANEL_MESSAGE_SET = "kids/panel/message/set";
static const size_t REWARD_RULE_MAX = 16;
// Web app job chooser (see webapp/):
//   kids/config/state  retained JSON — kids, jobs, who has each job, required flags,
//                      customSlotsFree, and custom:true on Jobs Chooser–created items
//   kids/config/set    from the web app, any of:
//     {"jobs":{"feed_cat":{"kids":["jax"],"required":true}}}
//     {"add":{"label":"pack lunch","cat":"expectation"|"paid","required":true,"kids":[...]}}
//     {"remove":["custom_0"]}
static const char *MQTT_CONFIG_STATE = "kids/config/state";
static const char *MQTT_CONFIG_SET = "kids/config/set";

// UI colours (RGB565-friendly hex for LVGL) — jobs screens
static const uint32_t COL_BG         = 0x1A2332;
static const uint32_t COL_CARD       = 0x243447;
static const uint32_t COL_EXPECT     = 0x3B82F6;  // blue — Expectations
static const uint32_t COL_PAID       = 0xF59E0B;  // amber — Paid jobs
static const uint32_t COL_DONE       = 0x22C55E;
static const uint32_t COL_TEXT       = 0xF8FAFC;
static const uint32_t COL_MUTED      = 0x94A3B8;
static const uint32_t COL_BTN        = 0x334155;
static const uint32_t COL_BTN_ACTIVE = 0x16A34A;

// Prize idle / neon station palette
static const uint32_t COL_PRIZE_BG = 0x070B16;
static const uint32_t COL_PRIZE_BG2 = 0x101A2E;
static const uint32_t COL_PRIZE_SKY = 0x0E1A33;
static const uint32_t COL_PRIZE_CARD = 0x121C33;
static const uint32_t COL_PRIZE_ACCENT = 0x22D3EE;
static const uint32_t COL_PRIZE_ACCENT2 = 0x67E8F9;
static const uint32_t COL_PRIZE_COST = 0xFDE047;
static const uint32_t COL_PRIZE_WIN = 0x10B981;
static const uint32_t COL_PRIZE_BONUS = 0xF43F5E;
static const uint32_t COL_PRIZE_KEEP = 0x3B82F6;
static const uint32_t COL_PRIZE_LINE = 0x1E3A5F;
static const uint32_t COL_PRIZE_BTN_BACK = 0x1E293B;

// How often to re-publish retained state (ms) — keeps HA fresh after broker restart
static const uint32_t MQTT_REPUBLISH_MS = 60000;

// Touch debounce for Done buttons (ms, per button)
static const uint32_t DONE_DEBOUNCE_MS = 400;
static const uint32_t CLAIM_DEBOUNCE_MS = 600;

// Idle = prize station; return here after jobs inactivity
static const uint32_t IDLE_TIMEOUT_MS = 3UL * 60UL * 1000UL;  // 3 minutes
static const uint32_t CAROUSEL_MS = 5000;
static const uint32_t SPLASH_MS = 7500;

// Legacy: photo idle brightness (unused when prize station is idle)
static const uint8_t IDLE_PHOTO_BRIGHTNESS_PCT = 100;
