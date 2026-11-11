#pragma once
#include <stddef.h>

template<class OutputType, class StringType>
void append_json_escaped(OutputType &out, const StringType &value) {
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = value[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c < 0x20) {
      out += "\\u00";
      // Only control bytes (0x00..0x1f) reach here. Avoid a per-template
      // static hex table when output is a String, checked JSON, or a stream.
      out += static_cast<char>('0' + (c >> 4));
      const unsigned low = c & 15;
      out += static_cast<char>(low < 10 ? '0' + low : 'a' + low - 10);
    } else out += static_cast<char>(c);
  }
}
