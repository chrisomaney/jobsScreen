#pragma once

#include <Arduino.h>

void clock_begin_ntp();
bool clock_is_synced();
bool clock_leftover_screen_open();  // true from 7:40am local until midnight
void clock_format_now(char *buf, size_t buflen);
int clock_minutes_until_leftover_open();  // -1 if open or unknown
