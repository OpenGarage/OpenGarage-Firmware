"""Exercise production options serialization and blank-secret save semantics."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'OpenGarage/main.cpp').read_text()
defines = (root / 'OpenGarage/defines.h').read_text()
names = re.findall(r'^\s*(OPTION_\w+)', defines, re.M)
serializer = re.search(r'void sta_options_fill_json\(.*?^}', source, re.M | re.S).group()
controller = re.search(r'void sta_controller_fill_json\(.*?^}', source, re.M | re.S).group()
save = source.split('// SECOND ROUND: change option values')[1].split('\n\tif(usi)')[0]
save = save[save.index('\n\to = og.options;'):]
validation = source.split('const char *cloud = req.getQueryParameter("cld");', 1)[1].split('\n\tchar *pem_value', 1)[0]
stub = r'''
#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <cassert>
#include <cstring>
#include "OpenGarage/json_escape.h"
#define F(x) x
#define HEX 16
#define STRING_RESERVE_SIZE 256
using byte=unsigned char;
struct String : std::string {
 using std::string::string; using std::string::operator=;
 String(unsigned n,int){std::ostringstream s;s<<std::hex<<n;assign(s.str());}
 template<class T> String& operator+=(T n){append(std::to_string(n));return *this;}
 String& operator+=(const char* s){append(s);return *this;}
 String& operator+=(const String& s){append(s);return *this;}
 String& operator+=(char c){push_back(c);return *this;}
 String operator+(const String& s)const{String r=*this;r.append(s);return r;}
 String operator+(const char* s)const{String r=*this;r.append(s);return r;}
 int toInt()const{return std::stoi(*this);}
};
struct OptionStruct {String name,sval;int ival=0,max=0;};
struct Request {
 std::map<std::string,std::string> values;
 char* getQueryParameter(const char* k){auto it=values.find(k);return it==values.end()?nullptr:&it->second[0];}
};
'''
setup = '\n'.join(f'og.options[{n}].name="{n[7:].lower()}";og.options[{n}].sval="SECRET-{n}";' for n in names)
code = stub + '\nenum {' + ','.join(names) + ',NUM_OPTIONS};\n'
code += '''struct {OptionStruct options[NUM_OPTIONS];int has_swrx=1;} og;
bool secplus2_identity_ready=true;unsigned secplus2_client_id=0x12342908;
String get_ap_ssid(){return "OG_TEST";}
'''
code += serializer + '\nvoid save(Request& req){OptionStruct* o;byte i;' + save + '}\n'
code += '''enum {CLOUD_NONE=0,CLOUD_OTC=2, OG_SN2_NONE=0, HTML_DATA_OUTOFBOUND=3,HTML_DATA_FORMATERROR=4};
int result=0,res=0;
void otf_send_result(int&,int code,const char*){result=code;}
void validate(Request& req){const char *cloud=req.getQueryParameter("cld");
''' + validation + '}\n'
code += '''
struct Garage {
 bool status_valid(){return true;} bool controls_faulted(){return false;}
 bool door_valid(){return true;} bool light_lock_valid(){return true;} bool obstruction_valid(){return true;}
 const char* recovery_status(){return "Waiting to retry synchronization";}
 const char* get_panel_description(){return "Panel timing insufficient for commands";}
 const char* get_action_result(){return "Bus wait expired; no command";}
 int get_panel_emu_status(){return 2;}
} secplus1_garage,secplus2_garage;
unsigned distance=450,sn2_value=1,door_status=5,vehicle_status=2,read_cnt=255;
unsigned light_status=1,lock_status=1,obstruction_status=1,opening_count=4294967295U;
int tempC=-40,humid=100;
String get_mac(){return "aa:bb:cc:dd:ee:ff";}
struct {unsigned getChipId(){return 16777215;}} ESP;
struct {int RSSI(){return -100;}} WiFi;
struct Cloud {int getCloudStatus(){return 1;}} cloud,*otf=&cloud;
''' + controller
code += 'int main(){\n' + setup + r'''
 og.options[OPTION_NAME].sval="name\"\\\n\t\x01";
 String out;sta_options_fill_json(out);std::cout<<out<<"\n";
 for(byte field:{byte(OPTION_AUTH),byte(OPTION_APWD),byte(OPTION_IFTT),byte(OPTION_MQPW)}){
  auto before=og.options[field].sval;
  Request empty;save(empty);assert(og.options[field].sval==before);
  Request blank;blank.values[og.options[field].name]="";save(blank);assert(og.options[field].sval==before);
  Request replace;replace.values[og.options[field].name]="new";save(replace);assert(og.options[field].sval=="new");
 }
 for(byte field:{byte(OPTION_AUTH),byte(OPTION_APWD),byte(OPTION_IFTT),byte(OPTION_MQPW)}){
  Request clear;clear.values[String("clear_")+og.options[field].name]="1";
  save(clear);assert(og.options[field].sval.empty());
 }
 sta_options_fill_json(out);std::cout<<out<<"\n";
 og.options[OPTION_AUTH].sval=String(32,'a');
 for(int old:{0,1,2}){
  og.options[OPTION_CLD].ival=old;
  Request retired;retired.values["cld"]="1";result=0;validate(retired);assert(result!=0);
  Request disable;disable.values["cld"]="0";result=0;validate(disable);assert(result==0);
  Request otc;otc.values["cld"]="2";result=0;validate(otc);assert((result==0)==(old==2));
  otc.values["auth"]=std::string(32,'b');result=0;validate(otc);assert(result==0);
  otc.values["clear_auth"]="1";result=0;validate(otc);assert(result!=0);
 }
 Request conflict;conflict.values["apwd"]="new";conflict.values["clear_apwd"]="1";
 result=0;validate(conflict);assert(result!=0);
 og.options[OPTION_NAME].sval=String(64,'\x01');
 og.options[OPTION_CLD].ival=CLOUD_OTC;
 og.options[OPTION_SN2].ival=1;og.options[OPTION_TSN].ival=1;
 for(int protocol:{0,1,2}){
  og.options[OPTION_SECV].ival=protocol;
  sta_controller_fill_json(out);
  assert(out.length()+64+9+7<=2048);
  std::cout<<out<<"\n";
 }
}
'''
with tempfile.TemporaryDirectory(prefix='og-options-') as tmp:
    cpp = Path(tmp)/'test.cpp'
    cpp.write_text(code)
    binary = Path(tmp)/'test'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),str(cpp),'-o',str(binary)],check=True)
    before, after, *mqtt = map(json.loads, subprocess.check_output([str(binary)],text=True).splitlines())
    for key in ('auth','apwd','iftt','pass','dkey','mqpw'):
        assert key not in before and key not in after, key
    for key in ('auth_set','apwd_set','iftt_set'):
        assert before[key] == 1 and after[key] == 0
    assert before['name'] == 'name"\\\n\t\x01'
    assert [value['secv'] for value in mqtt] == [0, 1, 2]
    assert all(value['name'] == '\x01'*64 for value in mqtt)
print('PASS: write-only credentials, save/migration policy, escaped MQTT JSON and packet capacity')
