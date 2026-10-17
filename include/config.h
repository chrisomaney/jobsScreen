#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Kids — edit names / slugs to match your family
// Slugs are used in MQTT topics (lowercase, no spaces)
// ---------------------------------------------------------------------------
struct KidProfile {
  const char *name;   // shown on screen
  const char *slug;   // MQTT path segment
};

static const KidProfile KIDS[] = {
  {"Xavier",  "xavier"},
  {"Presley", "presley"},
  {"Jax",     "jax"},
  {"Banx",    "banx"},
};
static const size_t KID_COUNT = sizeof(KIDS) / sizeof(KIDS[0]);

// ---------------------------------------------------------------------------
// Jobs from your spreadsheet
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
};

static const JobDef JOBS[] = {
  // Expectations
  {"make_bed",           "make bed",            "", JobCategory::Expectation, true},
  {"tidy_floor",         "tidy floor",          "", JobCategory::Expectation, true},
  {"washing_in_baskets", "washing in baskets",  "", JobCategory::Expectation, true},
  {"towels_on_hooks",    "towels on hooks",     "", JobCategory::Expectation, true},
  {"brush_teeth",        "brush teeth",         "", JobCategory::Expectation, true},
  {"get_dressed",        "get dressed",         "", JobCategory::Expectation, true},
  {"socks_on",           "socks on",            "", JobCategory::Expectation, true},
  {"medication",         "medication",          "", JobCategory::Expectation, true},
  {"loungeroom_tidy",    "loungeroom tidy",     "", JobCategory::Expectation, true},
  {"table_cleared",      "table cleared",       "", JobCategory::Expectation, true},
  {"lights_fans_off",    "lights and fans off", "", JobCategory::Expectation, true},

  // Paid jobs
  {"feed_cat",     "feed cat",     "morning and night, check water every time", JobCategory::Paid, true},
  {"feed_dog",     "feed dog",     "morning and night, check water every time", JobCategory::Paid, true},
  {"dishes_away",  "dishes away",  "put all dishes away", JobCategory::Paid, true},
  {"dishes_wash",  "dishes wash",  "wash all dishes", JobCategory::Paid, true},
  {"water_plants", "water plants", "water all 4 plants upstairs", JobCategory::Paid, true},
  {"bins_out",     "bins out",     "recycling + kitchen bin to street bin (every 2nd day / Sun night put bins out)", JobCategory::Paid, true},
  {"bathroom",     "bathroom",     "wipe vanity, bath edges, mirror — put everything away", JobCategory::Paid, true},
};
static const size_t JOB_COUNT = sizeof(JOBS) / sizeof(JOBS[0]);

// ---------------------------------------------------------------------------
// Local time — leftover / help screen unlocks at 7:40am
// POSIX TZ examples:
//   Queensland (no DST): "AEST-10"
//   NSW/VIC:             "AEST-10AEDT,M10.1.0,M4.1.0/3"
// ---------------------------------------------------------------------------
static const char *TIMEZONE_POSIX = "AEST-10";
static const int LEFTOVER_OPENS_HOUR = 7;
static const int LEFTOVER_OPENS_MINUTE = 40;

// ---------------------------------------------------------------------------
// MQTT topic layout (all retained so iOS admin + HA see last state)
//
//   kids/{slug}/job/{job_id}/state     payload: "done" | "todo"
//   kids/{slug}/internet               payload: "on" | "off"  (own list complete)
//   kids/{slug}/progress               payload: JSON {"done":n,"total":m,"pct":p}
//   kids/{slug}/rewards/tally          payload: integer count (help credits)
//   kids/rewards/event                 payload: JSON doer/owner/job (not retained)
//   kids/panel/status                  payload: "online" / "offline" (LWT)
//
// Admin commands (subscribe):
//   kids/{slug}/cmd/reset              payload: anything → clear that kid's jobs
//   kids/cmd/reset_all                 payload: anything → clear everyone
//   kids/cmd/reset_rewards             payload: anything → zero reward tallies
// ---------------------------------------------------------------------------
static const char *MQTT_BASE = "kids";
static const char *MQTT_STATUS_TOPIC = "kids/panel/status";
static const char *MQTT_REWARDS_EVENT = "kids/rewards/event";

// UI colours (RGB565-friendly hex for LVGL)
static const uint32_t COL_BG         = 0x1A2332;
static const uint32_t COL_CARD       = 0x243447;
static const uint32_t COL_EXPECT     = 0x3B82F6;  // blue — Expectations
static const uint32_t COL_PAID       = 0xF59E0B;  // amber — Paid jobs
static const uint32_t COL_DONE       = 0x22C55E;
static const uint32_t COL_TEXT       = 0xF8FAFC;
static const uint32_t COL_MUTED      = 0x94A3B8;
static const uint32_t COL_BTN        = 0x334155;
static const uint32_t COL_BTN_ACTIVE = 0x16A34A;

// How often to re-publish retained state (ms) — keeps HA fresh after broker restart
static const uint32_t MQTT_REPUBLISH_MS = 60000;

// Touch debounce for Done buttons (ms)
static const uint32_t DONE_DEBOUNCE_MS = 400;

// Photo / idle screen — always on at low backlight; return here after inactivity
static const uint32_t IDLE_TIMEOUT_MS = 3UL * 60UL * 1000UL;  // 3 minutes
static const uint8_t BACKLIGHT_IDLE_PERCENT = 8;               // lowest comfortable
static const uint8_t BACKLIGHT_ACTIVE_PERCENT = 90;
