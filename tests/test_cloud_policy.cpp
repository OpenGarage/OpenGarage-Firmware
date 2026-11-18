#include <initializer_list>
#include "../OpenGarage/cloud_policy.h"
#include <cassert>
#include <cstring>
int main(){
 using namespace og_cloud;
 for(const char *s:{"ws.cloud.openthings.io","WS.CLOUD.OPENTHINGS.IO","ws.openthings.io","blynk.openthings.io",""})assert(legacy_default(s,80));
 assert(!legacy_default("custom.example",80));assert(!legacy_default("ws.cloud.openthings.io",8080));
 auto a=endpoint(false,false,"kept.example",8080);assert(a.secure&&a.port==443&&!strcmp(a.host,"ws.openthings.io"));
 auto b=endpoint(false,true,"kept.example",443);assert(!b.secure&&b.port==80&&!strcmp(b.host,"ws.cloud.openthings.io"));
 auto c=endpoint(true,false,"kept.example",80);assert(c.secure&&c.port==80&&!strcmp(c.host,"kept.example"));
 auto d=endpoint(true,true,"kept.example",443);assert(!d.secure&&d.port==443); // No port-based inference.
 for(const char *s:std::initializer_list<const char*>{nullptr,"0","1"})assert(flag(s));
 for(const char *s:{"","2","true","-1","01"})assert(!flag(s));
 for(const char *s:{"localhost","192.168.1.1","otc.example.org"})assert(valid_host(s));
 for(const char *s:{"","http://x","a/b","a b","x\r\ny"})assert(!valid_host(s));
 for(const char *s:{"1","80","443","65535"})assert(valid_port(s));
 for(const char *s:{"","0","65536","-1","443junk","99999999999999999999"})assert(!valid_port(s));
}
