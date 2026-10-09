// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>

// Append the contents of a JSON string; the caller supplies the quotes.
template<class String>
void append_json_escaped(String &output, const String &value) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c == '"') output += "\\\"";
    else if (c == '\\') output += "\\\\";
    else if (c < 0x20) {
      output += "\\u00";
      output += hex[c >> 4];
      output += hex[c & 0x0f];
    } else output += static_cast<char>(c);
  }
}
