# Family panel web apps

| File | Purpose |
|------|---------|
| `index.html` | **Jobs Chooser** — who does which job on the jobs panel |
| `rewards-admin.html` | **Rewards HQ** — prizes, bonus, per-job points, scheduled rules, panel messages |
| `rewards-celebrate.html` | **Celebrate** — full-screen WINNER/BONUS for phone or Android TV browser |

Copy into Home Assistant `www` (e.g. `/config/www/kids-jobs/` and `/config/www/kids-rewards/`).

---

# Jobs Chooser

A single HTML file for choosing which kid does which job on the panel. Open it on your phone, tick the boxes and tap **Save to panel**.

- **Who does what:** one tick box per kid for every Expectation and Paid job. A job with nobody ticked is hidden on the panel.
- **Add expectation or job:** name + type (Expectation / Paid job), optional hint, and whether it's needed for internet. Up to 8 custom items; remove them with **Remove** on the row.
- **Needed for internet:** switch a job between *needed* (must be done before that kid's internet unlocks) and *optional*.
- **Live status:** each kid's job count, today's progress and internet on/off.
- **Panel actions:** clear today's ticks, zero reward tallies.

It talks to the panel over your MQTT broker. Nothing goes to the cloud except loading the fonts and the MQTT library from a CDN.

## How it works

| Topic | Direction | Payload |
|---|---|---|
| `kids/config/state` | panel → app (retained) | `{"v":1,"customSlotsFree":n,"kids":[{"slug","name"}],"jobs":[{"id","label","cat","required","custom","kids":[slugs]}]}` |
| `kids/config/set` | app → panel | Assign: `{"jobs":{"<id>":{"kids":["xavier","jax"],"required":true}}}`. Add: `{"add":{"label":"…","cat":"expectation\|paid","required":true,"kids":[…]}}`. Remove custom: `{"remove":["custom_0"]}` |
| `kids/{slug}/progress` | panel → app | live progress shown on the kid cards |
| `kids/panel/status` | panel → app | `online` / `offline` |

The panel applies the change, saves it to flash, re-publishes `kids/config/state` (that's how the app confirms the save) and re-publishes every kid's internet on/off, because changing jobs can unlock or lock someone.

## Setup (Home Assistant + Mosquitto add-on)

1. **Turn on WebSockets in Mosquitto.** The add-on listens on **1884** (plain WebSockets) by default, and on **8884** (WebSockets with SSL) if you've set `certfile`/`keyfile`. Make sure 1884 isn't blanked out in the add-on's Network settings.
2. **Create a broker login for the app,** e.g. a Home Assistant user called `parents` (*Settings → People → Users*). The Mosquitto add-on accepts HA users. You can optionally restrict it with the ACL in the main README.
3. **Copy the page into Home Assistant:**
   ```
   /config/www/kids-jobs/index.html
   ```
   (create `www` if it doesn't exist, then restart HA once).
4. **Open it on your phone:** `http://homeassistant.local:8123/local/kids-jobs/index.html`
   - The broker address defaults to `ws://<same host>:1884`. Enter the username and password and tap **Connect**.
   - Tick **Remember on this device** so it reconnects automatically next time. The login is stored in the browser's local storage, so only do this on your own devices.
   - Add it to your home screen for an app-like icon.

### If you open Home Assistant over https (Nabu Casa / reverse proxy)

Browsers block `ws://` from an `https://` page. Either open the page over plain `http://` on your home Wi-Fi, or use `wss://homeassistant.local:8884` with the Mosquitto SSL certificate set up.

### Without Home Assistant

Any static file host on your LAN works, e.g. `python3 -m http.server 8080` in this folder, as long as the broker has a WebSocket listener:

```
# mosquitto.conf
listener 1884
protocol websockets
```

## Try it without a broker

Open `index.html?demo`, or tap **Try demo**, to use a pretend panel.
