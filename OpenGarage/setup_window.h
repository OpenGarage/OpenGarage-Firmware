#pragma once
#include <stdint.h>
namespace og_setup {
constexpr uint32_t WINDOW_MS = 30UL * 60 * 1000;
// Updater parser has a 120-second total request limit. Leave extra time for
// headers/finalization. Detection takes up to 15 seconds; setup gets 30 seconds.
constexpr uint32_t UPDATE_RUNWAY_MS = 150000;
constexpr uint32_t SETUP_RUNWAY_MS = 30000;
inline uint32_t remaining(uint32_t now, uint32_t start) {
 const uint32_t elapsed=now-start;
 return elapsed>=WINDOW_MS ? 0 : WINDOW_MS-elapsed;
}
inline bool may_start(uint32_t now,uint32_t start,uint32_t required) {
 return remaining(now,start)>required;
}
}
