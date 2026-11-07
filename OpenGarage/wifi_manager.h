#pragma once
#include <stdint.h>

namespace og_wifi {
// Own every configured-STA attempt. Selection may insert an asynchronous scan
// before an attempt; plain SSID recovery remains available without selection.
// A result needs both association and an IP, not just an SDK status transition.
class Recovery {
 public:
  enum class State : uint8_t { STOPPED, WAITING, SCANNING, CONNECTING, ONLINE };
  enum class Action : uint8_t { NONE, CONNECT, DISCONNECT, SCAN };
  void start(uint32_t now) {
    *this = Recovery(); state_ = State::WAITING; since_ = now;
  }
  Action step(uint32_t now, bool online, bool critical_busy, bool motion_busy = false, bool select = false) {
    if (state_ == State::STOPPED) return Action::NONE;
    if (online) {
      motion_wait_ = false;
      if (state_ != State::ONLINE) { state_ = State::ONLINE; since_ = now; }
      if (uint32_t(now-since_) >= 30000) next_delay_ = 5000;
      return Action::NONE;
    }
    if (state_ == State::ONLINE) {
      ++disconnects_;
      // A healthy session gets prompt recovery. Short-lived sessions retain
      // failure backoff so a repeatedly rejected client cannot thrash the AP.
      const bool healthy = uint32_t(now-since_) >= 30000;
      state_ = State::WAITING; since_ = now;
      if (healthy) { next_delay_ = 5000; wait_ms_ = 5000; }
      else backoff_();
    } else if (state_ == State::CONNECTING && uint32_t(now-since_) >= 15000) {
      ++timeouts_; state_ = State::WAITING; since_ = now; backoff_();
      // Cancel the expired attempt even if local work has since become busy.
      // disconnect(false,false) does not wait for a network response.
      return Action::DISCONNECT;
    }
    if (!motion_busy) motion_wait_ = false;
    if (state_ == State::WAITING && uint32_t(now-since_) >= wait_ms_) {
      // Movement can be the last stale opener report. Bound only that hint;
      // never override an actual warning, pending command, upload or restart.
      if (motion_busy && !motion_wait_) { motion_wait_ = true; motion_since_ = now; }
      if (critical_busy || (motion_busy && uint32_t(now-motion_since_) < 60000)) return Action::NONE;
      motion_wait_ = false;
      if (select && !scan_ready_) { state_ = State::SCANNING; return Action::SCAN; }
      scan_ready_ = false;
      state_ = State::CONNECTING; since_ = now; ++attempts_;
      return Action::CONNECT;
    }
    return Action::NONE;
  }
  void scan_finished(uint32_t now) {
    if (state_ != State::SCANNING) return;
    state_ = State::WAITING; since_ = now; wait_ms_ = 0; scan_ready_ = true;
  }
  // A confirmed idle roam is an intentional migration, not a failed session.
  void roam(uint32_t now) {
    state_ = State::WAITING; since_ = now; wait_ms_ = 0; scan_ready_ = true;
    motion_wait_ = false;
  }
  State state() const { return state_; }
  const char *status() const {
    switch (state_) {
      case State::WAITING: return "Waiting to reconnect";
      case State::SCANNING: return "Scanning access points";
      case State::CONNECTING: return "Connecting";
      case State::ONLINE: return "Connected";
      default: return "Inactive";
    }
  }
  uint32_t retry_in_ms(uint32_t now) const {
    if (state_ != State::WAITING) return 0;
    const uint32_t elapsed = now-since_;
    return elapsed >= wait_ms_ ? 0 : wait_ms_-elapsed;
  }
  uint32_t attempts() const { return attempts_; }
  uint32_t timeouts() const { return timeouts_; }
  uint32_t disconnects() const { return disconnects_; }
 private:
  void backoff_() {
    wait_ms_ = next_delay_;
    next_delay_ = next_delay_ < 150000 ? next_delay_*2 : 300000;
  }
  State state_ = State::STOPPED;
  uint32_t since_ = 0, wait_ms_ = 0, next_delay_ = 5000;
  bool motion_wait_ = false, scan_ready_ = false;
  uint32_t motion_since_ = 0;
  uint32_t attempts_ = 0, timeouts_ = 0, disconnects_ = 0;
};
}
