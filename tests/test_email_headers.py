from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'OpenGarage/main.cpp').read_text();a=s.index('static void __attribute__((noinline)) prepare_email_headers');b=s.index('\nstatic og_smtp::Delivery record_email_result',a);helper=s[a:b]
preamble=r'''
#include <string>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cassert>
#define PROGMEM
#define memcpy_P memcpy
#define snprintf_P snprintf
#define PSTR(x) x
struct EMailSender {struct EMailMessage {std::string date,messageId;};};
struct {bool valid=false;} local_clock;
unsigned long curr_utc_time=1794146706UL;
struct {unsigned random(){return 42;}unsigned getChipId(){return 0x123456;}} ESP;
'''
# Derive epoch from UTC, independent of the test machine's timezone.
import calendar
stamp=calendar.timegm((2026,11,8,14,5,6))
preamble=preamble.replace('1794146706UL',str(stamp)+'UL')
program=preamble+helper+r'''
int main(){EMailSender::EMailMessage first,second,third;
 prepare_email_headers(first);assert(first.date.empty());assert(!first.messageId.empty());
 local_clock.valid=true;prepare_email_headers(second);
 assert(second.date=="Sun, 08 Nov 2026 14:05:06 +0000");
 local_clock.valid=false;prepare_email_headers(third);assert(third.date.empty());
 assert(first.messageId!=second.messageId&&second.messageId!=third.messageId);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(program)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('Email headers: UTC date, unsynced clock and unique Message-ID: PASS')
