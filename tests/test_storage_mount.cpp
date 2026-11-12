#include "OpenGarage/storage_mount.h"
#include <cassert>
#include <vector>
#include <cstring>
#include <iostream>
struct Flash {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(12288,255);
 int fail_at=-1;unsigned reads=0,feeds=0;
 bool blank(unsigned start=4096,unsigned end=12288,unsigned chip=12288) {
  reads=feeds=0;
  return og_storage::blank_partition(start,end,chip,[&](uint32_t at,uint32_t *out,size_t n){
   ++reads;assert(at>=4096&&at+n<=bytes.size());
   if(fail_at>=0&&at<=unsigned(fail_at)&&at+n>unsigned(fail_at))return false;
   memcpy(out,bytes.data()+at,n);return true;
  },[&](){++feeds;});
 }
};
struct FS {
 unsigned begins=0,formats=0;bool existing=false,format_ok=true,mount_after=true;
 bool begin(){++begins;return existing||(formats&&mount_after);}
 bool format(){++formats;return format_ok;}
};
int main(){
 Flash flash;assert(flash.blank()&&flash.reads==64&&flash.feeds==2);
 for(unsigned at:{4096u,4097u,4223u,4224u,8192u,12287u}){
  flash.bytes[at]=0;assert(!flash.blank());flash.bytes[at]=255;
 }
 flash.fail_at=12287;assert(!flash.blank());flash.fail_at=-1;
 assert(!flash.blank(4096,4096)&&flash.reads==0);
 assert(!flash.blank(4097,12288)&&flash.reads==0);
 assert(!flash.blank(4096,12287)&&flash.reads==0);
 assert(!flash.blank(4096,12288,8192)&&flash.reads==0);
 assert(flash.blank(4096,4100)&&flash.reads==1);
 // Bytes outside the FS partition do not make a fresh partition nonblank.
 flash.bytes[0]=0;assert(flash.blank());
 for(bool existing:{false,true})for(bool blank:{false,true}){
  FS fs;fs.existing=existing;unsigned probes=0;
  bool result=og_storage::mount(fs,[&](){++probes;return blank;});
  assert(result==(existing||blank));assert(probes==unsigned(!existing));
  assert(fs.formats==unsigned(!existing&&blank));
 }
 {FS fs;fs.format_ok=false;assert(!og_storage::mount(fs,[]{return true;}));assert(fs.begins==1&&fs.formats==1);}
 {FS fs;fs.mount_after=false;assert(!og_storage::mount(fs,[]{return true;}));assert(fs.begins==2&&fs.formats==1);}
 {FS fs;assert(!og_storage::mount(fs,[]{return false;}));assert(fs.formats==0);
  assert(og_storage::physical_recovery(fs));assert(fs.formats==1);}
 {FS fs;fs.format_ok=false;assert(!og_storage::physical_recovery(fs));assert(fs.begins==0);}
 {FS fs;fs.mount_after=false;assert(!og_storage::physical_recovery(fs));assert(fs.formats==1&&fs.begins==1);}
 std::cout<<"storage mount: partition bounds, full erased scan, read errors, format admission and physical recovery PASS\n";
}
