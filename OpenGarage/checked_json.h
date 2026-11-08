#pragma once
#include <stddef.h>

// Sticky allocation status for diagnostic JSON. No response is written until
// every append succeeds. StringT is Arduino String (or the host fault injector).
template<class StringT> class CheckedJsonBuffer : public StringT {
  bool ok_ = true;
public:
  bool reserve(unsigned n) {
    if (ok_ && !StringT::reserve(n)) ok_ = false;
    return ok_;
  }
  template<class T> CheckedJsonBuffer &operator+=(const T &value) {
    if (ok_ && !StringT::concat(value)) ok_ = false;
    return *this;
  }
  bool good() const { return ok_; }
  // Release a partially built response before sending the small fixed error.
  void discard() { StringT::operator=(StringT()); }
};
using DebugJson = CheckedJsonBuffer<String>;
