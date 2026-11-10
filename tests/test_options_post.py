"""POST /co: the form body decodes exactly like OTF's URL query, carries long escaped values, and is the only source."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
og = root / 'OpenGarage'
source = (og / 'main.cpp').read_text()
html = (og / 'html/sta_ui.html').read_text()
ini = (og / 'platformio.ini').read_text()
local = re.search(r'^\s*symlink://(\S+)', ini, re.M)
otf = (og / local.group(1)).resolve() if local else og / '.pio/libdeps/d1_mini/OpenThings-Framework-Firmware-Library'
assert (otf / 'Request.cpp').exists(), f'OTF source not found at {otf}; build once with pio first'

# Firmware wiring: GET keeps the documented query API; POST reads only the body.
assert 'otf->on("/co", on_sta_change_options);' in source
assert 'otf->on("/co", on_sta_change_options_post, OTF::HTTP_POST);' in source
assert re.search(r'void on_sta_change_options\(.*?\{\s*sta_change_options_main\(OptionParams\{req, nullptr\}, res\);', source, re.S)
post = re.search(r'void on_sta_change_options_post\(.*?^\}', source, re.M | re.S).group()
assert 'form.parse(req.getBody(), req.getBodyLength())' in post and 'OptionParams{req, &form}' in post
handler = re.search(r'void sta_change_options_main\(const OptionParams &req,.*?^\}', source, re.M | re.S).group()
assert 'verify_device_key(req)' in handler
params = re.search(r'struct OptionParams \{.*?^\};', source, re.M | re.S).group()
assert 'form ? form->get(key) : request.getQueryParameter(key)' in params
# Page: the save is a POST of an encoded form body; no option save goes through a URL.
# (Through OTC the page keeps GET until the deployed proxy relays POST replies.)
assert "(viaOTC ? getJSON('co?' + comm) : postForm('co', comm))" in html
assert re.search(r"const viaOTC = /\\/forward\\/v1\\//\.test\(location\.pathname\);", html)
assert html.count("'co?") == 1
assert re.search(r"function bc\(n\) \{ comm \+= '&' \+ n \+ '=' \+ encodeURIComponent\(\$\(n\)\.value\); \}", html)

code = r'''
#include "Request.h"
#include "OpenThingsFramework.h"
#include "OpenGarage/form_params.h"
#include <assert.h>
#include <memory>
#include <random>
#include <stdio.h>
#include <string>
#include <vector>
using namespace OTF;
WebsocketClient *testSocket = nullptr;
FakeLocalClient *OTF::nextLocalClient = nullptr;
bool failCloudAllocation = false;
uint32_t millis() { static uint32_t now = 0; return ++now; }
namespace OTF { struct RequestTestAccess {
  static Request *make(char *str, size_t length) { return new Request(str, length, false); }
}; }

// Parse `query` both ways; they must agree on acceptance and on every key's value.
static void same(const std::string &query, const std::vector<std::string> &keys) {
  // "/co?" with nothing after it gives OTF one empty key; an empty body is checked separately.
  if (query.empty()) return;
  std::string raw = "GET /co?" + query + " HTTP/1.1\r\n\r\n";
  std::vector<char> buf(raw.begin(), raw.end()); buf.push_back(0);
  std::unique_ptr<Request> owned(RequestTestAccess::make(buf.data(), raw.size()));
  const Request &request = *owned;
  og_form::Form form;
  const bool ok = form.parse(query.data(), query.size()) == og_form::Form::OK;
  if ((request.getType() != INVALID) != ok) { fprintf(stderr, "acceptance differs: [%s]\n", query.c_str()); assert(false); }
  if (!ok) return;
  for (const std::string &k : keys) {
    const char *a = request.getQueryParameter(k.c_str()), *b = form.get(k.c_str());
    if ((a == nullptr) != (b == nullptr) || (a && strcmp(a, b))) {
      fprintf(stderr, "value differs for [%s] in [%s]: [%s] vs [%s]\n", k.c_str(), query.c_str(), a ? a : "(null)", b ? b : "(null)");
      assert(false);
    }
  }
}

static std::string recorded;
static void record(const Request &req, Response &res) {
  og_form::Form form;
  assert(form.parse(req.getBody(), req.getBodyLength()) == og_form::Form::OK);
  recorded.clear();
  for (const char *k : {"dkey", "name", "apwd", "mqpw", "smtp", "dvip", "only_in_url"}) {
    const char *v = form.get(k);
    recorded += std::string(k) + "=" + (v ? v : "(absent)") + "\n";
  }
  res.writeStatus(200, "OK");
  res.writeBodyData("{}", 2);
}
static std::string send(OpenThingsFramework &otf, const std::string &input) {
  FakeLocalClient client; client.input = input; nextLocalClient = &client;
  recorded = "(not handled)"; otf.loop(false); return client.output;
}
static std::string enc(const std::string &s) {   // encodeURIComponent
  static const char *ok = "-_.!~*'()";
  std::string out; char b[4];
  for (unsigned char c : s) {
    if (isalnum(c) || strchr(ok, c)) out += char(c);
    else { snprintf(b, sizeof b, "%%%02X", c); out += b; }
  }
  return out;
}

int main() {
  // Hand-picked edge cases, then random ones over the characters that matter.
  const std::vector<std::string> keys = {"a", "b", "", "a=b", "c", "%41", "A"};
  for (const char *q : {"a=1", "a=1&b=2", "a=1&a=2", "a", "a=", "=v", "a=1&&b=2", "a=1&", "&", "a=b=c",
                        "a=%41%42", "a=%4", "a=%", "a=%zz", "a=%00", "a=+x+", "%41=1", "a=%2B%26%3D",
                        "a=%C3%A9", "a=%e9", "c=1&a=%25&b"})
    same(q, keys);
  std::mt19937 rng(127);
  const char alphabet[] = "ab=&%+0Ab9z";
  for (int i = 0; i < 20000; ++i) {
    std::string q; const int n = rng() % 14;
    for (int j = 0; j < n; ++j) q += alphabet[rng() % (sizeof(alphabet) - 1)];
    same(q, keys);
  }
  { og_form::Form f; assert(f.parse("a\0b", 3) == og_form::Form::MALFORMED); }
  { std::string many; for (int i = 0; i < 128; ++i) many += "k" + std::to_string(i) + "=1&";
    og_form::Form f; assert(f.parse(many.data(), many.size() - 1) == og_form::Form::OK && f.size() == 128);
    assert(f.parse(many.data(), many.size()) == og_form::Form::TOO_MANY); }
  { og_form::Form f; assert(f.parse("", 0) == og_form::Form::OK && f.size() == 0 && !f.get("dkey")); }

  // Long, URL-escaped values through the real local server.
  OpenThingsFramework otf(0);
  otf.on("/co", record, HTTP_POST);
  std::string name; for (int i = 0; i < 32; ++i) name += "\xc3\xa9";   // 64 bytes of UTF-8 'é'
  const std::string apwd = "p@ss w0rd&=+%#?/\\\"'<>" + std::string(42, '/'),
                    mqpw = std::string(64, '&'), smtp = "smtp.example.com";
  std::string body = "dkey=" + enc("k&y=+%") + "&name=" + enc(name) + "&apwd=" + enc(apwd) + "&mqpw=" + enc(mqpw) +
                     "&smtp=" + enc(smtp) + "&dvip=192.168.1.20";
  for (int i = 0; i < 60; ++i) body += "&n" + std::to_string(i) + "=" + std::to_string(i * 1000);
  if (body.size() <= 1100) { fprintf(stderr, "body only %zu bytes\n", body.size()); return 1; }   // beyond the 1,014-byte request line a GET can carry
  std::string out = send(otf, "POST /co?only_in_url=1&dkey=wrong HTTP/1.1\r\nContent-Length: " +
                     std::to_string(body.size()) + "\r\n\r\n" + body);
  assert(out.rfind("HTTP/1.1 200", 0) == 0);
  const std::string expect = "dkey=k&y=+%\nname=" + name + "\napwd=" + apwd + "\nmqpw=" + mqpw + "\nsmtp=" + smtp +
                             "\ndvip=192.168.1.20\nonly_in_url=(absent)\n";
  if (recorded != expect) { printf("POST values:\n%s\nexpected:\n%s\n", recorded.c_str(), expect.c_str()); return 1; }
  // The same fields as a GET do not fit the request line: rejected, never truncated.
  out = send(otf, "GET /co?" + body + " HTTP/1.1\r\n\r\n");
  assert(out.rfind("HTTP/1.1 414", 0) == 0 && recorded == "(not handled)");
  printf("ok\n");
}
'''
with tempfile.TemporaryDirectory(prefix='og-options-post-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(code)
    binary = Path(tmp) / 'test'
    sources = [str(otf / f) for f in ('OpenThingsFramework.cpp', 'ForwardedRequest.cpp', 'Request.cpp',
                                      'Response.cpp', 'StringBuilder.cpp', 'HttpParser.cpp')]
    subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(otf), '-I', str(otf / 'test/fakes'), '-I', str(root),
                    '-include', str(otf / 'test/fakes/cloud_transport.h'),
                    str(cpp), *sources, '-o', str(binary)], check=True)
    out = subprocess.run([str(binary)], capture_output=True, text=True, env={'ASAN_OPTIONS': 'detect_leaks=1'})
    assert out.returncode == 0 and out.stdout.strip() == 'ok', out.stdout + out.stderr

# Page side: bc() encodes every value so arbitrary input round-trips through the form body.
bc = re.search(r'function bc\(n\) \{.*?\}', html).group()
js = bc + r'''
const values = {a: 'p@ss w0rd&=+%#?/\\"\'<>', b: 'éé \u{1F600}', c: '192.168.1.20', d: '', e: '&&==++%%'};
var comm = 'dkey=' + encodeURIComponent('k&y');
const $ = id => ({value: values[id]});
for (const k of Object.keys(values)) bc(k);
const parsed = new URLSearchParams(comm);
if (parsed.get('dkey') !== 'k&y') throw new Error('dkey');
for (const [k, v] of Object.entries(values)) if (parsed.get(k) !== v) throw new Error(k + ': ' + parsed.get(k));
if (/[^A-Za-z0-9\-_.!~*'()%&=]/.test(comm)) throw new Error('unencoded character in ' + comm);
'''
subprocess.run(['node', '-e', js], check=True)
print('PASS: POST /co form body matches OTF query decoding (20k cases), long escaped values, body-only source, GET 414 and page encoding')
