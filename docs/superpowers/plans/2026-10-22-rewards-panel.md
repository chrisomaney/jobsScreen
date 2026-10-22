# Rewards Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Second Waveshare 7" firmware that advertises prizes, lets kids claim (deduct points + WINNER splash), accepts bonus points, and drives HA phone/TV celebrations via MQTT; phone admin for prizes/bonus.

**Architecture:** New PlatformIO project under `rewards-panel/` (LVGL kids UI + MQTT). Phone admin + TV celebrate page as MQTT/static web pages under `webapp/` (same proven pattern as Jobs Chooser — HA `www` or any static host), so admin works when the panel is on the LAN and matches how this family already uses HA/Android TV browser.

**Tech Stack:** PlatformIO, Arduino-ESP32 3.x (pioarduino 51.03.07), LVGL 8.4, PubSubClient, ArduinoJson, ESP32_Display_Panel, MQTT over Mosquitto, static HTML admin.

## Global Constraints

- Board: Waveshare ESP32-S3-Touch-LCD-7, 800×480, short env name `ws7r`.
- Kids/slugs must match jobs panel (`config.h` kids list including Hazel).
- Shared tallies: `kids/{slug}/rewards/tally` retained; rewards panel writes on claim/bonus.
- New topics per design spec: `catalog`, `claim`, `bonus`, `celebrate`, `rewards-panel/status`.
- No admin chrome on the glass.
- Deduct-before-celebrate; debounce claims; never negative tallies.
- Windows-safe includes (prefer `-Iinclude` style like current root `platformio.ini`, short env name).

## File map

| Path | Role |
|---|---|
| `rewards-panel/platformio.ini` | PIO env `ws7r` |
| `rewards-panel/include/config.h` | Kids, colours, MQTT topic constants, UI timing |
| `rewards-panel/include/secrets.example.h` | Wi-Fi/MQTT |
| `rewards-panel/include/lv_conf.h` | LVGL trim |
| `rewards-panel/include/esp_panel_board_supported_conf.h` | Waveshare board select |
| `rewards-panel/include/display_board.h` + `src/display_board.cpp` | Panel/LVGL/touch |
| `rewards-panel/include/reward_state.h` + `src/reward_state.cpp` | Catalog + tallies NVS |
| `rewards-panel/include/mqtt_bridge.h` + `src/mqtt_bridge.cpp` | Sync + publish |
| `rewards-panel/include/ui.h` + `src/ui.cpp` | Carousel / picker / WINNER / BONUS |
| `rewards-panel/src/main.cpp` | Setup/loop |
| `rewards-panel/lib/Network/` | Vendored Network (copy from root) |
| `webapp/rewards-admin.html` | Phone: CRUD prizes + bonus |
| `webapp/rewards-celebrate.html` | Full-screen TV/browser splash |
| `rewards-panel/README.md` | Flash + HA examples |
| Root `README.md` | Link to rewards panel |

### Task 1: Scaffold `rewards-panel/` + display bring-up

**Files:** create `rewards-panel/**` skeleton; copy Network lib + board conf patterns from repo root.

- [ ] Create `rewards-panel/platformio.ini` with env `ws7r`, same platform/board/lib_deps as jobs panel (plus no jobs-specific deps).
- [ ] Copy `lib/Network`, `lv_conf.h`, board supported conf, `display_board` from root; adapt includes.
- [ ] `main.cpp` prints boot banner and inits display; shows a solid-colour LVGL screen labeled "Rewards".
- [ ] Verify: `cd rewards-panel && pio run -e ws7r` → SUCCESS.
- [ ] Commit: `feat(rewards): scaffold Waveshare panel project`

### Task 2: Reward state + MQTT bridge

**Files:** `reward_state.*`, `mqtt_bridge.*`, `config.h`, `secrets.example.h`

**Interfaces:**
- `RewardState::begin()`, `tally(kid)`, `setTally(kid,v)`, `addBonus(kid,pts)`, `tryClaim(kid,prizeIndex)`, `prizeCount()`, `prize(i)`, `setCatalogFromJson`, `catalogToJson`, `revision()`
- `MqttBridge::begin(state)`, `loop()`, `publishAll()`, handlers for catalog set + tally updates

- [ ] Implement NVS-backed catalog (max 16 prizes) + tallies; default sample prizes.
- [ ] MQTT: subscribe tallies + `kids/rewards/catalog/set`; publish catalog, tallies, claim, bonus, celebrate, status LWT.
- [ ] Verify compile.
- [ ] Commit: `feat(rewards): state + MQTT catalog/tally/claim/bonus`

### Task 3: Kids LVGL UI

**Files:** `ui.*`, wire from `main.cpp`

- [ ] Ad carousel (timer rotate), kid picker, keep-going, WINNER, BONUS overlays with motion.
- [ ] Claim path calls `tryClaim` then mqtt publish; listen to state revision for live tallies.
- [ ] Verify compile.
- [ ] Commit: `feat(rewards): LVGL carousel claim WINNER BONUS UI`

### Task 4: Phone admin + TV celebrate web pages

**Files:** `webapp/rewards-admin.html`, `webapp/rewards-celebrate.html`, update `webapp/README.md`

- [ ] Admin: MQTT WS connect (same pattern as jobs chooser); edit catalog → `kids/rewards/catalog/set`; bonus → `kids/rewards/bonus/set`.
- [ ] Celebrate page: subscribe `kids/rewards/celebrate`; full-screen WINNER/BONUS; idle waiting state.
- [ ] Commit: `feat(rewards): admin + celebrate web pages for HA/TV`

### Task 5: Docs + root README link + PR polish

- [ ] `rewards-panel/README.md` with flash steps, secrets, HA notify + TV open-page examples.
- [ ] Link from root README.
- [ ] Final `pio run -e ws7r`.
- [ ] Commit: `docs(rewards): flash guide and HA/TV examples`

## Spec coverage check

| Spec item | Task |
|---|---|
| Carousel → picker → claim/keep-going/WINNER | 3 |
| Deduct + MQTT claim/celebrate | 2–3 |
| Bonus splash | 2–3 |
| Phone admin prizes/bonus | 4 |
| HA phone + Android TV celebrate | 4–5 |
| Shared tallies with jobs panel | 2 |
| Separate firmware tree | 1 |
