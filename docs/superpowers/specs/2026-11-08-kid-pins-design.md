# Per-kid PIN unlock (option A)

**Status:** approved  
**Date:** 2026-11-08 (Australia/Brisbane)

## Goal
Tapping a kid on Home asks for that kid’s 4-digit PIN before opening their jobs. Parents set/change PINs in iOS/web/Android admin only (no master PIN).

## Behaviour
- Empty PIN → no pad (open jobs as today). Migration-friendly.
- Non-empty 4-digit PIN → numeric pad; Cancel returns Home; wrong PIN clears entry; correct opens Expectations.
- PINs stored in NVS, echoed on `kids/config/state` as `"pins":{"xavier":"1234",...}`.
- Admin updates via `kids/config/set` with `{"pins":{"slug":"1234"}}` (or `""` to clear).

## Surfaces
Panel LVGL pad; iOS AdminKid + settings UI; web Jobs Chooser / Rewards HQ; Android AdminKids.
