#include "clock_util.h"

#include "config.h"

#include <time.h>

void clock_begin_ntp() {
  // configTzTime keeps our POSIX TZ. (configTime(0, 0, ...) overwrites TZ with
  // "UTC0", which made the 7:40am unlock happen at 5:40pm in Brisbane.)
  configTzTime(TIMEZONE_POSIX, "pool.ntp.org", "time.google.com");
}

bool clock_is_synced() {
  // Before Nov 2023 → not synced yet
  return time(nullptr) > 1700000000;
}

namespace {

bool local_now(struct tm *out) {
  if (!clock_is_synced()) {
    return false;
  }
  time_t now = time(nullptr);
  localtime_r(&now, out);
  return true;
}

int open_minutes() { return LEFTOVER_OPENS_HOUR * 60 + LEFTOVER_OPENS_MINUTE; }

}  // namespace

bool clock_leftover_screen_open() {
  struct tm local {};
  if (!local_now(&local)) {
    return false;
  }
  return local.tm_hour * 60 + local.tm_min >= open_minutes();
}

int clock_minutes_until_leftover_open() {
  struct tm local {};
  if (!local_now(&local)) {
    return -1;
  }
  const int mins = local.tm_hour * 60 + local.tm_min;
  return mins >= open_minutes() ? 0 : open_minutes() - mins;
}

void clock_format_now(char *buf, size_t buflen) {
  struct tm local {};
  if (!local_now(&local)) {
    snprintf(buf, buflen, "time syncing...");
    return;
  }
  strftime(buf, buflen, "%H:%M", &local);
}

uint32_t clock_job_day() {
  if (!clock_is_synced()) {
    return 0;
  }
  // Shift back by the reset time so e.g. 02:59 still counts as "yesterday".
  time_t shifted = time(nullptr) - (DAILY_RESET_HOUR * 3600 + DAILY_RESET_MINUTE * 60);
  struct tm local {};
  localtime_r(&shifted, &local);
  return static_cast<uint32_t>((local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday);
}

bool clock_local_parts(int *hour, int *minute, int *wday) {
  struct tm local {};
  if (!local_now(&local)) {
    return false;
  }
  if (hour) {
    *hour = local.tm_hour;
  }
  if (minute) {
    *minute = local.tm_min;
  }
  if (wday) {
    *wday = local.tm_wday;
  }
  return true;
}
