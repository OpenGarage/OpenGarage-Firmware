"""Execute AP completion: a failed save cannot acknowledge setup or reboot."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'OpenGarage/main.cpp').read_text()
body=s[s.index('static bool setup_save_failed='):s.index('static void send_sta_ui')]
code=r'''
#include <cassert>
#include "OpenGarage/defines.h"
struct Option {unsigned ival=0;};
struct {Option options[NUM_OPTIONS];bool saves=true;unsigned calls=0;
 bool options_save(){++calls;return saves;}
} og;
bool setup_reboot_pending=false;
namespace og_audio {enum class Tune {SETUP_SUCCESS};}
unsigned tunes=0;
void start_tune(og_audio::Tune){++tunes;}
'''+body+r'''
int main(){
 og.options[OPTION_MOD].ival=OG_MOD_AP;og.saves=false;
 assert(!complete_ap_setup());assert(!setup_reboot_pending&&tunes==0);
 assert(og.options[OPTION_MOD].ival==OG_MOD_AP&&og.calls==1);
 assert(!complete_ap_setup()&&og.calls==1); // no retry every loop tick
 setup_save_failed=false;og.saves=true; // a new configuration submission permits retry
 assert(complete_ap_setup()&&setup_reboot_pending&&tunes==1);
 assert(og.options[OPTION_MOD].ival==OG_MOD_STA&&og.calls==2);
 assert(complete_ap_setup()&&og.calls==2&&tunes==1);
}
'''
# Wiring checks complement the executed save/rollback and completion tests.
for name in ['sta_change_options_main','on_ap_change_config','on_ap_try_connect']:
    fn=re.search(r'void '+name+r'\(.*?^}',s,re.M|re.S).group()
    assert 'HTML_UPLOAD_FAILED, "save"' in fn
    if name=='on_ap_change_config':
        assert fn.count('options_save()')==1
        assert fn.index('options_save()')<fn.index('HTML_SUCCESS')<fn.index('OG_STATE_TRY_CONNECT')
    if name=='on_ap_try_connect':
        assert fn.index('!complete_ap_setup()')<fn.index('String json')
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),'-x','c++','-','-o',str(p)],input=code,text=True,check=True)
    subprocess.run([str(p)],check=True)
print('PASS: setup save failure, retry suppression, successful retry, response wiring')
