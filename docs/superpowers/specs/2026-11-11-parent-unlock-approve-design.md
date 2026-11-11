# Parent approve/decline before internet unlock

**Status:** implementing  
**Date:** 2026-11-11 (Australia/Brisbane)

## Goal
When a kid finishes required jobs (Auto mode), internet stays **off** until a parent Approves in Kids Admin (or Declines). Force On/Off overrides still work for emergencies.

## MQTT
- Retained `kids/{slug}/internet/request` — JSON `{status,kid,name,required_done,required_total}` with `status` = `none|pending|approved|declined`
- Command `kids/{slug}/internet/decision` — payload `approve` or `decline` (also JSON `{"decision":"approve"}`)
- Existing `kids/{slug}/internet` stays the MikroTik signal (`on` only after approve, or Force On)

## Apps
iOS/Android subscribe to `kids/+/internet/request`. While status is `pending`, show an alert/banner with Approve / Decline. No APNs yet — alert appears when the app is connected to MQTT; HA can notify phones from the same topic.

## Day roll
03:00 job wipe clears approval back to `none`.
