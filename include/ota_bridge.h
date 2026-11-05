#pragma once

#include <Arduino.h>

// Network firmware updates (ArduinoOTA). Call begin once Wi‑Fi is up enough
// to have a hostname; call loop() every main-loop iteration.
void ota_bridge_begin();
void ota_bridge_loop();
bool ota_bridge_active();  // true while an upload is in progress
