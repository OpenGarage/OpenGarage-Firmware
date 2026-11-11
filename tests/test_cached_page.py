"""Serve the station page through the real OTF local server; check raw 304/200 framing and ETag rules."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
og = root / 'OpenGarage'
source = (og / 'main.cpp').read_text()
ini = (og / 'platformio.ini').read_text()
buffer_flag = re.search(r'-DOTF_RESPONSE_BUFFER_SIZE=(\d+)', ini)
buffer_flags = ['-DOTF_RESPONSE_BUFFER_SIZE=' + buffer_flag.group(1)] if buffer_flag else []

# Use the OTF the build uses: a temporary local checkout, or the pinned copy PlatformIO fetched.
local = re.search(r'^\s*symlink://(\S+)', ini, re.M)
otf = (og / local.group(1)).resolve() if local else og / '.pio/libdeps/d1_mini/OpenThings-Framework-Firmware-Library'
assert (otf / 'OpenThingsFramework.cpp').exists(), f'OTF source not found at {otf}; build once with pio first'

# The page routes are GET-only, so a 304 never answers another method.
for path in ('/', '/vo', '/vl'):
    sta = source[source.index('otf->on("/", on_home, OTF::HTTP_GET);'):]
    assert re.search(r'otf->on\("' + re.escape(path) + r'", \w+, OTF::HTTP_GET\);', sta), path

sender = re.search(r'void otf_send_cached_page_P\(.*?^\}', source, re.M | re.S).group()
code = r'''
#include "OpenThingsFramework.h"
#include "OpenGarage/etag_match.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#define PGM_P const char *
using namespace OTF;
WebsocketClient *testSocket = nullptr;
FakeLocalClient *OTF::nextLocalClient = nullptr;
bool failCloudAllocation = false;
uint32_t millis() { static uint32_t now = 0; return ++now; }
static const char page[] = "GZIP-PAGE-BYTES";
''' + sender + r'''
static void send(const Request &req, Response &res) {
  otf_send_cached_page_P(req, res, F(page), sizeof(page) - 1, F("\"0123456789abcdef\""));
}
static std::string get(OpenThingsFramework &otf, const char *method, const char *inm) {
  FakeLocalClient client;
  client.input = std::string(method) + " / HTTP/1.1\r\nHost: og\r\n";
  if (inm) client.input += std::string("If-None-Match: ") + inm + "\r\n";
  client.input += "\r\n";
  nextLocalClient = &client;
  otf.loop(false);
  return client.output;
}
static bool m(const char *value) { return etag_list_matches(value, "\"0123456789abcdef\""); }
int main() {
  // ETag list rules.
  const char *hit[] = {"\"0123456789abcdef\"", "W/\"0123456789abcdef\"", "*", " * ",
    "\"aaaa\", \"0123456789abcdef\"", "\"0123456789abcdef\" , W/\"b\"", ",\"0123456789abcdef\",", "\t\"0123456789abcdef\"\t"};
  const char *miss[] = {"", " ", "\"0123456789abcdef\"junk", "0123456789abcdef", "\"0123456789abcde\"",
    "\"0123456789abcdef0\"", "\"0123456789abcdef", "w/\"0123456789abcdef\"", "*, \"0123456789abcdef\"",
    "\"0123456789abcdef\", junk", "\"a b\", \"0123456789abcdef\"", "\"a\x7f\", \"0123456789abcdef\"", "\"x\" \"0123456789abcdef\"", ","};
  for (const char *v : hit) if (!m(v)) { printf("expected match: [%s]\n", v); return 1; }
  for (const char *v : miss) if (m(v)) { printf("expected no match: [%s]\n", v); return 1; }

  OpenThingsFramework otf(0);
  otf.on("/", send, HTTP_GET);

  // A matching revalidation: complete header block, no body, no Content-Length.
  std::string r = get(otf, "GET", "\"0123456789abcdef\"");
  assert(r.rfind("HTTP/1.1 304 Not Modified\r\n", 0) == 0);
  assert(r.size() >= 4 && r.compare(r.size() - 4, 4, "\r\n\r\n") == 0);
  assert(r.find("\r\n\r\n") == r.size() - 4);
  assert(r.find("ETag: \"0123456789abcdef\"\r\n") != std::string::npos);
  assert(r.find("Cache-Control: private, max-age=300\r\n") != std::string::npos);
  assert(r.find("Content-Length") == std::string::npos && r.find("Content-Encoding") == std::string::npos);
  assert(r.find(page) == std::string::npos);
  assert(get(otf, "GET", "*").rfind("HTTP/1.1 304", 0) == 0);

  // No header, stale or malformed tags: the full page with its framing.
  for (const char *inm : {(const char *)nullptr, "\"0000000000000000\"", "\"0123456789abcdef\"junk"}) {
    r = get(otf, "GET", inm);
    assert(r.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
    const size_t end = r.find("\r\n\r\n");
    assert(end != std::string::npos && r.substr(end + 4) == page);
    assert(r.find("Content-Length: " + std::to_string(sizeof(page) - 1) + "\r\n") < end);
    assert(r.find("Content-Encoding: gzip\r\n") < end);
  }

  // Other methods never reach the page handler, so they never get a 304.
  r = get(otf, "POST", "\"0123456789abcdef\"");
  assert(r.find("304") == std::string::npos && r.find(page) == std::string::npos);
  printf("ok\n");
}
'''
with tempfile.TemporaryDirectory(prefix='og-cached-page-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(code)
    binary = Path(tmp) / 'test'
    sources = [str(otf / f) for f in ('OpenThingsFramework.cpp', 'ForwardedRequest.cpp', 'Request.cpp',
                                      'Response.cpp', 'StringBuilder.cpp', 'HttpParser.cpp')]
    subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(otf), '-I', str(otf / 'test/fakes'), '-I', str(root),
                    *buffer_flags, '-DARDUINO=10800', '-DESP8266', '-include', str(otf / 'test/fakes/cloud_transport.h'),
                    str(cpp), *sources, '-o', str(binary)], check=True)
    out = subprocess.run([str(binary)], capture_output=True, text=True, env={'ASAN_OPTIONS': 'detect_leaks=1'})
    assert out.returncode == 0 and out.stdout.strip() == 'ok', out.stdout + out.stderr
print('PASS: cached page 304 framing, full 200 fallback, ETag list rules and GET-only routes')
