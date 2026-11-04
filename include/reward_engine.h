#pragma once

#include "job_state.h"
#include <Preferences.h>

enum class RewardRuleKind : uint8_t { Flat = 0, JobSum = 1, Job = 2 };

struct RewardRule {
  char id[20]{};
  char label[40]{};
  char job_id[24]{};
  RewardRuleKind kind = RewardRuleKind::Flat;
  uint16_t points = 0;
  uint8_t days = 0x7F;  // bit0=Sun … bit6=Sat
  uint8_t hour = 7;
  uint8_t minute = 30;
  uint8_t kids_mask = 0xFF;
  bool enabled = true;
  bool require_ready = false;
  uint32_t last_fire_day = 0;  // YYYYMMDD local wall calendar
};

struct PanelMessage {
  bool active = false;
  char title[40]{};
  char text[140]{};
  uint32_t until_ms = 0;
};

// Scheduled rewards + per-job point table + panel messages.
class RewardEngine {
public:
  void begin(JobState *state);

  uint16_t jobPoints(size_t job) const;
  // Points when ALL assigned expectations are done (not per expectation item).
  uint16_t expectationsCompletePoints() const { return expectations_complete_points_; }
  size_t ruleCount() const { return rule_count_; }
  const RewardRule &rule(size_t i) const { return rules_[i]; }

  bool applyRulesJson(const char *json);
  String rulesStateJson() const;

  // Run from loop (~15s). Awards points; returns kids bitmask awarded.
  uint8_t tickSchedule();

  void setPanelMessage(const char *title, const char *text, uint32_t seconds);
  void clearPanelMessage();
  const PanelMessage &panelMessage() const { return message_; }
  void tickMessage();

  uint32_t revision() const { return revision_; }
  // Flush at most one pending NVS rewrite (rules/job points) per call.
  bool loopPersist();

private:
  JobState *state_ = nullptr;
  Preferences prefs_;
  uint16_t job_points_[JOB_COUNT]{};
  uint16_t expectations_complete_points_ = 0;
  RewardRule rules_[REWARD_RULE_MAX]{};
  size_t rule_count_ = 0;
  PanelMessage message_{};
  uint32_t revision_ = 1;
  uint32_t last_tick_ms_ = 0;
  bool persist_pending_ = false;

  void loadNv();
  void saveNv();
  void bump() { ++revision_; }
  uint32_t wallDay() const;
  bool dueNow(const RewardRule &r, int hour, int minute, int wday, uint32_t today) const;
  uint32_t awardForKid(const RewardRule &r, size_t kid) const;
};
