"""Compile actual secure-cloud policy with controllable SDK/allocator failures."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[1];src=root/'OpenGarage/lib/WebSockets/src'
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'umm_malloc').mkdir();(p/'bearssl').mkdir()
 for f in ['OGSecureCloud.h','OGWssRx.h']:shutil.copy(src/f,p/f)
 (p/'StackThunk.h').write_text('#pragma once\nextern unsigned refs,adds;inline unsigned stack_thunk_get_refcnt(){return refs;}inline void stack_thunk_add_ref(){++refs;++adds;}\n')
 (p/'WiFiClientSecureBearSSL.h').write_text('#pragma once\n#include "ExternalReceiveBuffer.h"\nnamespace BearSSL {struct WiFiClientSecureCtx{char bytes[400];};}\n')
 (p/'bearssl/bearssl.h').write_text('#pragma once\nstruct br_ssl_client_context{char bytes[3400];};struct br_x509_minimal_context{char bytes[3500];};\n')
 (p/'umm_malloc/umm_heap_select.h').write_text('#pragma once\nextern int heap;struct HeapSelectDram{int old=heap;HeapSelectDram(){heap=0;}~HeapSelectDram(){heap=old;}};struct HeapSelectIram{int old=heap;HeapSelectIram(){heap=1;}~HeapSelectIram(){heap=old;}};\n')
 (p/'OGWssTrustCatalog.h').write_text('#pragma once\nnamespace og_wss_trust {struct Store{bool allowed=true,ready=false;unsigned resets=0;void resetClaimForConnection(){++resets;}bool begin(){ready=allowed;return allowed;}};inline Store&catalog(){static Store s;return s;}}\n')
 (p/'t.cpp').write_text(r'''
#include <cstdlib>
#include <stdlib.h>
#include <time.h>
#include <ctime>
#include <cstdint>
#include <cassert>
int heap=0;unsigned refs=0,adds=0;unsigned iramBlock=16895,dramBlock=20000,dramFree=30000;
struct {unsigned getMaxFreeBlockSize(){return heap?iramBlock:dramBlock;}unsigned getFreeHeap(){return dramFree;}}ESP;
unsigned rxAllocations=0,rxFrees=0,probes=0,live=0;int budget=-1;uintptr_t next=0;
void *testMalloc(size_t n){if(heap==1){assert(n==16709);++rxAllocations;return reinterpret_cast<void*>(next);}++probes;if(budget==0)return nullptr;if(budget>0)--budget;++live;return std::malloc(n);}
void testFree(void*p){if(reinterpret_cast<uintptr_t>(p)>=0x3ffe0000 && reinterpret_cast<uintptr_t>(p)<0x4010c000){++rxFrees;return;}if(p){--live;std::free(p);}}
time_t now=0;time_t fakeTime(time_t*){return now;}
#define malloc testMalloc
#define free testFree
#define time fakeTime
#define OG_WSS_RESERVED_RX
#define OG_TLS_STACK_BYTES 6656
#include "OGSecureCloud.h"
#undef malloc
#undef free
#undef time
int main(){
 assert(!refs&&!rxAllocations); // Cloud-off/plain paths never call reserve.
 assert(!og_tls::reserve()&&!rxAllocations);
 iramBlock=17280;assert(!og_tls::reserve()&&rxAllocations==1);
 next=0x3ffe1000;assert(!og_tls::reserve()&&rxFrees==1);
 next=0x4010b000;assert(!og_tls::reserve()&&rxFrees==2);
 next=0x40101000;dramBlock=6000;assert(!og_tls::reserve()&&refs==0&&rxAllocations==4);
 dramBlock=20000;assert(og_tls::reserve()&&refs==1&&adds==1&&rxAllocations==4);
 assert(og_tls::reserve()&&adds==1&&rxAllocations==4);
 assert(!og_tls::prepare()&&og_tls::status().state==og_tls::State::WAIT_TIME);
 now=1800000000;dramBlock=8191;assert(!og_tls::prepare());dramBlock=20000;
 og_wss_trust::catalog().allowed=false;assert(!og_tls::prepare());
 og_wss_trust::catalog().allowed=true;assert(og_tls::prepare());
 assert(og_tls::rx().acquire(&refs,16709));assert(!og_tls::prepare());
 // Avoid wiping the synthetic IRAM address: no release is needed in this process.
 refs=2;assert(!og_tls::smtp_ready());refs=1;dramFree=15999;assert(!og_tls::smtp_ready());dramFree=16000;
 dramBlock=6143;assert(!og_tls::smtp_ready());dramBlock=20000;
 for(int b=0;b<3;++b){budget=b;assert(!og_tls::smtp_ready()&&!live);}
 budget=-1;assert(og_tls::smtp_ready()&&!live);
}
''')
 subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'OpenGarage/lib/ESP8266WiFi/src'),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('Actual resource policy: RX failure/bounds, retained stack retry/idempotence, time/heap/claim refusal and all SMTP probe failures PASS')
