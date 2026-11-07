#pragma once
#include <stdint.h>
#include <string.h>

namespace og_wifi {
struct AccessPoint {
  uint8_t bssid[6] = {};
  uint8_t channel = 0;
  int32_t rssi = -127;
  bool valid = false;
  bool same(const uint8_t *other) const { return valid && other && !memcmp(bssid, other, 6); }
};

// Radio-independent selection policy. No allocation, credentials, or SDK calls.
// Failed targets are cooled down; a targeted failure always earns a plain-SSID
// attempt before another scan. This also preserves hidden-network recovery.
class Selection {
 public:
  enum class Scan : uint8_t { NONE, RECOVERY, SEARCH, CONFIRM };
  bool plain_next = false;
  AccessPoint target;
  void reset() { *this = Selection(); }
  void failed(uint32_t now) {
    if (!target.valid) return;
    Failure &f = failures_[failure_slot_++ % 4];
    f.ap = target; f.since = now; f.active = true;
    target = AccessPoint(); plain_next = true; roaming_ = false;
  }
  void connected(uint32_t now) {
    target = AccessPoint(); plain_next = false;
    if (roaming_) { ++roams_; dwell_ = true; dwell_since_ = now; }
    roaming_ = false; online_ = true; sampled_ = weak_ = healthy_ = false;
    stage_ = Scan::NONE; scan_delay_ = 120000; scan_wait_ = false;
  }
  void disconnected() { target = AccessPoint(); online_ = false; weak_ = sampled_ = healthy_ = false; stage_ = Scan::NONE; }
  void sample(uint32_t now, int32_t rssi) {
    if (!online_ || rssi >= 0 || rssi < -120) return;
    if (sampled_ && uint32_t(now-sample_ms_) < 2000) return;
    filtered_ = sampled_ ? (filtered_*4 + rssi*8)/5 : rssi*8;
    sampled_ = true; sample_ms_ = now;
    if (filtered_ < -72*8) {
      healthy_ = false;
      if (!weak_) { weak_ = true; weak_since_ = now; }
    } else {
      weak_ = false;
      if (!healthy_) { healthy_ = true; healthy_since_ = now; }
      if (uint32_t(now-healthy_since_) >= 300000) { scan_wait_ = false; scan_delay_ = 120000; }
    }
  }
  Scan due(uint32_t now, bool idle) {
    if (!online_ || !idle) return Scan::NONE;
    if (stage_ == Scan::CONFIRM) {
      if (uint32_t(now-confirm_ms_) > 15000 || !weak_) { no_candidate(now); return Scan::NONE; }
      return uint32_t(now-confirm_ms_) >= 5000 ? Scan::CONFIRM : Scan::NONE;
    }
    if (!weak_ || uint32_t(now-weak_since_) < 60000 ||
        (dwell_ && uint32_t(now-dwell_since_) < 600000) ||
        (scan_wait_ && uint32_t(now-last_scan_) < scan_delay_)) return Scan::NONE;
    return Scan::SEARCH;
  }
  void begin(Scan scan) { scanning_ = scan; best_ = AccessPoint(); current_seen_ = false; }
  void offer(uint32_t now, const AccessPoint &ap, const uint8_t *current) {
    if (!ap.valid || ap.channel < 1 || ap.channel > 14) return;
    if (ap.same(current)) { current_seen_ = true; current_rssi_ = ap.rssi; }
    for (const auto &f : failures_)
      if (f.active && ap.same(f.ap.bssid) && uint32_t(now-f.since) < 1800000) return;
    if (scanning_ != Scan::RECOVERY) {
      if (ap.same(current)) return;
      if (scanning_ == Scan::CONFIRM && !ap.same(candidate_.bssid)) return;
    }
    if (!best_.valid || ap.rssi > best_.rssi) best_ = ap;
  }
  // Current-AP scan strength is preferred; confirmation on another channel uses
  // the more conservative of the smoothed value and current live RSSI.
  bool finish(uint32_t now, int32_t live_rssi) {
    const Scan scan = scanning_; scanning_ = Scan::NONE;
    ++scans_;
    if (scan == Scan::RECOVERY) { target = best_; return false; }
    const int32_t baseline = current_seen_ ? current_rssi_ :
        (live_rssi > smoothed() ? live_rssi : smoothed());
    if (!online_ || !weak_ || !best_.valid || best_.rssi < -75 || best_.rssi < baseline+8) { no_candidate(now); return false; }
    if (scan == Scan::SEARCH) {
      candidate_ = best_; stage_ = Scan::CONFIRM; confirm_ms_ = now; return false;
    }
    target = best_; stage_ = Scan::NONE; roaming_ = true; return true;
  }
  void no_candidate(uint32_t now) {
    stage_ = Scan::NONE; target = AccessPoint(); roaming_ = false;
    if (scan_wait_) scan_delay_ = scan_delay_ == 120000 ? 300000 : scan_delay_ == 300000 ? 900000 : 1800000;
    scan_wait_ = true; last_scan_ = now;
  }
  void cancel() { roaming_ = false; stage_ = scanning_ = Scan::NONE; target = AccessPoint(); }
  uint8_t confirm_channel() const { return candidate_.channel; }
  int32_t smoothed() const { return sampled_ ? filtered_/8 : -127; }
  uint32_t scans() const { return scans_; }
  uint32_t roams() const { return roams_; }
 private:
  struct Failure { AccessPoint ap; uint32_t since = 0; bool active = false; } failures_[4];
  uint8_t failure_slot_ = 0;
  AccessPoint best_, candidate_;
  Scan stage_ = Scan::NONE, scanning_ = Scan::NONE;
  bool online_ = false, sampled_ = false, weak_ = false, healthy_ = false;
  bool current_seen_ = false, roaming_ = false, dwell_ = false, scan_wait_ = false;
  int32_t filtered_ = -127*8, current_rssi_ = -127;
  uint32_t sample_ms_ = 0, weak_since_ = 0, healthy_since_ = 0, confirm_ms_ = 0;
  uint32_t dwell_since_ = 0, last_scan_ = 0, scan_delay_ = 120000;
  uint32_t scans_ = 0, roams_ = 0;
};
}
