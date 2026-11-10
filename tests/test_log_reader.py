"""Compile the real log-reader opener and verify descriptor ownership on errors."""
from pathlib import Path
import re
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/OpenGarage.cpp').read_text()
body = re.search(r'bool OpenGarage::read_log_start\(\) \{.*?\n\}', source, re.S).group()
code = r'''
#include <cassert>
#include <cstring>
using uint = unsigned int;
constexpr uint MAX_LOG_SIZE = 500;
int active = 0;
bool can_open = true;
size_t bytes = sizeof(uint);
uint index_value = 0;
struct File {
 bool opened = false;
 explicit operator bool() const { return opened; }
 void close() { if (opened) { opened=false; --active; } }
 size_t readBytes(char *dst, size_t n) { assert(opened); memcpy(dst,&index_value,n); return bytes; }
};
struct Fs {
 File open(const char*,const char*) { if (!can_open) return {}; ++active; return {true}; }
} FILESYS;
const char *log_fname = "/logs.dat";
struct OpenGarage { static File log_file; static bool read_log_start(); };
File OpenGarage::log_file;
''' + body + r'''
int main() {
 for (size_t n=0;n<sizeof(uint);++n) {
  bytes=n;assert(!OpenGarage::read_log_start());assert(active==0);
 }
 bytes=sizeof(uint);
 for (uint index : {MAX_LOG_SIZE,~uint(0)}) {
  index_value=index;assert(!OpenGarage::read_log_start());assert(active==0);
 }
 index_value=MAX_LOG_SIZE-1;
 assert(OpenGarage::read_log_start() && active==1);
 // A new read closes an existing reader before opening its replacement.
 assert(OpenGarage::read_log_start() && active==1);
 can_open=false;
 assert(!OpenGarage::read_log_start() && active==0);
}
'''
code = '#include <initializer_list>\n' + code
with tempfile.TemporaryDirectory(prefix='og-log-reader-') as tmp:
    cpp=Path(tmp)/'test.cpp'; binary=Path(tmp)/'test'
    cpp.write_text(code)
    subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: short/corrupt logs close the reader; valid logs retain it; reopening replaces it')
