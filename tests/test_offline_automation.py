"""Run the production automation function with a controllable clock and outputs."""
from pathlib import Path
import subprocess
import tempfile
source = Path('OpenGarage/main.cpp').read_text()
fn = source[source.index('void process_dynamics(byte event) {'):source.index('\nbyte check_door_event()')]
preamble = r'''
#include "OpenGarage/local_time.h"
#include <string>
#include <cassert>
#include <cstdio>
using byte = unsigned char;
struct String : std::string {
 using std::string::string;
 String(const std::string &s):std::string(s){}
 String &operator+=(unsigned n) { append(std::to_string(n)); return *this; }
 using std::string::operator+=;
};
#define F(x) x
#define DEBUG_PRINTLN(x) ((void)0)
enum { OPTION_ATO, OPTION_ATOB, OPTION_NOTO, OPTION_NAME, OPTION_ATIB, OPTION_ATI, OPTION_ALM };
enum { DOOR_EVENT_JUST_OPENED, DOOR_EVENT_JUST_CLOSED, DOOR_EVENT_JUST_STOPPED, DOOR_EVENT_REMAIN_OPEN, UNKNOWN };
enum { OG_AUTO_NOTIFY=1, OG_AUTO_CLOSE=2, OG_NOTIFY_DO=1, OG_NOTIFY_DC=2, OG_NOTIFY_DS=4, OG_ALM_5=1 };
struct Option { unsigned ival=0; String sval; };
struct OG { Option options[7]; unsigned alarms=0; void set_alarm(unsigned=1) { ++alarms; } } og;
og_local::OpenDuration open_duration;
og_local::DailyRule daily_rule;
og_local::Clock local_clock;
uint32_t curr_utc_time=0, curr_utc_hour=0, now_ms=0;
unsigned notifications=0;
uint32_t millis() { return now_ms; }
void perform_notify(String) { ++notifications; }
void reset() {
 og=OG(); open_duration={}; daily_rule={}; local_clock={};
 curr_utc_time=curr_utc_hour=now_ms=notifications=0;
}
'''
main = r'''
int main() {
 reset(); og.options[OPTION_ATO].ival=3; og.options[OPTION_ATI].ival=10;
 now_ms=1000; process_dynamics(DOOR_EVENT_REMAIN_OPEN);
 local_clock.valid=true; curr_utc_time=1790000000; curr_utc_hour=12;
 now_ms=2000; process_dynamics(DOOR_EVENT_REMAIN_OPEN);
 assert(og.alarms==0 && notifications==0); // NTP jump does not close early
 now_ms=600999; process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==0);
 now_ms=601000; process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==1 && notifications==1);
 reset(); og.options[OPTION_ATOB].ival=3; og.options[OPTION_ATIB].ival=0;
 process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==0); // invalid midnight
 for (unsigned hour=0; hour<24; ++hour) {
  reset(); og.options[OPTION_ATOB].ival=3; og.options[OPTION_ATIB].ival=hour;
  local_clock.valid=true; curr_utc_time=20000*86400U+hour*3600; curr_utc_hour=hour;
  process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==1);
  process_dynamics(DOOR_EVENT_JUST_CLOSED);
  curr_utc_time+=86400;
  process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==2);
  curr_utc_time-=86400;
  process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==2);
 }
 reset(); og.options[OPTION_ATOB].ival=2; og.options[OPTION_ATIB].ival=23;
 local_clock.valid=true; curr_utc_time=20000*86400U+23*3600; curr_utc_hour=23;
 process_dynamics(DOOR_EVENT_JUST_OPENED);
 process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==0); // manual-open exception
 reset(); og.options[OPTION_ATO].ival=2; og.options[OPTION_ATI].ival=1;
 now_ms=0xfffffff0U; process_dynamics(DOOR_EVENT_REMAIN_OPEN);
 now_ms+=60000U; process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==1);
 reset(); og.options[OPTION_ATO].ival=2; og.options[OPTION_ATI].ival=1;
 process_dynamics(DOOR_EVENT_REMAIN_OPEN); now_ms=59000;
 process_dynamics(UNKNOWN); now_ms=60000;
 process_dynamics(DOOR_EVENT_REMAIN_OPEN); assert(og.alarms==0);
 puts("production offline automation: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d); (p/'test.cpp').write_text(preamble+fn+main)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I.',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# Check the actual integration, not only helper behavior.
loop=source[source.index('void do_loop() {'):]
assert loop.index('time_keeping();') < loop.index('switch(og.state)')
assert loop.index('check_status();') < loop.index('switch(og.state)')
initial=loop[:loop.index('case OG_STATE_TRY_CONNECT:')]
assert 'otf->on("/jc", on_sta_controller)' in initial
connecting=loop[loop.index('case OG_STATE_CONNECTING:'):loop.index('case OG_STATE_RESET:')]
assert 'og.restart()' not in connecting
assert 'otf->on(' not in connecting
notify=source[source.index('void perform_notify(String s) {'):source.index('void process_dynamics')]
assert notify.index('WiFi.status() != WL_CONNECTED') < notify.index('pending_notification = s')
print('offline startup integration: PASS')
