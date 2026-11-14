"""Run real connection-budget code with deterministic DNS/TCP/TLS clocks."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = root / 'OpenGarage/lib/ESP8266WiFi/src'
h = (src / 'WiFiClientSecureBearSSL.h').read_text()
c = (src / 'WiFiClientSecureBearSSL.cpp').read_text()

def body(s, needle):
    a = s.index('{', s.index(needle))
    i, depth = a + 1, 1
    while depth:
        depth += (s[i] == '{') - (s[i] == '}')
        i += 1
    return s[a:i]

connect = body(c, 'int WiFiClientSecureCtx::connectWithTimeout(')
cleanup = body(c, 'void WiFiClientSecureCtx::_freeSSL()')
setup = c[c.index('  if (deadline) {', c.index('bool WiFiClientSecureCtx::_connectSSL(')):
          c.index('  auto ret = _wait_for_handshake();', c.index('bool WiFiClientSecureCtx::_connectSSL('))]
wrapper = body(h[h.index('class WiFiClientSecure :'):], 'int connectWithTimeout(')

program = r'''
#include <cassert>
#include <cstdint>
#include <memory>
#include "ConnectionDeadline.h"
using BearSSL::ConnectionDeadline;
uint32_t clock_ms=0;
uint32_t millis(){return clock_ms;}
uint32_t dns_ms=0,tcp_ms=0,setup_ms=0,tls_ms=0;
uint32_t dns_limit=0,tcp_limit=0,tls_limit=0;
int dns_calls=0,tcp_calls=0,tls_calls=0;
bool dns_ok=true,tcp_ok=true,tls_ok=true,tls_overruns=false;
void advance(uint32_t duration,uint32_t limit){clock_ms+=duration<limit?duration:limit;}
struct IPAddress{};
struct {
 bool hostByName(const char*,IPAddress&,uint32_t limit){
  ++dns_calls;dns_limit=limit;advance(dns_ms,limit);return dns_ok&&dns_ms<limit;
 }
}WiFi;
struct Socket {int aborts=0;void abort(){++aborts;}} socket;
struct WiFiClient {
 uint32_t _timeout=15000;Socket *_client=nullptr;
 bool connect(IPAddress,uint16_t){
  ++tcp_calls;tcp_limit=_timeout;advance(tcp_ms,_timeout);
  if(!tcp_ok||tcp_ms>=_timeout)return false;
  _client=&socket;return true;
 }
};
struct Ctx:WiFiClient {
 std::shared_ptr<int> _sc,_sc_svr,_x509_minimal,_x509_insecure,_x509_knownkey,_iobuf_in,_iobuf_out;
 void *_recvapp_buf=nullptr;unsigned _recvapp_len=0;bool _handshake_done=false;
 bool ctx_present(){return bool(_sc)||bool(_sc_svr);}
 void _freeSSL() CLEANUP
 bool _connectSSL(const char*,const ConnectionDeadline *deadline){
  _freeSSL(); // Real cleanup resets timeout to 15000, just like the device.
  _sc=std::make_shared<int>(0);clock_ms+=setup_ms;
  SETUP
  ++tls_calls;tls_limit=_timeout;
  if(tls_overruns)clock_ms+=tls_ms;else advance(tls_ms,_timeout);
  _handshake_done=tls_ok&&(tls_overruns||tls_ms<_timeout);
  return _handshake_done;
 }
 int connectWithTimeout(const char*name,uint16_t port,uint32_t timeout_ms) CONNECT
};
struct Wrapper {
 Ctx *_ctx;
 int connectWithTimeout(const char*name,uint16_t port,uint32_t timeout_ms) WRAPPER
};
void reset(uint32_t start=0){
 clock_ms=start;dns_ms=tcp_ms=setup_ms=tls_ms=0;
 dns_calls=tcp_calls=tls_calls=0;dns_ok=tcp_ok=tls_ok=true;tls_overruns=false;socket.aborts=0;
}
int main(){
 using D=ConnectionDeadline;
 assert((D{0xfffffffeu,100}.remaining(3)==95));
 assert((D{0xfffffffeu,100}.remaining(98)==0));
 assert((D{12,0}.remaining(12)==0));
 reset();{Ctx c;assert(!c.connectWithTimeout("server",443,0));assert(!dns_calls);}
 reset();{Ctx c;assert(!c.connectWithTimeout(nullptr,443,1000));assert(!c.connectWithTimeout("",443,1000));assert(!dns_calls);}
 reset();{Ctx c;c._client=&socket;assert(!c.connectWithTimeout("server",443,1000));assert(!dns_calls&&!socket.aborts);}
 reset();{Ctx c;c._sc=std::make_shared<int>();assert(!c.connectWithTimeout("server",443,1000));assert(!dns_calls);}
 reset();dns_ms=1200;{Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(clock_ms==1000&&!tcp_calls&&!tls_calls);}
 reset();dns_ms=100;dns_ok=false;{Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(!tcp_calls);}
 reset();dns_ms=200;tcp_ms=900;{Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(tcp_limit==800&&clock_ms==1000&&!tls_calls);}
 reset();tcp_ok=false;{Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(!tls_calls&&c._timeout==15000);}
 reset();dns_ms=200;tcp_ms=300;setup_ms=100;tls_ms=1000;
 {Ctx c;Wrapper w{&c};assert(!w.connectWithTimeout("server",443,1000));
  assert(dns_limit==1000&&tcp_limit==800&&tls_limit==400);
  assert(clock_ms==1000&&socket.aborts&&!c.ctx_present());}
 reset();dns_ms=200;tcp_ms=300;setup_ms=100;tls_ms=399;
 {Ctx c;Wrapper w{&c};assert(w.connectWithTimeout("server",443,1000));assert(clock_ms==999&&!socket.aborts&&c.ctx_present());}
 reset();setup_ms=1001;{Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(!tls_calls&&socket.aborts&&!c.ctx_present());}
 // A synchronous crypto/SDK step may overrun; never accept late success.
 reset();tls_overruns=true;tls_ms=1100;
 {Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(clock_ms==1100&&socket.aborts&&!c.ctx_present());}
 // Non-timeout TLS errors keep the engine for getLastSSLError() until stop.
 reset();tls_ok=false;tls_ms=10;
 {Ctx c;assert(!c.connectWithTimeout("server",443,1000));assert(socket.aborts&&c.ctx_present());c._freeSSL();assert(!c.ctx_present());}
 // Ordinary client calls retain the original 15-second TLS behavior.
 reset();tls_ms=14000;
 {Ctx c;c._client=&socket;assert(c._connectSSL("server",nullptr));assert(tls_limit==15000&&clock_ms==14000);}
 reset(0xfffffff0u);dns_ms=20;tcp_ms=30;tls_ms=40;
 {Ctx c;assert(c.connectWithTimeout("server",443,100));assert(dns_limit==100&&tcp_limit==80&&tls_limit==50);}
}
'''.replace('CLEANUP', cleanup).replace('SETUP', setup).replace('CONNECT', connect).replace('WRAPPER', wrapper)

with tempfile.TemporaryDirectory() as td:
    p = Path(td)
    (p / 'test.cpp').write_text(program)
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I'+str(src),
                    str(p/'test.cpp'), '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], check=True)

w = (root/'OpenGarage/lib/WebSockets/src/WebSocketsClient.cpp').read_text()
assert 'connectWithTimeout(_host.c_str(), _port, 10000)' in w
assert 'setTimeout(10000)' not in w
print('TLS deadline: actual wrapper/connect/setup/cleanup; shared DNS/TCP/TLS budget, failures, late success and rollover PASS')
