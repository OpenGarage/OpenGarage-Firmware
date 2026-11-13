"""Execute actual cloud field validation and legacy endpoint edit handling."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'OpenGarage/main.cpp').read_text()
a=s.index('    for(const char *field:{"otcm","nossl"})');b=s.index('\n\tbyte i;',a);validation=s[a:b]
a=s.index('    if(legacy_endpoint_edit){',b);b=s.index('\n\tfor (byte field',a);mutation=s[a:b]
code=r'''
#include "OpenGarage/cloud_policy.h"
#include <map>
#include <string>
#include <cstdlib>
#include <cassert>
#include <cstring>
using String=std::string;
struct Text:std::string{using std::string::string;using std::string::operator=;int toInt()const{return strtol(c_str(),nullptr,10);}};
// Arduino String's conversion used only for legacy port classification.
#define String Text
enum{OPTION_OTCM,OPTION_NOSSL,OPTION_BDMN,OPTION_BPRT,HTML_DATA_OUTOFBOUND=17};
struct Opt{unsigned ival=0;Text sval;};struct{Opt options[4];}og;
struct Request{std::map<std::string,std::string> p;char *getQueryParameter(const char*k)const{auto it=p.find(k);return it==p.end()?nullptr:const_cast<char*>(it->second.c_str());}};
int res=0,result=0;std::string item;void otf_send_result(int&,int code,const char*field){result=code;item=field;}
void apply(const Request&req){
'''+validation+r'''
for(auto v:{std::make_pair("otcm",OPTION_OTCM),std::make_pair("nossl",OPTION_NOSSL)}){
 const char *p=req.getQueryParameter(v.first);if(p)og.options[v.second].ival=*p-'0';}
'''+mutation+r'''
}
void init(){result=0;item="";og.options[OPTION_OTCM].ival=0;og.options[OPTION_NOSSL].ival=0;og.options[OPTION_BDMN].sval="ws.cloud.openthings.io";og.options[OPTION_BPRT].ival=80;}
int main(){
 init();apply({});assert(!result&&!og.options[OPTION_OTCM].ival&&!og.options[OPTION_NOSSL].ival);
 Request legacy;legacy.p={{"bdmn","self.example"},{"bprt","8080"}};init();apply(legacy);assert(!result&&og.options[OPTION_OTCM].ival==1&&og.options[OPTION_NOSSL].ival==1);
 init();og.options[OPTION_OTCM].ival=1;og.options[OPTION_NOSSL].ival=0;
 og.options[OPTION_BDMN].sval="self.example";og.options[OPTION_BPRT].ival=8080;
 apply(legacy);assert(!result&&og.options[OPTION_OTCM].ival==1&&og.options[OPTION_NOSSL].ival==0); // Old client must not clear an existing explicit TLS choice.
 Request secure=legacy;secure.p["nossl"]="0";init();apply(secure);assert(!result&&og.options[OPTION_OTCM].ival==1&&!og.options[OPTION_NOSSL].ival);
 Request explicitDefault=legacy;explicitDefault.p["otcm"]="0";init();apply(explicitDefault);assert(!result&&!og.options[OPTION_OTCM].ival&&!og.options[OPTION_NOSSL].ival);
 for(auto pair:{std::make_pair("otcm","true"),std::make_pair("nossl","2"),std::make_pair("bprt","443bad"),std::make_pair("bdmn","http://self.example")}){
  init();Request bad=legacy;bad.p[pair.first]=pair.second;apply(bad);assert(result==17&&!og.options[OPTION_OTCM].ival&&!og.options[OPTION_NOSSL].ival);
 }
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.cpp').write_text(code)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('Actual cloud form: strict fields, no partial mode mutation, legacy API custom/plain migration and explicit selection PASS')
