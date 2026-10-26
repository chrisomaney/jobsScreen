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
  //   {"jobs":{"<id>":{"kids":[...],"required":true,"label":"...","cat":"paid"}}}
  //   {"add":{"label":"pack lunch","cat":"expectation","required":true,"kids":["jax"]}}
  //   {"remove":["custom_0"]}
  // Partial updates allowed; unknown ids ignored. Returns true if anything changed.
  bool applyConfigJson(const char *json, size_t len);
  // Full config + kid/job catalogue for the web app (kids/config/state).
  String configStateJson() const;
  uint32_t configRevision() const { return config_revision_; }

  bool isDone(size_t kid, size_t job) const;
  void setDone(size_t kid, size_t job, bool done);
  void resetKid(size_t kid);
  void resetAll();

  size_t assignedCount(size_t kid) const;
  size_t doneCount(size_t kid) const;
  size_t remainingCount(size_t kid) const { return assignedCount(kid) - doneCount(kid); }
  size_t requiredTotal(size_t kid) const;
  size_t requiredDoneCount(size_t kid) const;
  bool internetReady(size_t kid) const;

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

private:
  bool done_[KID_COUNT][JOB_COUNT]{};
  uint32_t rewards_[KID_COUNT]{};
  uint32_t job_day_ = 0;
  Preferences prefs_;
  Preferences prize_prefs_;
  PrizeDef prizes_[PRIZE_MAX]{};
  size_t prize_count_ = 0;
  Splash splash_{};
  uint32_t revision_ = 1;
  uint32_t config_revision_ = 1;
  void persistConfig();
  void loadConfig();
  void bump() { ++revision_; }
  void persistKid(size_t kid);
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
