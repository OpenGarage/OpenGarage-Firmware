#pragma once
#include <stddef.h>
#include <string.h>

// OTF preserves POST bodies, but its local transport discards custom headers.
// Bodies are length-delimited, not necessarily NUL-terminated. Compare exactly:
// do not trim passwords or accept a prefix followed by NUL/trailing bytes.
inline bool body_device_key_matches(const char *body, size_t length, const char *expected) {
  return body && expected && length != 0 && length == strlen(expected) &&
         memcmp(body, expected, length) == 0;
}
