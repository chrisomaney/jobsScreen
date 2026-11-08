#pragma once

#include "config.h"
#include <Preferences.h>

// Per-kid done flags + reward tallies in RAM + NVS.
// All counts only consider jobs assigned to that kid.
// Built-in jobs come from JOBS[] in config.h; up to MAX_CUSTOM_JOBS extra jobs can
// be created from the Jobs Chooser (MQTT kids/config/set with an "add" object).
class JobState {
public:
  void begin();

  // Who-does-what + "needed for internet" are editable at runtime (web app via
  // MQTT kids/config/set). Defaults come from JOBS[] in config.h.
  static bool isAssigned(size_t kid, size_t job);
  static bool isRequired(size_t job);
  static uint8_t assignedMask(size_t job);

  // Catalogue accessors — use these instead of JOBS[j] so custom jobs work.
  static bool isCustom(size_t job);
  static bool isDefined(size_t job);  // built-in, or custom with a label
  static const char *jobId(size_t job);
  static const char *jobLabel(size_t job);
  static const char *jobDetail(size_t job);
  static JobCategory jobCategory(size_t job);

  // Apply config JSON from the web app. Supports:
  //   {"jobs":{"<id>":{"kids":[...],"required":true,"label":"...","detail":"...","cat":"paid"}}}
  //   {"add":{"label":"pack lunch","cat":"expectation","required":true,"kids":["jax"]}}
  //   {"remove":["custom_0"]}
  //   {"pins":{"xavier":"1234","presley":""}}  // 4 digits or "" to clear
  // Built-ins accept label/detail/cat overrides (saved to NVS). Custom slots only for add/remove.
  // Partial updates allowed; unknown ids ignored. Returns true if anything changed.
  bool applyConfigJson(const char *json, size_t len);
  // Full config + kid/job catalogue for the web app (kids/config/state).
  String configStateJson() const;
  uint32_t configRevision() const { return config_revision_; }

  // Per-kid 4-digit unlock PIN (empty = no pad on Home).
  bool hasPin(size_t kid) const;
  const char *pin(size_t kid) const;
  bool checkPin(size_t kid, const char *entered) const;
  // pin must be "" or exactly 4 digits. Returns true if stored value changed.
  bool setPin(size_t kid, const char *pin);

  bool isDone(size_t kid, size_t job) const;
  void setDone(size_t kid, size_t job, bool done);
  void resetKid(size_t kid);
  void resetAll();

  size_t assignedCount(size_t kid) const;
  size_t doneCount(size_t kid) const;
  size_t remainingCount(size_t kid) const { return assignedCount(kid) - doneCount(kid); }
  size_t requiredTotal(size_t kid) const;
  size_t requiredDoneCount(size_t kid) const;
  bool internetReady(size_t kid) const;  // based on required jobs only

  // Parent override for MikroTik unlock. Auto = follow internetReady().
  enum class InternetOverride : uint8_t { Auto = 0, On = 1, Off = 2 };
  InternetOverride internetOverride(size_t kid) const;
  void setInternetOverride(size_t kid, InternetOverride mode);
  // Effective on/off published to kids/{slug}/internet.
  // Priority: Force Off → scheduled off (unless Force On) → Force On → jobs Auto.
  bool internetEffective(size_t kid) const;
  bool internetScheduledOff(size_t kid) const;

  // Bedtime-style windows: off_time → on_time for selected kids/days.
  struct NetSchedule {
    char id[20]{};
    char label[40]{};
    uint8_t kids_mask = 0xFF;
    uint8_t days = 0x7F;  // bit0=Sun … bit6=Sat
    uint8_t off_hour = 20;
    uint8_t off_minute = 0;
    uint8_t on_hour = 7;
    uint8_t on_minute = 0;
    bool enabled = true;
  };
  size_t netScheduleCount() const { return net_schedule_count_; }
  const NetSchedule &netSchedule(size_t i) const { return net_schedules_[i]; }
  bool applyInternetSchedulesJson(const char *json);
  String internetSchedulesJson() const;
  // Call from loop; returns kid bitmask whose effective internet may have changed.
  uint8_t tickInternetSchedules();

  uint32_t rewardTally(size_t kid) const;
  void addReward(size_t doer);
  void setRewardTally(size_t kid, uint32_t value);
  void addBonus(size_t kid, uint32_t points);
  // Add points without a splash (scheduled rules); still bumps revision.
  void addPoints(size_t kid, uint32_t points);
  // Signed adjust (negative = take points off). Clamps at 0. Optional splash.
  void adjustPoints(size_t kid, int32_t delta, bool splash);
  void resetRewards();
  static int findJobIndex(const char *id);

  // Prize shop (idle screen) — catalog in NVS "prizes"; tallies share rewards_[]
  size_t prizeCount() const { return prize_count_; }
  const PrizeDef &prize(size_t i) const { return prizes_[i]; }
  bool setCatalogFromJson(const char *json);
  String catalogToJson() const;
  bool tryClaimPrize(size_t kid, size_t prize_index, uint32_t *tally_before, uint32_t *tally_after);

  enum class SplashKind : uint8_t { None = 0, Winner, Bonus, KeepGoing };
  struct Splash {
    SplashKind kind = SplashKind::None;
    size_t kid = 0;
    char title[40]{};
    char subtitle[64]{};
  };
  const Splash &pendingSplash() const { return splash_; }
  void clearSplash() { splash_.kind = SplashKind::None; }
  void queueSplash(SplashKind kind, size_t kid, const char *title, const char *subtitle);

  // Daily rollover. Returns true if the day changed and all jobs were reset.
  // The first call after flashing just records the day (no reset).
  bool rollDay(uint32_t job_day);

  uint32_t revision() const { return revision_; }

  // Flush at most one pending NVS write per call so Save-from-app doesn't stall
  // the RGB panel. Returns true if a write was performed.
  bool loopPersist();

private:
  bool done_[KID_COUNT][JOB_COUNT]{};
  uint32_t rewards_[KID_COUNT]{};
  InternetOverride internet_override_[KID_COUNT]{};
  NetSchedule net_schedules_[INTERNET_SCHEDULE_MAX]{};
  size_t net_schedule_count_ = 0;
  bool scheduled_off_cache_[KID_COUNT]{};
  uint32_t job_day_ = 0;
  Preferences prefs_;
  Preferences prize_prefs_;
  Preferences net_prefs_;
  Preferences net_sched_prefs_;
  void loadInternetOverrides();
  void persistInternetOverride(size_t kid);
  void loadInternetSchedules();
  void saveInternetSchedules();
  bool scheduleActiveNow(const NetSchedule &s, int hour, int minute, int wday) const;
  PrizeDef prizes_[PRIZE_MAX]{};
  size_t prize_count_ = 0;
  Splash splash_{};
  uint32_t revision_ = 1;
  uint32_t config_revision_ = 1;
  bool persist_config_pending_ = false;
  bool persist_catalog_pending_ = false;
  bool persist_net_sched_pending_ = false;
  bool persist_kid_pending_[KID_COUNT]{};
  char pins_[KID_COUNT][5]{};  // 4 digits + NUL; empty = unlocked
  void persistConfig();
  void loadConfig();
  void persistPins();
  void loadPins();
  void bump() { ++revision_; }
  void persistKid(size_t kid);
  void markPersistKid(size_t kid);
  void loadKid(size_t kid);
  void persistReward(size_t kid);
  void loadRewards();
  void loadPrizeDefaults();
  void loadPrizeCatalog();
  void savePrizeCatalog();
  static String key(size_t kid);
  bool clearCustomJob(size_t job);
  int allocCustomSlot();
};
