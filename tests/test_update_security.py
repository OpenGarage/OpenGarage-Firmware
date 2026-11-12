"""Run production upload/prepare HTTP callbacks against observable admission stubs.
The actual streaming/crypto service is tested separately by test_firmware_service.py.
"""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'OpenGarage/main.cpp').read_text()
callbacks=source[source.index('static std::unique_ptr<char[]> prepare_body'):source.index('static void on_update_info()')]
stub=r'''
#include <cassert>
#include <memory>
#include <new>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include "OpenGarage/update_auth.h"
#define F(x) x
using String=std::string;
enum {HTTP_POST,HTTP_GET};
enum {HTML_SUCCESS=1,HTML_UNAUTHORIZED=2,HTML_UPLOAD_FAILED=3,OPTION_DKEY,OPTION_SECV,OPTION_KSET};
enum {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED,OG_MOD_STA,OG_MOD_AP,OG_STATE_WAIT_RESTART};
enum class FirmwareUpdatePhase {Idle,Prepared,Uploading,Success,Error};
struct HTTPUpload {int status=UPLOAD_FILE_START;uint8_t buf[2048]={};size_t currentSize=0;String filename="fw.bin";} upload;
int http_status=0,closed=0,disruptions=0,result=0,curr_mode=OG_MOD_STA,actions=0;bool busy=false;
struct WiFiClient {void stop(){++closed;}void keepAlive(int,int,int){}static void stopAllExcept(WiFiClient*){++disruptions;}} client;
struct Server { void handleClient() {} int verb=HTTP_POST;int method(){return verb;}String path="/update";std::map<std::string,String>headers;
String header(const char*k){return headers[k];}String uri(){return path;}HTTPUpload&upload(){return ::upload;}
WiFiClient&client(){return ::client;}void sendHeader(const char*,const char*){}void send(int status,const char*,const String&){http_status=status;}
} server,*updateServer=&server;
struct Option {String sval="test";int ival=1;};struct {Option options[8];int state=0;bool config_locked=false;} og;
struct Garage {void finish_release(){++disruptions;}}secplus1_garage,secplus2_garage;
struct {void disconnect(){++disruptions;}}mqttclient;
struct WiFiUDP {static void stopAll(){++disruptions;}};
void cancel_diagnostic_audio(){++disruptions;}void delay(int){}
bool network_delivery_busy(){return busy;}
void updateserver_send_result(int code,const char* = nullptr){result=code;}
bool update_started=false,update_authorized=false,update_failed=false,update_file_seen=false;
struct Service {
FirmwareUpdatePhase state=FirmwareUpdatePhase::Idle;int begins=0,writes=0,finishes=0,prepares=0;bool write_ok=true;
FirmwareUpdatePhase phase(){return state;}bool busy(){return state==FirmwareUpdatePhase::Uploading;}
bool token_valid(const char*t){return state==FirmwareUpdatePhase::Prepared && !strcmp(t,"token");}
void abort_upload(){state=FirmwareUpdatePhase::Error;}void reject(const char*){state=FirmwareUpdatePhase::Error;}
const char*message(){return "test";}
bool begin_manual_file(const char*){++begins;state=FirmwareUpdatePhase::Uploading;return true;}
bool begin_verified(const char*,const char*t){if(!token_valid(t))return false;return begin_manual_file("");}
bool write_manual(uint8_t*,size_t){++writes;return write_ok;}bool write_verified(uint8_t*p,size_t n){return write_manual(p,n);}
bool finish_manual(){++finishes;state=FirmwareUpdatePhase::Success;return true;}bool finish_verified(){return finish_manual();}
unsigned floor=0;
bool prepare_verified(uint8_t*,size_t,const char*,const char*,bool,char*t,unsigned minimum){floor=minimum;++prepares;strcpy(t,"token");state=FirmwareUpdatePhase::Prepared;return true;}
} firmware_update;
'''
runner=r'''
void start(const char*k){server.headers["X-OG-Device-Key"]=k;upload.status=UPLOAD_FILE_START;on_firmware_upload();}
void chunk(size_t n){upload.currentSize=n;upload.status=UPLOAD_FILE_WRITE;on_firmware_upload();}
void aborted(){upload.status=UPLOAD_FILE_ABORTED;on_firmware_upload();}
void reset(){firmware_update=Service{};server=Server{};closed=disruptions=result=0;busy=false;og.state=0;
update_started=update_authorized=update_failed=update_file_seen=prepare_seen=prepare_ok=false;}
int main(){
for(int mode:{OG_MOD_STA,OG_MOD_AP}) {
curr_mode=mode;
reset();og.options[OPTION_KSET].ival=0;
server.headers["X-OG-Device-Key"]="hex:74657374";
assert(!updater_body_authorized() && http_status==403 && firmware_update.begins==0 && disruptions==0);
start("hex:74657374");assert(firmware_update.begins==0);aborted();
reset();server.path="/update/prepare";server.headers["X-OG-Device-Key"]="hex:74657374";
assert(updater_body_authorized());upload.status=UPLOAD_FILE_START;on_prepare_upload();
upload.status=UPLOAD_FILE_WRITE;upload.currentSize=512;on_prepare_upload();on_prepare_fin();
assert(firmware_update.floor==127);service_update_server();
og.options[OPTION_KSET].ival=1;
reset();assert(!updater_body_authorized() && http_status==401);
server.headers["X-OG-Device-Key"]="hex:74657374";
assert(updater_body_authorized() && firmware_update.begins==0 && disruptions==0);
server.path="/update/prepare";assert(updater_body_authorized());
busy=true;assert(!updater_body_authorized() && http_status==409);busy=false;
server.path="/other";assert(!updater_body_authorized());
server.path="/update/upload";assert(!updater_body_authorized());
firmware_update.state=FirmwareUpdatePhase::Prepared;server.headers["X-OG-Upload-Token"]="token";
assert(updater_body_authorized() && firmware_update.state==FirmwareUpdatePhase::Prepared);
og.config_locked=true;assert(!updater_body_authorized() && http_status==401);
server.path="/update";assert(!updater_body_authorized() && http_status==401);
server.path="/update/prepare";assert(!updater_body_authorized() && http_status==401);
og.config_locked=false;
server.verb=HTTP_GET;assert(!updater_body_authorized());

for(const char*k:{"","test","hex:77726f6e67"}){reset();start(k);assert(closed==1 && firmware_update.begins==0 && disruptions==0);aborted();}
reset();busy=true;start("hex:74657374");assert(closed==1 && firmware_update.begins==0 && disruptions==0);aborted();
reset();start("hex:74657374");assert(update_started && disruptions>0);chunk(32);on_firmware_upload_fin();assert(result==1 && firmware_update.finishes==1);
reset();start("hex:74657374");chunk(32);aborted();assert(!update_started && firmware_update.finishes==0);
reset();start("hex:74657374");start("hex:74657374");chunk(32);on_firmware_upload_fin();assert(result==3 && firmware_update.finishes==0);
reset();firmware_update.write_ok=false;start("hex:74657374");chunk(32);assert(closed==1);aborted();assert(firmware_update.finishes==0);
reset();start("hex:74657374");server.headers["X-OG-Device-Key"]="bad";on_firmware_upload_fin();assert(result==2 && firmware_update.finishes==0);
// Unauthorized requests must not cancel another client's prepared token/reboot.
reset();firmware_update.state=FirmwareUpdatePhase::Prepared;start("bad");aborted();assert(firmware_update.state==FirmwareUpdatePhase::Prepared);
reset();firmware_update.state=FirmwareUpdatePhase::Success;start("hex:74657374");aborted();assert(firmware_update.state==FirmwareUpdatePhase::Success);
reset();server.path="/update/upload";firmware_update.state=FirmwareUpdatePhase::Prepared;server.headers["X-OG-Upload-Token"]="token";
start("");chunk(32);on_firmware_upload_fin();assert(result==1 && firmware_update.finishes==1);
reset();upload.status=UPLOAD_FILE_START;on_prepare_upload();assert(closed==1 && firmware_update.prepares==0);
upload.status=UPLOAD_FILE_ABORTED;on_prepare_upload();
reset();server.headers["X-OG-Device-Key"]="hex:74657374";upload.status=UPLOAD_FILE_START;on_prepare_upload();
upload.status=UPLOAD_FILE_WRITE;upload.currentSize=1025;on_prepare_upload();on_prepare_fin();assert(firmware_update.prepares==0 && closed==1);
reset();server.headers["X-OG-Device-Key"]="hex:74657374";upload.status=UPLOAD_FILE_START;on_prepare_upload();
upload.status=UPLOAD_FILE_WRITE;upload.currentSize=512;on_prepare_upload();busy=true;on_prepare_fin();assert(firmware_update.prepares==0);
reset();server.headers["X-OG-Device-Key"]="hex:74657374";upload.status=UPLOAD_FILE_START;on_prepare_upload();
upload.status=UPLOAD_FILE_WRITE;upload.currentSize=512;on_prepare_upload();on_prepare_fin();assert(firmware_update.prepares==1 && firmware_update.floor==126);
assert(prepare_body);service_update_server();assert(!prepare_body && !prepare_seen && !prepare_ok && prepare_length==0);
// Simulate allocation refusal: no descriptor may be dereferenced or verified.
reset();server.headers["X-OG-Device-Key"]="hex:74657374";prepare_seen=true;prepare_ok=false;prepare_body.reset();
on_prepare_fin();assert(result==HTML_UPLOAD_FAILED && firmware_update.prepares==0 && !prepare_seen);
// Interrupted upload without a final or abort callback is cleaned up by request scope.
reset();server.headers["X-OG-Device-Key"]="hex:74657374";upload.status=UPLOAD_FILE_START;on_prepare_upload();
assert(prepare_body);service_update_server();assert(!prepare_body);
}
puts("Update callbacks (AP and STA): early auth, busy recheck, connection cleanup, abort, prepare cap and no unauthorized cancellation PASS");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t)/'test';subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),'-x','c++','-','-o',str(p)],input=stub+callbacks+runner,text=True,check=True);subprocess.run([str(p)],check=True)
for page in ('sta_update.html','ap_update.html'):
 text=(root/'OpenGarage/html'/page).read_text();assert "'X-OG-Device-Key':key_header()" in text
 assert "fd.append('dkey'" not in text and 'xhr.upload.onprogress' in text
 script=re.search(r'<script>(.*?)</script>',text,re.S).group(1)
 subprocess.run(['node','-e','new Function(require("fs").readFileSync(0,"utf8"))'],input=script,text=True,check=True)

# Exercise the real result writer with distinct RAM/flash types (F is not an identity).
writer=source[source.index('template<typename Message>'):source.index('// this is simplified')]
writer_stub=r"""
#include <cassert>
#include <string>
#include <cstdint>
#include <cstring>
using byte=uint8_t;
struct __FlashStringHelper {};
#define F(s) reinterpret_cast<const __FlashStringHelper*>(s)
int flash_appends=0;
struct String {
 std::string value;
 String(const __FlashStringHelper*p):value(reinterpret_cast<const char*>(p)){}
 String& operator+=(byte n){value+=std::to_string(n);return *this;}
 String& operator+=(const char*p){value+=p;return *this;}
 String& operator+=(const __FlashStringHelper*p){++flash_appends;value+=reinterpret_cast<const char*>(p);return *this;}
};
struct Server {
 std::string body;
 void sendHeader(const __FlashStringHelper*k,const __FlashStringHelper*v){
  assert(!strcmp(reinterpret_cast<const char*>(k),"Access-Control-Allow-Origin"));
  assert(!strcmp(reinterpret_cast<const char*>(v),"*"));
 }
 void send(int status,const char*type,const String&s){assert(status==200);assert(!strcmp(type,"application/json"));body=s.value;}
} server,*updateServer=&server;
"""
writer_runner=r"""
int main(){
 updateserver_send_result(1);
 assert(server.body=="{\"result\":1,\"item\":\"\"}");
 updateserver_send_result(3,"Runtime failure");
 assert(server.body=="{\"result\":3,\"item\":\"Runtime failure\"}");
 int before=flash_appends;
 updateserver_send_result(3,F("Flash failure"));
 assert(server.body=="{\"result\":3,\"item\":\"Flash failure\"}");
 assert(flash_appends==before+3);
}
"""
with tempfile.TemporaryDirectory() as t:
 p=Path(t)/'writer'
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-x','c++','-','-o',str(p)],input=writer_stub+writer+writer_runner,text=True,check=True)
 subprocess.run([str(p)],check=True)
print('Updater result writer: RAM, flash and empty messages preserve exact JSON PASS')
