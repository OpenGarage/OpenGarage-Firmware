"""Compile the real /db handlers with host stubs; verify compact/verbose JSON."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'OpenGarage/main.cpp').read_text()
functions = []
for name in ('append_age', 'append_health_json', 'append_debug_details', 'send_debug_json', 'on_sta_debug', 'on_ap_debug'):
    match = re.search(r'(?:static )?void ' + name + r'\([^\n]+\) \{.*?^\}', source, re.S | re.M)
    assert match, name
    functions.append(match.group())

stub = r'''
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cassert>
#include "OpenGarage/json_escape.h"
#include "OpenGarage/local_time.h"
#include "OpenGarage/wifi_manager.h"
#include "OpenGarage/wifi_selection.h"
og_wifi::Selection station_selection;
const char *station_selection_status(){return "Idle";}
og_wifi::Recovery station_recovery;
uint8_t station_disconnect_reason=0;
uint32_t millis(){return 100;}
og_local::Clock local_clock;
#define F(x) x
#define OG_TLS_STACK_BYTES 6656
unsigned stack_thunk_get_max_usage(){return 5940;}
struct HeapSelectIram {};
#define FPSTR(x) x
bool secure_cloud_enabled=true;
const char trustVersion[]="test-store",trustHash[]="test-hash";
namespace og_tls {struct S {unsigned state=1,refusals=0;int error=0;};inline S &status(){static S s;return s;}}
#define HEX 16
#define STRING_RESERVE_SIZE 256
static int allocation_budget=-1;
static bool alloc_ok(){if(allocation_budget<0)return true;if(!allocation_budget)return false;--allocation_budget;return true;}
struct String : std::string {
  using std::string::string;
  using std::string::operator+=;
  using std::string::operator=;
  bool reserve(unsigned){return alloc_ok();}
  template<class T> bool concat(const T &x){if(!alloc_ok())return false;*this+=x;return true;}
  String(unsigned n,int) { std::ostringstream s; s<<std::hex<<n; assign(s.str()); }
  template<class T> String& operator+=(T n) { append(std::to_string(n)); return *this; }
  String& operator+=(const char *s) { append(s); return *this; }
  String& operator+=(const String &s) { append(s); return *this; }
  String& operator+=(char c) { push_back(c); return *this; }
};
#include "OpenGarage/checked_json.h"
namespace OTF {
struct Request { const char *value; const char *getQueryParameter(const char*) const { return value; } };
struct Response {int status=200;void writeStatus(unsigned n){status=n;}template<class T>void writeHeader(const char*,T){}
 void writeBodyData(const char *p,size_t n){assert(status==503);std::cout.write(p,n);std::cout<<'\n';} };
}
struct Option { int ival=125; String sval="test"; };
enum { OPTION_FWV, OPTION_NAME, OPTION_SECV };
struct { unsigned config_status=0;bool config_locked=false; Option options[3]; int has_swrx=1; int read_distance(){return 100;} } og;
struct IP { String toString(){return "192.0.2.1";} };
struct { IP localIP(){return {};} int RSSI(){return -50;} int channel(){return 1;} int getPhyMode(){return 3;} String BSSIDstr(){return "00:00:00:00:00:00";} } WiFi;
struct ResetInfo { unsigned reason=0; };
struct {
  unsigned getMaxFreeBlockSize(){return 28000;} unsigned getCpuFreqMHz(){return 160;}
  unsigned getFreeContStack(){return 1536;}
  unsigned getChipId(){return 123;} unsigned getFreeHeap(){return 30000;}
  unsigned getFlashChipRealSize(){return 4194304;}
  const char *getResetReason(){return "Power on";}
  ResetInfo *getResetInfoPtr(){static ResetInfo r; return &r;}
} ESP;
struct { unsigned uptime_s(){return 10;} unsigned min_heap(){return 29000;} unsigned max_loop_us(){return 100;} } health_stats;
struct Garage {
  unsigned get_parity_errors(){return 0;} unsigned get_invalid_frames(){return 0;}
  unsigned get_overflows(){return 0;} unsigned get_tx_deferrals(){return 0;}
  unsigned get_expired_commands(){return 0;} unsigned get_door_age(){return 20;}
  unsigned get_light_lock_age(){return 20;} unsigned get_status_age(){return 20;}
  unsigned get_query_failures(){return 0;} unsigned get_write_failures(){return 0;}
  bool controls_faulted(){return false;} const char *recovery_status(){return "Observed";}
} secplus1_garage,secplus2_garage;
unsigned read_cnt=10,secplus2_client_id=0x12342908;
bool secplus2_identity_ready=true;
unsigned email_failures=2;const char *email_last="Refused: Not enough memory for SMTP (2)";
unsigned mqtt_json_failures=0,mqtt_json_bytes=0,mqtt_retained_dropped=0,notification_dropped=0;
bool mqtt_buffer_ready=true;
String mqtt_topic="test";
String get_mac(){return "00:00:00:00:00:01";}
void otf_send_json(OTF::Response&,const String &json){std::cout<<json<<'\n';}
#include <cstdlib>
namespace og_crash { unsigned last_index=99;
void append_json(DebugJson &json, unsigned index){ last_index=index;
  json += ",\"crash_count\":0,\"crash_new\":0,\"crash\":null,\"stage_max_us\":{}"; } }
'''
runner = r'''
int main() {
  OTF::Response response;
  for(int hardware : {0,1}) for(int protocol : {0,1,2}) {
  og.has_swrx=hardware; og.options[OPTION_SECV].ival=protocol;
  for(auto handler : {on_sta_debug,on_ap_debug}) {
    for(const char *value : {static_cast<const char*>(nullptr),"","0","true","01","1x","1"}) {
      handler(OTF::Request{value},response);
    }
  }
  }
}
'''
with tempfile.TemporaryDirectory(prefix='og-db-test-') as scratch:
    binary = str(Path(scratch) / 'test')
    subprocess.run(['g++', '-std=c++11', '-I', str(Path(__file__).resolve().parents[1]), '-x', 'c++', '-', '-o', binary],
                   input=stub + '\n'.join(functions) + runner, text=True, check=True)
    results = [json.loads(line) for line in subprocess.check_output([binary], text=True).splitlines()]

station = set('rcnt fwv name mac mqtt_topic devip cid rssi bssid build Freeheap flash_size has_swrx'.split())
ap = set('dist fwv has_swrx'.split())
common = set('cloud_tls cpu_mhz active_security_protocol free_heap largest_free_heap_block build_time otf_revision reset_reason reset_reason_code uptime_s min_free_heap max_loop_us min_free_cont_stack time_valid'.split())
one = {'secplus1_' + k for k in 'parity_errors invalid_frames rx_overflows tx_deferrals expired_commands door_age_ms light_lock_age_ms'.split()}
two = {'secplus2_' + k for k in 'client_id identity_ready status_age_ms query_failures action_write_failures tx_deferrals expired_commands control_fault recovery'.split()}
assert len(results) == 84
iterator = iter(results)
for hardware in (0, 1):
    for protocol in (0, 1, 2):
        extra = common | (one if hardware and protocol == 1 else two if hardware and protocol == 2 else set())
        for baseline in (station, ap):
            for offset in range(7):
                data = next(iterator)
                mqtt_extra = {'mqtt_json_failures', 'mqtt_json_bytes', 'mqtt_buffer_ready', 'mqtt_retained_dropped', 'notification_dropped', 'email_failures', 'email_last', 'wifi_recovery', 'wifi_attempts', 'wifi_timeouts', 'wifi_disconnects', 'wifi_retry_in_ms', 'wifi_disconnect_reason', 'wifi_selection', 'wifi_scans', 'wifi_roams', 'wifi_smoothed_rssi', 'wifi_channel', 'wifi_phy', 'config_status', 'config_locked', 'crash_count', 'crash_new', 'crash', 'stage_max_us'}
                assert set(data) == baseline | ((extra | mqtt_extra) if offset == 6 else set()), data
                if offset == 6:
                    assert data['active_security_protocol'] == protocol
                    assert data['wifi_phy'] == 3
                    assert data['cpu_mhz']==160
                    assert data['cloud_tls']=={'enabled':1,'state':1,'allocation_refusals':0,'last_ssl_error':0,'stack_bytes':6656,'stack_peak':5940,'iram_largest_block':28000,'catalog_version':'test-store','catalog_hash':'test-hash'}
                    if hardware and protocol == 2:
                        assert data['secplus2_client_id'] == '0x12342908'
                        assert data['secplus2_identity_ready'] is True
print('/db: 84 AP/station, compact/verbose, hardware and protocol response cases passed')

for item in results:
    if "min_free_cont_stack" in item:
        assert item["min_free_cont_stack"] == 1536

# Keep the diagnostic identity tied to the exact dependency pin used by the build.
ini = Path('OpenGarage/platformio.ini').read_text()
pin = re.search(r'OpenThings-Framework-Firmware-Library\.git#([0-9a-f]{40})', ini)
if pin:
    otf_pin = pin.group(1)
else:
    # Temporary unreleased local OTF checkout: the revision is that checkout's HEAD.
    local = re.search(r'^\s*symlink://(\S+)', ini, re.M).group(1)
    checkout = str(Path('OpenGarage') / local)
    otf_pin = subprocess.check_output(['git', '-C', checkout, 'rev-parse', 'HEAD'], text=True).strip()
    # Uncommitted changes would make otf_revision name code the build does not contain.
    assert not subprocess.check_output(['git', '-C', checkout, 'status', '--porcelain'], text=True).strip(), \
        f'local OTF checkout {checkout} has uncommitted changes; commit them and update otf_revision'
    # Development only: tools/firmware_release.py prepare refuses local dependencies.
    print(f'WARNING: OTF is a local unreleased checkout ({otf_pin[:7]}); pin a release before shipping')
for item in results:
    if 'otf_revision' in item:
        assert item['otf_revision'] == otf_pin
        assert item['largest_free_heap_block'] == 28000
        assert len(item['build_time']) == 8

# Fail each builder reservation/append in turn, including escaping and nested
# health/crash serializers. Every response must still be complete JSON.
fault_runner = r'''
int main() {
 og.has_swrx=1;og.options[OPTION_SECV].ival=2;
 for(auto handler : {on_sta_debug,on_ap_debug}) for(const char *value : {static_cast<const char*>(nullptr),"1"}) {
  for(int budget=0;budget<600;++budget) {
   allocation_budget=budget;OTF::Response response;
   std::ostringstream capture;auto *prior=std::cout.rdbuf(capture.rdbuf());
   handler(OTF::Request{value},response);std::cout.rdbuf(prior);
   std::cout<<"{\"status\":"<<response.status<<",\"body\":"<<capture.str()<<"}\n";
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='og-db-oom-') as scratch:
    binary = str(Path(scratch) / 'test')
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-I',str(Path(__file__).resolve().parents[1]),
                    '-x','c++','-','-o',binary],input=stub+'\n'.join(functions)+fault_runner,text=True,check=True)
    output=subprocess.check_output([binary],text=True)
    decoder=json.JSONDecoder();fault_results=[];offset=0
    while offset<len(output):
        while offset<len(output) and output[offset].isspace(): offset+=1
        if offset==len(output):break
        item,size=decoder.raw_decode(output[offset:]);fault_results.append(item);offset+=size
assert len(fault_results)==2400
assert any(x['status']==200 for x in fault_results)
assert any(x['status']==503 for x in fault_results)
for item in fault_results:
    if item['status']==503:
        assert item['body']=={'result':48,'item':'Not enough memory for diagnostics'}
    else:
        assert item['status']==200
        assert 'fwv' in item['body']
assert 'void otf_send_json(OTF::Response &res, const String &json)' in source
print('/db: 2400 injected reservation/append failure cases returned complete JSON or fixed HTTP503')
