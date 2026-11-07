"""Exercise the vendored parser's actual byte/line reader with a controlled TCP peer."""
from pathlib import Path
import subprocess,tempfile,re
s=Path('OpenGarage/lib/ESP8266WebServer/src/Parsing-impl.h').read_text()
def method(name):
 start=s.index('template <typename ServerType>\n',s.index(name)-120)
 # robust choose closest template before named definition
 pos=s.index('::'+name+'(');start=s.rfind('template <typename ServerType>',0,pos)
 end=s.index('\n}',pos)+2
 return s[start:end]
code=r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <cstdio>
#include <functional>
uint32_t now=0;uint32_t millis(){return now;}void yield(){++now;}
struct String:std::string {using std::string::string;using std::string::operator+=;};
struct Client {bool alive=true;std::string bytes;size_t pos=0;uint32_t interval=0,last=0;
bool connected(){return alive;}void stop(){alive=false;}
int read(){if(pos<bytes.size() && uint32_t(now-last)>=interval){last=now;return uint8_t(bytes[pos++]);}return -1;}};
struct Server {using ClientType=Client;};
template<class S>struct ESP8266WebServerTemplate {
using ClientType=typename S::ClientType;
uint32_t _ogRequestMs=0;size_t _ogHeaderBytes=0;bool _ogBody=false;
std::function<bool()> _ogBodyAuthorization;
bool _ogAcceptBody(bool,uint32_t,size_t);
int _uploadReadByte(ClientType&);String _ogReadLine(ClientType&,char);
};
'''+method('_ogAcceptBody')+method('_uploadReadByte')+method('_ogReadLine')+r'''
int main(){
 ESP8266WebServerTemplate<Server>s;Client c;
 // Admission happens before the first byte: even a 2048-byte slow plain body
 // is refused, and multipart cannot enter its longer window without auth.
 int auth_calls=0;
 assert(!s._ogAcceptBody(true,2048,10));
 s._ogBodyAuthorization=[&](){++auth_calls;return true;};
 assert(!s._ogAcceptBody(false,2048,0) && auth_calls==0 && now==0);
 assert(!s._ogAcceptBody(true,0,10));
 assert(!s._ogAcceptBody(true,2048,71));
 assert(!s._ogAcceptBody(true,3*1024*1024+1,10));
 assert(s._ogAcceptBody(true,2048,10) && auth_calls==1 && now==0);
 s._ogBodyAuthorization=[](){return false;};
 assert(!s._ogAcceptBody(true,2048,10) && now==0);

 assert(s._uploadReadByte(c)==-1 && !c.alive && now==5000);
 s._ogRequestMs=now;s._ogBody=true;c=Client{};auto start=now;
 assert(s._uploadReadByte(c)==-1 && !c.alive && now-start==10000);
 // A peer sending one byte per second cannot evade the overall request bound.
 s._ogRequestMs=now;c=Client{};c.bytes=std::string(1000,'a');c.interval=1000;c.last=now;start=now;
 while(s._uploadReadByte(c)>=0){}assert(now-start==120000 && !c.alive);
 // Header trickle uses the shorter total deadline even though bytes arrive.
 s._ogRequestMs=now;s._ogBody=false;c=Client{};c.bytes="long header without newline";c.interval=400;c.last=now;start=now;
 s._ogReadLine(c,'\n');assert(!c.alive && now-start==5000);
 s._ogRequestMs=now;c=Client{};c.bytes=std::string(1025,'x')+"\n";s._ogReadLine(c,'\n');assert(!c.alive);
 s._ogRequestMs=now;s._ogHeaderBytes=8192;c=Client{};c.bytes="x\n";s._ogReadLine(c,'\n');assert(!c.alive);
 // Uptime rollover and clean input.
 now=UINT32_MAX-100;s._ogRequestMs=now;s._ogBody=true;c=Client{};start=now;
 assert(s._uploadReadByte(c)==-1 && uint32_t(now-start)==10000);
 s._ogRequestMs=now;s._ogBody=false;s._ogHeaderBytes=0;c=Client{};c.bytes="abc\r\n";
 assert(s._ogReadLine(c,'\r')=="abc" && s._ogReadLine(c,'\n').empty() && c.alive);
 puts("Updater parser byte/line deadlines, trickle, caps, rollover PASS");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
assert s.index('_ogAcceptBody(isForm,') < s.index('_ogBody = true;') < s.index('_parseForm(client,')
parse=s[s.index('::_parseRequest'):s.index('::_parseArguments')]
assert 'readBytesWithTimeout' not in parse
assert 'if (!_currentUpload) return false;' in s
assert 'client.readStringUntil' not in s
