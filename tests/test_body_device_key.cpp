#include "../OpenGarage/body_device_key.h"
#include <cassert>
#include <cstdio>
int main() {
  const char raw[] = {'k','e','y'}; // no NUL terminator
  assert(body_device_key_matches(raw, sizeof(raw), "key"));
  assert(!body_device_key_matches(nullptr, 3, "key"));
  assert(!body_device_key_matches("", 0, ""));
  assert(!body_device_key_matches("ke", 2, "key"));
  assert(!body_device_key_matches("KEY", 3, "key"));
  assert(!body_device_key_matches("key\n", 4, "key"));
  assert(!body_device_key_matches("key\0extra", 9, "key"));
  const char special[] = " spaces & + % = é ";
  assert(body_device_key_matches(special, sizeof(special)-1, special));
  assert(!body_device_key_matches("wrong", 5, "key"));
  std::puts("POST device-key authentication: exact length, missing/wrong key, NUL, whitespace and UTF-8 checks passed");
}
