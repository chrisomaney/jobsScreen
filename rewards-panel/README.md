# Kids Rewards Panel (second Waveshare 7")

Exciting prize advertising on a dedicated ESP32-S3 Touch LCD 7". Kids tap a prize, pick their name, and get a big **WINNER** splash when they have enough points. Parents manage prizes and bonus points from a phone page. Home Assistant can notify phones and show the same splash on Android TV.

Shares reward tallies with the **jobs sign-off** panel via MQTT (`kids/{slug}/rewards/tally`).

## Flash

```bash
cd rewards-panel
cp include/secrets.example.h include/secrets.h
# edit Wi-Fi + MQTT (use client id kids-rewards-panel)
pio run -e ws7r -t upload
pio device monitor -b 115200
```

On Windows use a short path (e.g. `C:\js\rewards-panel`) and env **`ws7r`**.

## Kids UI

1. Rotating prize ads (name + point cost + live score ticker)
2. Tap prize → pick kid
3. Not enough points → “Keep going!”
4. Enough → deduct points → **WINNER!** → MQTT claim + celebrate
5. Bonus from admin → **BONUS!** splash

## Phone admin + TV page

Copy into Home Assistant:

```
/config/www/kids-rewards/rewards-admin.html
/config/www/kids-rewards/rewards-celebrate.html
```

(from this repo’s `webapp/` folder)

- Admin: `http://homeassistant.local:8123/local/kids-rewards/rewards-admin.html`
- TV celebrate (full screen browser / media): `…/rewards-celebrate.html`

Same Mosquitto WebSocket setup as the Jobs Chooser (`ws://…:1884`).

## MQTT

| Topic | Role |
|---|---|
| `kids/{slug}/rewards/tally` | retained points (shared with jobs panel) |
| `kids/rewards/catalog` | retained prize list JSON |
| `kids/rewards/catalog/set` | admin writes new catalog |
| `kids/rewards/bonus/set` | `{kid, points}` add bonus |
| `kids/rewards/claim` | claim event |
| `kids/rewards/bonus` | bonus event |
| `kids/rewards/celebrate` | `{type, kid, title, subtitle, …}` for HA/TV |
| `kids/rewards-panel/status` | LWT online/offline |

### Example HA notify (phones)

```yaml
alias: Rewards celebrate notify
trigger:
  - platform: mqtt
    topic: kids/rewards/celebrate
action:
  - service: notify.notify
    data:
      title: "{{ trigger.payload_json.title }}"
      message: "{{ trigger.payload_json.kid }} — {{ trigger.payload_json.subtitle }}"
```

For Android TV: open/cast the celebrate page (HA browser / media) on the same MQTT trigger.
