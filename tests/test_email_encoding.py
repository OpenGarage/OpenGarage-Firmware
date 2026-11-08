"""Check the production credential encoder at its output bound, under ASan/UBSan."""
from pathlib import Path
import base64
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/EMailSender.cpp').read_text()
a = source.index('const char PROGMEM b64_alphabet[]')
b = source.index('// END BASE64', a)
encoder = source[a:b]
vectors = [bytes((i * 17) % 256 for i in range(n)) for n in (0, 1, 2, 3, 63, 64, 188, 189)]
checks = []
for data in vectors:
    literal = ''.join('\\x%02x' % c for c in data)
    expected = base64.b64encode(data).decode()
    checks.append('assert(std::string(encode64_f((char*)"%s", %d)) == "%s");' % (literal, len(data), expected))
program = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#define PROGMEM
#define pgm_read_byte(p) (*(p))
#define EMAIL_DEBUG_PRINTLN(...) ((void)0)
''' + encoder + '\nint main(){\n' + '\n'.join(checks) + r'''
    for (size_t n : {190U, 255U, 256U, 257U, 512U}) {
        std::string value(n, 'a');
        assert(encode64_f(&value[0], value.size())[0] == '\0');
        // Exercise the strlen macro too: no uint8_t narrowing before the guard.
        assert(encode64(&value[0])[0] == '\0');
        assert(value == std::string(n, 'a'));
    }
    // Guard before overflow-prone arithmetic and before reading input bytes.
    char tiny[] = "x";
    assert(encode64_f(tiny, std::numeric_limits<size_t>::max())[0] == '\0');
    assert(encode64_f(nullptr, 1)[0] == '\0');
    assert(encode64_f(nullptr, 0)[0] == '\0');
    // A refusal cannot leave an earlier credential in the shared result buffer.
    char valid[] = "abc";
    assert(std::string(encode64(valid)) == "YWJj");
    assert(encode64_f(tiny, 256)[0] == '\0');
    assert(std::string(encode64(valid)) == "YWJj");
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d)
    (p / 'test.cpp').write_text(program)
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
print('Credential Base64: reference vectors, 189/190/255/256-byte bounds and overflow guard: PASS')
