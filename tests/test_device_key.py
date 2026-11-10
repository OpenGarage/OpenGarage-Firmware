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
#include <string.h>
#include <string>
#include <map>
#include "OpenGarage/form_params.h"
struct String : std::string { using std::string::string; const char *c_str() const { return std::string::c_str(); } };
struct { struct { String sval; } options[1]; } og;
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
print('PASS: device key required locally and over OTC, for GET queries and POST bodies')
