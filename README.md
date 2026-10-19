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
python -m pip install pillow

# Windows — use a normal drive path (not /c:/...):
python tools/make_idle_photo.py C:\1\photo.jpg
# or:
python tools/make_idle_photo.py "C:/1/photo.jpg"

pio run -t uploadfs    # uploads data/idle.rgb565 to the panel
pio run -t upload      # firmware if needed
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
5. Build & flash (default env is `ws7`):

```bash
pio run -e ws7 -t upload
pio device monitor -b 115200
```

**Windows tips (important)**

Env name is **`ws7`** (short on purpose — Windows `MAX_PATH` breaks LVGL with the long env name).

`ProjectEnvsNotAvailableError` / unknown env `ws7` means your `C:\js` copy is still on **old main** (`[env:waveshare-esp32s3-touch-lcd-7]` only). Re-clone or pull this branch:

```powershell
# Fresh short-path clone (keep path short: C:\js)
cd C:\
Remove-Item -Recurse -Force C:\js -ErrorAction SilentlyContinue
git clone -b cursor/fix-ws7-env-upload-43dc https://github.com/chrisomaney/jobsScreen.git js
cd C:\js

# Must print: default_envs = ws7
Select-String -Path platformio.ini -Pattern "default_envs|env:ws7"

copy include\secrets.example.h include\secrets.h
# edit Wi-Fi / MQTT in include\secrets.h

Remove-Item -Recurse -Force .pio -ErrorAction SilentlyContinue
pio run -e ws7 -t upload
pio device monitor -b 115200
```

After this lands on `main`, you can clone without `-b …`.

In Cursor/VS Code with the PlatformIO extension: open **`C:\js`**, toolbar target **ws7**. Do not open an old Downloads unzip folder.

Optional: `python -m pip install charset-normalizer` clears the harmless Requests warning.

`lib/Network` is already in the repo (Arduino-ESP32 3.x WiFi dependency).

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
