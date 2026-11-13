from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1];src=root/'OpenGarage/lib/ESP8266WiFi/src'
h=(src/'WiFiClientSecureBearSSL.h').read_text();c=(src/'WiFiClientSecureBearSSL.cpp').read_text()
def body(s,needle):
 start=s.index(needle);a=s.index('{',start);n=1;i=a+1
 while n:
  n += (s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
setter=body(h,'bool setExternalReceiveBuffer(')
cleanup=body(c,'void WiFiClientSecureCtx::_freeSSL()')
connect=c[c.index('  _freeSSL();',c.index('bool WiFiClientSecureCtx::_connectSSL(')):c.index('  // If no cipher list yet set',c.index('bool WiFiClientSecureCtx::_connectSSL('))]
program=r'''
#define OG_WSS_RESERVED_RX
#include <memory>
#include <cassert>
#include <cstdint>
#include "ExternalReceiveBuffer.h"
#define DEBUG_BSSL(...) do{}while(0)
#define DBG_MMU_PRINTF(...) do{}while(0)
using BearSSL::ExternalReceiveBuffer;
struct br_ssl_client_context {int eng;};
struct Client {
 std::shared_ptr<br_ssl_client_context> _sc;
 std::shared_ptr<int> _sc_svr,_x509_minimal,_x509_insecure,_x509_knownkey;
 std::shared_ptr<unsigned char> _iobuf_in,_iobuf_out;
 ExternalReceiveBuffer *_external_iobuf=nullptr;
 int *_eng=nullptr; unsigned char *_recvapp_buf=nullptr;
 size_t _recvapp_len=0; bool _handshake_done=false,_oom_err=false;
 int _timeout=15000; size_t _iobuf_in_size=16709,_iobuf_out_size=597;
 int allocations=0, budget=-1;
 bool ctx_present() {return bool(_sc)||bool(_sc_svr);}
 unsigned char *_inputBuffer() const {return _external_iobuf ? _external_iobuf->dataFor(this):_iobuf_in.get();}
 std::shared_ptr<unsigned char> _alloc_iobuf(size_t n) {
   ++allocations; if(budget==0)return {}; if(budget>0)--budget;
   return std::shared_ptr<unsigned char>(new unsigned char[n],std::default_delete<unsigned char[]>());
 }
 bool setExternalReceiveBuffer(ExternalReceiveBuffer *buffer) SETTER
 void _freeSSL() CLEANUP
 bool connect() { CONNECT return true; }
 ~Client(){_freeSSL();}
};
int main(){
 for (size_t capacity=1;capacity<=9;++capacity) {
  alignas(4) unsigned char bounded[12];for(auto &v:bounded)v=0x5A;
  ExternalReceiveBuffer small;assert(small.initialize(bounded,capacity));
  assert(small.acquire(&small,capacity));small.release(&small);
  for(size_t i=0;i<sizeof bounded;++i)assert(bounded[i]==(i<capacity?0:0x5A));
 }
 alignas(4) unsigned char bytes[16709]={};ExternalReceiveBuffer pool;
 assert(!pool.initialize(bytes+1,sizeof(bytes)-1));assert(pool.initialize(bytes,sizeof(bytes)));
 assert(!pool.initialize(bytes,sizeof(bytes)));assert(!pool.acquire(nullptr,1));
 assert(!pool.acquire(&pool,0));assert(!pool.acquire(&pool,sizeof(bytes)+1));
 {Client a,b;assert(a.setExternalReceiveBuffer(&pool));assert(b.setExternalReceiveBuffer(&pool));
  assert(a.connect());assert(a.allocations==1 && a._inputBuffer()==bytes && pool.leased());
  assert(!a.setExternalReceiveBuffer(nullptr));
  assert(!b.connect() && b.allocations==0 && b._oom_err);assert(pool.dataFor(&a)==bytes);
  for (auto &v:bytes) v=0xA5;
  pool.release(&b);pool.release(nullptr);assert(pool.leased());
  for (auto v:bytes) assert(v==0xA5);
  a._freeSSL();a._freeSSL();assert(!pool.leased());
  for (auto v:bytes) assert(v==0);
  assert(b.connect());
  assert(!a.connect());assert(pool.dataFor(&b)==bytes);
 }
 assert(!pool.leased());
 {Client a;a._iobuf_in_size=16710;assert(!a.setExternalReceiveBuffer(&pool));}
 {Client a;assert(a.setExternalReceiveBuffer(&pool));a._iobuf_in_size=16710;assert(!a.connect() && a.allocations==0);assert(!pool.leased());}
 {Client a;assert(a.setExternalReceiveBuffer(&pool));a.budget=0;assert(!a.connect());assert(!pool.leased());a.budget=-1;assert(a.connect());}
 assert(!pool.leased());
 for(int n=0;n<100;++n){Client a;assert(a.setExternalReceiveBuffer(&pool));assert(a.connect());assert(a._inputBuffer()==bytes);a._freeSSL();assert(a.connect());assert(a._inputBuffer()==bytes);}
 assert(!pool.leased());
 {Client smtp;smtp._iobuf_in_size=2373;assert(smtp.connect());assert(smtp.allocations==2);assert(!pool.leased());}
}
'''.replace('SETTER',setter).replace('CLEANUP',cleanup).replace('CONNECT',connect)
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(src),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Reserved RX: real setter/allocation/cleanup; exclusivity, bounds, OOM, stop/destruction, 100 reconnects and ordinary SMTP PASS')
# Test the real boot reservation wrapper without dereferencing simulated MCU addresses.
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'umm_malloc').mkdir();(p/'umm_malloc/umm_heap_select.h').write_text('struct HeapSelectIram {};\n')
 (p/'owner.cpp').write_text(r'''
#include <cstdlib>
#include <stdlib.h>
#include <cassert>
#include <cstdint>
struct {unsigned block=0;unsigned getMaxFreeBlockSize(){return block;}} ESP;
uintptr_t next=0;unsigned allocs=0,frees=0;
void *test_malloc(size_t n){assert(n==16709);++allocs;return reinterpret_cast<void*>(next);}
void test_free(void*){++frees;}
#define OG_WSS_RESERVED_RX
#define malloc test_malloc
#define free test_free
#include "OGWssRx.h"
#undef malloc
#undef free
int main(){
 ESP.block=16895;assert(!og_wss_rx::reserve_at_boot() && allocs==0);
 ESP.block=17144;assert(!og_wss_rx::reserve_at_boot() && allocs==1);
 next=0x3ffe0000;assert(!og_wss_rx::reserve_at_boot() && frees==1);
 next=0x4010b000;assert(!og_wss_rx::reserve_at_boot() && frees==2);
 next=0x40101000;assert(og_wss_rx::reserve_at_boot());assert(og_wss_rx::ready());
 unsigned count=allocs;assert(og_wss_rx::reserve_at_boot() && allocs==count);
 assert(og_wss_rx::storage().capacity()==16709 && !og_wss_rx::storage().leased());
}
''')
 subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-I',td,'-I',str(src),'-I',str(root/'OpenGarage/lib/WebSockets/src'),str(p/'owner.cpp'),'-o',str(p/'owner')],check=True)
 subprocess.run([str(p/'owner')],check=True)
print('Boot reservation: guard, allocation failure, DRAM fallback rejection, bounds and idempotence PASS')

# Compile the actual guard against matching, mismatched and absent version macros.
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'guard.cpp').write_text('#include "OGCoreVersion.h"\n')
 for version,ok in [((3,1,2),True),((3,1,1),False),((3,2,0),False),((4,1,2),False),(None,False)]:
  macros='' if version is None else ''.join(f'#define ARDUINO_ESP8266_{n} {v}\n' for n,v in zip(('MAJOR','MINOR','REVISION'),version))
  (p/'core_version.h').write_text(macros)
  result=subprocess.run(['g++','-I',td,'-I',str(src),'-fsyntax-only',str(p/'guard.cpp')],capture_output=True,text=True)
  assert (result.returncode==0)==ok,result.stderr
print('Vendored core version: 3.1.2 accepted; mismatched and missing versions rejected PASS')
