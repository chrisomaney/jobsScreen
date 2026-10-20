# Kids Jobs Sign-off (ESP32-S3)

Firmware for the **Waveshare ESP32-S3-Touch-LCD-7** (800×480). Four kids — **Xavier, Presley, Jax, Banx** — each have Expectations and Jobs screens. After **7:40am**, a shared **Jobs left to do** screen lets anyone finish leftovers, pick who did it (rewards tally), and only unlocks internet for kids whose **own** list is fully clear.

## Screens

| Screen | Purpose |
|--------|---------|
| **Photo / idle** | Always-on family photo + **Let's Go** (or tap anywhere). Returns here after **3 min** idle |
| **Home** | Four kid cards + **Jobs left to do** (locked until 7:40am) |
| **Expectations** | That kid’s daily expectations + Done |
| **Jobs** | That kid’s paid jobs + Done |

**Done buttons:** tap to mark done; **hold** a green (done) button to undo. **Reset** on a kid’s page also needs a hold.
| **Jobs left to do** | Opens **7:40am**. Outstanding items by kid. Done → **Who did this?** name picker |
| **Who did this?** | Credits the helper for Rewards; marks the job done on the **owner’s** list. The owner isn’t offered (no self-credit) |

### Family photo on the idle screen

Until you add a photo, a soft generated backdrop is used. To use your own:

```bash
pip install pillow
python3 tools/make_idle_photo.py /path/to/family.jpg   # writes data/idle.rgb565 (800×480)
pio run -t uploadfs                                     # upload LittleFS
pio run -t upload                                       # firmware if needed
```

Idle timeout and look: `IDLE_TIMEOUT_MS` and `IDLE_PHOTO_BRIGHTNESS_PCT` in `include/config.h`.

> The Waveshare 7" backlight is **on/off only** (driven through the CH422G IO expander, no PWM), so it can’t be dimmed in hardware. The idle photo is darkened in software instead (`IDLE_PHOTO_BRIGHTNESS_PCT`, default 60). Set it to `0` to switch the backlight fully off when idle (tap the screen to wake).

### Unlock & rewards rules

- **Internet ON** MQTT only when **that kid’s own** required jobs are all done. Helping someone else does **not** unlock the helper’s devices if their list is still incomplete.
- Completing via **Jobs left to do** + name picker publishes a **rewards** event/tally for the person who pressed their name (for your Rewards / HA automations).
- Own-screen Done still works anytime (no reward credit — that’s for the leftover help flow).
- **Daily reset:** the panel clears everyone’s ticks itself at `DAILY_RESET_HOUR:DAILY_RESET_MINUTE` (default 03:00 local), even if Home Assistant / MQTT is down. Reward tallies are kept. The HA 05:30 `reset_all` automation is optional now.
- **Who has which job:** the last column of each job in `include/config.h` is a kid mask (`ALL_KIDS` by default, or e.g. `KID_XAVIER | KID_JAX`). Counts, progress and the internet unlock only consider a kid’s assigned jobs.

## Web app: choose who does what

`webapp/index.html` is a single-file web app for your phone. It connects to the broker over MQTT WebSockets, shows every job with a tick box per kid plus a **Needed for internet** switch, and saves changes to the panel. The panel stores them in flash, so they survive reboots, and updates its screens straight away. It also shows each kid's live progress and has buttons to clear today's ticks or zero reward tallies.

Setup steps are in [`webapp/README.md`](webapp/README.md). `include/config.h` still sets the defaults; the web app's changes override them on the panel.

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
5. Build & flash:

```bash
pio run -e ws7 -t upload
pio run -e ws7 -t monitor
```

Use `-e ws7` (short env). Prefer `pio run -e ws7 -t monitor` over bare `pio device monitor` — this project disables DTR/RTS so opening the monitor does not reset the ESP32-S3 USB-CDC port (which looks like “Closed serial port COMxx due to disconnection”).

If you see `fatal error: Network.h: No such file or directory`, pull the latest `main` (the include paths in `platformio.ini` are now relative, so project folders with spaces or Windows paths work), then run `pio run -e ws7 -t clean` and build again.

If you see `lv_conf.h: No such file` on Windows, pull the latest (LV_CONF_PATH was removed — it breaks on Windows paths) and run `pio run -e ws7 -t clean` then `pio run -e ws7`.

`lib/Network` is a vendored copy of the Arduino-ESP32 3.0.7 Network library (link helper for `WiFi` → `Network.h`). If it’s ever missing:

```bash
mkdir -p lib/Network
cp -a ~/.platformio/packages/framework-arduinoespressif32/libraries/Network/. lib/Network/
# ensure library.properties name=Network
```

### Boot log notes

**GT911 `Unable to initialize the I2C address`:** expected and harmless on this board. Touch reset is on the CH422G expander (not an ESP32 GPIO), so the GT911 driver skips its own address-select dance and logs a warning. If you then see `TouchPad_ID:0x39,0x31,0x31` and `Board begin success`, touch is fine.

**COM port disconnects right after board begin:** usually the native USB-CDC port resetting when DTR/RTS is pulsed. Use `pio run -e ws7 -t monitor` (DTR/RTS off), or the board’s **UART** USB-C port for a stable log. Waveshare recommends UART for download and serial print.

### Debugging crashes

On boot the panel prints `[boot] reset reason: ...` (e.g. `PANIC`, `interrupt watchdog`, `brownout`). The native USB port disconnects when the chip resets, so the crash backtrace is often lost there — use the board’s **UART** USB-C port with `pio run -e ws7 -t monitor` to see the full panic output (decoded by `esp32_exception_decoder`). Use a good 5 V / 2 A supply; brownouts look like random reboots.

## MQTT topics

| Topic | Payload | Notes |
|-------|---------|--------|
| `kids/{slug}/job/{job_id}/state` | `done` / `todo` | retained — per job |
| `kids/{slug}/internet` | `on` / `off` | retained — **MikroTik unlock** for that kid only |
| `kids/{slug}/progress` | JSON | retained |
| `kids/{slug}/rewards/tally` | integer | retained — help credits |
| `kids/rewards/event` | JSON | not retained — `{doer, owner, job, …}` |
| `kids/panel/status` | `online` / `offline` | LWT |
| `kids/config/state` | JSON | retained — kids, jobs, who has each job, “needed for internet” flags (read by the web app) |
| `kids/config/set` | JSON | from the web app — `{"jobs":{"feed_cat":{"kids":["jax"],"required":true}}}` (partial updates OK) |

Slugs: `xavier`, `presley`, `jax`, `banx`.

**Admin commands:** `kids/{slug}/cmd/reset`, `kids/cmd/reset_all`, `kids/cmd/reset_rewards` (any non-empty payload, don’t retain). The panel clears any retained message on these topics when it connects.

### Lock down MQTT

The internet unlock is just a retained `kids/{slug}/internet` message, so anyone who can publish to your broker could send `on`. Give the panel its own Mosquitto user and restrict who can write `kids/#`, e.g. in Mosquitto’s `acl_file`:

```
# panel: the only writer of state topics
user kids-panel
topic readwrite kids/#

# Home Assistant + add-ons need full access when ACLs are on (HA requirement)
user homeassistant
topic readwrite #
user addons
topic readwrite #

# Web app (parents' phones): read state, send config + admin commands only
user parents
topic read kids/#
topic write kids/config/set
topic write kids/cmd/#
topic write kids/+/cmd/#
```

With the Mosquitto add-on, enable `customize` (folder `mosquitto`), put `acl_file /share/mosquitto/accesscontrollist` in `/share/mosquitto/acl.conf`, and the rules above in `/share/mosquitto/accesscontrollist`.

(Plain MQTT on 1883 isn’t encrypted — keep the broker on your trusted LAN / VLAN, not the kids’ one.)

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
