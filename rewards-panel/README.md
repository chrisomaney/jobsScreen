# Prize Shop (rewards 7" panel)

Bright, simple prize screen for kids. Parents manage prizes on their phone.

## Kids use it in 2 taps

1. **I want this!** on a prize  
2. **Tap your name** (buttons show if you have enough points)  
→ **YOU WIN!** (points deducted) or **Keep earning!**

Bonus points from parents show a **BONUS POINTS!** splash.

## Parents — 3 steps

### 1. Flash the panel
```bash
cd rewards-panel
cp include/secrets.example.h include/secrets.h   # Wi-Fi + MQTT
pio run -e ws7r -t upload
```

### 2. Put the phone pages in Home Assistant
Copy from this repo’s `webapp/` folder:
```
/config/www/kids-rewards/rewards-admin.html
/config/www/kids-rewards/rewards-celebrate.html
```

### 3. Open on your phone
`http://homeassistant.local:8123/local/kids-rewards/rewards-admin.html`  
Connect with the same Mosquitto login as Jobs Chooser → add prizes → **Save to screen**.

**TV splash:** open `rewards-celebrate.html` full screen in the HA browser / media app on the Android TV.

## MQTT (for HA automations)

| Topic | What |
|---|---|
| `kids/{slug}/rewards/tally` | points (shared with jobs panel) |
| `kids/rewards/catalog/set` | save prize list |
| `kids/rewards/bonus/set` | `{kid, points}` |
| `kids/rewards/celebrate` | phone/TV party splash |

```yaml
alias: Prize party notify
trigger:
  - platform: mqtt
    topic: kids/rewards/celebrate
action:
  - service: notify.notify
    data:
      title: "{{ trigger.payload_json.title }}"
      message: "{{ trigger.payload_json.kid }} — {{ trigger.payload_json.subtitle }}"
```
