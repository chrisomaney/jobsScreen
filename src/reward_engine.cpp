#include "reward_engine.h"

#include "clock_util.h"

#include <ArduinoJson.h>
#include <cstring>
#include <time.h>

namespace {

uint8_t kidsMaskFromJson(JsonVariantConst v) {
  if (v.isNull() || !v.is<JsonArray>()) {
    return 0xFF;
  }
  uint8_t mask = 0;
  for (JsonVariantConst item : v.as<JsonArrayConst>()) {
    const char *slug = item.as<const char *>();
    if (!slug) {
      continue;
    }
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if (strcmp(KIDS[k].slug, slug) == 0) {
        mask |= static_cast<uint8_t>(1u << k);
      }
    }
  }
  return mask ? mask : 0xFF;
}

}  // namespace

void RewardEngine::begin(JobState *state) {
  state_ = state;
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    job_points_[j] = 1;
  }
  prefs_.begin("rewrules", false);
  loadNv();
}

uint16_t RewardEngine::jobPoints(size_t job) const {
  return job < JOB_COUNT ? job_points_[job] : 0;
}

uint32_t RewardEngine::wallDay() const {
  if (!clock_is_synced()) {
    return 0;
  }
  time_t now = time(nullptr);
  struct tm local {};
  localtime_r(&now, &local);
  return static_cast<uint32_t>((local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 +
                               local.tm_mday);
}

bool RewardEngine::dueNow(const RewardRule &r, int hour, int minute, int wday,
                          uint32_t today) const {
  if (!r.enabled) {
    return false;
  }
  if (((r.days >> wday) & 1u) == 0) {
    return false;
  }
  if (r.hour != static_cast<uint8_t>(hour) || r.minute != static_cast<uint8_t>(minute)) {
    return false;
  }
  if (today == 0 || r.last_fire_day == today) {
    return false;
  }
  return true;
}

uint32_t RewardEngine::awardForKid(const RewardRule &r, size_t kid) const {
  if (!state_ || kid >= KID_COUNT) {
    return 0;
  }
  if (r.require_ready && !state_->internetReady(kid)) {
    return 0;
  }
  switch (r.kind) {
    case RewardRuleKind::Flat:
      return r.points;
    case RewardRuleKind::JobSum: {
      // Paid jobs: per-item points. Expectations: one bundle if all assigned are done.
      uint32_t sum = 0;
      bool any_exp = false;
      bool all_exp_done = true;
      for (size_t j = 0; j < JOB_COUNT; ++j) {
        if (!JobState::isAssigned(kid, j) || !JobState::isDefined(j)) {
          continue;
        }
        if (JobState::jobCategory(j) == JobCategory::Expectation) {
          any_exp = true;
          if (!state_->isDone(kid, j)) {
            all_exp_done = false;
          }
          continue;  // never per-item for expectations
        }
        if (state_->isDone(kid, j)) {
          sum += job_points_[j];
        }
      }
      if (any_exp && all_exp_done) {
        sum += expectations_complete_points_;
      }
      return sum;
    }
    case RewardRuleKind::Job: {
      const int j = JobState::findJobIndex(r.job_id);
      if (j < 0 || !JobState::isAssigned(kid, static_cast<size_t>(j))) {
        return 0;
      }
      if (!state_->isDone(kid, static_cast<size_t>(j))) {
        return 0;
      }
      // Expectations never pay per-item via a Job rule; use the bundle field instead.
      if (JobState::jobCategory(static_cast<size_t>(j)) == JobCategory::Expectation) {
        return 0;
      }
      // Prefer rule points when set; otherwise the Jobs table (0 allowed).
      return r.points ? r.points : job_points_[j];
    }
    default:
      return 0;
  }
}

uint8_t RewardEngine::tickSchedule() {
  const uint32_t now_ms = millis();
  if (last_tick_ms_ != 0 && now_ms - last_tick_ms_ < 15000) {
    return 0;
  }
  last_tick_ms_ = now_ms;

  int hour = 0, minute = 0, wday = 0;
  if (!clock_local_parts(&hour, &minute, &wday)) {
    return 0;
  }
  const uint32_t today = wallDay();
  if (today == 0) {
    return 0;
  }

  uint8_t awarded_mask = 0;
  bool dirty = false;
  for (size_t i = 0; i < rule_count_; ++i) {
    RewardRule &r = rules_[i];
    if (!dueNow(r, hour, minute, wday, today)) {
      continue;
    }
    bool any = false;
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if (((r.kids_mask >> k) & 1u) == 0) {
        continue;
      }
      const uint32_t pts = awardForKid(r, k);
      if (pts == 0) {
        continue;
      }
      state_->addPoints(k, pts);
      awarded_mask |= static_cast<uint8_t>(1u << k);
      any = true;
      char sub[64];
      snprintf(sub, sizeof(sub), "+%lu from %s", static_cast<unsigned long>(pts),
               r.label[0] ? r.label : r.id);
      state_->queueSplash(JobState::SplashKind::Bonus, k, "POINTS!", sub);
      Serial.printf("[rewards] rule %s → %s +%lu\n", r.id, KIDS[k].slug,
                    static_cast<unsigned long>(pts));
    }
    r.last_fire_day = today;
    dirty = true;
    if (!any) {
      Serial.printf("[rewards] rule %s fired (no awards)\n", r.id);
    }
  }
  if (dirty) {
    saveNv();
    bump();
  }
  return awarded_mask;
}

void RewardEngine::setPanelMessage(const char *title, const char *text, uint32_t seconds) {
  memset(&message_, 0, sizeof(message_));
  if (!text || !text[0]) {
    return;
  }
  message_.active = true;
  strncpy(message_.title, title && title[0] ? title : "Message", sizeof(message_.title) - 1);
  strncpy(message_.text, text, sizeof(message_.text) - 1);
  const uint32_t sec = seconds == 0 ? 12 : seconds;
  message_.until_ms = millis() + sec * 1000UL;
  bump();
}

void RewardEngine::clearPanelMessage() {
  message_.active = false;
  bump();
}

void RewardEngine::tickMessage() {
  if (message_.active && static_cast<int32_t>(millis() - message_.until_ms) >= 0) {
    message_.active = false;
    bump();
  }
}

void RewardEngine::loadNv() {
  rule_count_ = prefs_.getUChar("rc", 0);
  if (rule_count_ > REWARD_RULE_MAX) {
    rule_count_ = 0;
  }
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    // Default 1 for paid-style payouts; 0 is allowed (e.g. expectation items).
    job_points_[j] = prefs_.getUShort((String("jp") + j).c_str(), 1);
  }
  expectations_complete_points_ = prefs_.getUShort("exp_pts", 0);
  memset(rules_, 0, sizeof(rules_));
  for (size_t i = 0; i < rule_count_; ++i) {
    RewardRule &r = rules_[i];
    String prefix = String("r") + i + "_";
    String id = prefs_.getString((prefix + "id").c_str(), "");
    String label = prefs_.getString((prefix + "lb").c_str(), "");
    String job = prefs_.getString((prefix + "jid").c_str(), "");
    strncpy(r.id, id.c_str(), sizeof(r.id) - 1);
    strncpy(r.label, label.c_str(), sizeof(r.label) - 1);
    strncpy(r.job_id, job.c_str(), sizeof(r.job_id) - 1);
    r.kind = static_cast<RewardRuleKind>(prefs_.getUChar((prefix + "k").c_str(), 0));
    r.points = prefs_.getUShort((prefix + "pts").c_str(), 0);
    r.days = prefs_.getUChar((prefix + "d").c_str(), 0x7F);
    r.hour = prefs_.getUChar((prefix + "h").c_str(), 7);
    r.minute = prefs_.getUChar((prefix + "m").c_str(), 30);
    r.kids_mask = prefs_.getUChar((prefix + "km").c_str(), 0xFF);
    r.enabled = prefs_.getBool((prefix + "en").c_str(), true);
    r.require_ready = prefs_.getBool((prefix + "rr").c_str(), false);
    r.last_fire_day = prefs_.getUInt((prefix + "lf").c_str(), 0);
  }
}

void RewardEngine::saveNv() {
  prefs_.putUChar("rc", static_cast<uint8_t>(rule_count_));
  prefs_.putUShort("exp_pts", expectations_complete_points_);
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    prefs_.putUShort((String("jp") + j).c_str(), job_points_[j]);
  }
  for (size_t i = 0; i < rule_count_; ++i) {
    const RewardRule &r = rules_[i];
    String prefix = String("r") + i + "_";
    prefs_.putString((prefix + "id").c_str(), r.id);
    prefs_.putString((prefix + "lb").c_str(), r.label);
    prefs_.putString((prefix + "jid").c_str(), r.job_id);
    prefs_.putUChar((prefix + "k").c_str(), static_cast<uint8_t>(r.kind));
    prefs_.putUShort((prefix + "pts").c_str(), r.points);
    prefs_.putUChar((prefix + "d").c_str(), r.days);
    prefs_.putUChar((prefix + "h").c_str(), r.hour);
    prefs_.putUChar((prefix + "m").c_str(), r.minute);
    prefs_.putUChar((prefix + "km").c_str(), r.kids_mask);
    prefs_.putBool((prefix + "en").c_str(), r.enabled);
    prefs_.putBool((prefix + "rr").c_str(), r.require_ready);
    prefs_.putUInt((prefix + "lf").c_str(), r.last_fire_day);
  }
}

bool RewardEngine::applyRulesJson(const char *json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  const bool has_exp = !doc["expectations_complete_points"].isNull();
  if (has_exp) {
    expectations_complete_points_ = doc["expectations_complete_points"].as<uint16_t>();
  }

  const bool has_jp = doc["job_points"].is<JsonObject>();
  if (has_jp) {
    for (size_t j = 0; j < JOB_COUNT; ++j) {
      // Expectations default to 0 (bundle only); paid default to 1.
      job_points_[j] = JobState::jobCategory(j) == JobCategory::Expectation ? 0 : 1;
    }
    JsonObject jp = doc["job_points"].as<JsonObject>();
    for (JsonPair kv : jp) {
      const int j = JobState::findJobIndex(kv.key().c_str());
      if (j < 0) {
        continue;
      }
      job_points_[j] = kv.value().as<uint16_t>();
    }
  }

  if (!doc["rules"].is<JsonArray>()) {
    // Allow points-only updates (no rules array) from admin Expectations/Jobs tabs.
    if (has_jp || has_exp) {
      persist_pending_ = false;
      saveNv();  // sync so Save-from-app survives a freeze/power-cycle
      bump();
      return true;
    }
    return false;
  }

  JsonArray arr = doc["rules"].as<JsonArray>();
  size_t n = 0;
  RewardRule next[REWARD_RULE_MAX]{};
  for (JsonObject o : arr) {
    if (n >= REWARD_RULE_MAX) {
      break;
    }
    RewardRule &r = next[n];
    const char *id = o["id"] | "";
    const char *label = o["label"] | "";
    const char *kind = o["kind"] | "flat";
    if (!id[0]) {
      continue;
    }
    strncpy(r.id, id, sizeof(r.id) - 1);
    strncpy(r.label, label[0] ? label : id, sizeof(r.label) - 1);
    if (strcmp(kind, "job_sum") == 0) {
      r.kind = RewardRuleKind::JobSum;
    } else if (strcmp(kind, "job") == 0) {
      r.kind = RewardRuleKind::Job;
    } else {
      r.kind = RewardRuleKind::Flat;
    }
    const char *jid = o["job_id"] | "";
    strncpy(r.job_id, jid, sizeof(r.job_id) - 1);
    r.points = o["points"] | 0;
    r.days = o["days"] | 0x7F;
    r.hour = o["hour"] | 7;
    r.minute = o["minute"] | 0;
    r.enabled = o["enabled"] | true;
    r.require_ready = o["require_ready"] | false;
    r.kids_mask = kidsMaskFromJson(o["kids"]);
    r.last_fire_day = 0;
    for (size_t i = 0; i < rule_count_; ++i) {
      if (strcmp(rules_[i].id, r.id) == 0) {
        r.last_fire_day = rules_[i].last_fire_day;
        break;
      }
    }
    if (r.kind == RewardRuleKind::Job && !r.job_id[0]) {
      continue;
    }
    ++n;
  }

  memcpy(rules_, next, sizeof(rules_));
  rule_count_ = n;
  persist_pending_ = false;
  saveNv();  // sync so Save-from-app survives a freeze/power-cycle
  bump();
  return true;
}

bool RewardEngine::loopPersist() {
  if (!persist_pending_) {
    return false;
  }
  persist_pending_ = false;
  saveNv();
  Serial.println("[rewards] rules/points persisted");
  return true;
}

String RewardEngine::rulesStateJson() const {
  JsonDocument doc;
  doc["expectations_complete_points"] = expectations_complete_points_;
  JsonObject jp = doc["job_points"].to<JsonObject>();
  for (size_t j = 0; j < JOB_COUNT; ++j) {
    if (!JobState::isDefined(j)) {
      continue;
    }
    jp[JobState::jobId(j)] = job_points_[j];
  }
  JsonArray arr = doc["rules"].to<JsonArray>();
  for (size_t i = 0; i < rule_count_; ++i) {
    const RewardRule &r = rules_[i];
    JsonObject o = arr.add<JsonObject>();
    o["id"] = r.id;
    o["label"] = r.label;
    o["kind"] = r.kind == RewardRuleKind::JobSum ? "job_sum"
                : r.kind == RewardRuleKind::Job  ? "job"
                                                 : "flat";
    o["points"] = r.points;
    o["job_id"] = r.job_id;
    o["days"] = r.days;
    o["hour"] = r.hour;
    o["minute"] = r.minute;
    o["enabled"] = r.enabled;
    o["require_ready"] = r.require_ready;
    JsonArray kids = o["kids"].to<JsonArray>();
    for (size_t k = 0; k < KID_COUNT; ++k) {
      if ((r.kids_mask >> k) & 1u) {
        kids.add(KIDS[k].slug);
      }
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}
