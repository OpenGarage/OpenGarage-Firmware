#pragma once
#include <stddef.h>
#include <string.h>

namespace og_key {
// New keys: printable ASCII, 10–64 bytes, no leading/trailing spaces.
// Existing keys are authenticated unchanged; this is not an authentication rule.
inline bool valid_new(const char *key) {
 if(!key)return false;
 size_t length=strlen(key);
 if(length<10 || length>64 || key[0]==' ' || key[length-1]==' ')return false;
 for(size_t i=0;i<length;++i) {
  unsigned char c=static_cast<unsigned char>(key[i]);
  if(c<32 || c>126)return false;
 }
 return true;
}
inline bool allowed_change(const char *current,const char *replacement) {
 return replacement && ((current && strcmp(current,replacement)==0) || valid_new(replacement));
}
} // namespace og_key
