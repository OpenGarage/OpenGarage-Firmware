"""Compile the real /db handlers with host stubs; verify compact/verbose JSON."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'OpenGarage/main.cpp').read_text()
functions = []
for name in ('append_age', 'append_health_json', 'append_debug_details', 'on_sta_debug', 'on_ap_debug'):
    match = re.search(r'(?:static )?void ' + name + r'\([^\n]+\) \{.*?^\}', source, re.S | re.M)
    assert match, name
    functions.append(match.group())

stub = r'''
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#define F(x) x
#define HEX 16
#define STRING_RESERVE_SIZE 256
struct String : std::string {
  using std::string::string;
  using std::string::operator+=;
  using std::string::operator=;
  String(unsigned n,int) { std::ostringstream s; s<<std::hex<<n; assign(s.str()); }
  template<class T> String& operator+=(T n) { append(std::to_string(n)); return *this; }
  String& operator+=(const char *s) { append(s); return *this; }
  String& operator+=(const String &s) { append(s); return *this; }
};
namespace OTF {
struct Request { const char *value; const char *getQueryParameter(const char*) const { return value; } };
struct Response {};
}
struct Option { int ival=125; String sval="test"; };
enum { OPTION_FWV, OPTION_NAME, OPTION_SECV };
struct { Option options[3]; int has_swrx=1; int read_distance(){return 100;} } og;
struct IP { String toString(){return "192.0.2.1";} };
struct { IP localIP(){return {};} int RSSI(){return -50;} String BSSIDstr(){return "00:00:00:00:00:00";} } WiFi;
struct ResetInfo { unsigned reason=0; };
struct {
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
String mqtt_topic="test";
String get_mac(){return "00:00:00:00:00:01";}
void otf_send_json(OTF::Response&,const String &json){std::cout<<json<<'\n';}
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
    subprocess.run(['g++', '-std=c++11', '-x', 'c++', '-', '-o', binary],
                   input=stub + '\n'.join(functions) + runner, text=True, check=True)
    results = [json.loads(line) for line in subprocess.check_output([binary], text=True).splitlines()]

station = set('rcnt fwv name mac mqtt_topic devip cid rssi bssid build Freeheap flash_size has_swrx'.split())
ap = set('dist fwv has_swrx'.split())
common = set('active_security_protocol free_heap reset_reason reset_reason_code uptime_s min_free_heap max_loop_us'.split())
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
                assert set(data) == baseline | (extra if offset == 6 else set()), data
                if offset == 6:
                    assert data['active_security_protocol'] == protocol
                    if hardware and protocol == 2:
                        assert data['secplus2_client_id'] == '0x12342908'
                        assert data['secplus2_identity_ready'] is True
print('/db: 84 AP/station, compact/verbose, hardware and protocol response cases passed')
