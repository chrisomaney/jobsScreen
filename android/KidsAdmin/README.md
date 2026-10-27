# Kids Admin (native Android)

Kotlin + Jetpack Compose Android app matching the iOS Kids Admin app: **Internet On/Off/Auto**, ± points, behaviour messages, and add/edit expectations & jobs. Talks to the panel over **MQTT WebSockets** (same Mosquitto login).

## Open in Android Studio

1. Install [Android Studio](https://developer.android.com/studio) (Hedgehog / Ladybug or newer).
2. **File → Open** → select this folder: `android/KidsAdmin`
3. Let Gradle sync (downloads SDK / Compose).
4. Plug in a phone with USB debugging, or start an emulator.
5. Tap **Run** ▶ (`app`).

### First launch

1. Allow the app on your Wi‑Fi (same network as Mosquitto / the panel).
2. Open **Connect**, set broker e.g. `ws://192.168.1.199:1884` or `ws://homeassistant.local:1884`, username/password, tap **Connect**.
3. Use **Internet / Points / Behaviour / Jobs** like the iPhone app.

## Tabs

| Tab | What it does |
|-----|----------------|
| **Internet** | Force **On / Off / Auto** per kid + **common** profile |
| **Points** | Add or take points |
| **Behaviour** | ± points + panel / TV message |
| **Jobs** | Expectations bundle, add/edit/remove jobs, job points |
| **Connect** | Broker + login (remembered on device) |

## MQTT (same as iOS)

- `kids/{slug}/internet/set` → `on` \| `off` \| `auto`
- `kids/common/internet/set` → `on` \| `off`
- `kids/rewards/bonus/set`, `kids/config/set`, `kids/rewards/rules/set`

Flash the panel firmware that supports internet overrides.

## Requirements

- Android Studio with SDK 35 / JDK 17
- Android 8.0+ (API 26)
- Phone on the same LAN as Mosquitto

## Build from CLI (optional)

```bash
cd android/KidsAdmin
./gradlew :app:assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
```
