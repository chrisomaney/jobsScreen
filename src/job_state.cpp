#include "job_state.h"

String JobState::key(size_t kid) {
  return String("k") + String(kid);
}

void JobState::begin() {
  prefs_.begin("jobs", false);
  for (size_t k = 0; k < KID_COUNT; ++k) {
    loadKid(k);
  }
  loadRewards();
  job_day_ = prefs_.getUInt("day", 0);
}

bool JobState::isAssigned(size_t kid, size_t job) {
  if (kid >= KID_COUNT || job >= JOB_COUNT) {
    return false;
  }
  return (JOBS[job].kids >> kid) & 1u;
}

bool JobState::rollDay(uint32_t job_day) {
  if (job_day == 0 || job_day == job_day_) {
    return false;
  }
  const bool first = (job_day_ == 0);
  job_day_ = job_day;
  prefs_.putUInt("day", job_day_);
  if (first) {
    return false;  // don't wipe today's ticks right after a firmware update
  }
  resetAll();
  return true;
}

void JobState::loadKid(size_t kid) {
  if (kid >= KID_COUNT) {
    return;
  }
  String blob = prefs_.getString(key(kid).c_str(), "");
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    done_[kid][j] = (j < blob.length() && blob[j] == '1');
  }
}

void JobState::persistKid(size_t kid) {
  if (kid >= KID_COUNT) {
    return;
  }
  String blob;
  blob.reserve(JOB_COUNT);
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    blob += done_[kid][j] ? '1' : '0';
  }
  prefs_.putString(key(kid).c_str(), blob);
}

void JobState::loadRewards() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    String rk = String("rw") + String(k);
    rewards_[k] = prefs_.getUInt(rk.c_str(), 0);
  }
}

void JobState::persistReward(size_t kid) {
  String rk = String("rw") + String(kid);
  prefs_.putUInt(rk.c_str(), rewards_[kid]);
}

bool JobState::isDone(size_t kid, size_t job) const {
  if (kid >= KID_COUNT || job >= JOB_COUNT) {
    return false;
  }
  return isAssigned(kid, job) && done_[kid][job];
}

void JobState::setDone(size_t kid, size_t job, bool done) {
  if (!isAssigned(kid, job)) {
    return;
  }
  if (done_[kid][job] == done) {
    return;
  }
  done_[kid][job] = done;
  persistKid(kid);
  bump();
}

void JobState::resetKid(size_t kid) {
  if (kid >= KID_COUNT) {
    return;
  }
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    done_[kid][j] = false;
  }
  persistKid(kid);
  bump();
}

void JobState::resetAll() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    for (size_t j = 0; j < JOB_COUNT; ++j) {
      done_[k][j] = false;
    }
    persistKid(k);
  }
  bump();
}

size_t JobState::assignedCount(size_t kid) const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (isAssigned(kid, j)) {
      ++n;
    }
  }
  return n;
}

size_t JobState::doneCount(size_t kid) const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (isDone(kid, j)) {
      ++n;
    }
  }
  return n;
}

size_t JobState::requiredTotal(size_t kid) const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JOBS[j].required_for_internet && isAssigned(kid, j)) {
      ++n;
    }
  }
  return n;
}

size_t JobState::requiredDoneCount(size_t kid) const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JOBS[j].required_for_internet && isDone(kid, j)) {
      ++n;
    }
  }
  return n;
}

bool JobState::internetReady(size_t kid) const {
  if (kid >= KID_COUNT) {
    return false;
  }
  // A kid with no required jobs assigned has nothing blocking internet.
  return requiredDoneCount(kid) >= requiredTotal(kid);
}

uint32_t JobState::rewardTally(size_t kid) const {
  if (kid >= KID_COUNT) {
    return 0;
  }
  return rewards_[kid];
}

void JobState::addReward(size_t doer) {
  if (doer >= KID_COUNT) {
    return;
  }
  ++rewards_[doer];
  persistReward(doer);
  bump();
}

void JobState::resetRewards() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    rewards_[k] = 0;
    persistReward(k);
  }
  bump();
}
