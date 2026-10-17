#pragma once

#include "config.h"
#include <Preferences.h>

// Per-kid done flags + reward tallies in RAM + NVS.
class JobState {
public:
  void begin();
  bool isDone(size_t kid, size_t job) const;
  void setDone(size_t kid, size_t job, bool done);
  void resetKid(size_t kid);
  void resetAll();

  size_t doneCount(size_t kid) const;
  size_t requiredDoneCount(size_t kid) const;
  size_t requiredTotal() const;
  bool internetReady(size_t kid) const;

  uint32_t rewardTally(size_t kid) const;
  void addReward(size_t doer);
  void resetRewards();

  uint32_t revision() const { return revision_; }

private:
  bool done_[KID_COUNT][JOB_COUNT]{};
  uint32_t rewards_[KID_COUNT]{};
  Preferences prefs_;
  uint32_t revision_ = 1;
  void bump() { ++revision_; }
  void persistKid(size_t kid);
  void loadKid(size_t kid);
  void persistRewards();
  void loadRewards();
  static String key(size_t kid);
};
