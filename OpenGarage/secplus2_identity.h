#pragma once
#include <stdint.h>

namespace og_identity {
constexpr uint32_t LEGACY_ID = 0x777;
constexpr uint32_t MAGIC = 0x4f473201;
struct Record {
  uint32_t magic;
  uint32_t id;
  uint32_t inverse;
};
static_assert(sizeof(Record) == 12, "Identity record layout changed");
inline bool valid(const Record &r) {
  return r.magic == MAGIC && r.id != 0 && r.inverse == ~r.id;
}
inline uint32_t generate(uint32_t random) {
  return ((random & 0x7f7fU) << 16) | 0x2908U;
}
enum class ReadResult { MISSING, VALID, INVALID };

// Store provides read(Record&) and write(Record). Entropy is a callable.
// fresh=true means first setup or factory reset: generate a new identity.
// With an existing stock config (fresh=false), preserve a valid identity record;
// if only the identity record is missing, migrate using legacy 0x777, not a new ID.
// WiFi-only reset is not fresh setup.
template<class Store, class Entropy>
bool load(Store &store, Entropy entropy, bool fresh, uint32_t &id) {
  id = 0;
  Record old{};
  const auto result = store.read(old);
  if (!fresh && result == ReadResult::VALID) {
    if (!valid(old)) return false;
    id = old.id;
    return true;
  }
  if (!fresh && result != ReadResult::MISSING) return false;
  uint32_t next = LEGACY_ID;
  if (fresh) {
    // Avoid reusing the immediately previous identity on factory reset.
    unsigned attempts = 0;
    do {
      next = generate(entropy());
      if (++attempts > 16) return false;
    } while (result == ReadResult::VALID && next == old.id);
  }
  const Record record{MAGIC, next, ~next};
  if (!store.write(record)) return false;
  Record verified{};
  if (store.read(verified) != ReadResult::VALID || !valid(verified) || verified.id != next)
    return false;
  id = next;
  return true;
}
}  // namespace og_identity
