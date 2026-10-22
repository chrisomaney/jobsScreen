#include "job_state.h"

#include <ArduinoJson.h>

namespace {
uint8_t g_mask[JOB_COUNT];
bool g_required[JOB_COUNT];

int jobIndex(const char *id) {
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (strcmp(JOBS[j].id, id) == 0) {
      return static_cast<int>(j);
    }
  }
  return -1;
}

int kidIndex(const char *slug) {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    if (strcmp(KIDS[k].slug, slug) == 0) {
      return static_cast<int>(k);
    }
  }
  return -1;
}

// Parses a {"jobs":{...}} document into the given arrays. Returns true if they changed.
bool applyJobsObject(JsonObjectConst jobs, uint8_t *mask, bool *required) {
  bool changed = false;
  for (JsonPairConst kv : jobs) {
    const int j = jobIndex(kv.key().c_str());
    if (j < 0) {
      continue;
    }
    JsonObjectConst o = kv.value().as<JsonObjectConst>();
    if (o["kids"].is<JsonArrayConst>()) {
      uint8_t m = 0;
      for (JsonVariantConst v : o["kids"].as<JsonArrayConst>()) {
        const int k = v.is<const char *>() ? kidIndex(v.as<const char *>()) : -1;
        if (k >= 0) {
          m |= static_cast<uint8_t>(1u << k);
        }
      }
      if (m != mask[j]) {
        mask[j] = m;
        changed = true;
      }
    }
    if (o["required"].is<bool>()) {
      const bool r = o["required"].as<bool>();
      if (r != required[j]) {
        required[j] = r;
        changed = true;
      }
    }
  }
  return changed;
}

uint8_t allKidsMask() { return static_cast<uint8_t>((1u << KID_COUNT) - 1u); }
}  // namespace

String JobState::key(size_t kid) {
  return String("k") + String(kid);
}

void JobState::begin() {
  prefs_.begin("jobs", false);
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    g_mask[j] = JOBS[j].kids & allKidsMask();
    g_required[j] = JOBS[j].required_for_internet;
  }
  loadConfig();
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
  return (g_mask[job] >> kid) & 1u;
}

bool JobState::isRequired(size_t job) { return job < JOB_COUNT && g_required[job]; }

uint8_t JobState::assignedMask(size_t job) { return job < JOB_COUNT ? g_mask[job] : 0; }

void JobState::loadConfig() {
  const String saved = prefs_.getString("cfg", "");
  if (saved.isEmpty()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, saved) != DeserializationError::Ok) {
    Serial.println("[jobs] saved config unreadable — using defaults");
    return;
  }
  applyJobsObject(doc["jobs"].as<JsonObjectConst>(), g_mask, g_required);

  // Kids added in firmware after the panel last saved (e.g. Hazel) are missing
  // from the JSON. Give them the compile-time default assignments once.
  bool added = false;
  for (size_t k = 0; k < KID_COUNT; ++k) {
    if (saved.indexOf(KIDS[k].slug) >= 0) {
      continue;
    }
    for (size_t j = 0; j < JOB_COUNT; ++j) {
      if (JOBS[j].kids & (1u << k)) {
        const uint8_t next = static_cast<uint8_t>(g_mask[j] | (1u << k));
        if (next != g_mask[j]) {
          g_mask[j] = next;
          added = true;
        }
      }
    }
  }
  if (added) {
    Serial.println("[jobs] added newly configured kids to saved assignments");
    persistConfig();
  }
}

void JobState::persistConfig() {
  JsonDocument doc;
  JsonObject jobs = doc["jobs"].to<JsonObject>();
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    JsonObject o = jobs[JOBS[j].id].to<JsonObject>();
    JsonArray kids = o["kids"].to<JsonArray>();
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if (isAssigned(k, j)) {
        kids.add(KIDS[k].slug);
      }
    }
    o["required"] = g_required[j];
  }
  String out;
  serializeJson(doc, out);
  prefs_.putString("cfg", out);
}

bool JobState::applyConfigJson(const char *json, size_t len) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json, len);
  if (err) {
    Serial.printf("[jobs] bad config JSON: %s\n", err.c_str());
    return false;
  }
  if (!applyJobsObject(doc["jobs"].as<JsonObjectConst>(), g_mask, g_required)) {
    return false;
  }
  persistConfig();
  ++config_revision_;
  bump();
  return true;
}

String JobState::configStateJson() const {
  JsonDocument doc;
  doc["v"] = 1;
  JsonArray kids = doc["kids"].to<JsonArray>();
  for (size_t k = 0; k < KID_COUNT; ++k) {
    JsonObject o = kids.add<JsonObject>();
    o["slug"] = KIDS[k].slug;
    o["name"] = KIDS[k].name;
  }
  JsonArray jobs = doc["jobs"].to<JsonArray>();
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    JsonObject o = jobs.add<JsonObject>();
    o["id"] = JOBS[j].id;
    o["label"] = JOBS[j].label;
    o["cat"] = JOBS[j].category == JobCategory::Paid ? "paid" : "expectation";
    o["required"] = g_required[j];
    JsonArray who = o["kids"].to<JsonArray>();
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if (isAssigned(k, j)) {
        who.add(KIDS[k].slug);
      }
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
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
    if (isRequired(j) && isAssigned(kid, j)) {
      ++n;
    }
  }
  return n;
}

size_t JobState::requiredDoneCount(size_t kid) const {
  size_t n = 0;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (isRequired(j) && isDone(kid, j)) {
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
