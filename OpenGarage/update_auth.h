#pragma once
#include <stddef.h>
#include <string.h>

// ASCII-safe encoding of UTF-8 key bytes, supplied before the multipart body.
// A prefix distinguishes an intentionally empty configured key from no header.
inline bool update_key_matches(const char *header, const char *key) {
  if (!header || !key || strncmp(header, "hex:", 4) != 0) return false;
  const size_t length = strlen(key);
  if (strlen(header + 4) != length * 2) return false;
  static const char hex[] = "0123456789abcdef";
  unsigned difference = 0;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = key[i];
    difference |= header[4 + i * 2] ^ hex[c >> 4];
    difference |= header[5 + i * 2] ^ hex[c & 15];
  }
  return difference == 0;
}
