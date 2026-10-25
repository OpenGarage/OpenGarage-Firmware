"""Execute the actual WiFi adapter against an observable SDK stub."""
from pathlib import Path
import re, subprocess, tempfile
source=Path('OpenGarage/espconnect.cpp').read_text()
def function(signature):
 start=source.index(signature);end=source.index('\n}',start)+2
 return source[start:end]
functions=source[source.index('og_wifi::Selection station_selection;'):source.index('\n\nString scan_network')]+'\n'+function('void start_network_sta(const char *ssid, const char *pass, const char *hostname, bool staonly)')
stub=r'''
#include "OpenGarage/wifi_manager.h"
#include "OpenGarage/wifi_selection.h"
#include <vector>
#include <cassert>
#include <string>
#include <cstdio>
#define DEBUG_PRINTLN(x) ((void)0)
#define F(x) x
constexpr int WIFI_OFF=0,WIFI_STA=1,WIFI_AP_STA=3,WL_CONNECTED=3;
constexpr int WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
constexpr int ENC_TYPE_NONE=7,ENC_TYPE_CCMP=4,ENC_TYPE_TKIP=2,ENC_TYPE_AUTO=8;
uint32_t now=0;
uint32_t millis(){return now;}
struct WiFiEventStationModeDisconnected { uint8_t reason; };
using WiFiEventHandler=int;
og_wifi::Recovery station_recovery;
uint8_t station_disconnect_reason=0;
WiFiEventHandler station_disconnect_handler;
struct FakeWiFi {
 bool reconnect=true,connected=false,ip=false,sleep=true;
 bool static_config=false,require_restored=false;double power=0;
 int current_mode=0,disconnects=0,begins=0,scans=0,scan_result=-2;
 int target_channel=0;bool targeted=false;uint8_t actual[6]={1,2,3,4,5,6};
 struct AP {std::string ssid;og_wifi::AccessPoint ap;uint8_t enc;};
 std::vector<AP> aps;
 int scanNetworks(bool async,bool hidden,int channel,uint8_t *s){
  assert(async && hidden && s && channel>=0);++scans;scan_result=WIFI_SCAN_RUNNING;return scan_result;
 }
 int scanComplete(){return scan_result;}void scanDelete(){scan_result=WIFI_SCAN_FAILED;}
 std::string SSID(int i){return aps[i].ssid;}
 int RSSI(int i=-1){return i<0 ? -80 : aps[i].ap.rssi;}
 uint8_t encryptionType(int i){return aps[i].enc;}
 uint8_t *BSSID(int i=-1){return i<0 ? actual : aps[i].ap.bssid;}
 int channel(int i=-1){return i<0 ? 1 : aps[i].ap.channel;}
 std::string ssid,pass,host;
 int getMode(){return current_mode;} void mode(int m){current_mode=m;if(m==WIFI_OFF){static_config=false;power=0;host.clear();sleep=true;reconnect=true;}}
 void hostname(const char *h){host=h;} std::string hostname(){return host;}
 void setAutoReconnect(bool b){reconnect=b;}
 void disconnect(bool off,bool erase){assert(!off && !erase);connected=ip=false;++disconnects;}
 void setSleep(bool b){sleep=b;} void setOutputPower(double p){power=p;}
 int status(){return connected ? WL_CONNECTED : 0;} bool localIP(){return ip;}
 template<class C> WiFiEventHandler onStationModeDisconnected(C cb){cb({201});return 1;}
 void begin(const char *s,const char *p,int channel=0,const uint8_t *bssid=nullptr){
  if(require_restored) assert(static_config && power==20.5 && host=="host" && !sleep && !reconnect);
  targeted=bssid!=nullptr;target_channel=channel;ssid=s;pass=p;++begins;
 }
} WiFi;
struct OpenGarage { static void config_ip(){WiFi.static_config=true;} };
void service_station_network(const char*,const char*,bool,bool,bool=false,bool=false);
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
 // Automatic: scan before begin, strongest protected candidate, no open downgrade.
 start_network_sta("saved","password","host",true);now=200000;
 int before=WiFi.begins;
 service_station_network("saved","password",false,false,true);
 assert(WiFi.scans==1 && WiFi.begins==before);
 og_wifi::AccessPoint a;a.valid=true;a.channel=6;a.rssi=-50;a.bssid[0]=9;
 WiFi.aps.push_back({"saved",a,ENC_TYPE_CCMP});
 a.rssi=-20;WiFi.aps.push_back({"saved",a,ENC_TYPE_NONE});
 a.rssi=-10;WiFi.aps.push_back({"other",a,ENC_TYPE_CCMP});
 WiFi.scan_result=3;++now;service_station_network("saved","password",false,false,true);
 assert(WiFi.begins==before+1 && WiFi.targeted && WiFi.target_channel==6);
 // No scan during association. Timeout -> next attempt explicitly clears lock.
 now+=15000;service_station_network("saved","password",false,false,true);
 now+=5000;service_station_network("saved","password",false,false,true);
 assert(WiFi.begins==before+2 && !WiFi.targeted && WiFi.scans==1);
 // Empty/failed scans keep hidden SSIDs usable. Disabling selection while a
 // scan runs discards its target; neither path can leave a BSSID pin behind.
 for(int mode=0;mode<2;++mode){
  start_network_sta("saved","password","host",true);now+=20000;
  service_station_network("saved","password",false,false,true);
  before=WiFi.begins;WiFi.scan_result=mode ? 3 : WIFI_SCAN_FAILED;
  ++now;service_station_network("saved","password",false,false,mode==0);
  assert(WiFi.begins==before+1 && !WiFi.targeted);
 }
 // A busy transition during a connected scan must not disconnect to roam.
 start_network_sta("saved","password","host",true);now+=20000;
 WiFi.connected=WiFi.ip=true;
 service_station_network("saved","password",false,false,true);
 now+=60000;service_station_network("saved","password",false,false,true);
 assert(station_scan==og_wifi::Selection::Scan::SEARCH);
 WiFi.scan_result=3;++now;service_station_network("saved","password",false,false,true);
 now+=5000;service_station_network("saved","password",false,false,true);
 assert(station_scan==og_wifi::Selection::Scan::CONFIRM);
 WiFi.scan_result=3;before=WiFi.disconnects;++now;
 service_station_network("saved","password",true,false,true);
 assert(WiFi.connected && WiFi.disconnects==before);
 // Successful confirmation performs exactly one targeted migration, then
 // stays put even if its new RSSI is weak throughout the dwell period.
 start_network_sta("saved","password","host",true);now+=20000;
 WiFi.connected=WiFi.ip=true;service_station_network("saved","password",false,false,true);
 now+=60000;service_station_network("saved","password",false,false,true);
 WiFi.scan_result=3;++now;service_station_network("saved","password",false,false,true);
 now+=5000;service_station_network("saved","password",false,false,true);
 WiFi.scan_result=3;before=WiFi.begins;++now;
 service_station_network("saved","password",false,false,true);
 assert(WiFi.begins==before+1 && WiFi.targeted && !WiFi.connected);
 memcpy(WiFi.actual,WiFi.aps[0].ap.bssid,6);WiFi.connected=WiFi.ip=true;
 ++now;service_station_network("saved","password",false,false,true);
 assert(station_selection.roams()==1);
 before=WiFi.scans;now+=100000;service_station_network("saved","password",false,false,true);
 assert(WiFi.scans==before);
 // A stuck scan recovers using radio reset and SSID fallback, not MCU reboot.
 start_network_sta("saved","password","host",true);now+=20000;
 service_station_network("saved","password",false,false,true);
 before=WiFi.begins;now+=10000;WiFi.require_restored=true;
 service_station_network("saved","password",false,false,true);
 assert(WiFi.begins==before+1 && !WiFi.targeted && station_scan_broken);
 assert(WiFi.static_config && WiFi.power==20.5 && WiFi.host=="host");
 assert(!WiFi.sleep && !WiFi.reconnect);
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
