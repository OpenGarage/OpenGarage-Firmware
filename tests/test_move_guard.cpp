// Door command guard: ignore OpenGarage commands while a door move started by one is in progress.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include "../OpenGarage/move_guard.h"

// door states as in defines.h
enum { CLOSED = 0, OPEN, STOPPED, CLOSING, OPENING, UNKNOWN };
static void see(og_guard::MoveGuard &g, uint32_t now, uint8_t s) {
  g.observe(now, s, s == OPENING || s == CLOSING, s == OPEN || s == CLOSED || s == STOPPED);
}

int main() {
  const uint32_t T = 45000;

  { og_guard::MoveGuard g(T);  // idle: nothing blocked
    assert(!g.blocked(0)); }

  { og_guard::MoveGuard g(T);  // door opens: blocked until OPEN is reached
    g.arm(1000, CLOSED);
    assert(g.blocked(1500));
    see(g, 2000, CLOSED);   assert(g.blocked(2000));   // still reads closed: not moved yet
    see(g, 3000, OPENING);  assert(g.blocked(3000));
    see(g, 4000, OPENING);  assert(g.blocked(4000));
    see(g, 15000, OPEN);    assert(!g.blocked(15000)); }

  { og_guard::MoveGuard g(T);  // door closes
    g.arm(0, OPEN);
    see(g, 1000, CLOSING); assert(g.blocked(1000));
    see(g, 9000, CLOSED);  assert(!g.blocked(9000)); }

  { og_guard::MoveGuard g(T);  // a remote stops the door part-way: STOPPED ends the guard
    g.arm(0, CLOSED);
    see(g, 1000, OPENING); assert(g.blocked(1000));
    see(g, 3000, STOPPED); assert(!g.blocked(3000)); }

  { og_guard::MoveGuard g(T);  // a remote reverses the door: ends when it settles again
    g.arm(0, OPEN);
    see(g, 1000, CLOSING); see(g, 2000, OPENING); assert(g.blocked(2000));
    see(g, 9000, OPEN);    assert(!g.blocked(9000)); }

  { og_guard::MoveGuard g(T);  // relay door with no Security+ only reports open or closed
    g.arm(0, CLOSED);
    see(g, 500, CLOSED);  assert(g.blocked(500));
    see(g, 7000, OPEN);   assert(!g.blocked(7000)); }

  { og_guard::MoveGuard g(T);  // the command did nothing: the guard still ends at the timeout
    g.arm(0, CLOSED);
    see(g, 10000, CLOSED); assert(g.blocked(10000));
    assert(g.blocked(T - 1));
    assert(!g.blocked(T)); }

  { og_guard::MoveGuard g(T);  // a stuck sensor that never settles cannot lock commands out
    g.arm(0, CLOSED);
    see(g, 1000, OPENING);
    assert(g.blocked(T - 1));
    see(g, T + 5, OPENING); assert(!g.blocked(T + 5)); }

  { og_guard::MoveGuard g(T);  // millis() wraps around
    const uint32_t start = 0xFFFFFFFFu - 10000u;
    g.arm(start, CLOSED);
    assert(g.blocked(start + 20000u));       // wrapped past zero, 20 s later
    assert(!g.blocked(start + T));           // 45 s later, also wrapped
    g.arm(start, CLOSED); see(g, start + 3000u, OPENING); see(g, start + 12000u, OPEN);
    assert(!g.blocked(start + 12001u)); }

  { og_guard::MoveGuard g(T);  // a new command re-arms after the previous one finished
    g.arm(0, CLOSED); see(g, 1000, OPENING); see(g, 9000, OPEN); assert(!g.blocked(9000));
    g.arm(20000, OPEN); assert(g.blocked(20001)); see(g, 21000, CLOSING); see(g, 29000, CLOSED);
    assert(!g.blocked(29000)); }

  { og_guard::MoveGuard g(T);  // clear() always releases
    g.arm(0, CLOSED); g.clear(); assert(!g.blocked(1)); }

  // --- pending move: keeps the status updates saying OPENING / CLOSING until the sensor catches up ---
  { og_guard::PendingMove m(T);
    assert(!m.active(0) && m.state(0) == nullptr && m.legacy(0) == nullptr);
    m.start(1000, "OPENING", "Opening");
    assert(m.active(1500));
    assert(std::string(m.state(20000)) == "OPENING" && std::string(m.legacy(20000)) == "Opening");  // sensor still lags
    assert(m.state(T + 999) != nullptr);
    assert(m.state(T + 1000) == nullptr && !m.active(T + 1000));  // gives up after the timeout
    m.start(0, "CLOSING", "Closing"); m.clear();                   // sensor confirmed the new state
    assert(m.state(1) == nullptr && m.legacy(1) == nullptr);
    m.start(0, "CLOSING", "Closing"); m.start(10, "OPENING", "Opening");  // a later start replaces it
    assert(std::string(m.state(11)) == "OPENING");
    const uint32_t near_wrap = 0xFFFFFFFFu - 5000u;                // millis() wraps around
    m.start(near_wrap, "OPENING", "Opening");
    assert(m.active(near_wrap + 20000u) && !m.active(near_wrap + T)); }

  std::puts("move guard: PASS");
  return 0;
}
