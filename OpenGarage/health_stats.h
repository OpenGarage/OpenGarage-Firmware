#pragma once
#include <stdint.h>

// RAM-only, per-boot observations. Minimum heap is sampled, not an allocator
// high-water mark. A loop that resets the CPU cannot record its own duration.
class HealthStats {
 public:
  void enter(uint32_t now_ms) {
    uptime_ms_ += uint32_t(now_ms-last_ms_);
    last_ms_=now_ms;
  }
  void finish(uint32_t started_us, uint32_t now_us, uint32_t heap) {
    const uint32_t elapsed=now_us-started_us;
    if (elapsed>max_loop_us_) max_loop_us_=elapsed;
    if (heap<min_heap_) min_heap_=heap;
  }
  uint32_t uptime_s() const { return static_cast<uint32_t>(uptime_ms_/1000); }
  uint32_t min_heap() const { return min_heap_; }
  uint32_t max_loop_us() const { return max_loop_us_; }
 private:
  uint64_t uptime_ms_=0;
  uint32_t last_ms_=0, min_heap_=UINT32_MAX, max_loop_us_=0;
};
