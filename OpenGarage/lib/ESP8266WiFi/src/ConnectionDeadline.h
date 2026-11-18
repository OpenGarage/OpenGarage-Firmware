#pragma once
#include <stdint.h>

namespace BearSSL {
// Stack-owned, cooperative budget shared by DNS, TCP and the TLS handshake.
// A synchronous SDK/crypto call cannot be preempted; reject a late completion.
struct ConnectionDeadline {
  uint32_t started;
  uint32_t budget;
  uint32_t remaining(uint32_t now) const {
    const uint32_t elapsed = now - started;
    return elapsed < budget ? budget - elapsed : 0;
  }
};
}
