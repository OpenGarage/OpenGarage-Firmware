#pragma once
#include <ctype.h>
#include <new>
#include <stddef.h>
#include <string.h>

namespace og_form {

// An application/x-www-form-urlencoded request body, parsed with the same rules
// OTF applies to URL query strings, so GET and POST give identical values:
//  - '&' separates pairs and the first '=' separates a key from its value;
//  - a pair without '=' has an empty value;
//  - values are decoded ('+' is a space, %XX a byte); keys are taken as-is;
//  - a malformed escape, a decoded or raw NUL rejects the whole body;
//  - a repeated key keeps its last value.
// The body is copied, because OTC-forwarded bodies are not NUL-terminated.
class Form {
 public:
  enum Result { OK, MALFORMED, TOO_MANY, NO_MEMORY };
  static const size_t MAX_FIELDS = 128;

  Form() = default;
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;
  ~Form() { clear(); }

  Result parse(const char *body, size_t len) {
    clear();
    size_t pairs = len ? 1 : 0;
    for (size_t i = 0; i < len; ++i) {
      if (body[i] == '\0') return MALFORMED;
      if (body[i] == '&') ++pairs;
    }
    if (pairs > MAX_FIELDS) return TOO_MANY;
    data_ = new (std::nothrow) char[len + 1];
    keys_ = pairs ? new (std::nothrow) char *[pairs] : nullptr;
    values_ = pairs ? new (std::nothrow) char *[pairs] : nullptr;
    if (!data_ || (pairs && (!keys_ || !values_))) { clear(); return NO_MEMORY; }
    memcpy(data_, body, len);
    data_[len] = '\0';
    for (char *p = data_; len && p;) {
      char *next = strchr(p, '&');
      if (next) *next++ = '\0';
      char *value = strchr(p, '=');
      if (value) *value++ = '\0';
      else value = p + strlen(p);
      if (!decode(value)) { clear(); return MALFORMED; }
      set(p, value);  // like OTF, an empty pair ("a=1&&b=2") records key ""
      p = next;
    }
    return OK;
  }

  // The decoded value, or nullptr when the key is absent (an empty value is "").
  char *get(const char *key) const {
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(keys_[i], key) == 0) return values_[i];
    return nullptr;
  }
  size_t size() const { return count_; }

 private:
  void set(char *key, char *value) {
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(keys_[i], key) == 0) { values_[i] = value; return; }
    keys_[count_] = key;
    values_[count_++] = value;
  }
  static int hex(char c) { return isdigit((unsigned char)c) ? c - '0' : (tolower((unsigned char)c) - 'a' + 10); }
  static bool decode(char *value) {
    char *out = value;
    for (const char *in = value; *in; ++in) {
      char c = *in;
      if (c == '+') c = ' ';
      else if (c == '%') {
        if (!isxdigit((unsigned char)in[1]) || !isxdigit((unsigned char)in[2])) return false;
        c = char((hex(in[1]) << 4) | hex(in[2]));
        if (c == '\0') return false;
        in += 2;
      }
      *out++ = c;
    }
    *out = '\0';
    return true;
  }
  void clear() {
    delete[] data_; delete[] keys_; delete[] values_;
    data_ = nullptr; keys_ = values_ = nullptr; count_ = 0;
  }
  char *data_ = nullptr;
  char **keys_ = nullptr, **values_ = nullptr;
  size_t count_ = 0;
};

}  // namespace og_form
