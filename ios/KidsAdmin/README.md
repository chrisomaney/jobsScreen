# Kids Admin (native iOS / Xcode)

SwiftUI iPhone app for parent controls: **Internet On/Off/Auto**, ± points, behaviour messages, and add/edit expectations & jobs. Talks to the panel over **MQTT WebSockets** (same Mosquitto login as Rewards HQ).

## Open in Xcode

1. On a Mac, clone this repo (or copy the `ios/KidsAdmin` folder).
2. Double-open **`KidsAdmin.xcodeproj`**.
3. Select the **KidsAdmin** target → **Signing & Capabilities** → choose your **Team** (personal Apple ID is fine for your own iPhone).
4. Plug in your iPhone (or pick a simulator), then **Product → Run** (▶).
5. First launch: allow **Local Network** when iOS asks.
6. In **Connect**, set broker e.g. `ws://homeassistant.local:1884` (or `ws://192.168.1.199:1884`), username/password, tap **Connect**.

Keep the app on your home screen — it is a real native install from Xcode (Developer Mode may need enabling on the phone under Settings → Privacy & Security).

## Tabs

| Tab | What it does |
|-----|----------------|
| **Internet** | Force **On / Off / Auto** per kid (stays until you change it). Also **common** profile on/off. |
| **Points** | Add or take points for each kid. |
| **Behaviour** | ± points + message on the panel and TV celebrate page. |
| **Jobs** | Expectations bundle, add/edit/remove expectations & paid jobs, job points. |
| **Connect** | Broker + login. |

## MQTT used by this app

- `kids/{slug}/internet/set` → `on` \| `off` \| `auto` (panel stores override, republishes effective `kids/{slug}/internet`)
- `kids/common/internet/set` → `on` \| `off`
- `kids/rewards/bonus/set` — points / behaviour JSON
- `kids/config/set` — add / edit / remove jobs
- `kids/rewards/rules/set` — points table + expectations bundle

Flash the matching panel firmware so internet overrides stick when jobs change.

## Requirements

- Xcode 15+ (iOS 17 deployment target)
- iPhone on the same Wi‑Fi as Mosquitto / the panel
- No App Store account required for installing on your own device with a free Apple ID (7‑day refresh for free provisioning)

## iOS Downtime / Screen Time

Apple does **not** allow a home-network app to turn another iPhone’s Downtime on or off.
That needs Apple **Family Sharing → Screen Time** (on your parent iPhone), or a school/work **MDM**.
Kids Admin can unlock MikroTik internet; use Screen Time separately for app/device limits.
