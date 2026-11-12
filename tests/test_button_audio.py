"""Exercise the actual button handler and audio priority with a fake clock/GPIO."""
from pathlib import Path
import re
import subprocess
import tempfile

source = Path('OpenGarage/main.cpp').read_text()
handler = source[source.index('void process_ui() {'):source.index('\nvoid on_update(')]
start = source[source.index('void start_tune('):source.index('\nstatic HealthStats')]
defines = Path('OpenGarage/defines.h').read_text()
thresholds = '\n'.join(re.findall(r'^#define BUTTON_(?:REPORTIP|APRESET|FACRESET)_TIMEOUT\s+\d+.*$', defines, re.M))
stub = r'''
#include "OpenGarage/audio_timing.h"
#include <cassert>
#include <cstdint>
using byte=unsigned char;
using ulong=unsigned long;
constexpr int LOW=0,HIGH=1,OG_STATE_RESET=9,ACTION_TOGGLE=1;
uint32_t now=100;
uint32_t millis(){return now;}
struct {int button=HIGH,state=0,led=0;bool alarm=false;unsigned resets=0;
 int get_button(){return button;}void set_led(int x){led=x;}int get_led(){return led;}
 void reset_to_ap(){++resets;}} og;
struct Sink {unsigned notes=0;void note(unsigned,unsigned){++notes;}void silence(){}} sink;
og_audio::Melody<Sink> startup_audio(sink);
bool ip_audio_active=false,setup_reboot_pending=false,update_started=false;
unsigned reports=0,doors=0;ulong led_blink_ms=0;
struct Text {void replace(const char*,const char*){}} ipString;
Text get_ip(){return {};}
void report_ip(){++reports;}
void cancel_diagnostic_audio(){startup_audio.cancel();ip_audio_active=false;}
void performDoorAction(int,bool){++doors;}
'''
main = r'''
void hold(unsigned elapsed){now=100+elapsed;process_ui();startup_audio.tick(now);}
void begin(){now=100;og.button=LOW;process_ui();}
void release(){og.button=HIGH;process_ui();}
int main(){
 begin();hold(800);assert(sink.notes==0);hold(801);assert(sink.notes==1);
 for(unsigned t=802;t<=4500;++t)hold(t);
 assert(sink.notes==1 && !startup_audio.active());hold(4501);
 for(unsigned t=4502;t<=9500;++t)hold(t);
 assert(sink.notes==3);hold(9501);
 for(unsigned t=9502;t<=11000;++t)hold(t);
 assert(sink.notes==6 && !startup_audio.active());
 assert(!og.state && !og.resets && !reports && !doors);release();assert(og.state==OG_STATE_RESET);
 // Release actions and threshold stage reset on subsequent holds.
 begin();hold(900);release();assert(reports==1);ip_audio_active=false;
 begin();hold(5000);release();assert(og.resets==1);startup_audio.cancel();
 begin();hold(100);release();assert(doors==1);
 // Busy-loop jump announces only the latest threshold, no catch-up train.
 unsigned n=sink.notes;begin();hold(10000);
 for(unsigned t=10001;t<11000;++t)hold(t);
 assert(sink.notes==n+3);release();
 // Safety/IP audio take priority, with no delayed marker after suppression.
 for(unsigned mode=0;mode<4;++mode){
  og.alarm=mode==0;ip_audio_active=mode==1;update_started=mode==2;setup_reboot_pending=mode==3;
  n=sink.notes;begin();hold(5000);assert(sink.notes==n);
  og.alarm=ip_audio_active=update_started=setup_reboot_pending=false;
  hold(5100);assert(sink.notes==n);release();
 }
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory)/'button.cpp'
    exe = Path(directory)/'button'
    cpp.write_text(stub+'\n'+thresholds+'\n'+start+'\n'+handler+'\n'+main)
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I.',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Button markers: boundaries, once per hold, release actions, skipped stages and audio priority passed')
