# Kids Admin (native iOS / Xcode)

SwiftUI iPhone app for parent controls: **Internet On/Off/Auto**, ± points, behaviour messages, add/edit expectations & jobs, **bundled Jobs Chooser / Rewards HQ websites**, and **Bluetooth** job sync when MQTT is flaky. Talks to the panel over **MQTT WebSockets** (same Mosquitto login as Rewards HQ) or **BLE**.

## Open in Xcode

1. On a Mac, clone this repo (or copy the `ios/KidsAdmin` folder).
2. Double-open **`KidsAdmin.xcodeproj`**.
3. Select the **KidsAdmin** target → **Signing & Capabilities** → choose your **Team** (personal Apple ID is fine for your own iPhone).
4. Plug in your iPhone (or pick a simulator), then **Product → Run** (▶).
5. First launch: allow **Local Network** and **Bluetooth** when iOS asks.
6. In **Connect**, set broker e.g. `ws://homeassistant.local:1884` (or `ws://192.168.1.199:1884`), username/password, tap **Connect**.

Keep the app on your home screen — it is a real native install from Xcode (Developer Mode may need enabling on the phone under Settings → Privacy & Security).

## Tabs

| Tab | What it does |
|-----|----------------|
| **Internet** | Force **On / Off / Auto** per kid (stays until you change it). Also **common** profile on/off. |
| **Points** | Add or take points for each kid. |
| **Behaviour** | ± points + message on the panel and TV celebrate page. |
| **Jobs** | Expectations bundle, add/edit/remove expectations & paid jobs, job points. Saves over MQTT, or BLE if connected. |
| **Website** | Full **Jobs Chooser** + **Rewards HQ** pages (bundled in the app, or load from your HA `/local/kids` URL). |
| **Bluetooth** | Scan for **KidsPanel**, connect, then use Jobs to push config when MQTT won’t connect. |
| **Connect** | Broker + login + website source. Auto-reconnects if the broker drops. |

## MQTT reliability

The WebSocket client waits for the socket to open before sending MQTT CONNECT (fixes flaky connects), buffers partial frames, and reconnects with backoff so the UI doesn’t sit frozen offline.

## Bluetooth job sync

Flash panel firmware that includes the **KidsPanel** BLE service. In the app:

1. Open **Bluetooth** → **Scan for Kids Panel**
2. When Ready, open **Jobs** and Save / Add as usual — traffic goes over BLE if MQTT is down

## MQTT used by this app

- `kids/{slug}/internet/set` → `on` \| `off` \| `auto`
- `kids/common/internet/set` → `on` \| `off`
- `kids/rewards/bonus/set` — points / behaviour JSON
- `kids/config/set` — add / edit / remove jobs
- `kids/rewards/rules/set` — points table + expectations bundle

## Requirements

- Xcode 15+ (iOS 17 deployment target)
- iPhone on the same Wi‑Fi as Mosquitto / the panel (for MQTT)
- Bluetooth for the BLE fallback
- No App Store account required for installing on your own device with a free Apple ID (7‑day refresh for free provisioning)

## iOS Downtime / Screen Time

Apple does **not** allow a home-network app to turn another iPhone’s Downtime on or off.
That needs Apple **Family Sharing → Screen Time** (on your parent iPhone), or a school/work **MDM**.
Kids Admin can unlock MikroTik internet; use Screen Time separately for app/device limits.
