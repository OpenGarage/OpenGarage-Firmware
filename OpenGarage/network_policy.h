#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace og_network {
enum class Command { NONE, TOGGLE, OPEN, CLOSE, LIGHT, LOCK };
inline Command command(const uint8_t *p, size_t n, bool retained, bool command_topic, bool legacy_topic) {
  if (retained || !p) return Command::NONE;
  if (legacy_topic && n==6 && memcmp(p,"Button",6)==0) return Command::TOGGLE;
  if (!command_topic) return Command::NONE;
  if(n==5 && memcmp(p,"click",5)==0) return Command::TOGGLE;
  if(n==4 && memcmp(p,"open",4)==0) return Command::OPEN;
  if(n==5 && memcmp(p,"close",5)==0) return Command::CLOSE;
  if(n==11 && memcmp(p,"togglelight",11)==0) return Command::LIGHT;
  if(n==10 && memcmp(p,"togglelock",10)==0) return Command::LOCK;
  return Command::NONE;
}
struct Backoff {
  const uint32_t minimum, maximum;
  uint32_t delay_ms, last=0;
  bool failed=false;
  Backoff(uint32_t lo,uint32_t hi):minimum(lo),maximum(hi),delay_ms(lo){}
  bool ready(uint32_t now) const { return !failed || uint32_t(now-last)>=delay_ms; }
  void failure(uint32_t now) {
    if(failed) delay_ms=delay_ms>maximum/2 ? maximum : delay_ms*2;
    failed=true;last=now;
  }
  void success() { failed=false;delay_ms=minimum; }
};
// CONNACK alone is not proof of a healthy session. Repeated short sessions
// (including rejected oversized retained packets) must retain failure backoff.
struct MqttRecovery {
  Backoff retry{60000, 600000};
  bool attempted=false, up=false;
  uint32_t last_attempt=0, connected_since=0;
  bool ready(uint32_t now) const {
    return !up && (!attempted || uint32_t(now-last_attempt)>=5000) && retry.ready(now);
  }
  void attempt_result(uint32_t now, bool accepted) {
    attempted=true; last_attempt=now;
    up=accepted;
    if (accepted) connected_since=now;
    else retry.failure(now);
  }
  void observe(uint32_t now, bool connected) {
    if (up && !connected) {
      up=false;
      if (uint32_t(now-connected_since)>=30000) {
        // A previously healthy session gets one prompt recovery attempt.
        retry.success(); last_attempt=now;
      } else retry.failure(now);
    }
    else if (up && uint32_t(now-connected_since)>=30000) retry.success();
  }
};

}
