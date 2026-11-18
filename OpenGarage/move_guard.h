// Ignore further door commands sent through OpenGarage while the door is already moving because of
// one. SPDX-License-Identifier: GPL-3.0-or-later
//
// A hardware remote talks to the opener directly, so it is never blocked, and the guard watches the
// door state like everything else: if the remote stops or reverses the door, the new state ends the
// guard. The guard also ends by itself after timeout_ms, so a stuck sensor or a command that did
// nothing cannot lock the door out of OpenGarage commands.
#pragma once
#include <stdint.h>

namespace og_guard {
class MoveGuard {
 public:
  // timeout_ms: longest time commands stay blocked after one is sent.
  explicit MoveGuard(uint32_t timeout_ms): timeout_ms_(timeout_ms) {}

  // A command that should move the door was just sent. start_status is the door state at that time.
  void arm(uint32_t now, uint8_t start_status) {
    active_ = true; moved_ = false; armed_ = now; start_ = start_status;
  }

  // Call on every door status reading. moving: the door reports opening or closing. final_state: the
  // door reports open, closed or stopped.
  void observe(uint32_t now, uint8_t status, bool moving, bool final_state) {
    if (!active_) return;
    if (expired(now)) { active_ = false; return; }
    if (moving || status != start_) moved_ = true;
    if (final_state && moved_) active_ = false;
  }

  // True while commands from OpenGarage should be ignored.
  bool blocked(uint32_t now) {
    if (active_ && expired(now)) active_ = false;
    return active_;
  }

  void clear() { active_ = false; }

 private:
  bool expired(uint32_t now) const { return uint32_t(now - armed_) >= timeout_ms_; }
  uint32_t timeout_ms_, armed_ = 0;
  uint8_t start_ = 0;
  bool active_ = false, moved_ = false;
};

// OpenGarage just sent a command that should move the door, and which way. On a door with no
// Security+ link the sensor only reports open or closed and lags the real move by 20 seconds or
// more, so the regular status updates would keep repeating the old settled state. While a move is
// pending they report OPENING or CLOSING instead. It ends when the sensor confirms the new state
// (call clear()), when another command arrives, or after timeout_ms.
class PendingMove {
 public:
  explicit PendingMove(uint32_t timeout_ms): timeout_ms_(timeout_ms) {}

  // state and legacy are string literals, for example "OPENING" and "Opening".
  void start(uint32_t now, const char *state, const char *legacy) {
    state_ = state; legacy_ = legacy; started_ = now;
  }
  bool active(uint32_t now) { expire(now); return state_ != nullptr; }
  const char *state(uint32_t now) { expire(now); return state_; }
  const char *legacy(uint32_t now) { expire(now); return state_ ? legacy_ : nullptr; }
  void clear() { state_ = legacy_ = nullptr; }

 private:
  void expire(uint32_t now) { if (state_ && uint32_t(now - started_) >= timeout_ms_) clear(); }
  uint32_t timeout_ms_, started_ = 0;
  const char *state_ = nullptr, *legacy_ = nullptr;
};
}  // namespace og_guard
