"""Compile the real /jl handler with OTF; verify bounded streaming and file cleanup."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1];og=root/'OpenGarage'
source=(og/'main.cpp').read_text()
handler=source[source.index('void on_sta_logs('):source.index('// The device key is required')]
ini=(og/'platformio.ini').read_text()
local=re.search(r'^\s*symlink://(\S+)',ini,re.M)
otf=(og/local.group(1)).resolve() if local else og/'.pio/libdeps/d1_mini/OpenThings-Framework-Firmware-Library'
flag=re.search(r'-DOTF_RESPONSE_BUFFER_SIZE=(\d+)',ini)
code=r"""
#include "OpenThingsFramework.h"
#include "OpenGarage/json_escape.h"
#include <cassert>
#include <sstream>
#include <limits>
#include <new>
using namespace OTF;
using byte=unsigned char;
WebsocketClient *testSocket=nullptr;
FakeLocalClient *OTF::nextLocalClient=nullptr;
bool failCloudAllocation=false;
bool failResponseAllocation=false;
extern "C" void *__real__ZnamRKSt9nothrow_t(size_t,const std::nothrow_t&);
extern "C" void *__wrap__ZnamRKSt9nothrow_t(size_t size,const std::nothrow_t&tag){
 if(failResponseAllocation && size==OTF_RESPONSE_BUFFER_SIZE){failResponseAllocation=false;return nullptr;}
 return __real__ZnamRKSt9nothrow_t(size,tag);
}
uint32_t millis(){static uint32_t t=0;return ++t;}
enum {OG_MOD_AP=1,OG_MOD_STA=2,OG_SN2_NONE=0,OPTION_NAME,OPTION_SN2,OPTION_LSZ};
int curr_mode=OG_MOD_STA;
uint32_t start_utc_time=1,curr_utc_time=2;
struct {uint64_t uptime_ms=3000;} local_clock;
struct LogStruct{uint32_t tstamp;unsigned status,dist;byte sn2;};
struct Option {unsigned ival=0;String sval;};
struct Garage{
 Option options[4];std::vector<LogStruct> records;
 bool available=true,opened=false;unsigned closes=0;size_t pos=0,fail_at=9999;
 bool read_log_start(){assert(!opened);pos=0;opened=available;return opened;}
 bool read_log_next(LogStruct& l){assert(opened);if(pos>=records.size()||pos>=fail_at)return false;l=records[pos++];return true;}
 void read_log_end(){assert(opened);opened=false;++closes;}
}og;
""" + handler + r"""
std::string expected(const char*escapedName){
 std::ostringstream s;
 const bool extra=og.options[OPTION_SN2].ival>0;
 s<<"{\"name\":\""<<escapedName<<"\",\"starttime\":"<<start_utc_time<<",\"time\":"<<curr_utc_time
  <<",\"uptime\":"<<uint32_t(local_clock.uptime_ms/1000)<<",\"ncols\":"<<(extra?4:3)<<",\"logs\":[";
 bool first=true;
 if(og.available)for(size_t i=0;i<og.records.size()&&i<og.options[OPTION_LSZ].ival&&i<og.fail_at;++i){
  auto l=og.records[i];if(!l.tstamp)continue;
  if(!first)s<<",";first=false;
  s<<"["<<l.tstamp<<","<<l.status<<","<<l.dist;if(extra)s<<","<<unsigned(l.sn2);s<<"]";
 }
 return s.str()+"]}";
}
void checkResponse(const std::string&r,const std::string&body){
 auto end=r.find("\r\n\r\n");assert(end!=std::string::npos);
 assert(r.substr(end+4)==body);
 assert(r.find("Content-Length:")==std::string::npos);
 assert(r.find("Content-Type: application/json\r\n")<end);
 assert(r.find("Connection: close\r\n")<end);
}
std::string localGet(OpenThingsFramework&otf){
 FakeLocalClient client;client.input="GET /jl HTTP/1.1\r\n\r\n";nextLocalClient=&client;
 otf.loop(false);assert(!og.opened);return client.output;
}
void cloudGet(OpenThingsFramework&otf,const std::string&body){
 testSocket->responses.clear();testSocket->emit(WSEvent_CONNECTED);
 testSocket->events.push_back({WSEvent_TEXT,"FWD: 0001\r\nGET /jl HTTP/1.1\r\n\r\n"});
 otf.loop();assert(!og.opened);
 assert(testSocket->responses.size()==1);
 checkResponse(testSocket->responses[0],body);
}
int main(){
 OpenThingsFramework otf(0,"host",80,"test",false);otf.on("/jl",on_sta_logs);
 og.options[OPTION_LSZ].ival=400;
 og.options[OPTION_NAME].sval="Garage";
 for(unsigned extra=0;extra<2;++extra){
  og.options[OPTION_SN2].ival=extra;
  for(unsigned count:{0U,1U,400U}){
   og.records.clear();
   for(unsigned i=0;i<count;++i)og.records.push_back({1700000000U+i,i%3,200+i,byte(i)});
   auto body=expected("Garage");checkResponse(localGet(otf),body);cloudGet(otf,body);
  }
 }
 og.records.assign(400,{0,0,0,0});
 checkResponse(localGet(otf),expected("Garage")); // initialized but empty file
 og.records[0]={1,2,3,4};og.records[399]={5,6,7,8};
 checkResponse(localGet(otf),expected("Garage")); // gaps do not leave commas
 og.fail_at=1;checkResponse(localGet(otf),expected("Garage"));og.fail_at=9999;
 og.available=false;checkResponse(localGet(otf),expected("Garage"));og.available=true;
 og.options[OPTION_NAME].sval="a\"\\\n\x01é";
 checkResponse(localGet(otf),expected("a\\\"\\\\\\u000a\\u0001é"));
 // Exercise every control-byte escape, including an embedded NUL.
 og.options[OPTION_NAME].sval.clear();std::string escapedControls;
 for(unsigned c=0;c<32;++c){
  og.options[OPTION_NAME].sval.push_back(char(c));char escape[7];
  snprintf(escape,sizeof(escape),"\\u%04x",c);escapedControls+=escape;
 }
 checkResponse(localGet(otf),expected(escapedControls.c_str()));
 og.options[OPTION_NAME].sval="a\"\\\n\x01é";
 // All numeric fields at their maximum still fit the stack buffer.
 start_utc_time=curr_utc_time=UINT32_MAX;local_clock.uptime_ms=uint64_t(UINT32_MAX)*1000;
 og.records.assign(400,{UINT32_MAX,UINT32_MAX,UINT32_MAX,255});
 auto body=expected("a\\\"\\\\\\u000a\\u0001é");
 checkResponse(localGet(otf),body);cloudGet(otf,body);
 // A failed network write cannot leak the reader; the next request succeeds.
 unsigned closed=og.closes;testSocket->failSend=true;
 testSocket->events.push_back({WSEvent_TEXT,"FWD: 0002\r\nGET /jl HTTP/1.1\r\n\r\n"});
 otf.loop();assert(!og.opened&&og.closes==closed+1);
 testSocket->failSend=false;cloudGet(otf,body);
 struct Disconnected:FakeLocalClient{size_t write(const char*,size_t)override{return 0;}} disconnected;
 disconnected.input="GET /jl HTTP/1.1\r\n\r\n";nextLocalClient=&disconnected;
 closed=og.closes;otf.loop(false);assert(!og.opened&&og.closes==closed+1);
 checkResponse(localGet(otf),body);
 closed=og.closes;failResponseAllocation=true;
 auto failed=localGet(otf);
 assert(!failResponseAllocation && og.closes==closed);
 assert(failed.find("500 OTF error")!=std::string::npos);
 checkResponse(localGet(otf),body);
 curr_mode=OG_MOD_AP;closed=og.closes;localGet(otf);assert(og.closes==closed);
}
"""
with tempfile.TemporaryDirectory(prefix='og-log-stream-') as tmp:
 p=Path(tmp);cpp=p/'test.cpp';cpp.write_text(code)
 sources=[str(otf/f) for f in ('OpenThingsFramework.cpp','ForwardedRequest.cpp','Request.cpp','Response.cpp','StringBuilder.cpp','HttpParser.cpp')]
 args=['g++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-misleading-indentation',
       '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(otf),'-I',str(otf/'test/fakes'),'-I',str(root),
       '-DARDUINO=10800','-DESP8266','-include',str(otf/'test/fakes/cloud_transport.h')]
 if flag:args+=['-DOTF_RESPONSE_BUFFER_SIZE='+flag.group(1)]
 subprocess.run(args+[str(cpp),*sources,'-Wl,--wrap=_ZnamRKSt9nothrow_t','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={'ASAN_OPTIONS':'detect_leaks=1'})
print('Log streaming: exact local/cloud JSON, 0/1/400 rows, gaps, bounds, escaping, read/send failures and reader cleanup PASS')
