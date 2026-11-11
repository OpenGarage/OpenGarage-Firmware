"""Run the actual OG-only SMTP admission guard against unsupported library modes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'OpenGarage/EMailSender.cpp').read_text()
a=s.index('  network_attempted_=false;');b=s.index('  const uint32_t smtpStarted=',a)
guard=s[a:b]
code=r'''
#include <cassert>
#include <string>
#define F(x) x
#define OG_SMTP_BOUNDED_ONLY
struct Response {bool status=false;std::string code,desc;};
bool network_attempted_=true, useAuth=true,isSASLLogin=false,isCramMD5Login=false;
struct {std::string mime="text/plain";} email;
Response check(unsigned sizeOfTo,unsigned sizeOfCc,unsigned sizeOfCCn) {
'''+guard+r'''
 Response ok;ok.status=true;return ok;
}
int main() {
 assert(check(1,0,0).status && !network_attempted_);
 assert(!check(0,0,0).status);assert(!check(2,0,0).status);
 assert(!check(1,1,0).status);assert(!check(1,0,1).status);
 useAuth=false;assert(!check(1,0,0).status);useAuth=true;
 isSASLLogin=true;assert(!check(1,0,0).status);isSASLLogin=false;
 isCramMD5Login=true;assert(!check(1,0,0).status);isCramMD5Login=false;
 email.mime="text/html";assert(!check(1,0,0).status);
 assert(!network_attempted_);
}
'''
with tempfile.TemporaryDirectory(prefix='og-smtp-policy-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: OG SMTP policy accepts its production mode and refuses legacy modes before network work')
