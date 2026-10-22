#pragma once

#include "config.h"
#include <Preferences.h>

class RewardState {
public:
  void begin();

  size_t prizeCount() const { return prize_count_; }
  const PrizeDef &prize(size_t i) const { return prizes_[i]; }
  int findPrize(const char *id) const;

  uint32_t tally(size_t kid) const;
  void setTally(size_t kid, uint32_t value);
  void addBonus(size_t kid, uint32_t points);

  // Returns false if kid/prize invalid or not enough points.
  bool tryClaim(size_t kid, size_t prize_index, uint32_t *tally_before, uint32_t *tally_after);

  bool setCatalogFromJson(const char *json);
  String catalogToJson() const;

  uint32_t revision() const { return revision_; }
  void bump() { ++revision_; }

  // Last celebration request for UI (cleared by UI after shown).
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

private:
  Preferences prefs_;
  PrizeDef prizes_[PRIZE_MAX]{};
  size_t prize_count_ = 0;
  uint32_t tallies_[KID_COUNT]{};
  uint32_t revision_ = 1;
  Splash splash_{};

  void loadDefaults();
  void loadNv();
  void saveCatalog();
  void saveTallies();
};
