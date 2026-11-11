#include "OpenGarage/config_store.h"
#include <cassert>
#include <string>
#include <map>
#include <memory>
#include <stdexcept>
#include <iostream>
struct PowerLoss {};
struct FS {
 std::map<std::string,std::string> disk;
 int cut=-1,mutations=0,short_write=-1,written=0;
 bool remove_fail=false,rename_fail=false,open_fail=false,corrupt_flush=false;
 unsigned handles=0,peak=0;
 void tick(){if(cut>=0 && mutations++==cut)throw PowerLoss{};}
 struct File {
  FS *fs=nullptr;std::string name;size_t pos=0;bool valid=false;
  File()=default;
  File(FS &f,std::string n):fs(&f),name(n),valid(true){++fs->handles;fs->peak=std::max(fs->peak,fs->handles);}
  File(const File&)=delete;
  File(File&&o):fs(o.fs),name(o.name),pos(o.pos),valid(o.valid){o.valid=false;}
  ~File(){close();}
  explicit operator bool()const{return valid;}
  size_t size(){return fs->disk[name].size();}
  bool seek(size_t n){pos=n;return true;}
  int read(){return pos<size()?uint8_t(fs->disk[name][pos++]):-1;}
  size_t readBytes(char*b,size_t n){size_t i=0;for(;i<n;++i){int c=read();if(c<0)break;b[i]=c;}return i;}
  size_t write(const uint8_t*b,size_t n){
   size_t i=0;for(;i<n;++i){if(fs->short_write>=0&&fs->written++>=fs->short_write)break;fs->disk[name].push_back(b[i]);fs->tick();}return i;
  }
  void flush(){if(fs->corrupt_flush&&!fs->disk[name].empty())fs->disk[name][0]='X';fs->tick();}
  void close(){if(valid){valid=false;--fs->handles;}}
 };
 bool exists(const char*n){return disk.count(n);}
 File open(const char*n,const char*mode){
  if(open_fail)return {};
  if(*mode=='w'){disk[n]="";tick();}
  if(!exists(n))return {};
  return File(*this,n);
 }
 bool remove(const char*n){if(remove_fail)return false;disk.erase(n);tick();return true;}
 bool rename(const char*a,const char*b){if(rename_fail||exists(b))return false;disk[b]=disk.at(a);disk.erase(a);tick();return true;}
};
bool legacy(FS::File &f){std::string s;int c;while((c=f.read())>=0)s+=char(c);return s=="fwv:126\ndkey:old\n";}
auto payload(const std::string&s){return [s](auto&o){return o.text(s.c_str());};}
std::string readChosen(FS &fs){auto r=og_config::recover(fs,legacy);if(!r.path)return "";return fs.disk[r.path];}
int main(){
 using namespace og_config;
 FS base;assert(save(base,legacy,payload("fwv:127\ndkey:old\n")));auto old=base.disk[current];
 FS newer=base;assert(save(newer,legacy,payload("fwv:127\ndkey:new\n")));auto fresh=newer.disk[current];
 unsigned cuts=0;
 for(int n=0;n<200;++n){FS fs=base;fs.cut=n;
  try {save(fs,legacy,payload("fwv:127\ndkey:new\n"));}catch(PowerLoss&){++cuts;}
  fs.cut=-1;auto value=readChosen(fs);assert(value==old||value==fresh);assert(fs.handles==0&&fs.peak<=1);
 }
 assert(cuts>50);
 // With no previous settings, an interrupted first save is either absent,
 // recoverable, or explicitly corrupt; it must never be accepted as legacy.
 for(int n=0;n<100;++n){FS fs;fs.cut=n;
  try{save(fs,legacy,payload("fwv:127\ndkey:new\n"));}catch(PowerLoss&){}
  fs.cut=-1;auto r=recover(fs,legacy);
  assert(r.kind!=Kind::LEGACY);
  if(r.path)assert(fs.disk[r.path]==fresh);
  else assert(r.status==Status::FRESH||r.status==Status::CORRUPT);
 }

 for(int n=0;n<int(fresh.size());++n){FS fs=base;fs.short_write=n;assert(!save(fs,legacy,payload("fwv:127\ndkey:new\n")));assert(readChosen(fs)==old);}
 {FS fs=base;fs.remove_fail=true;assert(!save(fs,legacy,payload("fwv:127\ndkey:new\n")));assert(readChosen(fs)==old);}
 {FS fs=base;fs.rename_fail=true;assert(save(fs,legacy,payload("fwv:127\ndkey:new\n")));assert(readChosen(fs)==fresh);auto pendingCopy=fs.disk[pending];assert(!save(fs,legacy,payload("different")));assert(fs.disk[pending]==pendingCopy);fs.rename_fail=false;assert(readChosen(fs)==fresh&&fs.exists(current));}
 {FS fs=base;fs.corrupt_flush=true;assert(!save(fs,legacy,payload("new")));assert(readChosen(fs)==old);}
 {FS fs=base;fs.open_fail=true;assert(!save(fs,legacy,payload("new")));assert(fs.disk[current]==old);}
 for(size_t n=0;n<fresh.size();++n){FS fs;fs.disk[current]=fresh.substr(0,n);assert(inspect(fs,current,legacy)==Kind::INVALID);assert(recover(fs,legacy).status==Status::CORRUPT);}
 for(size_t n=0;n<fresh.size();++n){FS fs;fs.disk[current]=fresh;fs.disk[current][n]^=1;assert(inspect(fs,current,legacy)==Kind::INVALID);}
 {FS fs;fs.disk[current]="fwv:126\ndkey:old\n";assert(inspect(fs,current,legacy)==Kind::LEGACY);assert(save(fs,legacy,payload("fwv:127\ndkey:old\n")));assert(inspect(fs,current,legacy)==Kind::VERIFIED);}
 {FS fs=base;fs.disk[pending]=fresh;assert(readChosen(fs)==old);assert(reset(fs));assert(recover(fs,legacy).status==Status::FRESH);}
 for(int n=0;n<2;++n){FS fs=base;fs.disk[pending]=fresh;fs.cut=n;try{reset(fs);}catch(PowerLoss&){}fs.cut=-1;auto value=readChosen(fs);assert(value==old||value.empty());}
 {FS fs;fs.disk[current]="broken";assert(!save(fs,legacy,payload("defaults")));assert(fs.disk[current]=="broken");}
 std::cout<<"config store: interruption, short write, corruption, rename/remove/open failure, legacy, reset, handle bounds PASS\n";
}
