# Kids Jobs Sign-off (ESP32-S3)

Firmware for the **Waveshare ESP32-S3-Touch-LCD-7** (800×480). Four kids — **Xavier, Presley, Jax, Banx** — each have Expectations and Jobs screens. After **7:40am**, a shared **Jobs left to do** screen lets anyone finish leftovers, pick who did it (rewards tally), and only unlocks internet for kids whose **own** list is fully clear.

## Screens

| Screen | Purpose |
|--------|---------|
| **Photo / idle** | Always-on family photo + **Lets Go**. Lowest backlight. Returns here after **3 min** idle |
| **Home** | Four kid cards + **Jobs left to do** (locked until 7:40am) |
| **Expectations** | That kid’s daily expectations + Done |
| **Jobs** | That kid’s paid jobs + Done |
| **Jobs left to do** | Opens **7:40am**. Outstanding items by kid. Done → **Who did this?** name picker |
| **Who did this?** | Credits the helper for Rewards; marks the job done on the **owner’s** list |

### Family photo on the idle screen

Until you add a photo, a soft generated backdrop is used. To use your own:

```bash
pip install pillow
python3 tools/make_idle_photo.py /path/to/family.jpg   # writes data/idle.rgb565 (800×480)
pio run -t uploadfs                                     # upload LittleFS
pio run -t upload                                       # firmware if needed
```

Idle backlight / timeout: `BACKLIGHT_IDLE_PERCENT`, `BACKLIGHT_ACTIVE_PERCENT`, `IDLE_TIMEOUT_MS` in `include/config.h`.

### Unlock & rewards rules

- **Internet ON** MQTT only when **that kid’s own** required jobs are all done. Helping someone else does **not** unlock the helper’s devices if their list is still incomplete.
- Completing via **Jobs left to do** + name picker publishes a **rewards** event/tally for the person who pressed their name (for your Rewards / HA automations).
- Own-screen Done still works anytime (no reward credit — that’s for the leftover help flow).

## Hardware

- [Waveshare ESP32-S3-Touch-LCD-7](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7)
- Wi-Fi + MQTT (Home Assistant Mosquitto)

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Copy credentials:

```bash
cp include/secrets.example.h include/secrets.h
```

3. Edit `include/secrets.h` (Wi-Fi + MQTT). Use normal quotes and avoid unclosed strings, e.g. `#define WIFI_PASSWORD "yourpassword"`.
4. Set timezone in `include/config.h` (`TIMEZONE_POSIX`, default `AEST-10`). Leftover open time is `7:40`.
5. Build & flash (run from the repo root that contains `platformio.ini`):

```bash
pio device list
pio run -e ws7 -t upload
pio run -e ws7 -t monitor
```

Env names: `ws7` (short) or `waveshare-esp32s3-touch-lcd-7`. Plain `pio run -t upload` also works and builds the default env.

If you see `ProjectEnvsNotAvailableError`, you are either not in this project folder, or an old checkout is missing the `ws7` env — pull latest and confirm `platformio.ini` lists `[env:ws7]`.

### Serial disconnects immediately (ESP32-S3 USB)

This board uses **native USB-CDC**. Opening a serial monitor that pulses DTR/RTS resets the chip and the COM port vanishes. This repo sets `monitor_dtr = 0` / `monitor_rts = 0` so `pio run -e ws7 -t monitor` stays connected.

On Windows:

1. Close any other serial app (Arduino Serial Monitor, PuTTY, another `pio device monitor`).
2. Unplug/replug USB, then `pio device list` and note the COM port.
3. Upload with the monitor **closed**: `pio run -e ws7 -t upload`
4. Wait ~2s after upload finishes, then: `pio run -e ws7 -t monitor`
5. If upload fails with “port not found”, hold **BOOT**, tap **RESET**, release **BOOT**, then upload again.
6. Prefer the repo monitor target above — avoid raw `pio device monitor` from a random folder (it skips `platformio.ini` DTR/RTS settings).

If you see `lv_conf.h: No such file` on Windows, pull the latest from this branch (LV_CONF_PATH was removed — it breaks on Windows paths) and run `pio run -t clean` then `pio run`.

If `lib/Networking` is missing after clone, copy the framework Network library once (Arduino-ESP32 3.x link helper):

```bash
mkdir -p lib/Networking
cp -a ~/.platformio/packages/framework-arduinoespressif32/libraries/Network/. lib/Networking/
# ensure library.properties name=Network
```

## MQTT topics

| Topic | Payload | Notes |
|-------|---------|--------|
| `kids/{slug}/job/{job_id}/state` | `done` / `todo` | retained — per job |
| `kids/{slug}/internet` | `on` / `off` | retained — **MikroTik unlock** for that kid only |
| `kids/{slug}/progress` | JSON | retained |
| `kids/{slug}/rewards/tally` | integer | retained — help credits |
| `kids/rewards/event` | JSON | not retained — `{doer, owner, job, …}` |
| `kids/panel/status` | `online` / `offline` | LWT |

Slugs: `xavier`, `presley`, `jax`, `banx`.

**Admin commands:** `kids/{slug}/cmd/reset`, `kids/cmd/reset_all`, `kids/cmd/reset_rewards`.

### Home Assistant sketch

```yaml
automation:
  - alias: "Xavier internet on"
    trigger:
      - platform: mqtt
        topic: kids/xavier/internet
        payload: "on"
    action:
      - service: # enable Xavier MikroTik profile

  - alias: "Rewards — log help"
    trigger:
      - platform: mqtt
        topic: kids/rewards/event
    action:
      - service: notify.persistent_notification
        data:
          message: "{{ trigger.payload_json.doer_name }} helped with {{ trigger.payload_json.owner_name }}'s {{ trigger.payload_json.job_label }}"
```

See `examples/homeassistant-mqtt.yaml` for more.

## Jobs (from your list)

**Expectations:** make bed, tidy floor, washing in baskets, towels on hooks, brush teeth, get dressed, socks on, medication, loungeroom tidy, table cleared, lights and fans off.

**Paid jobs:** feed cat, feed dog, dishes away, dishes wash, water plants, bins out, bathroom.

Edit lists / `required_for_internet` in `include/config.h`.
