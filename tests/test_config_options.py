"""Execute real option migration/save/load against a fault-injected file system."""
from pathlib import Path
import re,subprocess,tempfile,json
root=Path(__file__).resolve().parents[1]
s=(root/'OpenGarage/OpenGarage.cpp').read_text()
names=re.findall(r'^\s*(OPTION_\w+)',(root/'OpenGarage/defines.h').read_text(),re.M)
fs=(root/'tests/test_config_store.cpp').read_text().split('bool legacy(')[0]
fs=fs.replace('  int read(){','  bool available(){return pos<size();}\n  int read(){')
start=s[s.index('enum class ConfigRecord'):s.index('void OpenGarage::log_reset()')]
find=s[s.index('int OpenGarage::find_option('):s.index('bool OpenGarage::options_load()')]
load=s[s.index('bool OpenGarage::options_load()'):s.index('uint OpenGarage::read_distance()')]
rows=re.findall(r'\{"([a-z0-9]+)",\s*([^,]+),\s*([^,]+),\s*([^}]+)\}',s[s.index('OptionStruct OpenGarage::options[]'):s.index('/* Variables')])
assert len(rows)==len(names)
# Check the independent default tables against the real initialization table.
for table,column in [('config_int_defaults',1),('config_string_defaults',3)]:
    values=re.search(table+r'\[\] PROGMEM = \{(.*?)\};',s,re.S).group(1)
    assert [v.strip() for v in values.split(',')]==[r[column].strip() for r in rows]
historical=''
for fixture in sorted((root/'tests/fixtures').glob('config-*.txt')):
    historical+='init();FILESYS.disk[og_config::current]='+json.dumps(fixture.read_text())+';OpenGarage::options_setup();assert(!OpenGarage::config_locked);assert(OpenGarage::options[OPTION_DKEY].sval==\"legacy-owner-key\");\n'

setup='\n'.join(f'OpenGarage::options[{i}].name="{name}";OpenGarage::options[{i}].max={int(mx.strip()) if mx.strip().isdigit() else 255};' for i,(name,iv,mx,sv) in enumerate(rows))
code=fs+r'''
#include <cstdlib>
#define PROGMEM
#define pgm_read_word(p) (*(p))
#define pgm_read_ptr(p) (*(p))
#include <algorithm>
using byte=unsigned char;
using File=FS::File;
FS FILESYS;
#include "OpenGarage/defines.h"
#undef FILESYS
bool fail_strings=false;
struct String:std::string {
 using std::string::string;using std::string::operator=;
 bool concat(char c){if(fail_strings)return false;push_back(c);return true;}
 bool reserve(size_t n){if(fail_strings)return false;std::string::reserve(n);return true;}
 int indexOf(char c)const{auto n=find(c);return n==npos?-1:int(n);}
 long toInt()const{return strtol(c_str(),nullptr,10);}
 void trim(){auto a=find_first_not_of(" \t\r\n");if(a==npos){clear();return;}erase(0,a);erase(find_last_not_of(" \t\r\n")+1);}
};
'''+r'''
struct Option {const char *name;unsigned ival=0,max=0;String sval;};
class OpenGarage {public:
 static Option options[NUM_OPTIONS];static uint8_t config_status;static bool config_locked;
 static void options_setup();static bool options_load(),options_save(),options_reset();static int find_option(String name);
 static void reset_alarm(){}
};
Option OpenGarage::options[NUM_OPTIONS];uint8_t OpenGarage::config_status=0;bool OpenGarage::config_locked=false;
'''+start+find+load+r'''
void init(){
 FILESYS=FS{};fail_strings=false;OpenGarage::config_locked=false;OpenGarage::config_status=0;
 for(auto &o:OpenGarage::options){o.ival=0;o.sval="";}
 SETUP
 OpenGarage::options[OPTION_FWV].ival=127;OpenGarage::options[OPTION_DKEY].sval="opendoor";
}
std::string legacyFile(unsigned version){
 unsigned last=version>=126?OPTION_WFR:version==125?OPTION_PEM:OPTION_HOST;
 std::string out;
 for(unsigned i=0;i<=last;++i){auto&o=OpenGarage::options[i];out+=o.name;out+=':';out+=i==OPTION_FWV?std::to_string(version):o.max?std::to_string(o.ival):std::string(o.sval);out+="\r\n";}
 return out;
}
int main(){
 HISTORICAL
 for(const char *tail:{"name:torn","name:","n","malformed trailing line"}){
  init();FILESYS.disk[og_config::current]=std::string("fwv:126\nmod:42\nssid:old-wifi\ndkey:kept-key\n")+tail;
  OpenGarage::options_setup();assert(!OpenGarage::config_locked);
  assert(OpenGarage::options[OPTION_DKEY].sval=="kept-key");
  assert(OpenGarage::options[OPTION_MOD].ival==OG_MOD_STA);
  assert(OpenGarage::options[OPTION_SSID].sval=="old-wifi");
  assert(OpenGarage::options[OPTION_NAME].sval==DEFAULT_NAME);
 }
 for(const char *contents:{
   "fwv:121\nmod:42\nssid:old-wifi\ndkey:kept-key\n",
   "fwv:200\nmod:42\nssid:old-wifi\ndkey:kept-key\n",
   "fwv:120\nmod:42\nssid:old-wifi\nname:line one\nline two without colon\nriv:300\ndkey:kept-key\n",
   "fwv:122\nmod:42\nssid:old-wifi\ncdt:-1\nriv:99999999999999999999999999999\ndkey:kept-key\n"
 }) {
   init();FILESYS.disk[og_config::current]=contents;OpenGarage::options_setup();
   assert(!OpenGarage::config_locked);assert(OpenGarage::options[OPTION_DKEY].sval=="kept-key");
   assert(OpenGarage::options[OPTION_MOD].ival==OG_MOD_STA);assert(OpenGarage::options[OPTION_SSID].sval=="old-wifi");
   assert(OpenGarage::options[OPTION_RIV].ival==1);assert(OpenGarage::options[OPTION_CDT].ival==1000);
 }
 for(const std::string &contents:std::initializer_list<std::string>{
   "fwv:126\nname:no key\n", "fwv:126\ndkey:   \n",
   "fwv:126\ndkey:a\ndkey:b\n", "fwv:126\ndkey:a\ndkey\n",
   "fwv:126\ndkey:a", "fwv:126\ndkey:a\ndkey:b", "name:wrong first\nfwv:126\ndkey:a\n",
   "fwv:126\ndkey:a\nogcfg:2\n", "ogcfg:2\nfwv:126\ndkey:a\n",
   std::string("fwv:126\ndkey:")+std::string(2100,'a')+"\n"
 }) {
   init();FILESYS.disk[og_config::current]=contents;OpenGarage::options_setup();
   assert(OpenGarage::config_locked);assert(FILESYS.disk[og_config::current]==contents);
 }
 init();FILESYS.disk[og_config::current]="fwv:126\n"+std::string(128,'x')+"dkey:fake\ndkey:real-key\n";
 OpenGarage::options_setup();assert(!OpenGarage::config_locked);assert(OpenGarage::options[OPTION_DKEY].sval=="real-key");
 init();assert(og_config::save(FILESYS,valid_legacy_config,[](auto &out){
   return out.text("fwv:200\ndkey:future-key\nmod:42\nssid:future-wifi\nfutureflag:yes\n");
 }));OpenGarage::options_setup();assert(!OpenGarage::config_locked);
 assert(OpenGarage::options[OPTION_DKEY].sval=="future-key");assert(OpenGarage::options[OPTION_MOD].ival==OG_MOD_STA);
 init();FILESYS.short_write=0;OpenGarage::options_setup();
 assert(OpenGarage::config_locked&&OpenGarage::config_status==uint8_t(og_config::Status::SAVE_FAILED));
 for(unsigned version:{124,125,126,127}){
  init();OpenGarage::options[OPTION_DKEY].sval="old-custom-key";
  FILESYS.disk[og_config::current]=legacyFile(version);OpenGarage::options[OPTION_DKEY].sval="opendoor";
  OpenGarage::options_setup();assert(!OpenGarage::config_locked);assert(OpenGarage::options[OPTION_DKEY].sval=="old-custom-key");
  assert(OpenGarage::config_status==uint8_t(og_config::Status::MIGRATED));
  assert(og_config::inspect(FILESYS,og_config::current,valid_legacy_config)==og_config::Kind::VERIFIED);
 }
 init();FILESYS.disk[og_config::current]=legacyFile(124);
 OpenGarage::options[OPTION_WFG].ival=1;FILESYS.short_write=20;
 assert(!OpenGarage::options_save());assert(OpenGarage::options[OPTION_WFG].ival==0);
 assert(OpenGarage::options[OPTION_WFR].ival==1);assert(!OpenGarage::config_locked);
 init();OpenGarage::options_setup();assert(!OpenGarage::config_locked);
 auto original=FILESYS.disk[og_config::current];
 OpenGarage::options[OPTION_DKEY].sval="new-secret";FILESYS.short_write=20;
 assert(!OpenGarage::options_save());assert(OpenGarage::options[OPTION_DKEY].sval=="opendoor");assert(FILESYS.disk[og_config::current]==original);assert(!OpenGarage::config_locked);
 FILESYS.short_write=-1;OpenGarage::options[OPTION_DKEY].sval="new-secret";assert(OpenGarage::options_save());
 OpenGarage::options[OPTION_DKEY].sval="wrong";assert(OpenGarage::options_load());assert(OpenGarage::options[OPTION_DKEY].sval=="new-secret");
 OpenGarage::options[OPTION_NAME].sval="bad\nline";assert(!OpenGarage::options_save());assert(OpenGarage::options[OPTION_NAME].sval.empty());
 FILESYS.rename_fail=true;OpenGarage::options[OPTION_DKEY].sval="durable-pending";assert(OpenGarage::options_save());
 assert(FILESYS.exists(og_config::pending));OpenGarage::options[OPTION_DKEY].sval="wrong";assert(OpenGarage::options_load());assert(OpenGarage::options[OPTION_DKEY].sval=="durable-pending");
 FILESYS.rename_fail=false;OpenGarage::options_setup();assert(!OpenGarage::config_locked&&OpenGarage::config_status==uint8_t(og_config::Status::RECOVERED));
 assert(OpenGarage::options_reset());assert(FILESYS.disk.empty());
 init();auto full=legacyFile(126);auto at=full.find("dkey:");full.erase(at,full.find('\n',at)-at+1);FILESYS.disk[og_config::current]=full;OpenGarage::options_setup();assert(OpenGarage::config_locked);
 init();FILESYS.disk[og_config::current]="ogcfg:1\nfwv:127\r\ndkey:opendoor\r\n";OpenGarage::options_setup();assert(OpenGarage::config_locked&&OpenGarage::config_status==uint8_t(og_config::Status::CORRUPT));
 init();OpenGarage::options_setup();fail_strings=true;assert(!OpenGarage::options_load());
 std::cout<<"actual options: released 120/122/123 and 124-127 migration, failed key rollback, reload, durable pending, reset, malformed records and OOM PASS\n";
}
'''.replace('SETUP',setup).replace('HISTORICAL',historical)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
