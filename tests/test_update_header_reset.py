"""Regression: missing credentials must not inherit a previous HTTP header.
Model ESP8266 String's length-only clear(), unlike std::string::clear().
Compile the actual parser reset loop and production credential comparator.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'OpenGarage/lib/ESP8266WebServer/src/Parsing-impl.h').read_text()
start=source.index('for (int i = 0; i < _headerKeysCount; ++i)')
end=source.index('\n   }',start)+5
reset=source[start:end]
code=r"""
#include <cassert>
#include <cstring>
#include <string>
#include "OpenGarage/update_auth.h"
struct String {
 std::string storage; size_t length=0;
 void clear(){length=0;} // Actual ESP8266 core semantics: no terminating NUL write.
 String& operator=(const char*s){storage=s;length=storage.size();return *this;}
 const char*c_str()const{return storage.c_str();}
};
struct Header {String value;};
struct Server {
 Header _currentHeaders[5];int _headerKeysCount=5;
 void reset(){
"""+reset+r"""
 }
};
int main(){
 Server s;
 // Establish why a std::string-based fake cannot detect this defect.
 String stale;stale="hex:74657374";stale.clear();
 assert(stale.length==0 && update_key_matches(stale.c_str(),"test"));
 for(int n=0;n<100;++n){
  // Completed authorized request, then an omitted key/token/release header.
  s._currentHeaders[0].value="hex:74657374";
  s._currentHeaders[1].value="upload-token";
  s._currentHeaders[2].value="release";
  s.reset();
  for(auto&h:s._currentHeaders)assert(h.value.c_str()[0]==0);
  assert(!update_key_matches(s._currentHeaders[0].value.c_str(),"test"));
  // Explicit wrong key remains rejected, valid next request still works.
  s._currentHeaders[0].value="hex:77726f6e67";
  assert(!update_key_matches(s._currentHeaders[0].value.c_str(),"test"));
  s.reset();s._currentHeaders[0].value="hex:74657374";
  assert(update_key_matches(s._currentHeaders[0].value.c_str(),"test"));
 }
}
"""
with tempfile.TemporaryDirectory() as t:
 p=Path(t)/'test'
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),'-x','c++','-','-o',str(p)],input=code,text=True,check=True)
 subprocess.run([str(p)],check=True)
# Prove the regression catches the old length-only reset.
with tempfile.TemporaryDirectory() as t:
 p=Path(t)/'old'
 broken=code.replace('_currentHeaders[i].value = "";','_currentHeaders[i].value.clear();')
 assert broken!=code
 subprocess.run(['g++','-std=c++11','-I',str(root),'-x','c++','-','-o',str(p)],input=broken,text=True,check=True)
 result=subprocess.run([str(p)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 assert result.returncode!=0, 'Old reset must fail the regression'
print('Updater request header reset: fixed passes, old clear fails PASS')
