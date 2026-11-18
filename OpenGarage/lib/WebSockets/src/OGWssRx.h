#pragma once
#ifdef OG_WSS_RESERVED_RX
#include <ExternalReceiveBuffer.h>
#include <umm_malloc/umm_heap_select.h>
#include <stdlib.h>
namespace og_wss_rx {
constexpr size_t bytes=16384+325; // Pinned core 3.1.2 MAX_IN_OVERHEAD.
inline BearSSL::ExternalReceiveBuffer &storage() { static BearSSL::ExternalReceiveBuffer b; return b; }
inline bool ready() { return storage().capacity()==bytes; }
inline bool reserve_at_boot() {
  if (ready()) return true;
  HeapSelectIram heap;
  if (ESP.getMaxFreeBlockSize()<16384+512) return false;
  auto *data=static_cast<unsigned char*>(malloc(bytes));
  if (!data) return false;
  const uintptr_t address=reinterpret_cast<uintptr_t>(data);
  if (address<0x40100000 || address+bytes>0x4010c000 || !storage().initialize(data,bytes)) {
    free(data); return false;
  }
  return true;
}
}
#endif
