"""Run the production upload callbacks with observable host flash/network stubs."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/main.cpp').read_text()
callbacks = source[source.index('static void cancel_firmware_upload()'):source.index('void check_status_ap()')]
stub = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "OpenGarage/update_auth.h"
using String=std::string;
enum {HTML_SUCCESS=1, HTML_UNAUTHORIZED=2, HTML_UPLOAD_FAILED=3, OPTION_DKEY, OPTION_SECV};
enum {UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED, OG_MOD_STA};
struct HTTPUpload {int status=UPLOAD_FILE_START; uint8_t buf[32]={}; size_t currentSize=0;} upload;
struct Server {
 String key="hex:74657374";
 String header(const char*) {return key;}
 HTTPUpload& upload(){return ::upload;}
 void sendHeader(const char*,const char*){}
 void send(int,const char*,const char*){}
} server,*updateServer=&server;
struct Option {String sval="test"; int ival=1;};
struct {Option options[8];} og;
struct {size_t getFreeSketchSpace(){return 0x2000;}} ESP;
int result=0,restarts=0,disruptions=0,curr_mode=OG_MOD_STA;
void updateserver_send_result(int code){result=code;}
void restart_in(int){++restarts;}
void delay(int){}
struct Garage {void finish_release(){++disruptions;}} secplus1_garage,secplus2_garage;
struct {void disconnect(){++disruptions;}} mqttclient;
struct WiFiUDP {static void stopAll(){++disruptions;}};
struct Flash {
 int begins=0,commits=0; bool begin_ok=true,write_ok=true,end_ok=true;
 size_t capacity=0; std::vector<uint8_t> data;
 bool begin(size_t n){++begins;capacity=n;data.clear();return begin_ok;}
 size_t write(uint8_t* p,size_t n){if(!write_ok)return 0;data.insert(data.end(),p,p+n);return n;}
 bool end(bool partial){if(end_ok && (partial || data.size()==capacity))++commits;return end_ok;}
 bool hasError(){return false;}
} Update;
static bool update_authorized=false,update_failed=false,update_started=false;
static bool update_tail_pending=false,update_file_seen=false;
static uint8_t update_tail=0;
static size_t update_received=0,update_capacity=0;
'''
runner = r'''
void start(const char* key){server.key=key;upload.status=UPLOAD_FILE_START;on_firmware_upload();}
void chunk(size_t size){upload.currentSize=size;upload.status=UPLOAD_FILE_WRITE;on_firmware_upload();}
void reset(){Update=Flash{};result=restarts=disruptions=0;update_authorized=update_started=update_tail_pending=update_file_seen=update_failed=false;}
int main(){
 assert(update_key_matches("hex:74657374","test"));
 assert(!update_key_matches("","test"));
 assert(!update_key_matches("hex:7465737400","test"));
 assert(!update_key_matches("hex:7465737x","test"));
 assert(update_key_matches("hex:",""));
 assert(!update_key_matches("",""));
 assert(update_key_matches("hex:c3a9","\xc3\xa9"));
 for(const char* key:{"","test","hex:77726f6e67"}){
  reset();start(key);chunk(8);on_firmware_upload_fin();
  assert(result==HTML_UNAUTHORIZED && Update.begins==0 && Update.data.empty() && disruptions==0 && restarts==0);
 }
 reset();start("hex:74657374");chunk(8);chunk(8);
 assert(Update.data.size()==15 && Update.commits==0);
 on_firmware_upload_fin();assert(result==HTML_SUCCESS && Update.data.size()==16 && Update.commits==1 && restarts==1);
 reset();start("hex:74657374");for(int i=0;i<128;++i)chunk(32);
 assert(Update.data.size()==4095);upload.status=UPLOAD_FILE_ABORTED;on_firmware_upload();
 assert(Update.commits==0 && !update_started);
 start("hex:74657374");chunk(8);on_firmware_upload_fin();assert(result==HTML_SUCCESS);
 reset();start("hex:74657374");for(int i=0;i<129;++i)chunk(32);
 on_firmware_upload_fin();assert(result==HTML_UPLOAD_FAILED && Update.commits==0);
 reset();start("hex:74657374");chunk(8);start("hex:74657374");start("hex:74657374");chunk(8);
 on_firmware_upload_fin();assert(result==HTML_UPLOAD_FAILED && Update.commits==0 && Update.begins==1);
 reset();Update.write_ok=false;start("hex:74657374");chunk(8);on_firmware_upload_fin();
 assert(result==HTML_UPLOAD_FAILED && restarts==0 && Update.commits==0);
 reset();Update.begin_ok=false;start("hex:74657374");chunk(8);on_firmware_upload_fin();assert(result==HTML_UPLOAD_FAILED);
 reset();start("hex:74657374");on_firmware_upload_fin();assert(result==HTML_UPLOAD_FAILED && Update.commits==0);
 reset();start("hex:74657374");chunk(8);server.key="hex:00";on_firmware_upload_fin();assert(Update.commits==0);
 reset();on_firmware_upload_fin();assert(result==HTML_UNAUTHORIZED && Update.begins==0);
}
'''
with tempfile.TemporaryDirectory(prefix='og-update-test-') as tmp:
    binary = str(Path(tmp) / 'test')
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', str(root),
                    '-x', 'c++', '-', '-o', binary], input=stub+callbacks+runner, text=True, check=True)
    subprocess.run([binary], check=True)
for page in ('sta_update.html', 'ap_update.html'):
    text = (root/'OpenGarage/html'/page).read_text()
    assert "setRequestHeader('X-OG-Device-Key'" in text
    assert "fd.append('dkey'" not in text
    assert 'upload_progress' in text and 'xhr.upload.onprogress' in text
    script = re.search(r'<script>(.*?)</script>', text, re.S).group(1)
    subprocess.run(['node', '-e', 'new Function(require("fs").readFileSync(0,"utf8"))'], input=script, text=True, check=True)
print('OTA: early authorization, abort/retry, bounds, multi-file rejection, failures and page contracts passed')
