"""Exercise the vendored MQTT parser with real PUBLISH packets and fake transport."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
headers={
'Arduino.h':'''#pragma once
#include <stdint.h>
#include <cstdlib>
#include <cstring>
using byte=uint8_t; using boolean=bool;
uint32_t millis();
#define PROGMEM
#define pgm_read_byte_near(x) (*(x))
#define yield() ((void)0)
''',
'IPAddress.h':'''#pragma once
#include <stdint.h>
#include <cstring>
struct IPAddress { uint8_t a[4]={}; IPAddress(){} IPAddress(uint32_t n){memcpy(a,&n,4);} IPAddress(const uint8_t*p){memcpy(a,p,4);} IPAddress(uint8_t x,uint8_t y,uint8_t z,uint8_t w):a{x,y,z,w}{} uint8_t& operator[](int i){return a[i];} };
''',
'Print.h':'''#pragma once
#include <stddef.h>
#include <stdint.h>
class Print {public:virtual size_t write(uint8_t)=0;};
''',
'Stream.h':'''#pragma once
#include "Print.h"
class Stream:public Print {public:size_t write(uint8_t) override{return 1;}};
''',
'Client.h':'''#pragma once
#include "IPAddress.h"
#include "Stream.h"
class Client: public Stream {public:
virtual int connect(IPAddress,uint16_t)=0;
virtual int connect(const char*,uint16_t)=0;
virtual size_t write(uint8_t)=0;
virtual size_t write(const uint8_t*,size_t)=0;
virtual int available()=0;virtual int read()=0;
virtual int read(uint8_t*,size_t)=0;virtual int peek()=0;
virtual void flush()=0;virtual void stop()=0;
virtual uint8_t connected()=0;virtual operator bool()=0;
};
'''}
code=r'''
#include "PubSubClient.h"
#include "OpenGarage/network_policy.h"
#include <vector>
#include <cassert>
#include <cstdio>
uint32_t millis(){static uint32_t now=0;return ++now;}
struct Fake:Client {
 std::vector<uint8_t> input; size_t pos=0; bool online=false;
 void feed(std::vector<uint8_t> b){input=b;pos=0;}
 int connect(IPAddress,uint16_t) override {online=true;return 1;}
 int connect(const char*,uint16_t) override {online=true;return 1;}
 size_t write(uint8_t) override{return 1;}
 size_t write(const uint8_t*,size_t n) override{return n;}
 int available() override{return input.size()-pos;}
 int read() override {return available()?input[pos++]:-1;}
 int read(uint8_t*p,size_t n) override{size_t i=0;while(i<n&&available())p[i++]=read();return i;}
 size_t peekBytes(uint8_t*p,size_t n){size_t count=0;while(count<n && pos+count<input.size()){p[count]=input[pos+count];++count;}return count;}
 int peek() override{return available()?input[pos]:-1;}
 void flush() override{} void stop() override {online=false;}
 uint8_t connected() override{return online;} operator bool() override{return online;}
};
Fake wificlient; PubSubClient mqttclient(wificlient);
FRAME_FUNCTION
PubSubClient *active;bool expected;unsigned calls=0,actions=0;
void callback(char* topic,byte* p,unsigned n) {
 ++calls;assert(active->incomingRetained()==expected);
 auto c=og_network::command(p,n,active->incomingRetained(),strcmp(topic,"og/IN/STATE")==0,true);
 if(c!=og_network::Command::NONE)++actions;
 active->publish("reply","ok"); // overwrites the packet buffer
 assert(active->incomingRetained()==expected);
}
int main(){
 wificlient.online=true;wificlient.feed({0x30});assert(!mqtt_frame_ready());
 wificlient.feed({0x30,4,0,1,'t'});assert(!mqtt_frame_ready());
 wificlient.feed({0x30,4,0,1,'t','x'});assert(mqtt_frame_ready());
 wificlient.feed({0x30,4,0});for(int i=0;i<1100;++i)mqtt_frame_ready();assert(!wificlient.online);
 wificlient.online=true;wificlient.feed({0x30,0xff,0xff,0xff,0xff});assert(!mqtt_frame_ready());assert(!wificlient.online);
 wificlient.online=true;wificlient.feed({0x30,0xff,0x7f});assert(!mqtt_frame_ready());assert(!wificlient.online);

 Fake transport;PubSubClient client(transport);active=&client;
 client.setServer(IPAddress(127,0,0,1),1883);client.setCallback(callback);
 for(int connection=0;connection<3;++connection){
  transport.feed({0x20,2,0,0});assert(client.connect("test"));
  for(bool qos:{false,true})for(bool retained:{true,false}){
   expected=retained;
   const char *topic="og/IN/STATE";size_t tl=strlen(topic);
   std::vector<uint8_t> packet={uint8_t(0x30|(qos?2:0)|(retained?1:0)),uint8_t(2+tl+(qos?2:0)+5),0,uint8_t(tl)};
   packet.insert(packet.end(),topic,topic+tl);if(qos){packet.push_back(0);packet.push_back(1);}
   packet.insert(packet.end(),{'c','l','i','c','k'});transport.feed(packet);assert(client.loop());
  }
  client.disconnect();
 }
 assert(calls==12 && actions==6);
 const uint8_t exact[5]={'c','l','i','c','k'};
 assert(og_network::command(exact,5,false,true,false)==og_network::Command::TOGGLE);
 assert(og_network::command(exact,4,false,true,false)==og_network::Command::NONE);
 assert(og_network::command(exact,5,true,true,false)==og_network::Command::NONE);
 const uint8_t bad[6]={'c','l','i','c','k',0};
 assert(og_network::command(bad,6,false,true,false)==og_network::Command::NONE);
 for(const char*s:{"Button","click","open","close","togglelight","togglelock"})
  assert(og_network::command((const uint8_t*)s,strlen(s),true,true,true)==og_network::Command::NONE);
 og_network::MqttRecovery recovery;
 assert(recovery.ready(0));recovery.attempt_result(100,true);
 recovery.observe(101,false);assert(!recovery.ready(5100));assert(!recovery.ready(60100));
 assert(recovery.ready(60101));recovery.attempt_result(60101,true);
 recovery.observe(60102,false);assert(recovery.retry.delay_ms==120000);
 assert(!recovery.ready(180101));assert(recovery.ready(180102));
 recovery.attempt_result(180102,true);recovery.observe(210101,true);
 assert(recovery.retry.failed);recovery.observe(210102,true);assert(!recovery.retry.failed);
 recovery.observe(210103,false);assert(!recovery.retry.failed);
 assert(!recovery.ready(215102));assert(recovery.ready(215103));
 recovery.attempt_result(215103,false); // fast retry fails: ordinary backoff resumes
 assert(recovery.retry.delay_ms==60000 && recovery.retry.failed);
 assert(!recovery.ready(275102));assert(recovery.ready(275103));
 og_network::MqttRecovery boundary;
 boundary.attempt_result(0,true);boundary.observe(29999,false);
 assert(boundary.retry.failed);assert(!boundary.ready(34999));
 og_network::MqttRecovery healthy;
 healthy.attempt_result(0xfffffff0U,true);
 healthy.observe(uint32_t(0xfffffff0U+30000U),false);
 assert(!healthy.retry.failed);
 assert(!healthy.ready(uint32_t(0xfffffff0U+34999U)));
 assert(healthy.ready(uint32_t(0xfffffff0U+35000U)));
 og_network::MqttRecovery wrap;
 wrap.attempt_result(0xfffffff0U,false);
 assert(!wrap.ready(4));assert(wrap.ready(uint32_t(0xfffffff0U+60000U)));
 // Independent floor still applies when backoff is cleared.
 wrap.retry.success();assert(!wrap.ready(uint32_t(0xfffffff0U+4999U)));
 assert(wrap.ready(uint32_t(0xfffffff0U+5000U)));
 og_network::Backoff retry(5000,300000);
 assert(retry.ready(0));retry.failure(0xfffffff0U);assert(!retry.ready(4));
 assert(retry.ready(uint32_t(0xfffffff0U+5000)));retry.failure(6000);assert(retry.delay_ms==10000);
 for(int i=0;i<20;++i)retry.failure(i);assert(retry.delay_ms==300000);retry.success();assert(retry.ready(0));
 puts("MQTT packets, retained replay rejection, bounded payload and backoff: PASS");
}
'''
source=(root/'OpenGarage/main.cpp').read_text()
frame=source[source.index('static bool mqtt_frame_ready()'):source.index('void mqtt_callback(')]
code=code.replace('FRAME_FUNCTION',frame)
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for n,s in headers.items():(p/n).write_text(s)
 (p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-g','-I'+d,'-I'+str(root),'-I'+str(root/'OpenGarage/lib/PubSubClient/src'),str(p/'test.cpp'),str(root/'OpenGarage/lib/PubSubClient/src/PubSubClient.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
