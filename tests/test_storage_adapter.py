"""Run the actual SPIFFS mount adapter with fake flash/core calls."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'OpenGarage/OpenGarage.cpp').read_text()
fn=re.search(r'static bool mount_filesystem\(\).*?^}',s,re.M|re.S).group()
main=(root/'OpenGarage/main.cpp').read_text()
# Physical authorization exists only in the long-button branch; APIs use default false.
assert main.count('options_reset(true)')==1
assert re.search(r'if\(diff > BUTTON_FACRESET_TIMEOUT\) \{\s*if\(og.options_reset\(true\)\)',main)
assert '!og.config_locked && !FILESYS.exists(CONFIG_FNAME)' in main
assert 'if(config_status==uint8_t(og_config::Status::MOUNT_FAILED)) {options[OPTION_MOD].ival=OG_MOD_AP;return;}' in s
code=r'''
#include "OpenGarage/storage_mount.h"
#include <cassert>
#define FS_PHYS_ADDR 4096U
#define FS_PHYS_SIZE 8192U
struct SPIFFSConfig{bool automatic;explicit SPIFFSConfig(bool a):automatic(a){}};
struct {bool config_ok=true,mount_ok=true,after_ok=true,automatic=true;unsigned formats=0,starts=0;
 bool setConfig(SPIFFSConfig c){automatic=c.automatic;return config_ok;}
 bool begin(){++starts;assert(!automatic);return mount_ok||(formats&&after_ok);}
 bool format(){++formats;return true;}
} FILESYS;
struct {bool read_ok=true,dirty=false;unsigned reads=0;
 unsigned getFlashChipRealSize(){return 12288;}
 bool flashRead(uint32_t at,uint32_t *out,size_t n){
  ++reads;assert(at>=4096&&at+n<=12288);if(!read_ok)return false;
  for(size_t i=0;i<n/4;++i)out[i]=UINT32_MAX;
  if(dirty&&at+n==12288)out[n/4-1]=0;
  return true;
 }
} ESP;
void yield(){}
'''+fn+r'''
int main(){
 assert(mount_filesystem());assert(FILESYS.formats==0&&ESP.reads==0);
 FILESYS.mount_ok=false;ESP.dirty=true;
 assert(!mount_filesystem());assert(FILESYS.formats==0&&ESP.reads==64);
 ESP.dirty=false;ESP.read_ok=false;
 assert(!mount_filesystem());assert(FILESYS.formats==0);
 ESP.read_ok=true;FILESYS.config_ok=false;unsigned starts=FILESYS.starts;
 assert(!mount_filesystem());assert(FILESYS.starts==starts&&FILESYS.formats==0);
 FILESYS.config_ok=true;assert(mount_filesystem());assert(FILESYS.formats==1);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test'
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I',str(root),'-x','c++','-','-o',str(p)],input=code,text=True,check=True)
 subprocess.run([str(p)],check=True)
print('PASS: actual mount adapter disables auto-format, bounds flash reads, preserves nonblank/read failures; physical-only reset wiring')
