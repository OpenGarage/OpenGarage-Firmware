#include "../OpenGarage/distance_consensus.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

static void check(const uint32_t (&values)[7], uint32_t margin,
                  bool accepted, uint32_t expected) {
  uint32_t mean = 123456;
  assert(og_filter::tightest_five(values, margin, mean) == accepted);
  assert(mean == (accepted ? expected : 123456));
}

int main() {
  check({100,100,100,100,100,100,100}, 0, true, 100);
  check({1,100,101,102,103,104,999}, 4, true, 102);
  check({1,2,100,101,102,103,104}, 4, true, 102);
  check({100,101,102,103,104,998,999}, 4, true, 102);
  check({1,100,101,102,103,104,999}, 3, false, 0);
  check({0,10,20,30,40,50,60}, 40, true, 20); // tie: lower window
  check({0,10,20,30,40,50,60}, 39, false, 0);
  check({0,0,0,0,0,0,0}, 0, true, 0); // invalid policy belongs to caller
  check({UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,
         UINT32_MAX,UINT32_MAX,UINT32_MAX}, 0, true, UINT32_MAX);
  uint32_t values[7] = {1,100,101,102,103,104,999};
  unsigned permutations = 0;
  do {
    uint32_t original[7];
    std::copy(values, values + 7, original);
    check(values, 4, true, 102);
    assert(std::equal(values, values + 7, original));
    ++permutations;
  } while (std::next_permutation(values, values + 7));
  assert(permutations == 5040);
  std::printf("Distance consensus: 9 cases and %u permutations passed\n", permutations);
}
