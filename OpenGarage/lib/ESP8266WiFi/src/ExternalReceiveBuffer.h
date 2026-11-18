// Experimental borrowed TLS receive storage. Caller retains memory ownership.
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace BearSSL {
class ExternalReceiveBuffer {
 public:
  ExternalReceiveBuffer() = default;
  ExternalReceiveBuffer(const ExternalReceiveBuffer&) = delete;
  ExternalReceiveBuffer& operator=(const ExternalReceiveBuffer&) = delete;
  bool initialize(unsigned char *data, size_t capacity) {
    if (data_ || !data || !capacity || (reinterpret_cast<uintptr_t>(data) & 3)) return false;
    data_=data; capacity_=capacity; return true;
  }
  bool acquire(const void *owner, size_t required) {
    if (!owner || !data_ || !required || required>capacity_ || (owner_ && owner_!=owner)) return false;
    owner_=owner; return true;
  }
  void release(const void *owner) {
    if (!owner || owner_!=owner) return;
    // Volatile stores prevent dead-store removal. Aligned word writes avoid
    // thousands of ESP8266 IRAM byte-access exceptions; handle the exact tail.
    typedef uint32_t AliasWord __attribute__((__may_alias__));
    volatile AliasWord *words=reinterpret_cast<volatile AliasWord*>(data_);
    const size_t count=capacity_/sizeof(uint32_t);
    for (size_t i=0;i<count;++i) words[i]=0;
    volatile unsigned char *tail=data_;
    for (size_t i=count*sizeof(uint32_t);i<capacity_;++i) tail[i]=0;
    owner_=nullptr;
  }
  unsigned char *dataFor(const void *owner) const { return owner && owner_==owner ? data_ : nullptr; }
  size_t capacity() const { return capacity_; }
  bool leased() const { return owner_!=nullptr; }
 private:
  unsigned char *data_=nullptr;
  size_t capacity_=0;
  const void *owner_=nullptr;
};
}
