"""The device key is required on every path: local and OTC, GET query and POST form body."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/main.cpp').read_text()

# The only remaining OTC special case refuses firmware updates; nothing grants access by origin.
uses = [line.strip() for line in source.splitlines() if 'isCloudRequest()' in line]
assert uses == ['if(req.isCloudRequest()) {'], uses
assert 'otf_send_result(res, HTML_NOT_PERMITTED, "fw update");' in source

def grab(pattern):
    return re.search(pattern, source, re.M | re.S).group()

code = r'''
#include <assert.h>
#include "OpenGarage/setup_window.h"
bool setup_room(uint32_t){return true;}
#include <string.h>
#include <string>
#include <map>
#include "OpenGarage/form_params.h"
struct String : std::string { using std::string::string; const char *c_str() const { return std::string::c_str(); } };
struct { bool config_locked=false; struct { String sval; } options[1]; } og;
enum { OPTION_DKEY = 0 };
namespace OTF { struct Request {
  bool cloud; std::map<std::string, std::string> q;
  bool isCloudRequest() const { return cloud; }
  char *getQueryParameter(const char *k) const {
    auto it = q.find(k); return it == q.end() ? nullptr : const_cast<char *>(it->second.c_str());
  }
}; }
''' + grab(r'^static bool device_key_matches\(.*?^\}') + '\n' \
    + grab(r'^bool verify_device_key\(const OTF::Request &req\) \{.*?^\}') + '\n' \
    + grab(r'^struct OptionParams \{.*?^\};') + '\n' \
    + grab(r'^bool verify_device_key\(const OptionParams &params\) \{.*?^\}') + r'''
int main() {
  og.options[OPTION_DKEY].sval = "s3cret";
  for (bool cloud : {false, true}) {
    OTF::Request none{cloud, {}}, wrong{cloud, {{"dkey", "nope"}}}, empty{cloud, {{"dkey", ""}}},
                 right{cloud, {{"dkey", "s3cret"}}};
    assert(!verify_device_key(none) && !verify_device_key(wrong) && !verify_device_key(empty));
    assert(verify_device_key(right));
    // POST /co: only the body counts, on both paths.
    og_form::Form body_right, body_wrong, body_none;
    assert(body_right.parse("dkey=s3cret&name=x", 18) == og_form::Form::OK);
    assert(body_wrong.parse("dkey=nope", 9) == og_form::Form::OK);
    assert(body_none.parse("name=x", 6) == og_form::Form::OK);
    assert(verify_device_key(OptionParams{none, &body_right}));
    assert(!verify_device_key(OptionParams{right, &body_wrong}));
    assert(!verify_device_key(OptionParams{right, &body_none}));
    assert(verify_device_key(OptionParams{right, nullptr}) && !verify_device_key(OptionParams{wrong, nullptr}));
  }
  og.config_locked=true;assert(!device_key_matches("s3cret"));og.config_locked=false;
  // An empty stored key never matches a missing one.
  og.options[OPTION_DKEY].sval = "";
  OTF::Request none{true, {}};
  assert(!verify_device_key(none));
}
'''
with tempfile.TemporaryDirectory(prefix='og-device-key-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(code)
    subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-I', str(root), str(cpp),
                    '-o', str(Path(tmp) / 'test')], check=True)
    subprocess.run([str(Path(tmp) / 'test')], check=True)
# /ad: key required in station mode (Security+ 2.0 probing can move a Security+ 1.0 door), keyless
# in first-use setup (AP) mode only; afterwards the configured protocol is restarted (detection leaves the shared
# serial port to whichever protocol it tried last).
detect_code = r"""
#include <assert.h>
#include "OpenGarage/setup_window.h"
bool setup_room(uint32_t){return true;}
#include <string.h>
#include <string>
#include <map>
#include <vector>
struct String : std::string { using std::string::string; String(int v) : std::string(std::to_string(v)) {}
  const char *c_str() const { return std::string::c_str(); } };
String operator+(const char *a, const String &b) { String r(a); r.append(b); return r; }
String operator+(const String &a, const char *b) { String r = a; r.append(b); return r; }
struct Opt { String sval; int ival = 0; };
struct { bool config_locked=false; Opt options[3]; } og;
enum { OPTION_DKEY = 0, OPTION_SECV = 1, OPTION_KSET = 2 };
enum { OG_MOD_AP = 0xA9, OG_MOD_STA = 0x2A };
enum { HTML_UNAUTHORIZED = 2, DOOR_STATUS_UNKNOWN = 3, PIN_RELAY = 15, LOW = 0, OUTPUT = 1 };
int curr_mode = OG_MOD_STA, secplus_door_status = 0;
std::vector<std::string> calls;
namespace OTF { struct Request {
  std::map<std::string, std::string> q;
  bool isCloudRequest() const { return false; }
  char *getQueryParameter(const char *k) const { auto it = q.find(k); return it == q.end() ? nullptr : const_cast<char *>(it->second.c_str()); }
}; struct Response {}; }
struct Garage { const char *n; void stop() { calls.push_back(std::string(n) + ".stop"); }
  void begin() { calls.push_back(std::string(n) + ".begin"); } void reset_state() { calls.push_back(std::string(n) + ".reset"); }
  template<class F> void enable_callback(F) { calls.push_back(std::string(n) + ".callback"); } };
Garage secplus1_garage{"1"}, secplus2_garage{"2"};
void secplus1_state_callback() {} void secplus2_state_callback() {}
namespace SecPlusCommon { void delete_sw_serial() { calls.push_back("serial.delete"); } }
void digitalWrite(int pin, int v) { assert(pin == PIN_RELAY && v == LOW); calls.push_back("relay.low"); }
void pinMode(int pin, int m) { assert(pin == PIN_RELAY && m == OUTPUT); calls.push_back("relay.output"); }
int run_auto_detect() { calls.push_back("detect"); return 1; }
bool busy = false;
static bool auto_detect_busy() { return busy; }
enum { HTML_NOT_PERMITTED = 0x30 };
const char *result_item = "";
int result = -1; std::string json;
void otf_send_result(OTF::Response &, int code, const char *item) { result = code; result_item = item ? item : ""; }
void otf_send_json(OTF::Response &, const String &j) { json = j; }
""" + grab(r'^static bool device_key_matches\(.*?^\}') + '\n' \
    + grab(r'^bool verify_device_key\(const OTF::Request &req\) \{.*?^\}') + '\n' \
    + grab(r'^static void restart_configured_protocol\(\) \{.*?^\}') + '\n' \
    + grab(r'^void on_auto_detect\(const OTF::Request &req, OTF::Response &res\) \{.*?^\}') + r"""
static std::string run(int mode, int secv, const char *key) {
  curr_mode = mode; og.options[OPTION_SECV].ival = secv; calls.clear(); result = -1; json.clear();
  OTF::Request req; if (key) req.q["dkey"] = key;
  OTF::Response res; on_auto_detect(req, res);
  std::string out; for (auto &c : calls) out += c + " ";
  return out;
}
int main() {
  og.options[OPTION_DKEY].sval = "s3cret";
  og.config_locked=true;
  assert(run(OG_MOD_AP,2,nullptr).empty() && result==HTML_UNAUTHORIZED);
  assert(run(OG_MOD_STA,2,"s3cret").empty() && result==HTML_UNAUTHORIZED);
  og.config_locked=false;
  // Established AP units: reject before probing, including while busy.
  og.options[OPTION_KSET].ival=1;
  for(bool b:{false,true}) {
    busy=b;
    for(const char *key:{(const char*)nullptr,"","nope"})
      assert(run(OG_MOD_AP,2,key).empty()&&result==HTML_UNAUTHORIZED&&json.empty());
  }
  busy=false;
  assert(run(OG_MOD_AP,2,"s3cret").find("detect ")==0);
  og.options[OPTION_KSET].ival=0;
  // Station mode: no probing without the key.
  for (const char *key : {(const char *)nullptr, "", "nope"}) {
    assert(run(OG_MOD_STA, 2, key).empty() && result == HTML_UNAUTHORIZED && json.empty());
  }
  // Station mode with the key, and setup mode without one: detect, then restart the configured protocol.
  for (int mode : {int(OG_MOD_STA), int(OG_MOD_AP)}) {
    const char *key = mode == OG_MOD_STA ? "s3cret" : nullptr;
    assert(run(mode, 2, key) == "detect 2.stop 1.stop 2.begin 2.reset 2.callback " && json.find("1") != std::string::npos);
    assert(secplus_door_status == DOOR_STATUS_UNKNOWN);
    assert(run(mode, 1, key) == "detect 2.stop 1.stop 1.begin 1.reset 1.callback ");
    assert(run(mode, 0, key) == "detect 2.stop 1.stop serial.delete relay.low relay.output ");
    // Busy: authenticated (or setup-mode) requests never probe or restart either protocol.
    busy = true;
    for (int secv : {0, 1, 2}) {
      assert(run(mode, secv, key).empty() && result == HTML_NOT_PERMITTED && strcmp(result_item, "busy") == 0 && json.empty());
    }
    busy = false;
  }
  // Authentication is checked before the busy state: a keyless station request learns nothing.
  busy = true;
  assert(run(OG_MOD_STA, 2, nullptr).empty() && result == HTML_UNAUTHORIZED);
  busy = false;
  // Any mode other than setup (AP) requires the key.
  assert(run(0x55, 2, nullptr).empty() && result == HTML_UNAUTHORIZED);
}
"""
with tempfile.TemporaryDirectory(prefix='og-auto-detect-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(detect_code)
    subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-I', str(root), str(cpp),
                    '-o', str(Path(tmp) / 'test')], check=True)
    subprocess.run([str(Path(tmp) / 'test')], check=True)
html = (root / 'OpenGarage/html/sta_ui.html').read_text()
assert "fetch('ad?dkey=' + encodeURIComponent(saveDeviceKey(okey))" in html
assert re.search(r"btn_autodetect'\).addEventListener[\s\S]{0,120}if \(!okey\.value\)", html)
assert 'data.result === 2' in html and 'data.result === 48' in html
busy_fn = grab(r'^static bool auto_detect_busy\(\) \{.*?^\}')
for signal in ('network_delivery_busy()', 'firmware_update.busy()', 'OG_STATE_WAIT_RESTART',
               'DOOR_STATUS_OPENING', 'DOOR_STATUS_CLOSING'):
    assert signal in busy_fn, signal
assert 'og.alarm' in grab(r'^static bool network_delivery_busy\(\) \{.*?^\}')
print('PASS: device key required locally and over OTC, for GET queries and POST bodies; /ad keyed outside first-use setup, refused while busy, protocol restarted')
