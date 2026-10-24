"""Execute the actual WiFi adapter against an observable SDK stub."""
from pathlib import Path
import re, subprocess, tempfile
source=Path('OpenGarage/espconnect.cpp').read_text()
def function(signature):
 start=source.index(signature);end=source.index('\n}',start)+2
 return source[start:end]
functions=function('void service_station_network(')+'\n'+function('void start_network_sta(const char *ssid, const char *pass, const char *hostname, bool staonly)')
stub=r'''
#include "OpenGarage/wifi_manager.h"
#include <cassert>
#include <string>
#include <cstdio>
#define DEBUG_PRINTLN(x) ((void)0)
#define F(x) x
constexpr int WIFI_STA=1,WIFI_AP_STA=3,WL_CONNECTED=3;
uint32_t now=0;
uint32_t millis(){return now;}
struct WiFiEventStationModeDisconnected { uint8_t reason; };
using WiFiEventHandler=int;
og_wifi::Recovery station_recovery;
uint8_t station_disconnect_reason=0;
WiFiEventHandler station_disconnect_handler;
struct FakeWiFi {
 bool reconnect=true,connected=false,ip=false,sleep=true;
 int current_mode=0,disconnects=0,begins=0;
 std::string ssid,pass,host;
 int getMode(){return current_mode;} void mode(int m){current_mode=m;}
 void hostname(const char *h){host=h;}
 void setAutoReconnect(bool b){reconnect=b;}
 void disconnect(bool off,bool erase){assert(!off && !erase);connected=ip=false;++disconnects;}
 void setSleep(bool b){sleep=b;} void setOutputPower(double){}
 int status(){return connected ? WL_CONNECTED : 0;} bool localIP(){return ip;}
 template<class C> WiFiEventHandler onStationModeDisconnected(C cb){cb({201});return 1;}
 void begin(const char *s,const char *p,int channel=0,const uint8_t *bssid=nullptr){
  assert(!bssid && channel==0);ssid=s;pass=p;++begins;
 }
} WiFi;
'''
main=r'''
int main(){
 start_network_sta("saved","password","host",true);
 assert(!WiFi.reconnect && WiFi.begins==0 && !WiFi.sleep && WiFi.host=="host");
 assert(station_disconnect_reason==201);
 service_station_network("saved","password",true,false);assert(WiFi.begins==0);
 service_station_network("saved","password",false,false);assert(WiFi.begins==1);
 assert(WiFi.ssid=="saved" && WiFi.pass=="password" && !WiFi.reconnect);
 // Association without an IP must still hit the deadline.
 WiFi.connected=true;now=15000;service_station_network("saved","password",false,false);
 assert(station_recovery.timeouts()==1 && !WiFi.connected);
 now=20000;service_station_network("saved","password",false,false);assert(WiFi.begins==2);
 WiFi.connected=WiFi.ip=true;now=20001;service_station_network("saved","password",false,false);
 assert(station_recovery.state()==og_wifi::Recovery::State::ONLINE);
 now=100000;service_station_network("saved","password",false,false);assert(WiFi.begins==2);
 WiFi.connected=WiFi.ip=false;service_station_network("saved","password",false,false);
 now+=5000;service_station_network("saved","password",false,false);assert(WiFi.begins==3);
 // A stale motion report cannot indefinitely suppress offline recovery.
 station_recovery.start(100001);now=100001;
 service_station_network("saved","password",false,true);assert(WiFi.begins==3);
 now+=60000;service_station_network("saved","password",true,true);assert(WiFi.begins==3);
 service_station_network("saved","password",false,true);assert(WiFi.begins==4);
 // Explicit AP provisioning retains its old retry behavior, separate from STA owner.
 start_network_sta("setup","new-pass",nullptr,false);
 assert(WiFi.reconnect && WiFi.begins==5 && WiFi.current_mode==WIFI_AP_STA);
 puts("production WiFi adapter: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(stub+functions+main)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I.',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
main_src=Path('OpenGarage/main.cpp').read_text()
loop=main_src[main_src.index('void do_loop() {'):]
assert loop.index('check_status();')<loop.index('service_station_network(')<loop.index('switch(og.state)')
assert 'ReconnectBackoff' not in main_src
assert 'WiFi.persistent(false)' in main_src
# Static IP config runs before the first attempt on the next loop.
assert loop.index('start_network_sta(')<loop.index('og.config_ip();')<loop.index('og.state = OG_STATE_CONNECTING;')
