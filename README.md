# Kids Jobs Sign-off (ESP32-S3)

Firmware for the **Waveshare ESP32-S3-Touch-LCD-7** (800×480). One panel for **jobs + prize shop**. When nobody’s using it, the neon **Prize Station** runs; a big **JOBS →** button opens the jobs flow. After **7:40am**, **Jobs left to do** lets anyone finish leftovers, pick who did it (reward points), and only unlocks internet for kids whose **own** list is fully clear.

Kids: **Xavier, Presley, Jax, Banx, Hazel**. Phone admin: Jobs Chooser + [rewards-admin.html](webapp/rewards-admin.html). Optional second display firmware still lives under [rewards-panel/](rewards-panel/) if you want a dedicated prize screen.

## Screens

| Screen | Purpose |
|--------|---------|
| **Prize Station (idle)** | Carousel of prizes, **I WANT THIS**, kid point chips, big **JOBS →**. Returns here after **3 min** idle |
| **Home** | Five kid cards + **Jobs left to do** (locked until 7:40am) |
| **Expectations** | That kid’s daily expectations + Done |
| **Jobs** | That kid’s paid jobs + Done |

**Done buttons:** tap to mark done; **hold** a green (done) button to undo. **Reset** on a kid’s page also needs a hold.
| **Jobs left to do** | Opens **7:40am**. Outstanding items by kid. Done → **Who did this?** name picker |
| **Who did this?** | Credits the helper for Rewards; marks the job done on the **owner’s** list. The owner isn’t offered (no self-credit) |

Idle timeout: `IDLE_TIMEOUT_MS` in `include/config.h`. Prizes are edited from `webapp/rewards-admin.html` (same Mosquitto login).

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
pio run -t upload
pio device monitor
```

If you see `fatal error: Network.h: No such file or directory`, pull the latest `main` (the include paths in `platformio.ini` are now relative, so project folders with spaces or Windows paths work), then run `pio run -t clean` and build again.

If you see `lv_conf.h: No such file` on Windows, pull the latest from this branch (LV_CONF_PATH was removed — it breaks on Windows paths) and run `pio run -t clean` then `pio run`.

`lib/Network` is a vendored copy of the Arduino-ESP32 3.0.7 Network library (link helper for `WiFi` → `Network.h`). If it’s ever missing:

```bash
mkdir -p lib/Network
cp -a ~/.platformio/packages/framework-arduinoespressif32/libraries/Network/. lib/Network/
# ensure library.properties name=Network
```

### Debugging crashes

On boot the panel prints `[boot] reset reason: ...` (e.g. `PANIC`, `interrupt watchdog`, `brownout`). The native USB port disconnects when the chip resets, so the crash backtrace is often lost there — use the board’s **UART** USB-C port with `pio device monitor` to see the full panic output (decoded by `esp32_exception_decoder`). Use a good 5 V / 2 A supply; brownouts look like random reboots.

## MQTT topics

| Topic | Payload | Notes |
|-------|---------|--------|
| `kids/{slug}/job/{job_id}/state` | `done` / `todo` | retained — per job |
| `kids/{slug}/internet` | `on` / `off` | retained — **MikroTik unlock** for that kid only |
| `kids/common/internet` | `on` / `off` | retained — **COMMON** profile: on if **Xavier or Presley** is unlocked |
| `kids/{slug}/progress` | JSON | retained |
| `kids/{slug}/rewards/tally` | integer | retained — help credits |
| `kids/rewards/event` | JSON | not retained — `{doer, owner, job, …}` |
| `kids/panel/status` | `online` / `offline` | LWT |
| `kids/config/state` | JSON | retained — kids, jobs, who has each job, “needed for internet” flags (read by the web app) |
| `kids/config/set` | JSON | from the web app — assign `{"jobs":{…}}`, add `{"add":{"label","cat":"expectation\|paid",…}}`, or remove `{"remove":["custom_0"]}` |

Slugs: `xavier`, `presley`, `jax`, `banx`, `hazel`.

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

Each expectation and paid job shows a **line (outline) icon** on the panel (and matching emoji in the web app) so kids who can’t read yet — like Jax — can still tell them apart. To redraw icons after editing the art: `python3 tools/gen_job_icons.py`.

The Jobs Chooser can also **add new expectations or paid jobs** (up to 8 custom items) without reflashing; they appear on the panel after **Add to panel**, and custom rows have a **Remove** control.
