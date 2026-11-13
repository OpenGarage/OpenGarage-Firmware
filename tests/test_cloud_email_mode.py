"""Run the real email suspension wrapper in plain and secure modes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'OpenGarage/main.cpp').read_text()
a=s.index('og_smtp::Delivery emailNotify(String s) {');b=s.index('\nvoid mqttNotify',a);wrapper=s[a:b]
code=r'''
#include <string>
#include <cassert>
#include <cstdint>
using String=std::string;
namespace og_smtp {enum class Delivery{SENT,REFUSED,FAILED};}
unsigned now=100;uint32_t millis(){return now;}
enum{OPTION_EMEN,OPTION_SMTP,OPTION_SEND,OPTION_RECP};
struct Opt{unsigned ival=1;std::string sval="configured";};struct{Opt options[4];}og;
struct Cloud{bool active=true;unsigned pauses=0;bool cloudTransportActive(){return active;}void pauseCloud(){++pauses;active=false;}}cloud;
Cloud*otf=&cloud;bool secure_cloud_enabled=false,smtp_cloud_paused=false;uint32_t smtp_finished_ms=0;
unsigned sends=0;og_smtp::Delivery result=og_smtp::Delivery::SENT;
og_smtp::Delivery emailNotify_impl(String){++sends;return result;}
'''+wrapper+r'''
int main(){
 assert(emailNotify("plain")==og_smtp::Delivery::SENT);assert(sends==1&&cloud.active&&!cloud.pauses&&!smtp_cloud_paused);
 secure_cloud_enabled=true;emailNotify("secure");assert(sends==2&&cloud.pauses==1&&!cloud.active&&smtp_cloud_paused&&smtp_finished_ms==100);
 now=200;result=og_smtp::Delivery::FAILED;emailNotify("failure");assert(cloud.pauses==1&&smtp_finished_ms==200);
 smtp_cloud_paused=false;og.options[OPTION_EMEN].ival=0;cloud.active=true;emailNotify("disabled");assert(cloud.pauses==1&&cloud.active&&!smtp_cloud_paused);
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.cpp').write_text(code)
 subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
assert 'OG_WSS_BENCH' not in s and '/wss-mail-bench' not in s
print('Actual email wrapper: WS remains connected; WSS pauses and records completion on success/failure PASS')
