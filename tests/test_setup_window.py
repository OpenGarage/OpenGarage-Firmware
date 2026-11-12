"""Execute the real deadline policy, timer action and deferred cleanup with fakes."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'OpenGarage/main.cpp').read_text()
policy=re.search(r'static bool setup_room\(.*?^}',s,re.M|re.S).group()
cleanup=s[s.index('\tif(curr_mode==OG_MOD_AP && setup_expired'):s.index('\tog_crash::mark(og_crash::STAGE_STATE);',s.index('\tif(curr_mode==OG_MOD_AP && setup_expired'))]
timer=s.split('setup_expiry_ticker.once_ms(og_setup::WINDOW_MS, [](){',1)[1].split('\n\t\t\t});',1)[0]
code=r'''
#include <cassert>
#include <string>
#include "OpenGarage/defines.h"
#include "OpenGarage/setup_window.h"
uint32_t now=0,setup_started_ms=0;bool setup_expired=false;
unsigned curr_mode=OG_MOD_AP,led_blink_ms=0;
uint32_t millis(){return now;}
constexpr int WIFI_OFF=0;
struct {int off=0;void mode(int){++off;}}WiFi;
struct S {int calls=0;void stop(){++calls;}} d,u;S*dns=&d,*updateServer=&u;
struct {int calls=0;void abort_upload(){++calls;}}firmware_update;
struct Opt {unsigned ival=0;std::string sval;};
struct {Opt options[NUM_OPTIONS];int state=OG_STATE_CONNECTED,calls=0;bool config_locked=false,ok=true;
 bool options_save(){++calls;if(!ok)options[OPTION_MOD].ival=OG_MOD_AP;return ok;}
}og;
int reboots=0;void restart_in(int){++reboots;og.state=OG_STATE_WAIT_RESTART;}
'''+policy+'\nvoid expire(){'+timer+'}\nvoid cleanup(){'+cleanup+r'''}
void reset(){setup_expired=false;og.state=OG_STATE_CONNECTED;og.config_locked=false;og.ok=true;
 og.options[OPTION_KSET].ival=0;og.options[OPTION_SSID].sval="";og.calls=reboots=WiFi.off=0;}
int main(){
 for(uint32_t start:{0U,UINT32_MAX-1000}) {
  setup_started_ms=start;now=start;reset();assert(setup_room(150000));
  now=start+og_setup::WINDOW_MS-150000;assert(!setup_room(150000));assert(setup_room(30000));
  now=start+og_setup::WINDOW_MS-30000;assert(!setup_room(30000));
  now=start+og_setup::WINDOW_MS;assert(!setup_room(0));
  expire();assert(setup_expired&&WiFi.off==1&&og.calls==0&&reboots==0);
  cleanup();assert(og.state==OG_STATE_AP_EXPIRED&&reboots==0&&led_blink_ms==2000);
  cleanup();assert(WiFi.off==2); // cleanup once, no AP restart
 }
 reset();og.options[OPTION_KSET].ival=1;og.options[OPTION_SSID].sval="saved";
 expire();cleanup();assert(reboots==1&&og.options[OPTION_MOD].ival==OG_MOD_STA);
 reset();og.options[OPTION_KSET].ival=1;og.options[OPTION_SSID].sval="saved";og.ok=false;
 expire();cleanup();assert(reboots==0&&og.state==OG_STATE_AP_EXPIRED);
 reset();og.options[OPTION_KSET].ival=1;og.options[OPTION_SSID].sval="saved";og.config_locked=true;
 expire();cleanup();assert(reboots==0&&og.calls==0);
 curr_mode=OG_MOD_STA;assert(setup_room(150000));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test';subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),'-x','c++','-','-o',str(p)],input=code,text=True,check=True);subprocess.run([str(p)],check=True)
assert s.count('setup_started_ms=millis()')==1
assert s.count('setup_expiry_ticker.once_ms(')==1
assert 'process_ui();' in s
print('PASS: 30-minute deadline, wraparound, admission margins, timer radio shutdown, deferred cleanup and save failure')
