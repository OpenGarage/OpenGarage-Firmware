#include "../OpenGarage/local_time.h"
#include <cassert>
#include <cstdio>
int main() {
  using namespace og_local;
  Clock clock;
  clock.tick(1000, 0); assert(!clock.valid && clock.utc == 0);
  OpenDuration open;
  assert(!open.expired(1000, 60000));
  clock.tick(2000, 1790000000); // first sync must not expire duration
  assert(!open.expired(2000, 60000));
  clock.tick(3000, 0); assert(clock.valid && clock.utc == 1790000001);
  clock.tick(4000, 1789999000); // backward correction
  assert(!open.expired(4000, 60000));
  assert(!open.expired(60999, 60000)); assert(open.expired(61000, 60000));
  open.start(0xfffffff0U);
  assert(!open.expired(15, 32)); assert(open.expired(16, 32));
  Interval schedule;
  assert(schedule.due(0xfffffff0U, 32));
  assert(!schedule.due(15, 32)); assert(schedule.due(16, 32));
  clock.tick(0xfffffff0U, 0);
  const auto before = clock.uptime_ms;
  clock.tick(16, 0); assert(clock.uptime_ms == before + 32);
  for (unsigned hour=0; hour<24; ++hour) {
    DailyRule rule;
    uint32_t utc = 20000U*86400U + hour*3600U;
    assert(!rule.eligible(false, utc, hour));
    assert(rule.eligible(true, utc, hour)); rule.mark(utc);
    assert(!rule.eligible(true, utc+1, hour));
    assert(!rule.eligible(true, utc-86400U, hour));
    assert(rule.eligible(true, utc+86400U, hour));
  }
  puts("local time: PASS");
}
