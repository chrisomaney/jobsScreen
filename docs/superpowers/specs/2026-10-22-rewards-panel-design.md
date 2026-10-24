# Rewards Panel Design — Waveshare ESP32-S3 Touch LCD 7"

**Date:** 2026-10-22  
**Status:** Draft for review  
**Hardware:** Second Waveshare ESP32-S3-Touch-LCD-7 (800×480), separate from the jobs sign-off panel  
**Related:** Existing jobs firmware publishes `kids/{slug}/rewards/tally`

---

## 1. Goal

A second always-on 7" panel that **advertises prizes** and makes earning reward points exciting. Kids claim prizes on the glass; parents manage the prize catalog and bonus points from a **phone web page**. Home Assistant notifies phones and can show the same celebration on **Android TV** (HA browser / media).

---

## 2. Decisions (locked)

| Topic | Choice |
|---|---|
| Admin UI | Phone web page only — not on the kids glass |
| Admin hosting | On the rewards panel at `http://<panel-ip>/` (Approach 1) |
| Claim flow | Ad carousel → tap prize → pick kid → points check → WINNER |
| Points on claim | Deduct prize cost immediately; publish updated tally |
| Bonus points | Added from phone admin; BONUS splash on panel (+ HA/TV) |
| Android TV | HA automation on celebrate events + full-screen celebration page the TV browser can open |
| Repo layout | Same GitHub repo; new tree `rewards-panel/` with its own PlatformIO env |

---

## 3. Product flows

### 3.1 Kids-facing (LVGL)

1. **Ad mode (default)** — Full-bleed rotating prize spotlight: large prize name, large point cost, light live score ticker for the four kids. Auto-advance every few seconds with simple motion (slide/fade). One composition, not a dashboard of cards.
2. **Tap prize** → **Who’s claiming?** — Four large name buttons (Xavier, Presley, Jax, Banx) + Cancel.
3. **Insufficient points** → Short full-screen “Keep going!” (points have / need), then return to ads.
4. **Enough points** → Deduct cost → persist + MQTT → **WINNER** full-screen celebration (kid, prize, fun motion) for ~5–8s → return to ads.
5. **Bonus** (from admin/MQTT) → **BONUS!** splash for that kid with points added → update on-screen totals → return to ads.

No PIN, no admin chrome, no prize-edit UI on the glass.

### 3.2 Phone admin (`http://<panel-ip>/`)

- List prizes: name, point cost; add / edit / delete.
- Per kid: current tally + control to add bonus points (positive integer).
- Save writes NVS on the panel and republishes MQTT (catalog / tally / bonus / celebrate).
- Plain, readable form UI — parents only; not shown to kids.

### 3.3 Home Assistant + Android TV

- On `kids/rewards/celebrate` (and optionally `claim` / `bonus`):
  - Notify parent phones.
  - Open or cast a full-screen celebration view on the Android TV (TV is already in HA and can use the HA browser / media path).
- Document example automations and a minimal celebration Lovelace/webpage in `rewards-panel/docs/` (or README). Firmware does not control the TV directly.

---

## 4. MQTT contract

Shared base: `kids` (same broker / credentials pattern as jobs panel).  
Rewards panel MQTT client id: e.g. `kids-rewards-panel` (must differ from jobs panel).

### 4.1 Existing (jobs panel — keep compatible)

| Topic | Payload | Retained | Notes |
|---|---|---|---|
| `kids/{slug}/rewards/tally` | integer string | yes | Live points; jobs panel increments on help credits |
| `kids/rewards/event` | JSON help credit | no | Unchanged; rewards panel may ignore |

Slugs: `xavier`, `presley`, `jax`, `banx`.

### 4.2 New (rewards panel owns)

| Topic | Payload | Retained | Notes |
|---|---|---|---|
| `kids/rewards/catalog` | JSON array of `{id, name, cost}` | yes | Written by phone admin; panel boots from NVS then MQTT |
| `kids/{slug}/rewards/tally` | integer string | yes | **Also written** by rewards panel on claim deduct and bonus add |
| `kids/rewards/claim` | JSON | no | `{kid, prize_id, prize_name, cost, tally_before, tally_after, ts}` |
| `kids/rewards/bonus` | JSON | no | `{kid, points, tally_before, tally_after, ts}` |
| `kids/rewards/celebrate` | JSON | no | Unified splash for HA/TV/page: `{type:"winner"\|"bonus", kid, title, subtitle, points?, prize_name?, tally_after?, ts}` |
| `kids/rewards-panel/status` | `online` / `offline` | yes | LWT — separate from jobs `kids/panel/status` |

### 4.3 Tally sync rules

- Both panels may publish retained tallies; last write wins.
- Jobs panel: increments on leftover-help credit (existing).
- Rewards panel: decrements on claim; increments on bonus.
- On connect, rewards panel subscribes to all four tally topics and refreshes the UI when payloads change (so help credits from the jobs panel appear live).
- Claims are rejected locally if in-memory tally &lt; cost (no negative tallies).

### 4.4 Catalog JSON example

```json
[
  {"id": "ice_cream", "name": "Ice cream night", "cost": 5},
  {"id": "movie", "name": "Pick the movie", "cost": 8},
  {"id": "lego", "name": "Small Lego set", "cost": 20}
]
```

Ids are stable strings (admin-generated or slugified from name). Cost is a positive integer.

---

## 5. On-device data

- **NVS namespace** e.g. `rewards`: prize catalog blob + last-known tallies (cache).
- Boot: load NVS → start UI → Wi-Fi/MQTT → apply retained catalog/tallies from broker (broker wins if newer/present).
- Default catalog if empty: 2–3 sample prizes so the carousel is never blank out of the box.

---

## 6. Software architecture (`rewards-panel/`)

PlatformIO project mirroring the jobs panel board support (Waveshare 7", LVGL 8, same Windows-safe `extra_script` patterns, env name short e.g. `ws7r`).

| Module | Responsibility |
|---|---|
| `display_board` | Waveshare init, LVGL flush/touch, backlight |
| `reward_state` | Catalog + tallies in RAM/NVS; claim/bonus mutations |
| `mqtt_bridge` | Subscribe tallies; publish tally/catalog/claim/bonus/celebrate/status |
| `ui` | Ad carousel, kid picker, keep-going, WINNER, BONUS |
| `web_admin` | Lightweight HTTP server (ESPAsyncWebServer or equivalent) — REST or form posts for catalog + bonus |
| `celebrate_page` | Static HTML/JS celebration page served by the panel for TV browser (listens via EventSource/WebSocket fed by panel, or polls last celebrate) |
| `config` / `secrets` | Kids list, colours, MQTT base; Wi-Fi/MQTT secrets |

Jobs panel tree under repo root remains the sign-off firmware; do not merge the two firmwares into one binary.

---

## 7. UI / motion (kids glass)

- Brand-first energy: prize name is the hero; cost is secondary but large.
- Full-bleed ad plane; avoid card grids and admin clutter.
- At least 2–3 intentional motions: carousel transition, WINNER entrance, BONUS entrance.
- Visual direction: vivid, playful, high-contrast for a hallway/kitchen glance — not purple-glow generic AI chrome; define CSS/LVGL colour tokens in config.
- Touch targets large enough for kids.

---

## 8. Security / household notes

- Web admin is on the LAN only (no internet exposure assumed). No auth in v1 is acceptable on a trusted home Wi-Fi; optional simple PIN query-param or basic auth can be a fast follow if desired.
- MQTT credentials same pattern as jobs panel (`secrets.h`).
- Deduct-before-celebrate ordering: mutate tally + persist + publish tally/claim/celebrate, then show splash (avoids double-claim if tapped twice — debounce + in-progress lock).

---

## 9. Out of scope (v1)

- Editing prizes on the 7" glass.
- Photo idle screen / dim timeout (can match jobs panel later).
- iOS native admin app.
- Automatic HA YAML install (docs + examples only).
- Multi-prize cart / wishlist queue.

---

## 10. Success criteria

1. Carousel shows catalog prizes and live tallies from MQTT.
2. Claim with enough points deducts, shows WINNER, publishes claim + celebrate + new tally.
3. Claim without enough points shows keep-going, no deduct.
4. Phone admin can CRUD prizes and add bonus; BONUS splash appears on panel.
5. HA can notify phones and drive Android TV browser/media from `celebrate`.
6. Jobs panel help-credit tally bumps still appear on the rewards panel without reboot.
7. `pio run -e ws7r` succeeds (Windows short-path guidance as for jobs panel).

---

## 11. Implementation order (preview)

1. Scaffold `rewards-panel/` PlatformIO project (board + LVGL + Wi-Fi/MQTT skeleton).
2. `reward_state` + NVS + MQTT tally/catalog sync.
3. Kids UI: carousel → picker → keep-going / WINNER / BONUS.
4. Web admin + celebrate page for TV.
5. README + HA automation examples for phone notify + TV.

(Detailed plan after this spec is approved.)
