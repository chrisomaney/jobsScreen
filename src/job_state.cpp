#include "job_state.h"

#include <ArduinoJson.h>

namespace {
uint8_t g_mask[JOB_COUNT];
bool g_required[JOB_COUNT];

// Custom job slots live after the built-ins. Empty label = unused.
char g_custom_label[MAX_CUSTOM_JOBS][JOB_LABEL_MAX];
char g_custom_detail[MAX_CUSTOM_JOBS][JOB_DETAIL_MAX];
JobCategory g_custom_cat[MAX_CUSTOM_JOBS];
char g_custom_ids[MAX_CUSTOM_JOBS][12];  // "custom_0" … "custom_7"

size_t customIndex(size_t job) { return job - BUILTIN_JOB_COUNT; }

void initCustomIds() {
  for (size_t i = 0; i < MAX_CUSTOM_JOBS; ++i) {
    snprintf(g_custom_ids[i], sizeof(g_custom_ids[i]), "custom_%u", static_cast<unsigned>(i));
    g_custom_label[i][0] = '\0';
    g_custom_detail[i][0] = '\0';
    g_custom_cat[i] = JobCategory::Expectation;
  }
}

int jobIndex(const char *id) {
  if (!id || !id[0]) {
    return -1;
  }
  for (size_t j = 0; j < BUILTIN_JOB_COUNT; ++j) {
    if (strcmp(JOBS[j].id, id) == 0) {
      return static_cast<int>(j);
    }
  }
  for (size_t i = 0; i < MAX_CUSTOM_JOBS; ++i) {
    if (strcmp(g_custom_ids[i], id) == 0) {
      return static_cast<int>(BUILTIN_JOB_COUNT + i);
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

uint8_t parseKidsArray(JsonArrayConst arr) {
  uint8_t m = 0;
  for (JsonVariantConst v : arr) {
    const int k = v.is<const char *>() ? kidIndex(v.as<const char *>()) : -1;
    if (k >= 0) {
      m |= static_cast<uint8_t>(1u << k);
    }
  }
  return m;
}

JobCategory parseCat(const char *cat) {
  if (cat && (strcmp(cat, "paid") == 0 || strcmp(cat, "job") == 0 || strcmp(cat, "jobs") == 0)) {
    return JobCategory::Paid;
  }
  return JobCategory::Expectation;
}

void copyTrunc(char *dst, size_t dst_len, const char *src) {
  if (!dst || dst_len == 0) {
    return;
  }
  if (!src) {
    dst[0] = '\0';
    return;
  }
  // Trim leading/trailing spaces
  while (*src == ' ' || *src == '\t') {
    ++src;
  }
  size_t n = strlen(src);
  while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\t')) {
    --n;
  }
  if (n >= dst_len) {
    n = dst_len - 1;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

uint8_t allKidsMask() { return static_cast<uint8_t>((1u << KID_COUNT) - 1u); }

// Apply kids/required/label/cat/detail on one job object. Returns true if changed.
bool applyJobFields(size_t j, JsonObjectConst o) {
  bool changed = false;
  if (o["kids"].is<JsonArrayConst>()) {
    const uint8_t m = parseKidsArray(o["kids"].as<JsonArrayConst>());
    if (m != g_mask[j]) {
      g_mask[j] = m;
      changed = true;
    }
  }
  if (o["required"].is<bool>()) {
    const bool r = o["required"].as<bool>();
    if (r != g_required[j]) {
      g_required[j] = r;
      changed = true;
    }
  }
  if (JobState::isCustom(j)) {
    const size_t ci = customIndex(j);
    if (o["label"].is<const char *>()) {
      char buf[JOB_LABEL_MAX];
      copyTrunc(buf, sizeof(buf), o["label"].as<const char *>());
      if (strcmp(buf, g_custom_label[ci]) != 0) {
        memcpy(g_custom_label[ci], buf, JOB_LABEL_MAX);
        changed = true;
      }
    }
    if (o["detail"].is<const char *>()) {
      char buf[JOB_DETAIL_MAX];
      copyTrunc(buf, sizeof(buf), o["detail"].as<const char *>());
      if (strcmp(buf, g_custom_detail[ci]) != 0) {
        memcpy(g_custom_detail[ci], buf, JOB_DETAIL_MAX);
        changed = true;
      }
    }
    if (o["cat"].is<const char *>()) {
      const JobCategory cat = parseCat(o["cat"].as<const char *>());
      if (cat != g_custom_cat[ci]) {
        g_custom_cat[ci] = cat;
        changed = true;
      }
    }
  }
  return changed;
}

bool applyJobsObject(JsonObjectConst jobs) {
  if (jobs.isNull()) {
    return false;
  }
  bool changed = false;
  for (JsonPairConst kv : jobs) {
    const int j = jobIndex(kv.key().c_str());
    if (j < 0) {
      continue;
    }
    if (!kv.value().is<JsonObjectConst>()) {
      continue;
    }
    if (applyJobFields(static_cast<size_t>(j), kv.value().as<JsonObjectConst>())) {
      changed = true;
    }
  }
  return changed;
}
}  // namespace

String JobState::key(size_t kid) {
  return String("k") + String(kid);
}

bool JobState::isCustom(size_t job) { return job >= BUILTIN_JOB_COUNT && job < JOB_COUNT; }

bool JobState::isDefined(size_t job) {
  if (job >= JOB_COUNT) {
    return false;
  }
  if (!isCustom(job)) {
    return true;
  }
  return g_custom_label[customIndex(job)][0] != '\0';
}

const char *JobState::jobId(size_t job) {
  if (job >= JOB_COUNT) {
    return "";
  }
  if (!isCustom(job)) {
    return JOBS[job].id;
  }
  return g_custom_ids[customIndex(job)];
}

const char *JobState::jobLabel(size_t job) {
  if (job >= JOB_COUNT) {
    return "";
  }
  if (!isCustom(job)) {
    return JOBS[job].label;
  }
  return g_custom_label[customIndex(job)];
}

const char *JobState::jobDetail(size_t job) {
  if (job >= JOB_COUNT) {
    return "";
  }
  if (!isCustom(job)) {
    return JOBS[job].detail;
  }
  return g_custom_detail[customIndex(job)];
}

JobCategory JobState::jobCategory(size_t job) {
  if (job >= JOB_COUNT) {
    return JobCategory::Expectation;
  }
  if (!isCustom(job)) {
    return JOBS[job].category;
  }
  return g_custom_cat[customIndex(job)];
}

int JobState::allocCustomSlot() {
  for (size_t i = 0; i < MAX_CUSTOM_JOBS; ++i) {
    if (g_custom_label[i][0] == '\0') {
      return static_cast<int>(BUILTIN_JOB_COUNT + i);
    }
  }
  return -1;
}

bool JobState::clearCustomJob(size_t job) {
  if (!isCustom(job) || !isDefined(job)) {
    return false;
  }
  const size_t ci = customIndex(job);
  g_custom_label[ci][0] = '\0';
  g_custom_detail[ci][0] = '\0';
  g_custom_cat[ci] = JobCategory::Expectation;
  g_mask[job] = 0;
  g_required[job] = true;
  for (size_t k = 0; k < KID_COUNT; ++k) {
    done_[k][job] = false;
    persistKid(k);
  }
  return true;
}

void JobState::begin() {
  prefs_.begin("jobs", false);
  initCustomIds();
  for (size_t j = 0; j < BUILTIN_JOB_COUNT; ++j) {
    g_mask[j] = JOBS[j].kids & allKidsMask();
    g_required[j] = JOBS[j].required_for_internet;
  }
  for (size_t j = BUILTIN_JOB_COUNT; j < JOB_COUNT; ++j) {
    g_mask[j] = 0;
    g_required[j] = true;
  }
  loadConfig();
  for (size_t k = 0; k < KID_COUNT; ++k) {
    loadKid(k);
  }
  loadRewards();
  job_day_ = prefs_.getUInt("day", 0);
}

bool JobState::isAssigned(size_t kid, size_t job) {
  if (kid >= KID_COUNT || job >= JOB_COUNT || !isDefined(job)) {
    return false;
  }
  return (g_mask[job] >> kid) & 1u;
}

bool JobState::isRequired(size_t job) { return job < JOB_COUNT && isDefined(job) && g_required[job]; }

uint8_t JobState::assignedMask(size_t job) {
  return (job < JOB_COUNT && isDefined(job)) ? g_mask[job] : 0;
}

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
  applyJobsObject(doc["jobs"].as<JsonObjectConst>());

  // Kids added in firmware after the panel last saved (e.g. Hazel) are missing
  // from the JSON. Give them the compile-time default assignments once.
  bool added = false;
  for (size_t k = 0; k < KID_COUNT; ++k) {
    if (saved.indexOf(KIDS[k].slug) >= 0) {
      continue;
    }
    for (size_t j = 0; j < BUILTIN_JOB_COUNT; ++j) {
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
    if (!isDefined(j)) {
      continue;
    }
    JsonObject o = jobs[jobId(j)].to<JsonObject>();
    JsonArray kids = o["kids"].to<JsonArray>();
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if (isAssigned(k, j)) {
        kids.add(KIDS[k].slug);
      }
    }
    o["required"] = g_required[j];
    if (isCustom(j)) {
      o["label"] = jobLabel(j);
      o["cat"] = jobCategory(j) == JobCategory::Paid ? "paid" : "expectation";
      o["detail"] = jobDetail(j);
    }
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

  bool changed = applyJobsObject(doc["jobs"].as<JsonObjectConst>());

  // {"add":{"label":"pack lunch","cat":"expectation"|"paid",...}}
  if (doc["add"].is<JsonObjectConst>()) {
    JsonObjectConst add = doc["add"].as<JsonObjectConst>();
    const char *label = add["label"].is<const char *>() ? add["label"].as<const char *>() : nullptr;
    char trimmed[JOB_LABEL_MAX];
    copyTrunc(trimmed, sizeof(trimmed), label);
    if (trimmed[0]) {
      const int slot = allocCustomSlot();
      if (slot < 0) {
        Serial.println("[jobs] no free custom job slots");
      } else {
        const size_t j = static_cast<size_t>(slot);
        const size_t ci = customIndex(j);
        memcpy(g_custom_label[ci], trimmed, JOB_LABEL_MAX);
        g_custom_cat[ci] = parseCat(add["cat"].is<const char *>() ? add["cat"].as<const char *>() : "expectation");
        if (add["detail"].is<const char *>()) {
          copyTrunc(g_custom_detail[ci], JOB_DETAIL_MAX, add["detail"].as<const char *>());
        } else {
          g_custom_detail[ci][0] = '\0';
        }
        g_required[j] = add["required"].is<bool>() ? add["required"].as<bool>() : true;
        if (add["kids"].is<JsonArrayConst>()) {
          g_mask[j] = parseKidsArray(add["kids"].as<JsonArrayConst>());
        } else {
          g_mask[j] = allKidsMask();
        }
        for (size_t k = 0; k < KID_COUNT; ++k) {
          done_[k][j] = false;
          persistKid(k);
        }
        changed = true;
        Serial.printf("[jobs] added %s (%s)\n", g_custom_ids[ci], g_custom_label[ci]);
      }
    }
  }

  // {"remove":["custom_0"]} or {"remove":"custom_0"}
  if (doc["remove"].is<JsonArrayConst>()) {
    for (JsonVariantConst v : doc["remove"].as<JsonArrayConst>()) {
      if (!v.is<const char *>()) {
        continue;
      }
      const int j = jobIndex(v.as<const char *>());
      if (j >= 0 && clearCustomJob(static_cast<size_t>(j))) {
        changed = true;
      }
    }
  } else if (doc["remove"].is<const char *>()) {
    const int j = jobIndex(doc["remove"].as<const char *>());
    if (j >= 0 && clearCustomJob(static_cast<size_t>(j))) {
      changed = true;
    }
  }

  if (!changed) {
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
  doc["customSlotsFree"] = 0;
  {
    int free = 0;
    for (size_t i = 0; i < MAX_CUSTOM_JOBS; ++i) {
      if (g_custom_label[i][0] == '\0') {
        ++free;
      }
    }
    doc["customSlotsFree"] = free;
  }
  JsonArray kids = doc["kids"].to<JsonArray>();
  for (size_t k = 0; k < KID_COUNT; ++k) {
    JsonObject o = kids.add<JsonObject>();
    o["slug"] = KIDS[k].slug;
    o["name"] = KIDS[k].name;
  }
  JsonArray jobs = doc["jobs"].to<JsonArray>();
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (!isDefined(j)) {
      continue;
    }
    JsonObject o = jobs.add<JsonObject>();
    o["id"] = jobId(j);
    o["label"] = jobLabel(j);
    o["cat"] = jobCategory(j) == JobCategory::Paid ? "paid" : "expectation";
    o["required"] = g_required[j];
    o["custom"] = isCustom(j);
    if (jobDetail(j)[0]) {
      o["detail"] = jobDetail(j);
    }
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
