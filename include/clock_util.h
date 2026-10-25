#pragma once

#include <Arduino.h>

void clock_begin_ntp();
bool clock_is_synced();
bool clock_leftover_screen_open();  // true from LEFTOVER_OPENS (7:40am) local until midnight
void clock_format_now(char *buf, size_t buflen);
int clock_minutes_until_leftover_open();  // -1 if unknown (not synced), 0 if open

// The "jobs day" as YYYYMMDD in local time. The day rolls over at
// DAILY_RESET_HOUR:DAILY_RESET_MINUTE (not midnight). 0 if the clock isn't synced.
uint32_t clock_job_day();

// Local wall clock parts. Returns false if NTP not synced yet.
// wday: 0=Sunday … 6=Saturday (same as struct tm).
bool clock_local_parts(int *hour, int *minute, int *wday);
