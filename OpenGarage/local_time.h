#pragma once
#include <stdint.h>

namespace og_local {
// Call regularly: unsigned subtraction remains valid across millis() rollover.
struct Interval {
  bool started = false;
  uint32_t last = 0;
  bool due(uint32_t now, uint32_t interval) {
    if (started && uint32_t(now - last) < interval) return false;
    started = true; last = now; return true;
  }
};
// Interim SSID-only recovery. Replaced by the AP-selection manager later.
struct ReconnectBackoff {
  bool waiting = false;
  uint32_t since = 0, delay_ms = 120000;
  bool due(uint32_t now, bool online, bool busy) {
    if (online) { waiting = false; delay_ms = 120000; return false; }
    if (!waiting) { waiting = true; since = now; return false; }
    if (busy || uint32_t(now - since) < delay_ms) return false;
    since = now;
    delay_ms = delay_ms < 300000 ? delay_ms * 2 : 600000;
    return true;
  }
};
struct OpenDuration {
  bool active = false;
  uint32_t since = 0;
  void reset() { active = false; }
  void start(uint32_t now) { active = true; since = now; }
  bool expired(uint32_t now, uint32_t duration) {
    if (!active) { start(now); return false; }
    if (uint32_t(now - since) < duration) return false;
    reset(); return true;
  }
};
struct DailyRule {
  bool handled = false;
  uint32_t high_day = 0;
  bool eligible(bool valid, uint32_t utc, uint8_t hour) const {
    return valid && (utc % 86400U) / 3600U == hour &&
           (!handled || utc / 86400U > high_day);
  }
  void mark(uint32_t utc) { handled = true; high_day = utc / 86400U; }
};
// Uptime is extended independently of UTC, including before the first sync.
struct Clock {
  static constexpr uint32_t MIN_UTC = 1577836800UL;
  uint32_t previous = 0;
  uint64_t uptime_ms = 0;
  bool valid = false;
  uint32_t utc = 0;
  uint64_t anchor_ms = 0;
  uint32_t anchor_utc = 0;
  void tick(uint32_t now, uint32_t observed_utc) {
    uptime_ms += uint32_t(now - previous); previous = now;
    if (observed_utc >= MIN_UTC) {
      valid = true; anchor_ms = uptime_ms; anchor_utc = observed_utc;
    }
    utc = valid ? anchor_utc + uint32_t((uptime_ms - anchor_ms) / 1000) : 0;
  }
};
}
