#pragma once
#include <stdint.h>
#include <stddef.h>

namespace og_storage {
// Only an entirely erased filesystem partition can be initialized automatically.
// Never infer "blank" from a failed mount or from a few erased metadata pages.
template<class Read, class Feed> bool blank_partition(uint32_t start, uint32_t end,
                                                     uint32_t chip_size, Read read, Feed feed) {
 if(start>=end || end>chip_size || (start&3) || (end&3))return false;
 uint32_t words[32];
 for(uint32_t at=start;at<end;) {
  size_t n=end-at<sizeof(words)?end-at:sizeof(words);
  if(!read(at,words,n))return false;
  for(size_t i=0;i<n/sizeof(words[0]);++i)if(words[i]!=UINT32_MAX)return false;
  at+=n;
  if(((at-start)&4095)==0)feed();
 }
 return true;
}
// Caller must disable the filesystem's own auto-format before entering here.
template<class FS, class Blank> bool mount(FS &fs, Blank blank) {
 if(fs.begin())return true;
 if(!blank())return false;
 return fs.format() && fs.begin();
}
// Used only after the physical factory-reset gesture on an unmountable partition.
template<class FS> bool physical_recovery(FS &fs) {
 return fs.format() && fs.begin();
}
} // namespace og_storage
