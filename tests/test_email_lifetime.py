"""Exercise actual sender ownership code, including failed copies, under ASan/LSan."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
h = (root / 'OpenGarage/EMailSender.h').read_text()
s = (root / 'OpenGarage/EMailSender.cpp').read_text()
a = h.index('public:', h.index('class EMailSender')) + len('public:')
b = h.index('#define STORAGE_SPIFFS', a)
decls = h[a:b]
for name in ('setSMTPPort', 'setSMTPServer', 'setEMailLogin', 'setEMailFrom', 'setNameFrom', 'setEMailPassword'):
    decls += re.search(r'void ' + name + r'\([^;{]+\);', h).group(0) + '\n'
a = h.index('bool network_attempted_ = false;')
b = h.index('const char* publicIPDescriptor', a)
fields = h[a:b]
a = s.index('EMailSender::EMailSender(')
b = s.index('void EMailSender::setIsSecure', a)
defs = s[a:b]
program = r'''
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <cassert>
static int copyBudget = -1;
static char *controlled_strdup(const char *s) {
    if (copyBudget == 0) return nullptr;
    if (copyBudget > 0) --copyBudget;
    return ::strdup(s);
}
#define strdup controlled_strdup
class EMailSender { public:
''' + decls + ' private:\n' + fields + '};\n' + defs + r'''
static_assert(!std::is_copy_constructible<EMailSender>::value, "owning strings must not alias");
static_assert(!std::is_move_constructible<EMailSender>::value, "no implicit shallow move");
int main() {
    for (int i = 0; i < 30; ++i) {
        EMailSender a("sender@example.test", "dummy-password");
        assert(a.readyToSend());
        a.setSMTPServer("mail.example.test");
        a.setSMTPServer("smtp.example.test");
        a.setEMailLogin("another@example.test");
        a.setEMailFrom("sender@example.test");
        a.setNameFrom("Bench");
        a.setEMailPassword("replacement");
        assert(a.readyToSend());
        EMailSender b("u", "p", "f"), c("u", "p", "f", "n");
        EMailSender d("u", "p", "f", "s", 465), e("u", "p", "f", "n", "s", 465);
        assert(b.readyToSend() && c.readyToSend() && d.readyToSend() && e.readyToSend());
    }
    // Default hostname + login/from/password + replacement hostname: five copies.
    for (int budget = 0; budget <= 5; ++budget) {
        copyBudget = budget;
        EMailSender a("sender@example.test", "dummy-password");
        a.setSMTPServer("mail.example.test");
        assert(a.readyToSend() == (budget == 5));
    }
    copyBudget = -1;
    { EMailSender a("u", "p"); copyBudget = 0; a.setNameFrom("Bench"); assert(!a.readyToSend()); }
    copyBudget = -1;
    { EMailSender a(nullptr, "p"); assert(!a.readyToSend()); }
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d)
    (p / 't.cpp').write_text(program)
    subprocess.run(['g++', '-std=c++11', '-g', '-fsanitize=address', '-fno-omit-frame-pointer',
                    str(p / 't.cpp'), '-o', str(p / 't')], check=True)
    subprocess.run([str(p / 't')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:alloc_dealloc_mismatch=1'})
a = s.index('EMailSender::Response EMailSender::send(const char* to[], byte sizeOfTo,  byte sizeOfCc,byte sizeOfCCn,')
assert s.index('if (!readyToSend())', a) < s.index('EMAIL_NETWORK_CLASS client;', a)
print('EMailSender lifetime, failed-copy injection and pre-TLS guard: ASan/LSan PASS')
