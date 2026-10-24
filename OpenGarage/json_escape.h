#pragma once
#include <stddef.h>

template<class StringType>
void append_json_escaped(StringType &out, const StringType &value) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = value[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c < 0x20) {
      out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
    } else out += static_cast<char>(c);
  }
}
