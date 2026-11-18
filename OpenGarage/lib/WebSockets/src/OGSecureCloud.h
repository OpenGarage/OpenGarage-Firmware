#pragma once
#include <StackThunk.h>
#include <WiFiClientSecureBearSSL.h>
#include <umm_malloc/umm_heap_select.h>
#include <bearssl/bearssl.h>
#include <time.h>
#include "OGWssTrustCatalog.h"
#include "OGWssRx.h"
namespace og_tls {
constexpr unsigned stack_bytes=6656,rx_bytes=16709;
static_assert(OG_TLS_STACK_BYTES==stack_bytes,"Keep TLS allocation and resource budget aligned");
enum class State:uint8_t {OFF,READY,NO_MEMORY,WAIT_TIME,CONNECTING,CONNECTED,FAILED};
struct Status {State state=State::OFF;uint32_t refusals=0;int error=0;bool retained=false;};
inline Status &status(){static Status s;return s;}
inline BearSSL::ExternalReceiveBuffer &rx(){return og_wss_rx::storage();}
inline bool reserve(){
 if(!og_wss_rx::reserve_at_boot()){status().state=State::NO_MEMORY;return false;}
 if(!status().retained){HeapSelectDram h;if(ESP.getMaxFreeBlockSize()<stack_bytes+200 || ESP.getFreeHeap()<stack_bytes+12000){status().state=State::NO_MEMORY;return false;}
 stack_thunk_add_ref();status().retained=true;}
 status().state=State::READY;return true;
}
inline bool refuse(){status().state=State::NO_MEMORY;++status().refusals;return false;}
inline bool prepare(){
 if(!status().retained || rx().capacity()!=rx_bytes || rx().leased())return refuse();
 if(time(nullptr)<1700000000){status().state=State::WAIT_TIME;return false;}
 HeapSelectDram h;
 if(ESP.getMaxFreeBlockSize()<8192 || ESP.getFreeHeap()<12000)return refuse();
 auto &s=og_wss_trust::catalog();s.resetClaimForConnection();
 if(!s.begin())return refuse();
 return true;
}
inline bool smtp_ready(){
 if(!status().retained || stack_thunk_get_refcnt()!=1)return false;
 HeapSelectDram h;if(ESP.getFreeHeap()<16000 || ESP.getMaxFreeBlockSize()<6144)return false;
 const size_t sizes[]={sizeof(BearSSL::WiFiClientSecureCtx)+64,sizeof(br_ssl_client_context)+64,sizeof(br_x509_minimal_context)+64};
 void *p[3]={};bool ok=true;
 for(unsigned i=0;i<3;++i){p[i]=malloc(sizes[i]);if(!p[i]){ok=false;break;}}
 for(unsigned i=3;i>0;--i)free(p[i-1]);
 return ok;
}
}
