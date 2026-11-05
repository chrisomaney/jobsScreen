# OTA Updates Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** ArduinoOTA on the jobs panel + PlatformIO espota env so Cloud Agent can push over LAN/WAN after one USB bootstrap.

**Architecture:** Small `ota_bridge` module started after Wi‑Fi; `loop()` calls `ota_bridge_loop()`. Password from `secrets.h`. Separate PlatformIO env uses `espota`.

**Tech Stack:** Arduino-ESP32 ArduinoOTA, PlatformIO espota, existing 16MB dual-app partitions.

### Task 1: Secrets + OTA module + main wire + pio env + README

**Files:**
- Modify: `include/secrets.example.h`, `include/app_secrets.h`, `src/main.cpp`, `platformio.ini`, `README.md`
- Create: `include/ota_bridge.h`, `src/ota_bridge.cpp`

**Steps:**
1. Add `OTA_PASSWORD` / `OTA_HOSTNAME` defaults
2. Implement ArduinoOTA begin/handle with password
3. Call from `setup`/`loop`
4. Add `-ota` PlatformIO env
5. Document USB bootstrap + router forward
6. Build, commit, PR
