"""New-key rules, unchanged legacy keys, real /co key validation and browser parity."""
from pathlib import Path
import json,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'OpenGarage/main.cpp').read_text()
validation=source.split('// Check device key change',1)[1].split('// SECOND ROUND:',1)[0]
apply=re.search(r'\tif\(nkey != NULL\) \{\s*og.options\[OPTION_DKEY\].sval = nkey;.*?\n\t}',source,re.S).group()
code=r'''
#include <cassert>
#include <string>
#include <map>
#include <iostream>
#include "OpenGarage/device_key_policy.h"
enum {OPTION_DKEY,OPTION_KSET,HTML_DATA_OUTOFBOUND=17,HTML_MISMATCH=3,HTML_DATA_MISSING=16};
struct {struct Option {std::string sval;unsigned ival=0;} options[2];} og;
struct Request {std::map<std::string,std::string> values;char *getQueryParameter(const char *key){auto it=values.find(key);return it==values.end()?nullptr:&it->second[0];}};
int result;std::string item;
void otf_send_result(int&,int value,const char *field){result=value;item=field;}
void change(Request &req){int res=0;
'''+validation+apply+r'''
result=1;
}
int main(){
 assert(!og_key::valid_new(nullptr));
 for(const char *old:{"opendoor","short","old-\xc3\xa9",""}) {
  og.options[OPTION_DKEY].sval=old;og.options[OPTION_KSET].ival=1;
  Request same;same.values["nkey"]=same.values["ckey"]=old;change(same);
  assert(result==1&&og.options[OPTION_DKEY].sval==old&&og.options[OPTION_KSET].ival==1);
  Request bad;bad.values["nkey"]=bad.values["ckey"]="bad";change(bad);
  assert(result==17&&item=="nkey"&&og.options[OPTION_DKEY].sval==old);
 }
 og.options[OPTION_DKEY].sval="opendoor";og.options[OPTION_KSET].ival=0;
 Request first;first.values["nkey"]=first.values["ckey"]="opendoor";change(first);
 assert(result==1&&og.options[OPTION_KSET].ival==0);
 first.values["nkey"]="long new key";change(first);assert(result==HTML_MISMATCH);
 first.values["ckey"]="long new key";change(first);assert(result==1&&og.options[OPTION_KSET].ival==1);
 std::string input;
 while(std::getline(std::cin,input))std::cout<<og_key::valid_new(input.c_str())<<'\n';
}
'''
# Avoid newline/NUL in the line-oriented parity protocol; both rules reject controls.
cases=['','opendoor','a'*9,'a'*10,'a'*64,'a'*65,' '*10,' aaaaaaaaa','aaaaaaaaa ','a nice passphrase','abcdefghijé','😀'*10,'012345678&+%','old\tkey long']
cases += ['aaaaa'+chr(c)+'aaaaa' for c in range(1,256) if c not in (10,13)]
js=(root/'OpenGarage/html/_og.js').read_text()
fn=re.search(r'function validNewDeviceKey\(key\) \{.*?^}',js,re.M|re.S).group()
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test'
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),'-x','c++','-','-o',str(p)],input=code,text=True,check=True)
 actual=subprocess.check_output([str(p)],input='\n'.join(cases)+'\n',text=True).splitlines()
 browser=json.loads(subprocess.check_output(['node','-e',fn+';console.log(JSON.stringify('+json.dumps(cases)+'.map(validNewDeviceKey)))'],text=True))
 assert [x=='1' for x in actual]==browser
html=(root/'OpenGarage/html/sta_ui.html').read_text()
assert 'submittedNewKey !== null' in html and 'okey.value = submittedNewKey; saveDeviceKey(okey); dkey.value = submittedNewKey' in html
print('PASS: new-key boundaries/ASCII, browser parity, unchanged legacy credentials, real /co key checks and marker update')
