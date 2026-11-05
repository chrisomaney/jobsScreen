# OTA updates design (kids jobs panel)

**Status:** approved  
**Date:** 2026-11-05 (Australia/Brisbane)

## Goal
Stop USB reflashing for routine firmware updates. Cloud agent (or laptop) compiles and pushes over the network; panel stays upstairs.

## Approach
**ArduinoOTA** (ESP32 password-auth) + PlatformIO `espota` upload.

- Hostname: `kids-jobs-panel` (already used for Wi‑Fi)
- Default listen: TCP **3232** (ArduinoOTA)
- Password: `OTA_PASSWORD` in gitignored `include/secrets.h`
- One **USB bootstrap** flash after this lands, then network forever

## WAN path (when Cloud Agent cannot see LAN)
1. Panel on Wi‑Fi, OTA listening.
2. Router port-forward: external TCP port → panel LAN IP:3232.
3. Agent uploads with `pio run -e waveshare-esp32s3-touch-lcd-7-ota -t upload` using `OTA_HOST` / `OTA_PORT` / password.
4. Close or restrict the forward when idle (strong password required).

## Out of scope (for now)
- HTTPS pull from GitHub Releases
- Rewards-panel OTA (same pattern later if needed)
- Auto-update without an explicit push

## Success
After one USB flash: agent can push a build to the panel over WAN/LAN without moving the screen.
