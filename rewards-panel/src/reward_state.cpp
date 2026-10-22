#include "reward_state.h"

#include <ArduinoJson.h>
#include <cstring>

void RewardState::loadDefaults() {
  prize_count_ = 3;
  strncpy(prizes_[0].id, "ice_cream", sizeof(prizes_[0].id) - 1);
  strncpy(prizes_[0].name, "Ice cream night", sizeof(prizes_[0].name) - 1);
  prizes_[0].cost = 5;
  strncpy(prizes_[1].id, "movie", sizeof(prizes_[1].id) - 1);
  strncpy(prizes_[1].name, "Pick the movie", sizeof(prizes_[1].name) - 1);
  prizes_[1].cost = 8;
  strncpy(prizes_[2].id, "lego", sizeof(prizes_[2].id) - 1);
  strncpy(prizes_[2].name, "Small Lego set", sizeof(prizes_[2].name) - 1);
  prizes_[2].cost = 20;
}

void RewardState::begin() {
  prefs_.begin("rewards", false);
  loadNv();
  if (prize_count_ == 0) {
    loadDefaults();
    saveCatalog();
  }
}

void RewardState::loadNv() {
  prize_count_ = prefs_.getUChar("pc", 0);
  if (prize_count_ > PRIZE_MAX) {
    prize_count_ = 0;
  }
  for (size_t i = 0; i < prize_count_; ++i) {
    String idk = String("pid") + i;
    String nk = String("pn") + i;
    String ck = String("pcost") + i;
    String id = prefs_.getString(idk.c_str(), "");
    String name = prefs_.getString(nk.c_str(), "");
    prizes_[i].cost = prefs_.getUShort(ck.c_str(), 1);
    strncpy(prizes_[i].id, id.c_str(), sizeof(prizes_[i].id) - 1);
    strncpy(prizes_[i].name, name.c_str(), sizeof(prizes_[i].name) - 1);
  }
  for (size_t k = 0; k < KID_COUNT; ++k) {
    String tk = String("t") + k;
    if (prefs_.isKey(tk.c_str())) {
      tallies_[k] = prefs_.getUInt(tk.c_str(), 0);
    }
  }
}

void RewardState::saveCatalog() {
  prefs_.putUChar("pc", static_cast<uint8_t>(prize_count_));
  for (size_t i = 0; i < prize_count_; ++i) {
    prefs_.putString((String("pid") + i).c_str(), prizes_[i].id);
    prefs_.putString((String("pn") + i).c_str(), prizes_[i].name);
    prefs_.putUShort((String("pcost") + i).c_str(), prizes_[i].cost);
  }
  bump();
}

void RewardState::saveTallies() {
  for (size_t k = 0; k < KID_COUNT; ++k) {
    prefs_.putUInt((String("t") + k).c_str(), tallies_[k]);
  }
  bump();
}

int RewardState::findPrize(const char *id) const {
  if (!id) {
    return -1;
  }
  for (size_t i = 0; i < prize_count_; ++i) {
    if (strcmp(prizes_[i].id, id) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

uint32_t RewardState::tally(size_t kid) const {
  return kid < KID_COUNT ? tallies_[kid] : 0;
}

void RewardState::setTally(size_t kid, uint32_t value) {
  if (kid >= KID_COUNT) {
    return;
  }
  if (tallies_[kid] == value) {
    return;
  }
  tallies_[kid] = value;
  saveTallies();
}

void RewardState::addBonus(size_t kid, uint32_t points) {
  if (kid >= KID_COUNT || points == 0) {
    return;
  }
  const uint32_t before = tallies_[kid];
  tallies_[kid] += points;
  saveTallies();
  char sub[64];
  snprintf(sub, sizeof(sub), "+%lu points!  Now you have %lu",
           static_cast<unsigned long>(points),
           static_cast<unsigned long>(tallies_[kid]));
  queueSplash(SplashKind::Bonus, kid, "BONUS POINTS!", sub);
  (void)before;
}

bool RewardState::tryClaim(size_t kid, size_t prize_index, uint32_t *tally_before, uint32_t *tally_after) {
  if (kid >= KID_COUNT || prize_index >= prize_count_) {
    return false;
  }
  const uint32_t cost = prizes_[prize_index].cost;
  const uint32_t before = tallies_[kid];
  if (before < cost) {
    char sub[64];
    snprintf(sub, sizeof(sub), "You need %lu more points",
             static_cast<unsigned long>(cost - before));
    queueSplash(SplashKind::KeepGoing, kid, "Keep earning!", sub);
    return false;
  }
  tallies_[kid] = before - cost;
  saveTallies();
  if (tally_before) {
    *tally_before = before;
  }
  if (tally_after) {
    *tally_after = tallies_[kid];
  }
  char sub[64];
  snprintf(sub, sizeof(sub), "You got: %s", prizes_[prize_index].name);
  queueSplash(SplashKind::Winner, kid, "YOU WIN!", sub);
  return true;
}

void RewardState::queueSplash(SplashKind kind, size_t kid, const char *title, const char *subtitle) {
  splash_.kind = kind;
  splash_.kid = kid;
  strncpy(splash_.title, title ? title : "", sizeof(splash_.title) - 1);
  strncpy(splash_.subtitle, subtitle ? subtitle : "", sizeof(splash_.subtitle) - 1);
  bump();
}

bool RewardState::setCatalogFromJson(const char *json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }
  if (!doc.is<JsonArray>()) {
    return false;
  }
  JsonArray arr = doc.as<JsonArray>();
  size_t n = 0;
  for (JsonObject o : arr) {
    if (n >= PRIZE_MAX) {
      break;
    }
    const char *id = o["id"] | "";
    const char *name = o["name"] | "";
    uint16_t cost = o["cost"] | 1;
    if (!id[0] || !name[0] || cost == 0) {
      continue;
    }
    memset(&prizes_[n], 0, sizeof(prizes_[n]));
    strncpy(prizes_[n].id, id, sizeof(prizes_[n].id) - 1);
    strncpy(prizes_[n].name, name, sizeof(prizes_[n].name) - 1);
    prizes_[n].cost = cost;
    ++n;
  }
  if (n == 0) {
    return false;
  }
  prize_count_ = n;
  saveCatalog();
  return true;
}

String RewardState::catalogToJson() const {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (size_t i = 0; i < prize_count_; ++i) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = prizes_[i].id;
    o["name"] = prizes_[i].name;
    o["cost"] = prizes_[i].cost;
  }
  String out;
  serializeJson(doc, out);
  return out;
}
