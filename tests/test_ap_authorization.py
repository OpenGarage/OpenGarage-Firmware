"""Run the real AP handlers: unauthorized requests cannot save or complete setup."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'OpenGarage/main.cpp').read_text()
def fn(name):
    return re.search(r'(?:static )?(?:bool|void) '+name+r'\(.*?^}',s,re.M|re.S).group()
code=r'''
#include <cassert>
#include "OpenGarage/setup_window.h"
bool setup_expired=false,room=true;bool setup_room(uint32_t){return room;}
#include <string>
#include <map>
#include <cstring>
#include <cstdint>
#include "OpenGarage/defines.h"
#include "OpenGarage/device_key_policy.h"
#include "OpenGarage/body_device_key.h"
#define F(x) x
enum {HTML_SUCCESS=1,HTML_UNAUTHORIZED=2,HTML_DATA_MISSING=16,HTML_DATA_OUTOFBOUND=17,HTML_NOT_PERMITTED=48,HTML_UPLOAD_FAILED=63};
struct String:std::string {
 using std::string::string;using std::string::operator=;
 String& operator=(const char*p){assign(p?p:"");return *this;}
 String& operator+=(uint32_t n){append(std::to_string(n));return *this;}
 String& operator+=(const char*p){append(p);return *this;}
 void setCharAt(size_t n,char c){if(n<size())(*this)[n]=c;}
 bool reserve(size_t n){std::string::reserve(n);return true;}
 int toInt()const{return atoi(c_str());}
};
struct Option {String sval;unsigned ival=0;};
struct {Option options[NUM_OPTIONS];bool config_locked=false,has_swrx=true;int state=0,calls=0;
 bool saves=true;
 bool options_save(){++calls;if(!saves){options[OPTION_DKEY].sval="opendoor";options[OPTION_KSET].ival=0;}return saves;}
} og;
namespace OTF {
struct Request {std::string body; const char*getBody()const{return body.data();}size_t getBodyLength()const{return body.size();}
 std::map<std::string,std::string> values;
 char* getQueryParameter(const char*k)const {auto it=values.find(k);return it==values.end()?nullptr:const_cast<char*>(it->second.c_str());}
};
struct Response {int result=0;String body;};
}
void otf_send_result(OTF::Response&r,int code,const char*){r.result=code;}
void otf_send_json(OTF::Response&r,const String&j){r.body=j;}
String scanned_ssids="{\"ssids\":[],\"key_required\":0}";
int curr_mode=OG_MOD_AP,completions=0;
bool setup_save_failed=false;
bool complete_ap_setup(){++completions;return true;}
constexpr int WL_CONNECTED=3;
struct {int status(){return WL_CONNECTED;}uint32_t localIP(){return 123;}} WiFi;
'''+ '\n'.join(fn(n) for n in ['device_key_matches','verify_device_key','on_ap_change_config','on_ap_try_connect','on_ap_set_key','on_ap_scan'])+r'''
int main(){
 for(const char*key:{"opendoor","old weak key","old-\xc3\xa9"}) {
  og.options[OPTION_DKEY].sval=key;og.options[OPTION_KSET].ival=1;
  for(const char*sent:{static_cast<const char*>(nullptr),"","wrong",key}) {
   OTF::Request req;req.values={{"ssid","replacement"},{"pass","wifi"},{"host","og"},{"secv","2"},{"cld","otc"},{"auth","token"}};
   if(sent)req.values["dkey"]=sent;
   og.options[OPTION_SSID].sval="original";og.options[OPTION_AUTH].sval="original-token";
   og.options[OPTION_SECV].ival=0;og.calls=0;og.state=OG_STATE_INITIAL;completions=0;
   OTF::Response r;on_ap_change_config(req,r);
   bool good=sent&&!strcmp(sent,key);
   if(!good){assert(r.result==HTML_UNAUTHORIZED&&og.calls==0&&og.state==OG_STATE_INITIAL);
    assert(og.options[OPTION_SSID].sval=="original"&&og.options[OPTION_AUTH].sval=="original-token"&&og.options[OPTION_SECV].ival==0);}
   else assert(r.result==HTML_SUCCESS&&og.calls==1&&og.state==OG_STATE_TRY_CONNECT);
   r={};on_ap_try_connect(req,r);
   if(!good)assert(r.result==HTML_UNAUTHORIZED&&completions==0&&r.body.empty());
   else assert(completions==1&&r.body=="{\"ip\":123}");
  }
 }
 room=false;og.options[OPTION_KSET].ival=1;og.calls=completions=0;
 OTF::Request late;late.values["dkey"]=og.options[OPTION_DKEY].sval;late.values["ssid"]="late";OTF::Response denied;
 on_ap_change_config(late,denied);assert(denied.result==HTML_NOT_PERMITTED&&og.calls==0);
 on_ap_try_connect(late,denied);assert(denied.result==HTML_NOT_PERMITTED&&completions==0);room=true;
 // First use cannot save WiFi or complete setup until its key is established.
 og.options[OPTION_KSET].ival=0;og.calls=completions=0;
 OTF::Request req;req.values["ssid"]="first-use";OTF::Response r;
 on_ap_change_config(req,r);assert(r.result==HTML_NOT_PERMITTED&&og.calls==0);
 on_ap_try_connect(req,r);assert(r.result==HTML_NOT_PERMITTED&&completions==0);
 for(const std::string &bad:{std::string("short"),std::string(65,'a'),std::string("longkey123\0suffix",17),std::string(" space-edge")}) {
  req.body=bad;on_ap_set_key(req,r);assert(r.result==HTML_DATA_OUTOFBOUND&&og.calls==0);
 }
 req.body="a valid new key";og.saves=false;on_ap_set_key(req,r);
 assert(r.result==HTML_UPLOAD_FAILED&&og.options[OPTION_KSET].ival==0&&og.options[OPTION_DKEY].sval=="opendoor");
 og.saves=true;on_ap_set_key(req,r);assert(r.result==HTML_SUCCESS&&og.options[OPTION_KSET].ival==1);
 on_ap_scan(req,r);assert(r.body=="{\"ssids\":[],\"key_required\":1}");
 int calls=og.calls;on_ap_set_key(req,r);assert(r.result==HTML_SUCCESS&&og.calls==calls);
 req.body="a different key";on_ap_set_key(req,r);assert(r.result==HTML_UNAUTHORIZED&&og.calls==calls);
 curr_mode=OG_MOD_STA;on_ap_set_key(req,r);assert(r.result==HTML_NOT_PERMITTED&&og.calls==calls);curr_mode=OG_MOD_AP;
 // A corrupt configuration is never first-use authorization.
 og.config_locked=true;og.calls=completions=0;r={};
 on_ap_change_config(req,r);assert(r.result==HTML_NOT_PERMITTED&&og.calls==0);
 on_ap_try_connect(req,r);assert(r.result==HTML_NOT_PERMITTED&&completions==0);
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I',str(root),'-x','c++','-','-o',str(p)],input=code,text=True,check=True)
    subprocess.run([str(p)],check=True)
assert 'otf->on("/setup_key", on_ap_set_key, OTF::HTTP_POST)' in s
print('PASS: first-use POST key save/refusal/retry, scan state, WiFi gate and established AP authorization')
