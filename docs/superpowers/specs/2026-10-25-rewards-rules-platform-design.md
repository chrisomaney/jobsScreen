# Rewards rules platform

**Date:** 2026-10-25  
**Status:** Approved direction (parent-chosen frequency + amounts; flat + per-job sum + per-job rules)  
**Surface:** Phone **Rewards HQ** (`webapp/rewards-admin.html`) + jobs panel firmware

## Goal

Parents fully control **when** and **how many** points kids earn, without YAML:

1. **Flat bonus** — at chosen days/times, give N points (optionally only if required jobs are done)  
2. **Per-job sum** — each job has a point value; at chosen days/times, add up finished jobs for that day  
3. **Specific job rule** — at chosen days/times, if job X is done → N points  

Plus: **custom jobs** (already in Jobs Chooser), **messages on the panel screen**.

## Rule model

Each rule (max 16):

| Field | Meaning |
|-------|---------|
| `id` | stable key |
| `label` | admin name |
| `enabled` | on/off |
| `kind` | `flat` \| `job_sum` \| `job` |
| `points` | amount for `flat` / `job`; ignored for `job_sum` (uses per-job table) |
| `job_id` | required when `kind=job` |
| `days` | weekday mask (Sun…Sat) |
| `hour` / `minute` | local time to fire |
| `kids` | which kids (slugs); default all |
| `require_ready` | if true, only award when that kid’s required jobs are done |

**Per-job points table:** map `job_id → points` (default 1). Used by `job_sum`.

**Anti-double-fire:** each rule stores last local calendar day it ran; fires at most once per day when clock crosses the scheduled minute.

## MQTT

| Topic | Role |
|-------|------|
| `kids/rewards/rules/state` | retained — `{job_points:{…}, rules:[…]}` |
| `kids/rewards/rules/set` | replace full rules payload |
| `kids/panel/message/set` | `{title?, text, seconds?}` → show on glass |
| existing tallies / bonus / catalog / celebrate | unchanged |

## Admin UI (Rewards HQ)

Same file, new sections: **Job points**, **Scheduled rules**, **Message to screen** (keep Prizes + Bonus).

## Panel

- Load/save rules in NVS  
- Every ~15s check local time vs rules; award + publish tallies + optional splash  
- Message overlay on current screen (auto-dismiss)

## Non-goals (this slice)

- Recurring “every N hours” (use multiple day/time rules)  
- Separate cloud backend  
- Changing MikroTik from rules  
