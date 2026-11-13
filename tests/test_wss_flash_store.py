"""Real store + core BearSSL decoder: pinning, DN/key extraction and ownership."""
import importlib.util,json,subprocess,tempfile
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import serialization
root=Path(__file__).resolve().parents[1];sdk=Path.home()/'.platformio/packages/framework-arduinoespressif8266/tools/sdk/ssl/bearssl'
spec=importlib.util.spec_from_file_location('gen',root/'tools/generate_wss_store.py');gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
m=json.loads((root/'tools/wss-roots/manifest.json').read_text());pems=[(r['name'],(root/'tools/wss-roots'/(r['name']+'.pem')).read_bytes()) for r in m['roots']];pins={r['name']:r['sha256'] for r in m['roots']}
assert gen.generate(pems,m['catalog_version'],pins)==(root/'OpenGarage/lib/WebSockets/src/OGWssPublicAnchors.h').read_text()
try:gen.generate(pems,m['catalog_version'],{n:'0'*64 for n,_ in pems});raise AssertionError('bad hash accepted')
except ValueError:pass
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'umm_malloc').mkdir();(p/'umm_malloc/umm_heap_select.h').write_text('struct HeapSelectDram {};')
 (p/'pgmspace.h').write_text('''#pragma once
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define memcpy_P memcpy
#define memcmp_P memcmp
#define pgm_read_byte(p) (*(const uint8_t*)(p))
static inline uint16_t pgm_read_word(const void *p){uint16_t v;memcpy(&v,p,2);return v;}
static inline uint32_t pgm_read_dword(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
''')
 (p/'CertStoreBearSSL.h').write_text('''#pragma once
#include "pgmspace.h"
#include <bearssl.h>
namespace BearSSL {class CertStoreBase {public:virtual ~CertStoreBase(){};virtual void installCertStore(br_x509_minimal_context*)=0;};}
''')
 (p/'test.cpp').write_text(r'''
#include "OGWssTrustStore.h"
#include "OGWssPublicAnchors.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
static bool failAllocation=false;
void *operator new(size_t n,const std::nothrow_t&) noexcept {if(failAllocation)return nullptr;try{return ::operator new(n);}catch(...){return nullptr;}}
static void dn(void *p,const void *v,size_t n){auto &s=*static_cast<std::vector<unsigned char>*>(p);auto b=static_cast<const unsigned char*>(v);s.insert(s.end(),b,b+n);}
int main(int argc,char **argv){
 assert(argc==int(sizeof(trustEntries)/sizeof(trustEntries[0]))+1);
 og_wss_trust::Store s(trustEntries,sizeof(trustEntries)/sizeof(trustEntries[0]));
 failAllocation=true;assert(!s.begin());failAllocation=false;assert(!s.ready());assert(!s.lookup(&s,(void*)trustEntries[0].dn,32));assert(s.begin() && s.begin());
 assert(!s.lookup(nullptr,nullptr,0));assert(!s.lookup(&s,nullptr,32));assert(!s.lookup(&s,(void*)trustEntries[0].dn,31));
 unsigned char missing[32]={};assert(!s.lookup(&s,missing,32));
 for(int i=1;i<argc;++i){
  std::ifstream in(argv[i],std::ios::binary);std::vector<unsigned char> der((std::istreambuf_iterator<char>(in)),{}),subject;
  br_x509_decoder_context ctx;br_x509_decoder_init(&ctx,dn,&subject,nullptr,nullptr);br_x509_decoder_push(&ctx,der.data(),der.size());assert(br_x509_decoder_last_error(&ctx)==0);
  auto *pk=br_x509_decoder_get_pkey(&ctx);auto &entry=trustEntries[i-1];auto *ta=s.lookup(&s,(void*)entry.dn,32);assert(ta && s.claimed());assert(ta->flags==BR_X509_TA_CA && ta->dn.len==32);assert(ta->pkey.key_type==pk->key_type);assert(!s.lookup(&s,(void*)entry.dn,32));
  if(pk->key_type==BR_KEYTYPE_RSA){assert(ta->pkey.key.rsa.nlen==pk->key.rsa.nlen && ta->pkey.key.rsa.elen==pk->key.rsa.elen);assert(!memcmp(ta->pkey.key.rsa.n,pk->key.rsa.n,pk->key.rsa.nlen));assert(!memcmp(ta->pkey.key.rsa.e,pk->key.rsa.e,pk->key.rsa.elen));}
  else{assert(ta->pkey.key.ec.curve==pk->key.ec.curve && ta->pkey.key.ec.qlen==pk->key.ec.qlen);assert(!memcmp(ta->pkey.key.ec.q,pk->key.ec.q,pk->key.ec.qlen));}
  s.release(&s,nullptr);assert(s.claimed());s.release(&s,ta);assert(!s.claimed());s.release(&s,ta);
  for(auto x:subject)printf("%02x",x);puts("");
 }
 auto *abandoned=s.lookup(&s,(void*)trustEntries[0].dn,32);assert(abandoned && s.claimed());
 s.resetClaimForConnection();assert(!s.claimed());assert(s.lookup(&s,(void*)trustEntries[0].dn,32));
 s.endHandshake();assert(!s.ready() && !s.claimed());s.endHandshake();
 failAllocation=true;assert(!s.begin());failAllocation=false;assert(s.begin());
 auto *again=s.lookup(&s,(void*)trustEntries[0].dn,32);assert(again);s.release(&s,again);s.endHandshake();
 auto bad=trustEntries[0];bad.length=516;og_wss_trust::Store b(&bad,1);assert(b.begin());assert(!b.lookup(&b,(void*)bad.dn,32));
 bad=trustEntries[0];bad.kind=9;og_wss_trust::Store c(&bad,1);assert(c.begin());assert(!c.lookup(&c,(void*)bad.dn,32));
}
''')
 flags=['-I'+str(p),'-I'+str(sdk/'inc'),'-I'+str(sdk/'src')]
 subprocess.run(['gcc','-std=c99','-fsanitize=address,undefined',*flags,'-c',str(sdk/'src/x509/x509_decoder.c'),'-o',str(p/'decoder.o')],check=True)
 subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined',*flags,'-I'+str(root/'OpenGarage/lib/WebSockets/src'),str(p/'test.cpp'),str(p/'decoder.o'),'-o',str(p/'test')],check=True)
 files=[]
 for i,(_,pem) in enumerate(pems):
  path=p/(str(i)+'.der');path.write_bytes(x509.load_pem_x509_certificate(pem).public_bytes(serialization.Encoding.DER));files.append(str(path))
 output=subprocess.check_output([str(p/'test'),*files],text=True).splitlines();assert len(output)==len(pems)
 for raw,(_,pem) in zip(output,pems):assert bytes.fromhex(raw)==gen.subject_der(x509.load_pem_x509_certificate(pem).public_bytes(serialization.Encoding.DER))
print('PASS: 14 pinned roots, actual BearSSL DN/key extraction, workspace claims/releases, bounds, missing anchors, reproducibility and bad-fingerprint refusal')
