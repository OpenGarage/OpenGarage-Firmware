#include "../OpenGarage/health_stats.h"
#include <cassert>
#include <cstdio>
int main() {
  HealthStats h;
  assert(h.uptime_s()==0 && h.min_heap()==UINT32_MAX && h.max_loop_us()==0);
  h.enter(1000); h.finish(10,110,25000);
  assert(h.uptime_s()==1 && h.min_heap()==25000 && h.max_loop_us()==100);
  h.enter(2500); h.finish(1000,1010,26000);
  assert(h.uptime_s()==2 && h.min_heap()==25000 && h.max_loop_us()==100);
  h.finish(UINT32_MAX-9,10,23000);
  assert(h.min_heap()==23000 && h.max_loop_us()==100);
  h.finish(0,350000,24000); assert(h.max_loop_us()==350000);
  HealthStats wrap; wrap.enter(UINT32_MAX-10); wrap.enter(989);
  assert(wrap.uptime_s()==static_cast<uint32_t>((uint64_t(UINT32_MAX)+990)/1000));
  std::puts("Health stats: minimum heap, loop maximum and timer wraparound passed");
}
