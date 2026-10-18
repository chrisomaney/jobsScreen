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
}

void JobState::loadKid(size_t kid) {
  if (kid >= KID_COUNT) {
    return;
  }
  // Prefer isKey so first boot does not spam Preferences ERROR logs for missing keys.
  const String k = key(kid);
  String blob;
  if (prefs_.isKey(k.c_str())) {
    blob = prefs_.getString(k.c_str(), "");
  }
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

void JobState::persistRewards() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    String rk = String("rw") + String(k);
    prefs_.putUInt(rk.c_str(), rewards_[k]);
  }
}

bool JobState::isDone(size_t kid, size_t job) const {
  if (kid >= KID_COUNT || job >= JOB_COUNT) {
    return false;
  }
  return done_[kid][job];
}

void JobState::setDone(size_t kid, size_t job, bool done) {
  if (kid >= KID_COUNT || job >= JOB_COUNT) {
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

size_t JobState::doneCount(size_t kid) const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (isDone(kid, j)) {
      ++n;
    }
  }
  return n;
}

size_t JobState::requiredTotal() const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (JOBS[j].required_for_internet) {
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
  return requiredDoneCount(kid) >= requiredTotal() && requiredTotal() > 0;
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
  persistRewards();
  bump();
}

void JobState::resetRewards() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    rewards_[k] = 0;
  }
  persistRewards();
  bump();
}
