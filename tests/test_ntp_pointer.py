"""lwIP keeps the NTP server name pointers passed to configTime: they must outlive an options save."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/main.cpp').read_text()
time_keeping = re.search(r'^void time_keeping\(\) \{.*?^\}', source, re.M | re.S).group()

code = r'''
#include <assert.h>
#include <string.h>
#include <string>
#include <time.h>
#include "OpenGarage/local_time.h"
struct String : std::string { using std::string::string; using std::string::operator=; };
bool valid_url(const String &s) { return s.length() > 0 && s[0] != '-' && s.find('.') != std::string::npos && s.find('.') > 0; }
#define DEFAULT_NTP1 "time.google.com"
#define DEFAULT_NTP2 "time.cloudflare.com"
#define DEFAULT_NTP3 "time.windows.com"
enum { OPTION_NTP1 = 0 };
struct { struct { String sval; } options[1]; } og;
enum { WL_CONNECTED = 3 };
struct { int status() { return WL_CONNECTED; } bool localIP() { return true; } } WiFi;
static const char *servers[3];
static int configured_calls = 0;
void configTime(int, int, const char *a, const char *b, const char *c) { servers[0] = a; servers[1] = b; servers[2] = c; ++configured_calls; }
uint32_t millis() { return 1000; }
og_local::Clock local_clock;
uint32_t curr_utc_time = 0, curr_utc_hour = 0, start_utc_time = 0;
''' + time_keeping + r'''
static void boot(const char *ntp1) {
  og.options[OPTION_NTP1].sval = ntp1;
  time_keeping();
}
int main(int argc, char **argv) {
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "configured") {
    boot("ntp.lan");
    assert(configured_calls == 1 && strcmp(servers[0], "ntp.lan") == 0 && strcmp(servers[1], DEFAULT_NTP1) == 0);
    // An options save replaces the String; its old buffer is freed (ASan reports any later read).
    og.options[OPTION_NTP1].sval = String(200, 'x');
    og.options[OPTION_NTP1].sval = "y";
    assert(strcmp(servers[0], "ntp.lan") == 0);   // lwIP's next poll still reads the configured name
  } else if (mode == "longest") {
    std::string host(60, 'h'); host += ".lan";   // 64 bytes: the /co limit
    boot(host.c_str());
    assert(strcmp(servers[0], host.c_str()) == 0);
  } else if (mode == "too-long") {
    std::string host(61, 'h'); host += ".lan";   // 65 bytes: never shortened, defaults instead
    boot(host.c_str());
    assert(strcmp(servers[0], DEFAULT_NTP1) == 0 && strcmp(servers[2], DEFAULT_NTP3) == 0);
  } else if (mode == "none") {
    boot("");
    assert(strcmp(servers[0], DEFAULT_NTP1) == 0 && strcmp(servers[1], DEFAULT_NTP2) == 0);
  }
  time_keeping();                                 // configured once per boot
  assert(configured_calls == 1);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='og-ntp-pointer-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(code)
    binary = Path(tmp) / 'test'
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root), str(cpp), '-o', str(binary)], check=True)
    for mode in ('configured', 'longest', 'too-long', 'none'):
        result = subprocess.run([str(binary), mode], capture_output=True, text=True)
        assert result.returncode == 0, mode + ': ' + result.stderr[-600:]
print('PASS: configTime receives a stable ntp1 copy (survives option reallocation), 64-byte names whole, longer ignored')
