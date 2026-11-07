"""Run production update service using host flash/clock and OpenSSL crypto adapters."""
from pathlib import Path
import tempfile,subprocess,re,json,hashlib
root=Path(__file__).resolve().parents[1]
header=(root/'OpenGarage/firmware_update.h').read_text()
source=(root/'OpenGarage/firmware_update.cpp').read_text()
# Keep production logic and JSON parser, replace platform includes only.
header=re.sub(r'^#include.*$','',header,flags=re.M)
header=re.sub(r'^\s*#include.*$','',header,flags=re.M)
source=re.sub(r'^\s*#include.*$','',source,flags=re.M)
stub=r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>
#include <string>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/ecdsa.h>
#include <ArduinoJson.h>
#define ARDUINO 1
#define ESP8266 1
#define OG_FWV 126
#define FW_UPDATE_PUBLIC_KEY_CONFIGURED 1
uint32_t now=0;int reboots=0,refs=0;
uint32_t millis(){return now;}
void restart_in(uint32_t){++reboots;}
struct HeapSelectDram {};
struct {uint32_t block=20000;uint32_t getMaxFreeBlockSize(){return block;}
uint32_t getFreeSketchSpace(){return 0x100000;}uint32_t getFlashChipRealSize(){return 4194304;}
uint32_t random(){static uint32_t n=123;return ++n;}} ESP;
uint32_t stack_thunk_get_refcnt(){return refs;}
void stack_thunk_add_ref(){assert(ESP.block>=6200);++refs;}
void stack_thunk_del_ref(){--refs;}
using br_sha256_context=SHA256_CTX;
void br_sha256_init(br_sha256_context*c){SHA256_Init(c);}
void br_sha256_update(br_sha256_context*c,const void*p,size_t n){SHA256_Update(c,p,n);}
void br_sha256_out(br_sha256_context*c,uint8_t*d){auto copy=*c;SHA256_Final(d,&copy);}
constexpr int BR_EC_secp256r1=23;
struct br_ec_public_key {int curve;unsigned char*q;size_t qlen;};
const void*br_ec_get_default(){return nullptr;}
uint32_t verify_ssl(const void*,const void*d,size_t n,const br_ec_public_key*k,const void*s,size_t len){
 EC_KEY*ec=EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
 const unsigned char*p=k->q;assert(o2i_ECPublicKey(&ec,&p,k->qlen));
 int ok=ECDSA_verify(0,(const unsigned char*)d,n,(const unsigned char*)s,len,ec);EC_KEY_free(ec);return ok==1;
}
using br_ecdsa_vrfy=decltype(&verify_ssl);
br_ecdsa_vrfy br_ecdsa_vrfy_asn1_get_default(){return verify_ssl;}
#define make_stack_thunk(fn) extern "C" uint32_t thunk_##fn(const uint8_t*d,const uint8_t*s,size_t n){return fn(d,s,n);}
struct Flash {
 bool started=false,write_ok=true,end_ok=true;size_t size=0;unsigned commits=0,begins=0;std::vector<uint8_t>data;
 bool begin(size_t n){++begins;started=true;size=n;data.clear();return true;}
 size_t write(uint8_t*p,size_t n){if(!write_ok)return 0;data.insert(data.end(),p,p+n);return n;}
 bool end(bool partial){bool ok=end_ok && started && (partial || data.size()==size);if(ok)++commits;started=false;return ok;}
 bool hasError(){return !end_ok;}
} Update;
std::vector<uint8_t>read(const char*p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
'''
runner=r'''
int main(int argc,char**argv){assert(argc==4);auto raw=read(argv[1]),sigb=read(argv[2]),image=read(argv[3]);
std::string sig(sigb.begin(),sigb.end());char token[17];
auto prepare=[&](){return firmware_update.prepare_verified(raw.data(),raw.size(),sig.c_str(),"1.2.6",true,token);};
auto stream=[&](){for(size_t i=0;i<image.size();i+=113){size_t n=std::min(size_t(113),image.size()-i);if(!firmware_update.write_verified(image.data()+i,n))return false;}return true;};
ESP.block=6000;assert(!prepare() && Update.begins==0 && refs==0);ESP.block=20000;
assert(!firmware_update.prepare_verified(raw.data(),raw.size(),sig.c_str(),"1.2.6",false,token));
assert(prepare() && refs==0);assert(!firmware_update.begin_verified("fw.bin","wrong"));assert(!firmware_update.token_valid(token));
assert(prepare());now+=300000;firmware_update.loop();assert(!firmware_update.token_valid(token));
now=UINT32_MAX-100;assert(prepare());now+=200;assert(firmware_update.token_valid(token));
assert(firmware_update.begin_verified("fw.bin",token));assert(!firmware_update.token_valid(token));
assert(stream() && Update.commits==0 && Update.data.size()==image.size()-1);
firmware_update.abort_upload();assert(Update.commits==0);
assert(prepare());assert(firmware_update.begin_verified("fw.bin",token));
image.back()^=1;assert(stream());assert(!firmware_update.finish_verified() && Update.commits==0);image.back()^=1;
assert(prepare());assert(firmware_update.begin_verified("fw.bin",token));
assert(firmware_update.write_verified(image.data(),16));assert(!firmware_update.finish_verified());assert(Update.commits==0);
assert(prepare());assert(firmware_update.begin_verified("fw.bin",token));Update.write_ok=false;assert(!stream());Update.write_ok=true;assert(Update.commits==0);
auto original=raw[0];raw[0]^=1;assert(!prepare());raw[0]=original;
assert(prepare());assert(firmware_update.begin_verified("fw.bin",token));assert(stream());assert(firmware_update.finish_verified());assert(Update.commits==1);
now+=1600;firmware_update.loop();assert(reboots==1);firmware_update.loop();assert(reboots==1);
assert(!firmware_update.begin_manual_file("fw.bin"));
// New boot: authenticated legacy-format manual uploads share the byte hold.
firmware_update=FirmwareUpdateService();assert(firmware_update.begin_manual_file("fw.bin"));
assert(firmware_update.write_manual(image.data(),image.size()));assert(Update.data.size()==image.size()-1);assert(firmware_update.finish_manual());assert(Update.commits==2);
puts("Firmware service: signatures, low heap, tokens, rollover, corruption, abort, manual and finalization PASS");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);key=p/'key.pem'
 subprocess.run(['openssl','ecparam','-name','prime256v1','-genkey','-noout','-out',str(key)],check=True)
 pub=subprocess.check_output(['openssl','ec','-in',str(key),'-pubout','-conv_form','uncompressed','-outform','DER'],stderr=subprocess.DEVNULL)[-65:]
 image=bytearray(2048);image[0]=0xe9;image[1]=2;image[4:8]=(0x40100000).to_bytes(4,'little');(p/'fw.bin').write_bytes(image)
 d={'schema':1,'product':'opengarage-stock','id':'1.2.6','version':126,'targets':{'og-esp8266-4m-v1':{'file':'/v1/releases/1.2.6/fw.bin','size':len(image),'sha256':hashlib.sha256(image).hexdigest(),'min_flash':4194304}}}
 (p/'release.json').write_text(json.dumps(d,separators=(',',':')))
 subprocess.run(['openssl','dgst','-sha256','-sign',str(key),'-out',str(p/'sig.der'),str(p/'release.json')],check=True)
 (p/'sig.txt').write_text((p/'sig.der').read_bytes().hex())
 text=stub+'\nstatic const uint8_t FW_UPDATE_PUBLIC_KEY[65]={'+','.join(map(str,pub))+'};\n'+header+source+runner
 (p/'test.cpp').write_text(text)
 subprocess.run(['g++','-std=c++17','-Wno-deprecated-declarations','-I'+str(root/'OpenGarage/.pio/libdeps/d1_mini/ArduinoJson/src'),str(p/'test.cpp'),'-lcrypto','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'release.json'),str(p/'sig.txt'),str(p/'fw.bin')],check=True)
