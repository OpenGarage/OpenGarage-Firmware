#pragma once
#include <stdint.h>

namespace og_filter {
// Input is a complete batch of seven echo durations. Preserve the caller's
// invalid-echo and last-good-value policies; leave mean unchanged on rejection.
inline bool tightest_five(const uint32_t (&samples)[7], uint32_t margin,
                         uint32_t &mean) {
  uint32_t sorted[7];
  for (unsigned i = 0; i < 7; ++i) sorted[i] = samples[i];
  for (unsigned i = 1; i < 7; ++i) {
    const uint32_t value = sorted[i];
    unsigned j = i;
    while (j && sorted[j - 1] > value) {
      sorted[j] = sorted[j - 1];
      --j;
    }
    sorted[j] = value;
  }
  unsigned best = 0;
  for (unsigned start = 1; start < 3; ++start) {
    // Strict comparison retains the lower-distance window on equal spreads.
    if (sorted[start + 4] - sorted[start] < sorted[best + 4] - sorted[best])
      best = start;
  }
  if (sorted[best + 4] - sorted[best] > margin) return false;
  uint64_t sum = 0;
  for (unsigned i = best; i < best + 5; ++i) sum += sorted[i];
  mean = static_cast<uint32_t>(sum / 5);
  return true;
}
}  // namespace og_filter
