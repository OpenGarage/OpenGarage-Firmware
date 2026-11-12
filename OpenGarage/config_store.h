#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
#include <pgmspace.h>
#define OG_CONFIG_FORMAT snprintf_P
#define OG_CONFIG_LITERAL(s) PSTR(s)
#else
#define OG_CONFIG_FORMAT snprintf
#define OG_CONFIG_LITERAL(s) (s)
#endif

// Envelope v1 is frozen: future option/schema versions belong in the payload.
// An incompatible envelope must not be deployed without a downgrade conversion
// path. Unknown/damaged envelope headers are never accepted as legacy files.
// File API only; host tests inject torn writes and interruption at every step.
namespace og_config {
static const char current[] = "/config.dat";
static const char pending[] = "/config.new";
static const char header[] = "ogcfg:1\n";
constexpr size_t footer_size = 24; // ogcrc: + 8 hex + : + 8 hex + newline
inline uint32_t crc_byte(uint32_t crc, uint8_t b) {
 crc ^= b;
 for (unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320UL:0);
 return crc;
}
enum class Kind : uint8_t { MISSING, INVALID, LEGACY, VERIFIED };
enum class Status : uint8_t { NORMAL, FRESH, MIGRATED, RECOVERED, CORRUPT, SAVE_FAILED, MOUNT_FAILED };
template<class File> struct Writer {
 explicit Writer(File &f):file(f) {}
 File &file; uint32_t crc=~uint32_t(0), length=0; bool ok=true;
 bool write(const char *s, size_t n) {
  if (!ok || n>UINT32_MAX-length) return ok=false;
  if (file.write(reinterpret_cast<const uint8_t*>(s),n)!=n) return ok=false;
  for(size_t i=0;i<n;++i) crc=crc_byte(crc,uint8_t(s[i]));
  length+=n;return true;
 }
 bool text(const char *s) { return write(s,strlen(s)); }
 bool number(uint32_t n) { char b[11];OG_CONFIG_FORMAT(b,sizeof(b),OG_CONFIG_LITERAL("%lu"),(unsigned long)n);return text(b); }
};
inline bool hex8(const char *p,uint32_t &out) {
 out=0;
 for(unsigned i=0;i<8;++i){unsigned c=uint8_t(p[i]);unsigned v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:16;if(v>15)return false;out=(out<<4)|v;}
 return true;
}
template<class FS,class Legacy> Kind inspect(FS &fs,const char *path,Legacy legacy) {
 if(!fs.exists(path))return Kind::MISSING;
 auto f=fs.open(path,"r");if(!f)return Kind::INVALID;
 char h[sizeof(header)-1];bool modern=f.readBytes(h,sizeof(h))==sizeof(h)&&memcmp(h,header,sizeof(h))==0;
 if(!modern){f.seek(0);bool valid=legacy(f);f.close();return valid?Kind::LEGACY:Kind::INVALID;}
 if(f.size()<sizeof(h)+footer_size){f.close();return Kind::INVALID;}
 uint32_t size=f.size(),n=0,expected=0;char tail[footer_size];
 f.seek(size-footer_size);
 bool valid=f.readBytes(tail,sizeof(tail))==sizeof(tail)&&memcmp(tail,"ogcrc:",6)==0&&tail[14]==':'&&tail[23]=='\n'&&hex8(tail+6,n)&&hex8(tail+15,expected)&&n==size-sizeof(h)-footer_size;
 if(valid){f.seek(sizeof(h));uint32_t crc=~uint32_t(0);for(uint32_t i=0;i<n;++i){int c=f.read();if(c<0){valid=false;break;}crc=crc_byte(crc,uint8_t(c));}valid=valid&&(~crc==expected);}
 f.close();return valid?Kind::VERIFIED:Kind::INVALID;
}
struct Recovery { const char *path; Kind kind; Status status; };
template<class FS,class Legacy> Recovery recover(FS &fs,Legacy legacy) {
 auto a=inspect(fs,current,legacy);
 if(a==Kind::VERIFIED||a==Kind::LEGACY)return {current,a,Status::NORMAL};
 auto b=inspect(fs,pending,legacy);
 if(b==Kind::VERIFIED){
  if((a==Kind::MISSING||fs.remove(current))&&fs.rename(pending,current))return {current,b,Status::RECOVERED};
  return {pending,b,Status::RECOVERED}; // Keep the only durable copy, even if rename failed.
 }
 return {nullptr,Kind::INVALID,a==Kind::MISSING&&b==Kind::MISSING?Status::FRESH:Status::CORRUPT};
}
template<class FS,class Legacy,class Serialize> bool save(FS &fs,Legacy legacy,Serialize serialize) {
 auto old=recover(fs,legacy);
 // Never truncate the sole recoverable copy, nor overwrite unrecoverable evidence.
 if(old.status==Status::CORRUPT||(old.path&&strcmp(old.path,pending)==0))return false;
 auto f=fs.open(pending,"w");if(!f)return false;
 bool ok=f.write(reinterpret_cast<const uint8_t*>(header),sizeof(header)-1)==sizeof(header)-1;
 Writer<decltype(f)> out{f};
 if(ok)ok=serialize(out)&&out.ok;
 char tail[footer_size+1];
 if(ok){OG_CONFIG_FORMAT(tail,sizeof(tail),OG_CONFIG_LITERAL("ogcrc:%08lx:%08lx\n"),(unsigned long)out.length,(unsigned long)~out.crc);ok=f.write(reinterpret_cast<const uint8_t*>(tail),footer_size)==footer_size;}
 f.flush();f.close();
 if(!ok||inspect(fs,pending,legacy)!=Kind::VERIFIED){fs.remove(pending);return false;}
 // Keep the verified pending copy even if deleting current reports failure.
 // Never destroy the recovery copy after an ambiguous filesystem error.
 if(fs.exists(current)&&!fs.remove(current))return false;
 // A verified pending file is committed once the old current is gone.
 // Rename failure does not turn a durable save into a reported failure.
 fs.rename(pending,current);return true;
}
template<class FS> bool reset(FS &fs) {
 // Delete pending first, so interruption cannot resurrect it after deleting current.
 if(fs.exists(pending)&&!fs.remove(pending))return false;
 return !fs.exists(current)||fs.remove(current);
}
} // namespace og_config

#undef OG_CONFIG_FORMAT
#undef OG_CONFIG_LITERAL
