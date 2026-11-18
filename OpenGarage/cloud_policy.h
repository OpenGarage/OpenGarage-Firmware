#pragma once
#include <stdint.h>
#include <strings.h>
#include <stddef.h>
namespace og_cloud {
inline bool legacy_default(const char *host,unsigned port) {
 return port==80 && (!host || !*host || !strcasecmp(host,"ws.cloud.openthings.io") || !strcasecmp(host,"ws.openthings.io") || !strcasecmp(host,"blynk.openthings.io"));
}
inline bool flag(const char *s){return !s || ((s[0]=='0'||s[0]=='1')&&!s[1]);}
inline bool valid_host(const char *s){
 if(!s||!*s)return false;
 size_t n=0;
 for(;s[n];++n){const unsigned char c=s[n];if(n>=64 || !((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_'))return false;}
 return true;
}
inline bool valid_port(const char *s){
 if(!s||!*s)return false;
 unsigned n=0;
 for(;*s;++s){if(*s<'0'||*s>'9')return false;n=n*10+(*s-'0');if(n>65535)return false;}return n>0;
}
struct Endpoint {const char *host;uint16_t port;bool secure;};
inline Endpoint endpoint(bool custom,bool disabled,const char *host,uint16_t port) {
 return {custom?host:(disabled?"ws.cloud.openthings.io":"ws.openthings.io"),uint16_t(custom?port:(disabled?80:443)),!disabled};
}
}
