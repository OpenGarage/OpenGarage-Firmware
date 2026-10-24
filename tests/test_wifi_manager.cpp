#include "../OpenGarage/wifi_manager.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using og_wifi::Recovery;
using A = Recovery::Action;
using S = Recovery::State;
int main() {
  Recovery r;
  assert(r.step(100, false, false)==A::NONE && r.state()==S::STOPPED);
  r.start(100);
  assert(r.step(100, false, true)==A::NONE); // local work gates startup too
  assert(r.step(101, false, false)==A::CONNECT && r.attempts()==1);
  assert(r.step(15100, false, false)==A::NONE);
  assert(r.step(15101, false, true)==A::DISCONNECT); // bound a stalled DHCP attempt
  assert(r.timeouts()==1 && r.retry_in_ms(15101)==5000);
  assert(r.step(20100, false, false)==A::NONE);
  assert(r.step(20101, false, true)==A::NONE);
  assert(r.step(20102, false, false)==A::CONNECT);
  assert(r.step(35102, false, false)==A::DISCONNECT);
  assert(r.retry_in_ms(35102)==10000);
  // Repeated failures never spin; the retry wait stops growing at five minutes.
  uint32_t now=35102;
  for(unsigned wait : {10000U,20000U,40000U,80000U,160000U,300000U,300000U}) {
    assert(r.retry_in_ms(now)==wait);
    assert(r.step(now+wait-1,false,false)==A::NONE);
    now+=wait; assert(r.step(now,false,false)==A::CONNECT);
    now+=15000; assert(r.step(now,false,false)==A::DISCONNECT);
  }
  now+=300000; assert(r.step(now,false,false)==A::CONNECT);
  ++now; assert(r.step(now,true,false)==A::NONE && r.state()==S::ONLINE);
  assert(r.retry_in_ms(now)==0);
  const auto attempts=r.attempts();
  assert(r.step(now+30000,true,false)==A::NONE && r.attempts()==attempts);
  now+=30001; assert(r.step(now,false,false)==A::NONE);
  assert(r.disconnects()==1 && r.retry_in_ms(now)==5000);
  assert(r.step(now+4999,false,false)==A::NONE);
  assert(r.step(now+5000,false,true)==A::NONE); // no new connect during alarm/motion
  now+=5001; assert(r.step(now,false,false)==A::CONNECT);
  now+=15000; assert(r.step(now,false,false)==A::DISCONNECT);
  assert(r.retry_in_ms(now)==5000); // stable session reset prior failure backoff
  // Repeated quick disconnects also escalate; association alone isn't stability.
  r.start(0); assert(r.step(0,false,false)==A::CONNECT);
  r.step(1,true,false);r.step(2,false,false);
  assert(r.retry_in_ms(2)==5000);
  assert(r.step(5002,false,false)==A::CONNECT);
  r.step(5003,true,false);r.step(5004,false,false);
  assert(r.retry_in_ms(5004)==10000);
  // All time comparisons survive millis rollover, including stability threshold.
  for(uint32_t start : {0xfffffff0U,0xffff8000U}) {
    r.start(start);assert(r.step(start,false,false)==A::CONNECT);
    assert(r.step(start+15000U,false,false)==A::DISCONNECT);
    assert(r.step(start+20000U,false,false)==A::CONNECT);
    r.step(start+20001U,true,false);
    r.step(start+50001U,false,false);
    assert(r.retry_in_ms(start+50001U)==5000);
    assert(r.step(start+55001U,false,false)==A::CONNECT);
  }
  // Stale movement only: a due reconnect cannot be suppressed indefinitely.
  for(uint32_t start : {0U,0xfffffff0U}) {
    r.start(start);
    assert(r.step(start,false,false,true)==A::NONE);
    assert(r.step(start+59999U,false,false,true)==A::NONE);
    // A new warning/upload at the cap must still block a fresh attempt.
    assert(r.step(start+60000U,false,true,true)==A::NONE);
    assert(r.step(start+60001U,false,false,true)==A::CONNECT);
    // Expired attempt still cancels during both kinds of busy.
    assert(r.step(start+75001U,false,true,true)==A::DISCONNECT);
  }
  // Cap starts only when retry is due; time in backoff doesn't consume it.
  r.start(0);r.step(0,false,false);r.step(15000,false,false);
  r.step(19000,false,false,true);r.step(20000,false,false,true);
  assert(r.step(79000,false,false,true)==A::NONE);
  assert(r.step(80000,false,false,true)==A::CONNECT);
  // Ending the motion deferral allows immediate recovery, no extra delay.
  r.start(0);r.step(0,false,false,true);
  assert(r.step(1,false,false,false)==A::CONNECT);
  // Persistent hard blocks do not get overridden, and an online link stays put.
  r.start(0);r.step(0,false,true,true);
  assert(r.step(120000,false,true,true)==A::NONE);
  r.step(120001,true,false,true);
  assert(r.step(300000,true,false,true)==A::NONE && r.state()==S::ONLINE);
  puts("WiFi recovery deadlines, busy gates, stability, backoff and rollover: PASS");
}
