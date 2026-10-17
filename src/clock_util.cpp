#include "clock_util.h"

#include "config.h"

#include <time.h>

namespace {

bool synced_ = false;

}  // namespace

void clock_begin_ntp() {
  // POSIX TZ — edit TIMEZONE_POSIX in config.h for your location
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com");
}

bool clock_is_synced() {
  time_t now = time(nullptr);
  // Before 2024 → not synced yet
  synced_ = (now > 1700000000);
  return synced_;
}

bool clock_leftover_screen_open() {
  if (!clock_is_synced()) {
    return false;
  }
  time_t now = time(nullptr);
  struct tm local {};
  localtime_r(&now, &local);
  const int mins = local.tm_hour * 60 + local.tm_min;
  const int open_mins = LEFTOVER_OPENS_HOUR * 60 + LEFTOVER_OPENS_MINUTE;
  return mins >= open_mins;
}

int clock_minutes_until_leftover_open() {
  if (!clock_is_synced()) {
    return -1;
  }
  if (clock_leftover_screen_open()) {
    return 0;
  }
  time_t now = time(nullptr);
  struct tm local {};
  localtime_r(&now, &local);
  const int mins = local.tm_hour * 60 + local.tm_min;
  const int open_mins = LEFTOVER_OPENS_HOUR * 60 + LEFTOVER_OPENS_MINUTE;
  return open_mins - mins;
}

void clock_format_now(char *buf, size_t buflen) {
  if (!clock_is_synced()) {
    snprintf(buf, buflen, "time syncing…");
    return;
  }
  time_t now = time(nullptr);
  struct tm local {};
  localtime_r(&now, &local);
  strftime(buf, buflen, "%H:%M", &local);
}
