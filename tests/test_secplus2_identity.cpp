#include "../OpenGarage/secplus2_identity.h"
#include <cassert>
#include <cstdio>
using namespace og_identity;
struct Store {
  ReadResult result = ReadResult::MISSING;
  Record record{};
  unsigned writes = 0;
  bool fail = false, corrupt = false;
  ReadResult read(Record &out) { out = record; return result; }
  bool write(const Record &r) {
    ++writes;
    if (fail) return false;
    record = r;
    if (corrupt) record.inverse ^= 1;
    result = valid(record) ? ReadResult::VALID : ReadResult::INVALID;
    return true;
  }
};
int main() {
  uint32_t id;
  unsigned calls = 0;
  auto random = [&]() { return ++calls; };
  Store fresh;
  assert(load(fresh, random, true, id));
  assert(id == 0x00012908 && fresh.writes == 1);
  for (unsigned i = 0; i < 5; ++i) {
    assert(load(fresh, random, false, id)); // reboot / OTA / WiFi reset
    assert(id == 0x00012908 && fresh.writes == 1 && calls == 1);
  }
  assert(load(fresh, random, true, id)); // factory reset
  assert(id == 0x00022908 && fresh.writes == 2);
  // The authenticated regeneration endpoint requests a fresh identity without
  // deleting configuration. The next ordinary boot must retain that new ID.
  const auto prior = id;
  assert(load(fresh, random, true, id));
  assert(id != prior && (id & 0xffff) == 0x2908);
  const auto regenerated = id;
  const auto write_count = fresh.writes;
  assert(load(fresh, random, false, id));
  assert(id == regenerated && fresh.writes == write_count);
  Store legacy;
  assert(load(legacy, random, false, id) && id == LEGACY_ID);
  assert(load(legacy, random, false, id) && id == LEGACY_ID && legacy.writes == 1);
  Store invalid;
  invalid.result = ReadResult::INVALID;
  assert(!load(invalid, random, false, id) && id == 0 && invalid.writes == 0);
  assert(load(invalid, random, true, id)); // explicit factory reset can repair
  Store failed;
  failed.fail = true;
  assert(!load(failed, random, true, id) && id == 0);
  Store torn;
  torn.corrupt = true;
  assert(!load(torn, random, true, id) && id == 0);
  assert(!load(torn, random, false, id) && id == 0 && torn.writes == 1);
  Store repeated;
  assert(load(repeated, [](){ return 5U; }, true, id));
  assert(!load(repeated, [](){ return 5U; }, true, id) && id == 0);
  assert(repeated.writes == 1); // never overwrite old ID with identical replacement
  for (uint32_t bits = 0; bits < 65536; ++bits) {
    auto candidate = generate(bits);
    assert((candidate & 0xffff) == 0x2908);
    assert((candidate & 0x80800000) == 0);
    Record r{MAGIC, candidate, ~candidate};
    assert(valid(r));
    r.inverse ^= 1;
    assert(!valid(r));
  }
  std::puts("Sec+ 2.0 identity: lifecycle/failure cases and 65536 random-input patterns passed");
}
