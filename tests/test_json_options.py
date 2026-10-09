"""Compile the production options JSON builder and check string round trips."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/main.cpp').read_text()
match = re.search(r'void sta_options_fill_json\([^\n]+\) \{.*?^\}',
                  source, re.S | re.M)
assert match
stub = r'''
#include <string>
#include <iostream>
#include "json_escape.h"
#define F(x) x
#define HEX 16
#define STRING_RESERVE_SIZE 256
using byte = unsigned char;
struct String : std::string {
  using std::string::string;
  using std::string::operator=;
  template<class T> String& operator+=(T n) { append(std::to_string(n)); return *this; }
  String& operator+=(char c) { push_back(c); return *this; }
  String& operator+=(const char *s) { append(s); return *this; }
  String& operator+=(const String &s) { append(s); return *this; }
  String(unsigned,int) { assign("12342908"); }
};
struct OptionStruct { String name; unsigned ival; unsigned max; String sval; };
enum { OPTION_NAME, OPTION_MQTP, OPTION_HOST, OPTION_SSID,
       OPTION_PASS, OPTION_DKEY, OPTION_MQPW, OPTION_ALM, NUM_OPTIONS };
struct { OptionStruct options[NUM_OPTIONS]; unsigned has_swrx=1; } og;
bool secplus2_identity_ready=true;
unsigned secplus2_client_id=0x12342908;
String get_ap_ssid() { return "OpenGarage-123456"; }
'''
runner = r'''
int main() {
  og.options[OPTION_NAME] = {"name",0,0,"Garage \"East\"\\row"};
  og.options[OPTION_MQTP] = {"mqtp",0,0,""};
  og.options[OPTION_HOST] = {"host",0,0,""};
  og.options[OPTION_SSID] = {"ssid",0,0,"Cafe \"WiFi\"\\\tMálaga"};
  og.options[OPTION_PASS] = {"pass",0,0,"wifi-secret"};
  og.options[OPTION_DKEY] = {"dkey",0,0,"device-secret"};
  og.options[OPTION_MQPW] = {"mqpw",0,0,"mqtt-secret"};
  og.options[OPTION_ALM] = {"alm",5,60,""};
  String output;
  sta_options_fill_json(output);
  std::cout << output << '\n';
  og.options[OPTION_MQTP].sval = "garage/\"status\"\\topic";
  og.options[OPTION_HOST].sval = "host\\name\"";
  sta_options_fill_json(output);
  std::cout << output << '\n';
  for (unsigned c=0;c<32;++c) {
    std::string value(1,static_cast<char>(c));
    std::string escaped="\"";
    append_json_escaped(escaped,value);
    escaped+='"';
    std::cout << escaped << '\n';
  }
  std::string value="plain / text \"quoted\" \\ Málaga";
  std::string escaped="\"";
  append_json_escaped(escaped,value);
  escaped+='"';
  std::cout << escaped << '\n';
}
'''
scratch_root = root / '.local'
scratch_root.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='og-json-test-', dir=scratch_root) as scratch:
    binary = str(Path(scratch) / 'test')
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(root / 'OpenGarage'), '-x', 'c++', '-', '-o', binary],
                   input=stub + match.group() + runner, text=True, check=True)
    results = [json.loads(line) for line in
               subprocess.check_output([binary], text=True).splitlines()]

assert len(results) == 35
for data in results[:2]:
    assert data['name'] == 'Garage "East"\\row'
    assert data['ssid'] == 'Cafe "WiFi"\\\tMálaga'
    assert data['alm'] == 5
    assert data['has_swrx'] == 1
    assert data['s2id'] == '0x12342908'
    assert not {'pass', 'dkey', 'mqpw'} & set(data)
assert results[0]['mqtp'] == results[0]['name']
assert results[0]['host'] == 'OpenGarage-123456'
assert results[1]['mqtp'] == 'garage/"status"\\topic'
assert results[1]['host'] == 'host\\name"'
assert results[2:34] == [chr(c) for c in range(32)]
assert results[34] == 'plain / text "quoted" \\ Málaga'
print('Options defaults, explicit values and all JSON control characters round-trip')
